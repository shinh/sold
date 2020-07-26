#include "elf_binary.h"

#include <iostream>

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <in-elf>\nThis program parse DT_RELA and DT_RELA of the given ELF file." << std::endl;
        return 1;
    }

    auto b = ReadELF(argv[1]);
    std::cout << b->ShowDtRela();
    return 0;
}
