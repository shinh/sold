#include <assert.h>
#include <elf.h>
#include <err.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <vector>

#define Elf_Ehdr Elf64_Ehdr
#define Elf_Phdr Elf64_Phdr
#define Elf_Dyn Elf64_Dyn
#define Elf_Addr Elf64_Addr
#define Elf_Sym Elf64_Sym
#define ELF_ST_BIND(val) ELF64_ST_BIND(val)
#define ELF_ST_TYPE(val) ELF64_ST_TYPE(val)
#define Elf_Rel Elf64_Rela
#define ELF_R_SYM(val) ELF64_R_SYM(val)
#define ELF_R_TYPE(val) ELF64_R_TYPE(val)
#define ELF_R_INFO(sym, type) ELF64_R_INFO(sym, type)

#define CHECK(r) do { if (!(r)) assert(r); } while (0)

namespace {

bool FLAGS_LOG{true};

#ifdef NOLOG
# define LOGF(...) if (0) fprintf(stderr, __VA_ARGS__)
#else
# define LOGF(...) if (FLAGS_LOG) fprintf(stderr, __VA_ARGS__)
#endif

std::vector<std::string> SplitString(const std::string& str, const std::string& sep) {
    std::vector<std::string> ret;
    if (str.empty()) return ret;
    size_t index = 0;
    while (true) {
        size_t next = str.find(sep, index);
        ret.push_back(str.substr(index, next - index));
        if (next == std::string::npos) break;
        index = next + 1;
    }
    return ret;
}

bool HasPrefix(const std::string& str, const std::string& prefix) {
    ssize_t size_diff = str.size() - prefix.size();
    return size_diff >= 0 && str.substr(0, prefix.size()) == prefix;
}

uintptr_t AlignUp(uintptr_t a) {
    return (a + 4095) & ~4095;
}

struct Range {
    uintptr_t start;
    uintptr_t end;
    ptrdiff_t size() const { return end - start; }
    Range operator+(uintptr_t offset) const {
        return Range{start + offset, end + offset};
    }
};

struct Elf_GnuHash {
    uint32_t nbuckets{0};
    uint32_t symndx{0};
    uint32_t maskwords{0};
    uint32_t shift2{0};
    uint8_t tail[1];

    Elf_Addr* bloom_filter() {
        return reinterpret_cast<Elf_Addr*>(tail);
    }

    uint32_t* buckets() {
        return reinterpret_cast<uint32_t*>(&bloom_filter()[maskwords]);
    }

    uint32_t* hashvals() {
        return reinterpret_cast<uint32_t*>(&buckets()[nbuckets]);
    }
};

}

class ELFBinary {
public:
    ELFBinary(const std::string& filename, int fd, char* head, size_t size)
      : filename_(filename),
        fd_(fd),
        head_(head),
        size_(size) {
        ehdr_ = reinterpret_cast<Elf_Ehdr*>(head);

        {
            size_t found = filename.rfind('/');
            if (found == std::string::npos) {
                name_ = filename;
            } else {
                name_ = filename.substr(found + 1);
            }
        }

        ParsePhdrs();
    }

    ~ELFBinary() {
        munmap(head_, size_);
        close(fd_);
    }

    static bool IsELF(const char* p) {
        if (strncmp(p, ELFMAG, SELFMAG)) {
            return false;
        }
        if (p[EI_CLASS] != ELFCLASS64) {
            err(1, "non 64bit ELF isn't supported yet");
        }
        return true;
    }

    const std::string& filename() const { return filename_; }

    const Elf_Ehdr* ehdr() const { return ehdr_; }
    const std::vector<Elf_Phdr*> phdrs() const { return phdrs_; }

    const std::vector<std::string>& neededs() const { return neededs_; }
    const std::string& soname() const { return soname_; }
    const std::string& runpath() const { return runpath_; }
    const std::string& rpath() const { return rpath_; }

    const Elf_Sym* symtab() const { return symtab_; }
    const Elf_Rel* rel() const { return rel_; }
    size_t num_rels() const { return num_rels_; }

