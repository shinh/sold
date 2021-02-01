//
// print_tls
//
// This program shows tls entries.
//
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

#include "elf_binary.h"

#include <iostream>

int main(int argc, const char* argv[]) {
    google::InitGoogleLogging(argv[0]);

    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <in-elf>\nThis program shows TLS entries of the given ELF file." << std::endl;
        return 1;
    }

    auto b = ReadELF(argv[1]);
    std::cout << b->ShowTLS();
    return 0;
}
