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

#include <stdint.h>
#include <vector>

#include "utils.h"

class MprotectBuilder {
public:
    void SetMachineType(const Elf64_Half machine_type) {
        CHECK(machine_type == EM_X86_64 || machine_type == EM_AARCH64);
        machine_type_ = machine_type;
    }
    void Add(uintptr_t offset, uintptr_t size) {
        offsets.emplace_back(offset);
        sizes.emplace_back(size);
    }
    uintptr_t Size() const { return sizeof(memprotect_body_code) * offsets.size() + sizeof(memprotect_end_code); }
    void Emit(FILE* fp, uintptr_t mprotect_code_offset) {
        CHECK(machine_type_ == EM_X86_64 || machine_type_ == EM_AARCH64) << "SetMachineType before Emit";
        if (machine_type_ == EM_X86_64) {
            EmitX86_64(fp, mprotect_code_offset);
        }
    }

    // call SYS_mprotect syscall
    //
    // mov $0xdeadbeefdeadbeef, %rdi
    // lea (%rip), %rsi
    // add %rsi, %rdi
    // mov $0xaabbcc, %rsi (size)
    // mov $0x1, %rdx (0x1 = PROT_READ)
    // mov $10, %eax (10 = SYS_mprotect)
    // syscall
    // test %eax, %eax
    // jz ok
    // ud2
    // ok:
    static constexpr uint8_t memprotect_body_code_x86_64[] = {0x48, 0xbf, 0xef, 0xbe, 0xad, 0xde, 0xef, 0xbe, 0xad, 0xde, 0x48, 0x8d,
                                                              0x35, 0x00, 0x00, 0x00, 0x00, 0x48, 0x01, 0xf7, 0x48, 0xc7, 0xc6, 0xcc,
                                                              0xbb, 0xaa, 0x00, 0x48, 0xc7, 0xc2, 0x01, 0x00, 0x00, 0x00, 0xb8, 0x0a,
                                                              0x00, 0x00, 0x00, 0x0f, 0x05, 0x85, 0xc0, 0x74, 0x02, 0x0f, 0x0b};
    // offset to 0xabbccdd
    static constexpr int memprotect_body_addr_offset_x86_64 = 2;
    // offset to 0xaabbcc
    static constexpr int memprotect_body_size_offset_x86_64 = 23;

    // ret
    static constexpr uint8_t memprotect_end_code_x86_64[] = {0xc3};

private:
    void EmitX86_64(FILE* fp, uintptr_t mprotect_code_offset);
    Elf64_Half machine_type_;
    std::vector<int64_t> offsets;
    std::vector<uint32_t> sizes;
};
