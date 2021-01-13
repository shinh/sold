#pragma once

#include <elf.h>
#include <libdwarf/dwarf.h>

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
#define SOLD_LOG_8BITS(key) SOLD_LOG_KEY_VALUE(#key, HexString(key, 2))
#define SOLD_LOG_BITS(key) SOLD_LOG_KEY_VALUE(#key, HexString(key))
#define SOLD_LOG_DWEHPE(type) SOLD_LOG_KEY_VALUE(#type, ShowDW_EH_PE(type))
#define SOLD_CHECK_EQ(a, b) CHECK(a == b) << SOLD_LOG_BITS(a) << SOLD_LOG_BITS(b)

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

// 0xee is never used as a valid DWARF Exception Header value
#define DW_EH_PE_SOLD_DUMMY 0xee

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
std::string ShowDW_EH_PE(uint8_t type);
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

struct EHFrameHeader {
    struct FDETableEntry {
        int32_t initial_loc;
        int32_t fde_ptr;
    };

    struct CIE {
        uint32_t length;
        int32_t CIE_id;
        uint8_t version;
        const char* aug_str;
        uint8_t FDE_encoding;
        uint8_t LSDA_encoding;
    };

    struct FDE {
        uint32_t length;
        uint64_t extended_length;
        int32_t CIE_delta;
        int32_t initial_loc;
    };

    uint8_t version;
    uint8_t eh_frame_ptr_enc;
    uint8_t fde_count_enc;
    uint8_t table_enc;

    int32_t eh_frame_ptr;
    uint32_t fde_count;

    std::vector<FDETableEntry> table;
    std::vector<FDE> fdes;
    std::vector<CIE> cies;
};

bool is_special_ver_ndx(Elf64_Versym v);

std::string special_ver_ndx_to_str(Elf_Versym v);

template <class T>
inline std::string HexString(T num, int length = -1) {
    if (length == -1) {
        length = sizeof(T) * 2;
    }
    std::stringstream ss;
    ss << "0x" << std::uppercase << std::setfill('0') << std::setw(length) << std::hex << +num;
    return ss.str();
}

const char* read_uleb128(const char* p, uint32_t* val);
const char* read_sleb128(const char* p, int32_t* val);

typedef unsigned sold_Unwind_Ptr __attribute__((__mode__(__word__)));
const char* read_encoded_value_with_base(unsigned char encoding, sold_Unwind_Ptr base, const char* p, uint32_t* val);