    const char* head() const { return head_; }

    const std::string& name() const { return name_; }

    Range GetRange() const {
        Range range{std::numeric_limits<uintptr_t>::max(), std::numeric_limits<uintptr_t>::min()};
        for (Elf_Phdr* phdr : phdrs_) {
            if (phdr->p_type != PT_LOAD) {
                continue;
            }
            range.start = std::min(range.start, phdr->p_vaddr);
            range.end = std::max(range.end, AlignUp(phdr->p_vaddr + phdr->p_memsz));
        }
        return range;
    }

    void ReadDynSymtab() {
        CHECK(symtab_);
        // TODO(hamaji): Support DT_HASH.
        CHECK(gnu_hash_);
        LOGF("Read dynsymtab of %s\n", name().c_str());
        const uint32_t* buckets = gnu_hash_->buckets();
        const uint32_t* hashvals = gnu_hash_->hashvals();
        for (int i = 0; i < gnu_hash_->nbuckets; ++i) {
            int n = buckets[i];
            if (!n) continue;
            const uint32_t* hv = &hashvals[n - gnu_hash_->symndx];
            for (Elf_Sym* sym = &symtab_[n];; ++sym) {
                uint32_t h2 = *hv++;
                const std::string name(strtab_ + sym->st_name);
                // LOGF("%s@%s\n", name.c_str(), name_.c_str());
                // TODO(hamaji): Handle version symbols.
                CHECK(syms_.emplace(name, sym).second);
                if (h2 & 1) break;
            }
        }
    }

    void LoadDynSymtab(uintptr_t offset, std::map<std::string, Elf_Sym*>* symtab) {
        ReadDynSymtab();

        for (const auto& p : syms_) {
            const std::string& name = p.first;
            Elf_Sym* sym = p.second;
            if (sym->st_value) {
                sym->st_value += offset;
            }
            LOGF("%s@%s %08lx\n", name.c_str(), name_.c_str(), sym->st_value);

            auto inserted = symtab->emplace(name, sym);
            if (!inserted.second) {
                if (ELF_ST_BIND(sym->st_info) != STB_WEAK && ELF_ST_BIND(inserted.first->second->st_info) == STB_WEAK) {
                    inserted.first->second = sym;
                }
            }
        }
    }

    const char* Str(uintptr_t name) {
        return strtab_ + name;
    }

    char* GetPtr(uintptr_t offset) {
        return head_ + OffsetFromAddr(offset);
    }

    Elf_Phdr* FindPhdr(uint64_t type) {
        for (Elf_Phdr* phdr : phdrs_) {
            if (phdr->p_type == type) {
                return phdr;
            }
        }
        CHECK(false);
    }

private:
    void ParsePhdrs() {
        for (int i = 0; i < ehdr_->e_phnum; ++i) {
            Elf_Phdr* phdr = reinterpret_cast<Elf_Phdr*>(
                head_ + ehdr_->e_phoff + ehdr_->e_phentsize * i);
            phdrs_.push_back(phdr);
        }

        for (Elf_Phdr* phdr : phdrs_) {
            if (phdr->p_type == PT_DYNAMIC) {
                ParseDynamic(phdr->p_offset, phdr->p_filesz);
            }
        }
        CHECK(!phdrs_.empty());
    }

