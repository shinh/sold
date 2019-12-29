#include <elf.h>
#include <err.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#define Elf_Ehdr Elf64_Ehdr
#define Elf_Phdr Elf64_Phdr

class ELFBinary {
public:
    ELFBinary(const std::string& filename,
              int fd, char* mapped_head, size_t mapped_size)
      : filename_(filename),
        fd_(fd),
        mapped_head_(mapped_head),
        mapped_size_(mapped_size) {
        Elf_Ehdr* ehdr = reinterpret_cast<Elf_Ehdr*>(mapped_head);
        if (!ehdr->e_shoff || !ehdr->e_shnum)
            err(1, "no section header: %s", filename.c_str());
        if (!ehdr->e_shstrndx)
            err(1, "no section name: %s", filename.c_str());
    }

  ~ELFBinary() {
      munmap(mapped_head_, mapped_size_);
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

private:
    const std::string filename_;
    int fd_;
    size_t size_;
    char* mapped_head_;
    size_t mapped_size_;
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
    }

    void Link(const std::string& out_filename) {
    }

private:
    std::unique_ptr<ELFBinary> main_binary_;
};

int main(int argc, const char* argv[]) {
    if (argc <= 2) {
        fprintf(stderr, "Usage: %s <in-elf> <out-elf>\n", argv[0]);
        return 1;
    }

    Sold sold(argv[1]);
    sold.Link(argv[2]);
}
