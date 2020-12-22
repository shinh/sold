#pragma once

#include <elf.h>

#include <cassert>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <glog/logging.h>

#define SOLD_LOG_KEY_VALUE(key, value) " " << key << "=" << value
#define SOLD_LOG_KEY(key) SOLD_LOG_KEY_VALUE(#key, key)
#define SOLD_LOG_64BITS(key) SOLD_LOG_KEY_VALUE(#key, HexString(key, 16))
#define SOLD_LOG_32BITS(key) SOLD_LOG_KEY_VALUE(#key, HexString(key, 8))
#define SOLD_LOG_16BITS(key) SOLD_LOG_KEY_VALUE(#key, HexString(key, 4))

#define Elf_Ehdr Elf64_Ehdr
#define Elf_Phdr Elf64_Phdr
#define Elf_Dyn Elf64_Dyn
#define Elf_Addr Elf64_Addr
#define Elf_Sym Elf64_Sym
#define Elf_Shdr Elf64_Shdr
#define Elf_Off Elf64_Off
#define Elf_Half Elf64_Half
#define Elf_Versym Elf64_Versym
#define Elf_Xword Elf64_Xword
#define Elf_Word Elf64_Word
#define Elf_Vernaux Elf64_Vernaux
#define Elf_Verdaux Elf64_Verdaux
#define Elf_Verneed Elf64_Verneed
#define Elf_Verdef Elf64_Verdef
#define ELF_ST_BIND(val) ELF64_ST_BIND(val)
#define ELF_ST_TYPE(val) ELF64_ST_TYPE(val)
#define ELF_ST_INFO(bind, type) ELF64_ST_INFO(bind, type)
#define Elf_Rel Elf64_Rela
#define ELF_R_SYM(val) ELF64_R_SYM(val)
#define ELF_R_TYPE(val) ELF64_R_TYPE(val)
#define ELF_R_INFO(sym, type) ELF64_R_INFO(sym, type)

// Although there is no description of VERSYM_HIDDEN in glibc, you can find it
// in binutils source code.
// https://github.com/gittup/binutils/blob/8db2e9c8d085222ac7b57272ee263733ae193565/include/elf/common.h#L816
#define VERSYM_HIDDEN 0x8000
#define VERSYM_VERSION 0x7fff

static const Elf_Versym NO_VERSION_INFO = 0xffff;

std::vector<std::string> SplitString(const std::string& str, const std::string& sep);

bool HasPrefix(const std::string& str, const std::string& prefix);

uintptr_t AlignNext(uintptr_t a, uintptr_t mask = 4095);

struct Range {
    uintptr_t start;
    uintptr_t end;
    ptrdiff_t size() const { return end - start; }
    Range operator+(uintptr_t offset) const { return Range{start + offset, end + offset}; }
};

bool IsTLS(const Elf_Sym& sym);

bool IsDefined(const Elf_Sym& sym);

class ELFBinary;

struct Syminfo {
    std::string name;
    std::string soname;
    std::string version;
    Elf_Versym versym;
    Elf_Sym* sym;
};

std::string ShowRelocationType(int type);
std::ostream& operator<<(std::ostream& os, const Syminfo& s);
std::ostream& operator<<(std::ostream& os, const Elf_Rel& s);

struct TLS {
    struct Data {
        ELFBinary* bin;
        uint8_t* start;
        size_t size;
        uintptr_t file_offset;
        uintptr_t bss_offset;
    };

    std::vector<Data> data;
    std::map<ELFBinary*, size_t> bin_to_index;
    uintptr_t filesz{0};
    uintptr_t memsz{0};
    Elf_Xword align{0};
};

bool is_special_ver_ndx(Elf64_Versym v);

std::string special_ver_ndx_to_str(Elf_Versym v);

template <class T>
inline std::string HexString(T num, int length = 16) {
    std::stringstream ss;
    ss << "0x" << std::uppercase << std::setfill('0') << std::setw(length) << std::hex << num;
    return ss.str();
}

template <>
inline std::string HexString<char*>(char* num, int length) {
    std::stringstream ss;
    ss << "0x" << std::uppercase << std::setfill('0') << std::setw(length) << std::hex << reinterpret_cast<uint64_t>(num);
    return ss.str();
}

template <>
inline std::string HexString<const char*>(const char* num, int length) {
    std::stringstream ss;
    ss << "0x" << std::uppercase << std::setfill('0') << std::setw(length) << std::hex << reinterpret_cast<const uint64_t>(num);
    return ss.str();
}
