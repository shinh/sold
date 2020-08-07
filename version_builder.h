#pragma once

#include <stdio.h>

#include <map>

#include "hash.h"
#include "strtab_builder.h"
#include "utils.h"

class VersionBuilder {
public:
    static const Elf_Versym NEED_NEW_VERNUM = -1;

    void Add(Elf_Versym ver, std::string filename, std::string version_name, StrtabBuilder& strtab);

    uintptr_t SizeVersym() const { return vers.size() * sizeof(Elf_Versym); }

    uintptr_t SizeVerneed() const;

    int NumVerneed() const { return data.size(); }

    void EmitVersym(FILE* fp);

    void EmitVerneed(FILE* fp, StrtabBuilder& strtab);

private:
    // vernum is start from 2 because 0 and 1 are used as VER_NDX_LOCAL	and VER_NDX_GLOBAL.
    int vernum = 2;
    std::map<std::string, std::map<std::string, int>> data;
    std::vector<Elf_Versym> vers;
};