    void ParseDynamic(size_t off, size_t size) {
        size_t dyn_size = sizeof(Elf_Dyn);
        CHECK(size % dyn_size == 0);
        std::vector<Elf_Dyn*> dyns;
        for (size_t i = 0; i < size / dyn_size; ++i) {
            Elf_Dyn* dyn = reinterpret_cast<Elf_Dyn*>(
                head_ + off + dyn_size * i);
            dyns.push_back(dyn);
        }

        for (Elf_Dyn* dyn : dyns) {
            auto get_ptr = [this, dyn]() { return GetPtr(dyn->d_un.d_ptr); };
            if (dyn->d_tag == DT_STRTAB) {
                strtab_ = get_ptr();
            } else if (dyn->d_tag == DT_SYMTAB) {
                symtab_ = reinterpret_cast<Elf_Sym*>(get_ptr());
            } else if (dyn->d_tag == DT_GNU_HASH) {
                gnu_hash_ = reinterpret_cast<Elf_GnuHash*>(get_ptr());
            } else if (dyn->d_tag == DT_RELA) {
                rel_ = reinterpret_cast<Elf_Rel*>(get_ptr());
            } else if (dyn->d_tag == DT_RELASZ) {
                num_rels_ = dyn->d_un.d_val / sizeof(Elf_Rel);
            } else if (dyn->d_tag == DT_RELAENT) {
                CHECK(sizeof(Elf_Rel) == dyn->d_un.d_val);
            } else if (dyn->d_tag == DT_REL || dyn->d_tag == DT_RELSZ || dyn->d_tag == DT_RELENT) {
                // TODO(hamaji): Support 32bit?
                CHECK(false);
            }
        }
        CHECK(strtab_);

        for (Elf_Dyn* dyn : dyns) {
            if (dyn->d_tag == DT_NEEDED) {
                const char* needed = strtab_ + dyn->d_un.d_val;
                neededs_.push_back(needed);
            } else if (dyn->d_tag == DT_SONAME) {
                name_ = soname_ = strtab_ + dyn->d_un.d_val;
            } else if (dyn->d_tag == DT_RUNPATH) {
                runpath_ = strtab_ + dyn->d_un.d_val;
            } else if (dyn->d_tag == DT_RPATH) {
                rpath_ = strtab_ + dyn->d_un.d_val;
            }
        }
    }

    Elf_Addr OffsetFromAddr(Elf_Addr addr) {
        for (Elf_Phdr* phdr : phdrs_) {
            if (phdr->p_type == PT_LOAD &&
                phdr->p_vaddr <= addr && addr < phdr->p_vaddr + phdr->p_memsz) {
                return addr - phdr->p_vaddr + phdr->p_offset;
            }
        }
        LOGF("Address %llx cannot be resolved\n", static_cast<long long>(addr));
        abort();
    }

    const std::string filename_;
    int fd_;
    char* head_;
    size_t size_;

    Elf_Ehdr* ehdr_{nullptr};
    std::vector<Elf_Phdr*> phdrs_;
    const char* strtab_{nullptr};
    Elf_Sym* symtab_{nullptr};

    std::vector<std::string> neededs_;
    std::string soname_;
    std::string runpath_;
    std::string rpath_;

    Elf_Rel* rel_{nullptr};
    size_t num_rels_{0};

    Elf_GnuHash* gnu_hash_{nullptr};

    std::string name_;
    std::map<std::string, Elf_Sym*> syms_;
};

std::unique_ptr<ELFBinary> ReadELF(const std::string& filename) {
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0)
        err(1, "open failed: %s", filename.c_str());

    size_t size = lseek(fd, 0, SEEK_END);
    if (size < 8 + 16)
        err(1, "too small file: %s", filename.c_str());

    size_t mapped_size = (size + 0xfff) & ~0xfff;

    char* p = (char*)mmap(NULL, mapped_size,
                          PROT_READ | PROT_WRITE, MAP_PRIVATE,
                          fd, 0);
    if (p == MAP_FAILED)
        err(1, "mmap failed: %s", filename.c_str());

    if (ELFBinary::IsELF(p)) {
        return std::make_unique<ELFBinary>(
            filename.c_str(), fd, p, mapped_size);
    }
    err(1, "unknown file format: %s", filename.c_str());
}

class Sold {
public:
    Sold(const std::string& elf_filename) {
        main_binary_ = ReadELF(elf_filename);

        InitLdLibraryPaths();
        ResolveLibraryPaths(main_binary_.get());
    }

