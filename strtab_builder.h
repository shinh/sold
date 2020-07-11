#pragma once

#include <stdint.h>
#include <string>

#include "utils.h"

class StrtabBuilder {
public:
    uintptr_t Add(const std::string& s);

    void Freeze() { is_freezed_ = true; }

    size_t size() const { return strtab_.size(); }

    const void* data() const { return strtab_.data(); }

private:
    std::string strtab_;
    bool is_freezed_{false};
};
