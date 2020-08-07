#include "version_builder.h"

void VersionBuilder::Add(Elf_Versym versym, std::string soname, std::string version, StrtabBuilder& strtab) {
    if (soname != "") strtab.Add(soname);
    if (version != "") strtab.Add(version);

    if (versym == VER_NDX_LOCAL) {
        LOGF("VersionBuilder::VER_NDX_LOCAL\n");
        vers.push_back(versym);
    } else if (versym == VER_NDX_GLOBAL) {
        LOGF("VersionBuilder::VER_NDX_GLOBAL\n");
        vers.push_back(versym);
    } else if (versym == NEED_NEW_VERNUM) {
        if (data.find(soname) != data.end()) {
            if (data[soname].find(version) != data[soname].end()) {
                ;
            } else {
                data[soname][version] = vernum;
                vernum++;
            }
        } else {
            std::map<std::string, int> ma;
            ma[version] = vernum;
            data[soname] = ma;
            vernum++;
        }
        LOGF("VersionBuilder::Add(%d, %s, %s)\n", data[soname][version], soname.c_str(), version.c_str());
        vers.push_back(data[soname][version]);
    } else {
        LOGF("Inappropriate versym = %d\n", versym);
        exit(1);
    }
}

uintptr_t VersionBuilder::SizeVerneed() const {
    uintptr_t s = 0;
    for (const auto& m1 : data) {
        s += sizeof(Elf_Verneed);
        for (const auto& m2 : m1.second) {
            s += sizeof(Elf_Vernaux);
        }
    }
    return s;
}

void VersionBuilder::EmitVersym(FILE* fp) {
    for (auto v : vers) {
        CHECK(fwrite(&v, sizeof(v), 1, fp) == 1);
    }
}

void VersionBuilder::EmitVerneed(FILE* fp, StrtabBuilder& strtab) {
    int n_verneed = 0;
    for (const auto& m1 : data) {
        n_verneed++;

        Elf_Verneed v;
        v.vn_version = 1;
        v.vn_cnt = m1.second.size();
        v.vn_file = strtab.GetPos(m1.first);
        v.vn_aux = sizeof(Elf_Verneed);
        v.vn_next = (n_verneed == data.size()) ? 0 : sizeof(Elf_Verneed) + m1.second.size() * sizeof(Elf_Vernaux);

        CHECK(fwrite(&v, sizeof(v), 1, fp) == 1);

        int n_vernaux = 0;
        for (const auto& m2 : m1.second) {
            n_vernaux++;

            Elf_Vernaux a;
            a.vna_hash = CalcHash(m2.first);
            a.vna_flags = 0;  // TODO(akawashiro) check formal document
            a.vna_other = m2.second;
            a.vna_name = strtab.GetPos(m2.first);
            a.vna_next = (n_vernaux == m1.second.size()) ? 0 : sizeof(Elf_Vernaux);

            CHECK(fwrite(&a, sizeof(a), 1, fp) == 1);
        }
    }
}