    void Link(const std::string& out_filename) {
        DecideOffsets();
        BuildSymtab();
        Relocate();
        Emit(out_filename);
    }

private:
    template <class T>
    void Write(FILE* fp, const T& v) {
        CHECK(fwrite(&v, sizeof(v), 1, fp) == 1);
    }

    void Emit(const std::string& out_filename) {
        FILE* fp = fopen(out_filename.c_str(), "wb");
        EmitEhdr(fp);
        BuildDynamic();
        EmitPhdrs(fp);
        EmitDynamic(fp);
        fclose(fp);
    }

    size_t CountPhdrs() const {
        size_t num_phdrs = 3;
        for (ELFBinary* bin : link_binaries_) {
            for (Elf_Phdr* phdr : bin->phdrs()) {
                if (phdr->p_type == PT_LOAD) {
                    ++num_phdrs;
                }
            }
        }
        return num_phdrs;
    }

    void EmitEhdr(FILE* fp) {
        ehdr_ = *main_binary_->ehdr();
        ehdr_.e_shoff = 0;
        ehdr_.e_shnum = 0;
        ehdr_.e_shstrndx = 0;
        ehdr_.e_phnum = CountPhdrs();
        Write(fp, ehdr_);
    }

    Elf_Dyn MakeDyn(uint64_t tag, uintptr_t ptr) {
        Elf_Dyn dyn;
        dyn.d_tag = tag;
        dyn.d_un.d_ptr = ptr;
        return dyn;
    }

    void BuildDynamic() {
        std::set<ELFBinary*> linked(link_binaries_.begin(), link_binaries_.end());
        std::set<std::string> neededs;
        for (const auto& p : libraries_) {
            ELFBinary* bin = p.second.get();
            if (!linked.count(bin)) {
                neededs.insert(bin->soname());
            }
        }

        for (const std::string& needed : neededs) {
            dynamic_.push_back(MakeDyn(DT_NEEDED, AddStr(needed)));
        }
        if (!main_binary_->rpath().empty()) {
            dynamic_.push_back(MakeDyn(DT_RPATH, AddStr(main_binary_->rpath())));
        }
        if (main_binary_->runpath().empty()) {
            dynamic_.push_back(MakeDyn(DT_RUNPATH, AddStr(main_binary_->runpath())));
        }

        dynamic_.push_back(MakeDyn(DT_NULL, 0));
    }

    void EmitPhdrs(FILE* fp) {
        std::vector<Elf_Phdr> phdrs;

        CHECK(main_binary_->phdrs().size() > 2);
        phdrs.push_back(*main_binary_->FindPhdr(PT_PHDR));
        phdrs.push_back(*main_binary_->FindPhdr(PT_INTERP));
        const std::string interp = main_binary_->head() + main_binary_->phdrs()[1]->p_offset;
        LOGF("Interp: %s\n", interp.c_str());
        phdrs[1].p_offset = phdrs[1].p_vaddr = phdrs[1].p_paddr = AddStr(interp);

        size_t dyn_start = sizeof(Elf_Ehdr) + sizeof(Elf_Phdr) * ehdr_.e_phnum;
        size_t dyn_size = sizeof(Elf_Dyn) * dynamic_.size();
        size_t seg_start = AlignUp(dyn_start + dyn_size);

        {
            Elf_Phdr phdr = *main_binary_->FindPhdr(PT_DYNAMIC);
            // TODO(hamaji): Fill these values.
            phdr.p_offset = dyn_start;
            phdr.p_vaddr = dyn_start;
            phdr.p_paddr = dyn_start;
            phdr.p_filesz = dyn_size;
            phdrs.push_back(phdr);
        }

        for (ELFBinary* bin : link_binaries_) {
            const bool is_main = bin == main_binary_.get();
            uintptr_t offset = offsets_[bin];
            for (Elf_Phdr* phdr : bin->phdrs()) {
                if (phdr->p_type == PT_LOAD) {
                    phdrs.push_back(*phdr);
                    phdrs.back().p_offset += offset + seg_start;
                    phdrs.back().p_vaddr += offset;
                    phdrs.back().p_paddr += offset;
                }
            }
        }

        for (const Elf_Phdr& phdr : phdrs) {
            Write(fp, phdr);
        }
    }

