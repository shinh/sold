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

#include <stdio.h>

#include <map>

#include "hash.h"
#include "strtab_builder.h"
#include "utils.h"

class VersionBuilder {
public:
    void Add(Elf_Versym versym, const std::string& soname, const std::string& version, StrtabBuilder& strtab, const unsigned char st_info);

    uintptr_t SizeVersym() const { return (data.size() > 0) ? vers.size() * sizeof(Elf_Versym) : 0; }

    uintptr_t SizeVerneed() const;

    int NumVerneed() const { return data.size(); }

    void EmitVersym(FILE* fp);

    void EmitVerneed(FILE* fp, StrtabBuilder& strtab);

    void SetSonameToFilename(const std::map<std::string, std::string>& soname_to_filename) { soname_to_filename_ = soname_to_filename; }

private:
    // vernum starts from 2 because 0 and 1 are used as VER_NDX_LOCAL and VER_NDX_GLOBAL.
    int vernum = 2;
    std::map<std::string, std::map<std::string, int>> data;
    std::vector<Elf_Versym> vers;
    std::map<std::string, std::string> soname_to_filename_;
};
