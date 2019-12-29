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
#include <map>
#include <memory>
#include <string>
#include <vector>

#define Elf_Ehdr Elf64_Ehdr
#define Elf_Phdr Elf64_Phdr
#define Elf_Dyn Elf64_Dyn
#define Elf_Addr Elf64_Addr

namespace {

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

}

class ELFBinary {
public:
    ELFBinary(const std::string& filename, int fd, char* head, size_t size)
      : filename_(filename),
        fd_(fd),
        head_(head),
        size_(size) {
        ehdr_ = reinterpret_cast<Elf_Ehdr*>(head);

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

    const std::vector<std::string>& neededs() const { return neededs_; }
    const std::string& soname() const { return soname_; }
    const std::string& runpath() const { return runpath_; }
    const std::string& rpath() const { return rpath_; }

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
        assert(!phdrs_.empty());
    }

    void ParseDynamic(size_t off, size_t size) {
        size_t dyn_size = sizeof(Elf_Dyn);
        assert(size % dyn_size == 0);
        std::vector<Elf_Dyn*> dyns;
        for (size_t i = 0; i < size / dyn_size; ++i) {
            Elf_Dyn* dyn = reinterpret_cast<Elf_Dyn*>(
                head_ + off + dyn_size * i);
            dyns.push_back(dyn);
        }

        for (Elf_Dyn* dyn : dyns) {
            if (dyn->d_tag == DT_STRTAB) {
                strtab_ = head_ + OffsetFromAddr(dyn->d_un.d_ptr);
            }
        }
        assert(strtab_);

        for (Elf_Dyn* dyn : dyns) {
            if (dyn->d_tag == DT_NEEDED) {
                const char* needed = strtab_ + dyn->d_un.d_val;
                neededs_.push_back(needed);
            } else if (dyn->d_tag == DT_SONAME) {
                soname_ = strtab_ + dyn->d_un.d_val;
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
        fprintf(stderr, "Address %llx cannot be resolved\n",
                static_cast<long long>(addr));
        abort();
    }

    const std::string filename_;
    int fd_;
    char* head_;
    size_t size_;

    Elf_Ehdr* ehdr_{nullptr};
    std::vector<Elf_Phdr*> phdrs_;
    const char* strtab_{nullptr};
    std::vector<std::string> neededs_;
    std::string soname_;
    std::string runpath_;
    std::string rpath_;
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
                          PROT_READ, MAP_SHARED,
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
    }

private:
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
            fprintf(stderr, "Unsupported runpath: %s\n", runpath.c_str());
            abort();
        }
        return out;
    }

    std::vector<std::string> GetLibraryPaths(const ELFBinary* binary) {
        std::vector<std::string> library_paths;
        const std::string& runpath = binary->runpath();
        const std::string& rpath = binary->rpath();
        if (runpath.empty() && !rpath.empty()) {
            library_paths.push_back(ResolveRunPathVariables(binary, rpath));
        }
        for (const std::string& path : ld_library_paths_) {
            library_paths.push_back(path);
        }
        if (!runpath.empty()) {
            library_paths.push_back(ResolveRunPathVariables(binary, runpath));
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
            auto found = libraries.find(needed);
            if (found != libraries.end()) {
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
                fprintf(stderr, "Library %s not found\n", needed.c_str());
                abort();
            }

            fprintf(stderr, "Loaded: %s => %s\n",
                    needed.c_str(), library->filename().c_str());

            auto inserted = libraries.emplace(needed, std::move(library));
            assert(inserted.second);
            ResolveLibraryPaths(inserted.first->second.get());
        }
    }

    bool Exists(const std::string& filename) {
        std::ifstream ifs(filename);
        return static_cast<bool>(ifs);
    }

    std::unique_ptr<ELFBinary> main_binary_;
    std::vector<std::string> ld_library_paths_;
    std::map<std::string, std::unique_ptr<ELFBinary>> libraries;
};

int main(int argc, const char* argv[]) {
    if (argc <= 2) {
        fprintf(stderr, "Usage: %s <in-elf> <out-elf>\n", argv[0]);
        return 1;
    }

    Sold sold(argv[1]);
    sold.Link(argv[2]);
}