    void EmitDynamic(FILE* fp) {
        for (const Elf_Dyn& dyn : dynamic_) {
            Write(fp, dyn);
        }
        CHECK(fwrite(strtab_.data(), 1, strtab_.size(), fp) == strtab_.size());
    }

    void DecideOffsets() {
        link_binaries_.push_back(main_binary_.get());
        offsets_.emplace(main_binary_.get(), 0);
        Range main_range = main_binary_->GetRange();
        uintptr_t offset = main_range.end;
        for (const auto& p : libraries_) {
            ELFBinary* bin = p.second.get();
            if (!ShouldLink(bin->soname())) {
                continue;
            }

            const Range range = bin->GetRange() + offset;
            CHECK(range.start == offset);
            link_binaries_.push_back(bin);
            offsets_.emplace(bin, range.start);
            LOGF("Assigned: %s %08lx-%08lx\n",
                 bin->soname().c_str(), range.start, range.end);
            offset = range.end;
        }
    }

    void BuildSymtab() {
        for (ELFBinary* bin : link_binaries_) {
            bin->LoadDynSymtab(offsets_[bin], &symtab_);
        }
    }

    void Relocate() {
        for (ELFBinary* bin : link_binaries_) {
            RelocateBinary(bin);
        }
    }

    void RelocateBinary(ELFBinary* bin) {
        CHECK(bin->symtab());
        CHECK(bin->rel());
        uintptr_t offset = offsets_[bin];
        for (size_t i = 0; i < bin->num_rels(); ++i) {
            const Elf_Rel* rel = &bin->rel()[i];
            // TODO(hamaji): Support non-x86-64 architectures.
            RelocateSymbol_x86_64(bin, rel, offset);
        }
    }

    void RelocateSymbol_x86_64(ELFBinary* bin, const Elf_Rel* rel, uintptr_t offset) {
        const Elf_Sym* sym = &bin->symtab()[ELF_R_SYM(rel->r_info)];
        int type = ELF_R_TYPE(rel->r_info);
        const uintptr_t addend = rel->r_addend;
        void* target = bin->GetPtr(rel->r_offset);
        Elf_Rel newrel = *rel;
        newrel.r_offset += offset;

        auto resolve_sym = [this, bin, sym]() {
            const std::string name = bin->Str(sym->st_name);
            auto found = symtab_.find(name);
            if (found != symtab_.end()) {
                return found->second->st_value;
            } else {
                return static_cast<uintptr_t>(0);
            }
        };

        switch (type) {
        case R_X86_64_RELATIVE: {
            *reinterpret_cast<uintptr_t*>(target) += offset;
            break;
        }

        case R_X86_64_GLOB_DAT: {
            uintptr_t val = resolve_sym();
            if (val) {
                *reinterpret_cast<uintptr_t*>(target) = val + addend;
                newrel.r_info = ELF_R_INFO(0, R_X86_64_RELATIVE);
                newrel.r_addend = 0;
            }
            break;
        }

        default:
            LOGF("Unknown relocation type: %d\n", type);
            CHECK(false);
        }

        rels_.push_back(newrel);
    }

    void InitLdLibraryPaths() {
        if (const char* paths = getenv("LD_LIBRARY_PATH")) {
            for (const std::string& path : SplitString(paths, ":")) {
                ld_library_paths_.push_back(path);
            }
        }
    }

    std::string ResolveRunPathVariables(const ELFBinary* binary,
                                        const std::string& runpath) {
        std::string out = runpath;

        auto replace = [](std::string& s,
                          const std::string& f,
                          const std::string& t) {
            size_t found = s.find(f);
            while (found != std::string::npos) {
                s.replace(found, f.size(), t);
                found = s.find(f);
            }
        };

        std::string origin = binary->filename();
        origin = dirname(&origin[0]);
        replace(out, "$ORIGIN", origin);
        replace(out, "${ORIGIN}", origin);
        if (out.find('$') != std::string::npos) {
            LOGF("Unsupported runpath: %s\n", runpath.c_str());
            abort();
        }
        return out;
    }

