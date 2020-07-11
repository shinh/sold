#include "symtab_builder.h"

#include <limits>

SymtabBuilder::SymtabBuilder() {
    Symbol sym = {0};
    AddSym("");
    CHECK(syms_.emplace("", sym).second);
}

uintptr_t SymtabBuilder::AddSym(const std::string& name) {
    uintptr_t index = sym_names_.size();
    sym_names_.push_back(name);
    return index;
}

bool SymtabBuilder::Resolve(const std::string& name, uintptr_t& val_or_index) {
    Symbol sym{};
    sym.sym.st_name = 0;
    sym.sym.st_info = 0;
    sym.sym.st_other = 0;
    sym.sym.st_shndx = 0;
    sym.sym.st_value = 0;
    sym.sym.st_size = 0;

    auto found = syms_.find(name);
    if (found != syms_.end()) {
        sym = found->second;
    } else {
        auto found = src_syms_.find(name);
        if (found != src_syms_.end()) {
            sym.sym = *found->second;
            if (IsDefined(sym.sym)) {
                LOGF("Symbol %s found\n", name.c_str());
            } else {
                LOGF("Symbol (undef/weak) %s found\n", name.c_str());
                sym.index = AddSym(name);
                CHECK(syms_.emplace(name, sym).second);
            }
        } else {
            LOGF("Symbol %s not found\n", name.c_str());
            sym.index = AddSym(name);
            CHECK(syms_.emplace(name, sym).second);
        }
    }

    if (!sym.sym.st_value) {
        val_or_index = sym.index;
        return false;
    } else {
        val_or_index = sym.sym.st_value;
        return true;
    }
}

uintptr_t SymtabBuilder::ResolveCopy(const std::string& name) {
    // TODO(hamaji): Refactor.
    Symbol sym{};
    sym.sym.st_name = 0;
    sym.sym.st_info = 0;
    sym.sym.st_other = 0;
    sym.sym.st_shndx = 0;
    sym.sym.st_value = 0;
    sym.sym.st_size = 0;

    auto found = syms_.find(name);
    if (found != syms_.end()) {
        sym = found->second;
    } else {
        auto found = src_syms_.find(name);
        if (found != src_syms_.end()) {
            LOGF("Symbol %s found for copy\n", name.c_str());
            sym.sym = *found->second;
            sym.index = AddSym(name);
            CHECK(syms_.emplace(name, sym).second);
        } else {
            LOGF("Symbol %s not found for copy\n", name.c_str());
            CHECK(false);
        }
    }

    return sym.index;
}

void SymtabBuilder::Build(StrtabBuilder& strtab) {
    for (const std::string& name : sym_names_) {
        auto found = syms_.find(name);
        CHECK(found != syms_.end());
        Elf_Sym sym = found->second.sym;
        sym.st_name = strtab.Add(name);
        symtab_.push_back(sym);
    }
}

void SymtabBuilder::MergePublicSymbols(StrtabBuilder& strtab) {
    gnu_hash_.nbuckets = 1;
    CHECK(symtab_.size() <= std::numeric_limits<uint32_t>::max());
    gnu_hash_.symndx = symtab_.size();
    gnu_hash_.maskwords = 1;
    gnu_hash_.shift2 = 1;

    for (const auto& p : public_syms_) {
        const std::string& name = p.first;
        Elf_Sym sym = p.second;
        sym.st_name = strtab.Add(name);
        sym.st_shndx = 1;
        sym_names_.push_back(name);
        symtab_.push_back(sym);
    }
    public_syms_.clear();
}

uintptr_t SymtabBuilder::GnuHashSize() const {
    CHECK(!symtab_.empty());
    CHECK(public_syms_.empty());
    CHECK(gnu_hash_.nbuckets);
    return (sizeof(uint32_t) * 4 + sizeof(Elf_Addr) + sizeof(uint32_t) * (1 + symtab_.size() - gnu_hash_.symndx));
}
