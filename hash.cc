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

#include "hash.h"

uint32_t CalcGnuHash(const std::string& name) {
    uint32_t h = 5381;
    for (unsigned char c : name) {
        h = h * 33 + c;
    }
    return h;
}

uint32_t CalcHash(const std::string& name) {
    uint32_t h = 0, g;
    for (unsigned char c : name) {
        h = (h << 4) + c;
        g = h & 0xf0000000;
        h ^= g >> 24;
        h ^= g;
    }
    return h;
}
