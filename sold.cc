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
#define ELF_ST_INFO(bind, type) ELF64_ST_INFO(bind, type)
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

uintptr_t AlignNext(uintptr_t a) {
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
    const std::vector<Elf_Phdr*> loads() const { return loads_; }

    const std::vector<std::string>& neededs() const { return neededs_; }
    const std::string& soname() const { return soname_; }
    const std::string& runpath() const { return runpath_; }
    const std::string& rpath() const { return rpath_; }

    const Elf_Sym* symtab() const { return symtab_; }
    const Elf_Rel* rel() const { return rel_; }
    size_t num_rels() const { return num_rels_; }
    const Elf_Rel* plt_rel() const { return plt_rel_; }
    size_t num_plt_rels() const { return num_plt_rels_; }

    const char* head() const { return head_; }

    const std::string& name() const { return name_; }

    Range GetRange() const {
        Range range{std::numeric_limits<uintptr_t>::max(), std::numeric_limits<uintptr_t>::min()};
        for (Elf_Phdr* phdr : loads_) {
            range.start = std::min(range.start, phdr->p_vaddr);
            range.end = std::max(range.end, AlignNext(phdr->p_vaddr + phdr->p_memsz));
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
        for (int n = 0; n < gnu_hash_->symndx; ++n) {
            Elf_Sym* sym = &symtab_[n];
            if (sym->st_name) {
                const std::string name(strtab_ + sym->st_name);
                CHECK(syms_.emplace(name, sym).second);
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
            LOGF("Symbol %s@%s %08lx\n", name.c_str(), name_.c_str(), sym->st_value);

            auto inserted = symtab->emplace(name, sym);
            if (!inserted.second) {
                Elf_Sym* sym2 = inserted.first->second;
                int prio = sym->st_value ? 2 : ELF_ST_BIND(sym->st_info) == STB_WEAK;
                int prio2 = sym2->st_value ? 2 : ELF_ST_BIND(sym2->st_info) == STB_WEAK;
                if (prio > prio2) {
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
            if (phdr->p_type == PT_LOAD) {
                loads_.push_back(phdr);
            }
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
            } else if (dyn->d_tag == DT_JMPREL) {
                plt_rel_ = reinterpret_cast<Elf_Rel*>(get_ptr());
            } else if (dyn->d_tag == DT_PLTRELSZ) {
                num_plt_rels_ = dyn->d_un.d_val / sizeof(Elf_Rel);
            } else if (dyn->d_tag == DT_PLTREL) {
                CHECK(dyn->d_un.d_val == DT_RELA);
            } else if (dyn->d_tag == DT_PLTREL) {
                // TODO(hamaji): Check
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
        for (Elf_Phdr* phdr : loads_) {
            if (phdr->p_vaddr <= addr && addr < phdr->p_vaddr + phdr->p_memsz) {
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
    std::vector<Elf_Phdr*> loads_;
    const char* strtab_{nullptr};
    Elf_Sym* symtab_{nullptr};

    std::vector<std::string> neededs_;
    std::string soname_;
    std::string runpath_;
    std::string rpath_;

    Elf_Rel* rel_{nullptr};
    size_t num_rels_{0};
    Elf_Rel* plt_rel_{nullptr};
    size_t num_plt_rels_{0};

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

class StrtabBuilder {
public:
    uintptr_t Add(const std::string& s) {
        CHECK(!is_freezed_);
        uintptr_t pos = static_cast<uintptr_t>(strtab_.size());
        strtab_ += s;
        strtab_ += '\0';
        return pos;
    }

    void Freeze() {
        is_freezed_ = true;
    }

    size_t size() const {
        return strtab_.size();
    }

    const void* data() const {
        return strtab_.data();
    }

private:
    std::string strtab_;
    bool is_freezed_{false};
};

class SymtabBuilder {
public:
    struct Symbol {
        Elf_Sym sym;
        uintptr_t index;
    };

    void SetSrcSyms(std::map<std::string, Elf_Sym*> syms) {
        src_syms_ = syms;
    }

    uintptr_t AddSym(const std::string& name) {
        uintptr_t index = sym_names_.size();
        sym_names_.push_back(name);
        return index;
    }

    bool Resolve(const std::string& name, uintptr_t* val_or_index) {
        Symbol sym{};
        sym.sym.st_name = 0;
        sym.sym.st_info = 0;
        sym.sym.st_other = 0;
        sym.sym.st_shndx = 0;
        sym.sym.st_value = 0;
        sym.sym.st_size = 0;

        auto found = syms_.find(name);
        if (found != syms_.end()) {
            sym = found->second;
        } else {
            auto found = src_syms_.find(name);
            if (found != src_syms_.end()) {
                sym.sym = *found->second;
                if (sym.sym.st_value) {
                    LOGF("Symbol %s found\n", name.c_str());
                } else {
                    LOGF("Symbol (undef/weak) %s found\n", name.c_str());
                    sym.index = AddSym(name);
                    CHECK(syms_.emplace(name, sym).second);
                }
            } else {
                LOGF("Symbol %s not found\n", name.c_str());
                sym.index = AddSym(name);
                CHECK(syms_.emplace(name, sym).second);
            }
        }

        if (!sym.sym.st_value) {
            *val_or_index = sym.index;
            return false;
        } else {
            *val_or_index = sym.sym.st_value;
            return true;
        }
    }

    uintptr_t ResolveCopy(const std::string& name) {
        // TODO(hamaji): Refactor.
        Symbol sym{};
        sym.sym.st_name = 0;
        sym.sym.st_info = 0;
        sym.sym.st_other = 0;
        sym.sym.st_shndx = 0;
        sym.sym.st_value = 0;
        sym.sym.st_size = 0;

        auto found = syms_.find(name);
        if (found != syms_.end()) {
            sym = found->second;
        } else {
            auto found = src_syms_.find(name);
            if (found != src_syms_.end()) {
                LOGF("Symbol %s found for copy\n", name.c_str());
                sym.sym = *found->second;
                sym.index = AddSym(name);
                CHECK(syms_.emplace(name, sym).second);
            } else {
                LOGF("Symbol %s not found for copy\n", name.c_str());
                CHECK(false);
            }
        }

        return sym.index;
    }

    void Build(StrtabBuilder* strtab) {
        for (const std::string& name : sym_names_) {
            auto found = syms_.find(name);
            CHECK(found != syms_.end());
            Elf_Sym sym = found->second.sym;
            sym.st_name = strtab->Add(name);
            symtab_.push_back(sym);
        }
    }

    uintptr_t size() const {
        return symtab_.size();
    }

    const std::vector<Elf_Sym>& Get() {
        return symtab_;
    }

private:
    std::map<std::string, Elf_Sym*> src_syms_;
    std::map<std::string, Symbol> syms_;

    std::vector<std::string> sym_names_;
    std::vector<Elf_Sym> symtab_;
};

class Sold {
public:
    Sold(const std::string& elf_filename) {
        main_binary_ = ReadELF(elf_filename);
        link_binaries_.push_back(main_binary_.get());

        InitLdLibraryPaths();
        ResolveLibraryPaths(main_binary_.get());
    }

    void Link(const std::string& out_filename) {
        DecideOffsets();
        CollectSymbols();
        Relocate();

        syms_.Build(&strtab_);
        BuildEhdr();
        BuildInterp();
        BuildDynamic();

        strtab_.Freeze();

        Emit(out_filename);
    }

private:
    template <class T>
    void Write(FILE* fp, const T& v) {
        CHECK(fwrite(&v, sizeof(v), 1, fp) == 1);
    }

    void WriteBuf(FILE* fp, const void* buf, size_t size) {
        CHECK(fwrite(buf, 1, size, fp) == size);
    }

    void EmitZeros(FILE* fp, uintptr_t cnt) {
        std::string zero(cnt, '\0');
        WriteBuf(fp, zero.data(), zero.size());
    }

    void EmitPad(FILE* fp, uintptr_t to) {
        uint pos = ftell(fp);
        CHECK(pos >= 0);
        CHECK(pos <= to);
        EmitZeros(fp, to - pos);
    }

    void EmitAlign(FILE* fp) {
        long pos = ftell(fp);
        CHECK(pos >= 0);
        EmitZeros(fp, AlignNext(pos) - pos);
    }

    void Emit(const std::string& out_filename) {
        FILE* fp = fopen(out_filename.c_str(), "wb");
        Write(fp, ehdr_);
        EmitPhdrs(fp);
        EmitSymtab(fp);
        EmitStrtab(fp);
        EmitRel(fp);
        EmitDynamic(fp);
        EmitAlign(fp);

        EmitCode(fp);
        fclose(fp);
    }

    size_t CountPhdrs() const {
        size_t num_phdrs = 4;
        for (ELFBinary* bin : link_binaries_) {
            num_phdrs += bin->loads().size();
        }
        return num_phdrs;
    }

    uintptr_t SymtabOffset() const {
        return sizeof(Elf_Ehdr) + sizeof(Elf_Phdr) * ehdr_.e_phnum;
    }

    uintptr_t StrtabOffset() const {
        return SymtabOffset() + syms_.size() * sizeof(Elf_Sym);
    }

    uintptr_t RelOffset() const {
        return StrtabOffset() + strtab_.size();
    }

    uintptr_t DynamicOffset() const {
        return RelOffset() + rels_.size() * sizeof(Elf_Rel);
    }

    void BuildEhdr() {
        ehdr_ = *main_binary_->ehdr();
        ehdr_.e_entry += offsets_[main_binary_.get()];
        ehdr_.e_shoff = 0;
        ehdr_.e_shnum = 0;
        ehdr_.e_shstrndx = 0;
        ehdr_.e_phnum = CountPhdrs();
    }

    void MakeDyn(uint64_t tag, uintptr_t ptr) {
        Elf_Dyn dyn;
        dyn.d_tag = tag;
        dyn.d_un.d_ptr = ptr;
        dynamic_.push_back(dyn);
    }

    void BuildInterp() {
        const std::string interp = main_binary_->head() + main_binary_->FindPhdr(PT_INTERP)->p_offset;
        LOGF("Interp: %s\n", interp.c_str());
        interp_offset_ = AddStr(interp);
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
            MakeDyn(DT_NEEDED, AddStr(needed));
        }
        if (!main_binary_->rpath().empty()) {
            MakeDyn(DT_RPATH, AddStr(main_binary_->rpath()));
        }
        if (main_binary_->runpath().empty()) {
            MakeDyn(DT_RUNPATH, AddStr(main_binary_->runpath()));
        }

        MakeDyn(DT_STRTAB, StrtabOffset());
        MakeDyn(DT_STRSZ, strtab_.size());

        MakeDyn(DT_SYMTAB, SymtabOffset());
        MakeDyn(DT_SYMENT, sizeof(Elf_Sym));

        MakeDyn(DT_RELA, RelOffset());
        MakeDyn(DT_RELAENT, sizeof(Elf_Rel));
        MakeDyn(DT_RELASZ, rels_.size() * sizeof(Elf_Rel));

        MakeDyn(DT_NULL, 0);
    }

    void EmitPhdrs(FILE* fp) {
        std::vector<Elf_Phdr> phdrs;

        CHECK(main_binary_->phdrs().size() > 2);
        phdrs.push_back(*main_binary_->FindPhdr(PT_PHDR));
        phdrs.push_back(*main_binary_->FindPhdr(PT_INTERP));
        phdrs[1].p_offset = phdrs[1].p_vaddr = phdrs[1].p_paddr = StrtabOffset() + interp_offset_;

        size_t dyn_start = DynamicOffset();
        size_t dyn_size = sizeof(Elf_Dyn) * dynamic_.size();
        size_t seg_start = AlignNext(dyn_start + dyn_size);

        {
            Elf_Phdr phdr = *main_binary_->FindPhdr(PT_LOAD);
            phdr.p_offset = 0;
            phdr.p_flags = PF_R | PF_W;
            phdr.p_vaddr = 0;
            phdr.p_paddr = 0;
            phdr.p_filesz = seg_start;
            phdr.p_memsz = seg_start;
            phdrs.push_back(phdr);
        }
        {
            Elf_Phdr phdr = *main_binary_->FindPhdr(PT_DYNAMIC);
            phdr.p_offset = dyn_start;
            phdr.p_flags = PF_R | PF_W;
            phdr.p_vaddr = dyn_start;
            phdr.p_paddr = dyn_start;
            phdr.p_filesz = dyn_size;
            phdr.p_memsz = dyn_size;
            phdrs.push_back(phdr);
        }

        for (ELFBinary* bin : link_binaries_) {
            const bool is_main = bin == main_binary_.get();
            uintptr_t offset = offsets_[bin];
            for (Elf_Phdr* phdr : bin->loads()) {
                Elf_Phdr new_phdr = *phdr;
                new_phdr.p_offset += offset;
                new_phdr.p_vaddr += offset;
                new_phdr.p_paddr += offset;
                // TODO(hamaji): Add PF_W only for GOT.
                new_phdr.p_flags |= PF_W;
                phdrs.push_back(new_phdr);
            }
        }

        for (const Elf_Phdr& phdr : phdrs) {
            Write(fp, phdr);
        }
    }

    void EmitSymtab(FILE* fp) {
        CHECK(ftell(fp) == SymtabOffset());
        for (const Elf_Sym& sym : syms_.Get()) {
            Write(fp, sym);
        }
    }

    void EmitStrtab(FILE* fp) {
        CHECK(ftell(fp) == StrtabOffset());
        WriteBuf(fp, strtab_.data(), strtab_.size());
    }

    void EmitRel(FILE* fp) {
        CHECK(ftell(fp) == RelOffset());
        for (const Elf_Rel& rel : rels_) {
            Write(fp, rel);
        }
    }

    void EmitDynamic(FILE* fp) {
        CHECK(ftell(fp) == DynamicOffset());
        for (const Elf_Dyn& dyn : dynamic_) {
            Write(fp, dyn);
        }
    }

    void EmitCode(FILE* fp) {
        for (ELFBinary* bin : link_binaries_) {
            fprintf(stderr, "Emitting code of %s from %lx\n",
                    bin->name().c_str(), ftell(fp));
            for (Elf_Phdr* phdr : bin->loads()) {
                EmitPad(fp, offsets_[bin] + phdr->p_offset);
                WriteBuf(fp, bin->head() + phdr->p_offset, phdr->p_filesz);
            }
            EmitAlign(fp);
        }
    }

    void DecideOffsets() {
        // TODO(hamaji): Use actual size of the headers.
        uintptr_t offset = 0x1000;
        for (ELFBinary* bin : link_binaries_) {
            const Range range = bin->GetRange() + offset;
            CHECK(range.start == offset);
            offsets_.emplace(bin, range.start);
            LOGF("Assigned: %s %08lx-%08lx\n",
                 bin->soname().c_str(), range.start, range.end);
            offset = range.end;
        }
    }

    void CollectSymbols() {
        std::map<std::string, Elf_Sym*> syms;
        for (ELFBinary* bin : link_binaries_) {
            bin->LoadDynSymtab(offsets_[bin], &syms);
        }
        syms_.SetSrcSyms(syms);
    }

    void Relocate() {
        for (ELFBinary* bin : link_binaries_) {
            RelocateBinary(bin);
        }
    }

    void RelocateBinary(ELFBinary* bin) {
        CHECK(bin->symtab());
        CHECK(bin->rel());
        RelocateSymbols(bin, bin->rel(), bin->num_rels());
        RelocateSymbols(bin, bin->plt_rel(), bin->num_plt_rels());
    }

    void RelocateSymbols(ELFBinary* bin, const Elf_Rel* rels, size_t num) {
        uintptr_t offset = offsets_[bin];
        for (size_t i = 0; i < num; ++i) {
            // TODO(hamaji): Support non-x86-64 architectures.
            RelocateSymbol_x86_64(bin, &rels[i], offset);
        }
    }

    void RelocateSymbol_x86_64(ELFBinary* bin, const Elf_Rel* rel, uintptr_t offset) {
        const Elf_Sym* sym = &bin->symtab()[ELF_R_SYM(rel->r_info)];
        int type = ELF_R_TYPE(rel->r_info);
        const uintptr_t addend = rel->r_addend;
        Elf_Rel newrel = *rel;
        newrel.r_offset += offset;

        LOGF("Relocate %s at %lx\n", bin->Str(sym->st_name), rel->r_offset);

        switch (type) {
        case R_X86_64_RELATIVE: {
            newrel.r_addend += offset;
            break;
        }

        case R_X86_64_GLOB_DAT:
        case R_X86_64_JUMP_SLOT: {
            uintptr_t val_or_index;
            if (syms_.Resolve(bin->Str(sym->st_name), &val_or_index)) {
                newrel.r_info = ELF_R_INFO(0, R_X86_64_RELATIVE);
                newrel.r_addend += val_or_index;
            } else {
                newrel.r_info = ELF_R_INFO(val_or_index, type);
            }
            break;
        }

        case R_X86_64_COPY: {
            const std::string name = bin->Str(sym->st_name);
            uintptr_t index = syms_.ResolveCopy(name);
            newrel.r_info = ELF_R_INFO(index, type);
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
            if (ShouldLink(library->soname())) {
                link_binaries_.push_back(library.get());
            }

            LOGF("Loaded: %s => %s\n",
                 needed.c_str(), library->filename().c_str());

            auto inserted = libraries_.emplace(needed, std::move(library));
            CHECK(inserted.second);
            ResolveLibraryPaths(inserted.first->second.get());
        }
    }

    bool Exists(const std::string& filename) {
        struct stat st;
        if (stat(filename.c_str(), &st) != 0) {
            return false;
        }
        return (st.st_mode & S_IFMT) & S_IFREG;
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
        return strtab_.Add(s);
    }

    std::unique_ptr<ELFBinary> main_binary_;
    std::vector<std::string> ld_library_paths_;
    std::map<std::string, std::unique_ptr<ELFBinary>> libraries_;
    std::vector<ELFBinary*> link_binaries_;
    std::map<ELFBinary*, uintptr_t> offsets_;

    uintptr_t interp_offset_;
    SymtabBuilder syms_;
    std::vector<Elf_Rel> rels_;
    StrtabBuilder strtab_;
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
