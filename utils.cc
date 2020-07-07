#include "utils.h"

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

uintptr_t AlignNext(uintptr_t a, uintptr_t mask) {
    return (a + mask) & ~mask;
}

bool IsTLS(const Elf_Sym& sym) {
    return ELF_ST_TYPE(sym.st_info) == STT_TLS;
}

bool IsDefined(const Elf_Sym& sym) {
    return sym.st_value || IsTLS(sym);
}
