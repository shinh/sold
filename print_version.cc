//
// print_verneed
//
// This program shows version information.
//

#include "elf_binary.h"

#include <iostream>

int main(int argc, const char* argv[]) {
    google::InitGoogleLogging(argv[0]);

    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <in-elf>\nThis program shows verneed information of the given ELF file." << std::endl;
        return 1;
    }

    auto b = ReadELF(argv[1]);
    std::cout << b->ShowVersion();
    return 0;
}
