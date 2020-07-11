#include "strtab_builder.h"

uintptr_t StrtabBuilder::Add(const std::string& s) {
    CHECK(!is_freezed_);
    uintptr_t pos = static_cast<uintptr_t>(strtab_.size());
    strtab_ += s;
    strtab_ += '\0';
    return pos;
}
