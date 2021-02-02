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

#include <map>
#include <string>
#include <vector>

#include "utils.h"

class ShdrBuilder {
public:
    enum ShdrType { GnuHash, Dynsym, GnuVersion, GnuVersionR, Dynstr, RelaDyn, InitArray, FiniArray, Strtab, Shstrtab, Dynamic, Text, TLS };
    void EmitShstrtab(FILE* fp);
    void EmitShdrs(FILE* fp);
    uintptr_t ShstrtabSize() const;
    Elf_Half CountShdrs() const { return shdrs.size(); }
    void RegisterShdr(Elf_Off offset, uint64_t size, ShdrType type, uint64_t entsize = 0, Elf_Word info = 0);
    Elf_Half Shstrndx() const { return shdrs.size() - 1; }

    // After register all shdrs, you must call Freeze.
    void Freeze();

private:
    const std::map<ShdrType, std::string> type_to_str = {
        {GnuHash, ".gnu.hash"}, {Dynsym, ".dynsym"},     {GnuVersion, ".gnu.version"}, {GnuVersionR, ".gnu.version_r"},
        {Dynstr, ".dynstr"},    {RelaDyn, ".rela.dyn"},  {InitArray, ".init_array"},   {FiniArray, ".fini_array"},
        {Strtab, ".strtab"},    {Shstrtab, ".shstrtab"}, {Dynamic, ".dynamic"},        {Text, ".text"},
        {TLS, ".tls"}};

    // The first section header must be NULL.
    std::vector<Elf_Shdr> shdrs = {Elf_Shdr{0}};
    uint32_t GetShName(ShdrType type) const;
    uint32_t GetIndex(ShdrType type) const;
};
