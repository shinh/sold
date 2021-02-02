// Copyright (C) 2021 The sold authors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "utils.h"

struct Elf_GnuHash {
    uint32_t nbuckets{0};
    uint32_t symndx{0};
    uint32_t maskwords{0};
    uint32_t shift2{0};
    uint8_t tail[1];

    Elf_Addr* bloom_filter() { return reinterpret_cast<Elf_Addr*>(tail); }

    uint32_t* buckets() { return reinterpret_cast<uint32_t*>(&bloom_filter()[maskwords]); }

    uint32_t* hashvals() { return reinterpret_cast<uint32_t*>(&buckets()[nbuckets]); }
};

uint32_t CalcGnuHash(const std::string& name);

uint32_t CalcHash(const std::string& name);

struct Elf_Hash {
    uint32_t nbuckets{0};
    uint32_t nchains{0};
    uint8_t tail[1];

    const uint32_t* buckets() const { return reinterpret_cast<const uint32_t*>(tail); }

    const uint32_t* chains() const { return buckets() + nbuckets; }
};
