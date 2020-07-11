#pragma once

#include <map>
#include <vector>

#include "hash.h"
#include "strtab_builder.h"
#include "utils.h"

class SymtabBuilder {
public:
    SymtabBuilder() {
        Symbol sym = {0};
        AddSym("");
        CHECK(syms_.emplace("", sym).second);
    }

    struct Symbol {
        Elf_Sym sym;
        uintptr_t index;
    };

    void SetSrcSyms(std::map<std::string, Elf_Sym*> syms) { src_syms_ = syms; }

    uintptr_t AddSym(const std::string& name) {
        uintptr_t index = sym_names_.size();
        sym_names_.push_back(name);
        return index;
    }

    bool Resolve(const std::string& name, uintptr_t* val_or_index);

    uintptr_t ResolveCopy(const std::string& name);

    void Build(StrtabBuilder* strtab);

    void AddPublicSymbol(const std::string& name, Elf_Sym sym) { public_syms_.emplace(name, sym); }

    void MergePublicSymbols(StrtabBuilder* strtab);

    uintptr_t size() const { return symtab_.size() + public_syms_.size(); }

    const Elf_GnuHash& gnu_hash() const { return gnu_hash_; }

    uintptr_t gnu_hash_size() const;

    const std::vector<Elf_Sym>& Get() { return symtab_; }

    const std::vector<std::string>& GetNames() const { return sym_names_; }

private:
    std::map<std::string, Elf_Sym*> src_syms_;
    std::map<std::string, Symbol> syms_;

    std::vector<std::string> sym_names_;
    std::vector<Elf_Sym> symtab_;
    std::map<std::string, Elf_Sym> public_syms_;

    Elf_GnuHash gnu_hash_;
};
