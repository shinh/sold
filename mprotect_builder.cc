#include "mprotect_builder.h"

constexpr uint8_t MprotectBuilder::memprotect_body_code_x86_64[];
constexpr uint8_t MprotectBuilder::memprotect_end_code_x86_64[];
void MprotectBuilder::EmitX86_64(FILE* fp, uintptr_t mprotect_code_offset) {
    uint8_t* mprotect_code = (uint8_t*)malloc(Size());
    uint8_t* mprotect_code_head = mprotect_code;
    for (int i = 0; i < offsets.size(); i++) {
        LOG(INFO) << "MprotectBuilder::Emit: " << SOLD_LOG_BITS(offsets[i]) << SOLD_LOG_BITS(sizes[i]);

        memcpy(mprotect_code_head, memprotect_body_code_x86_64, sizeof(memprotect_body_code_x86_64));
        int64_t* offset_p = (int64_t*)(mprotect_code_head + memprotect_body_addr_offset_x86_64);
        // 17 is the length of `mov $0xdeadbeefdeadbeef, %rdi; lea (%rip), %rsi'
        int64_t offset_v = (offsets[i] & (~(0x1000 - 1))) -
                           (static_cast<int64_t>(mprotect_code_offset) + static_cast<int64_t>(mprotect_code_head - mprotect_code) + 17);
        LOG(INFO) << "MprotectBuilder::Emit" << SOLD_LOG_BITS(offset_v) << SOLD_LOG_KEY(offset_v);
        *offset_p = offset_v;
        uint32_t* size_p = (uint32_t*)(mprotect_code_head + memprotect_body_size_offset_x86_64);
        *size_p = ((offsets[i] + sizes[i]) & (~(0x1000 - 1))) - (offsets[i] & (~(0x1000 - 1)));
        mprotect_code_head += sizeof(memprotect_body_code_x86_64);
    }
    memcpy(mprotect_code_head, memprotect_end_code_x86_64, sizeof(memprotect_end_code_x86_64));
    mprotect_code_head += sizeof(memprotect_end_code_x86_64);

    CHECK(fwrite(mprotect_code, sizeof(uint8_t), Size(), fp) == Size());
    free(mprotect_code);
}
