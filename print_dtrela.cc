//
// print_dtrela
//
// This program shows relocation entries in DT_RELA.
//

#include "elf_binary.h"

#include <iostream>

int main(int argc, const char* argv[]) {
    google::InitGoogleLogging(argv[0]);

    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <in-elf>\nThis program parse DT_RELA of the given ELF file." << std::endl;
        return 1;
    }

    auto b = ReadELF(argv[1]);
    std::cout << b->ShowDtRela();
    return 0;
}