    std::vector<std::string> GetLibraryPaths(const ELFBinary* binary) {
        std::vector<std::string> library_paths;
        const std::string& runpath = binary->runpath();
        const std::string& rpath = binary->rpath();
        if (runpath.empty() && !rpath.empty()) {
            for (const std::string& path : SplitString(rpath, ":")) {
                library_paths.push_back(ResolveRunPathVariables(binary, path));
            }
        }
        for (const std::string& path : ld_library_paths_) {
            library_paths.push_back(path);
        }
        if (!runpath.empty()) {
            for (const std::string& path : SplitString(runpath, ":")) {
                library_paths.push_back(ResolveRunPathVariables(binary, path));
            }
        }

        // TODO(hamaji): From ld.so.conf. Make this customizable.
        library_paths.push_back("/usr/local/lib");
        library_paths.push_back("/usr/local/lib/x86_64-linux-gnu");
        library_paths.push_back("/lib/x86_64-linux-gnu");
        library_paths.push_back("/usr/lib/x86_64-linux-gnu");
        return library_paths;
    }

    void ResolveLibraryPaths(const ELFBinary* binary) {
        std::vector<std::string> library_paths = GetLibraryPaths(binary);

        for (const std::string& needed : binary->neededs()) {
            auto found = libraries_.find(needed);
            if (found != libraries_.end()) {
                continue;
            }

            std::unique_ptr<ELFBinary> library;
            for (const std::string& path : library_paths) {
                const std::string& filename = path + '/' + needed;
                if (Exists(filename)) {
                    library = ReadELF(filename);
                    break;
                }
            }
            if (!library) {
                LOGF("Library %s not found\n", needed.c_str());
                abort();
            }

            LOGF("Loaded: %s => %s\n",
                 needed.c_str(), library->filename().c_str());

            auto inserted = libraries_.emplace(needed, std::move(library));
            CHECK(inserted.second);
            ResolveLibraryPaths(inserted.first->second.get());
        }
    }

    bool Exists(const std::string& filename) {
        std::ifstream ifs(filename);
        return static_cast<bool>(ifs);
    }

    static bool ShouldLink(const std::string& soname) {
        // TODO(hamaji): Make this customizable.
        std::vector<std::string> nolink_prefixes = {
            "libc.so",
            "libm.so",
            "libdl.so",
            "librt.so",
            "libpthread.so",
            "libgcc_s.so",
            "libstdc++.so",
            "libgomp.so",
            "ld-linux",
            "libcuda.so",
        };
        for (const std::string& prefix : nolink_prefixes) {
            if (HasPrefix(soname, prefix)) {
                return false;
            }
        }
        return true;
    }

    uintptr_t AddStr(const std::string& s) {
        uintptr_t pos = static_cast<uintptr_t>(strtab_.size());
        strtab_ += s;
        strtab_ += '\0';
        return pos;
    }

    std::unique_ptr<ELFBinary> main_binary_;
    std::vector<std::string> ld_library_paths_;
    std::map<std::string, std::unique_ptr<ELFBinary>> libraries_;
    std::vector<ELFBinary*> link_binaries_;
    std::map<ELFBinary*, uintptr_t> offsets_;
    std::map<std::string, Elf_Sym*> symtab_;
    std::vector<Elf_Rel> rels_;
    std::string strtab_;
    Elf_Ehdr ehdr_;
    std::vector<Elf_Dyn> dynamic_;
};

int main(int argc, const char* argv[]) {
    if (argc <= 2) {
        LOGF("Usage: %s <in-elf> <out-elf>\n", argv[0]);
        return 1;
    }

    Sold sold(argv[1]);
    sold.Link(argv[2]);
}
