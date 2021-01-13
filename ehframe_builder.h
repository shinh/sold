#pragma once

#include "utils.h"

class EHFrameBuilder {
public:
    EHFrameBuilder() {
        eh_frame_header_.version = 0x1;
        eh_frame_header_.eh_frame_ptr_enc = DW_EH_PE_sdata4 + DW_EH_PE_pcrel;
        eh_frame_header_.fde_count_enc = DW_EH_PE_udata4;
        eh_frame_header_.table_enc = DW_EH_PE_sdata4 | DW_EH_PE_datarel;
        eh_frame_header_.eh_frame_ptr = 0;
        eh_frame_header_.fde_count = 0;
        eh_frame_header_.table = std::vector<EHFrameHeader::FDETableEntry>();
    }

    void Emit(FILE* fp) {
        Write(fp, eh_frame_header_.version);
        Write(fp, eh_frame_header_.eh_frame_ptr_enc);
        Write(fp, eh_frame_header_.fde_count_enc);
        Write(fp, eh_frame_header_.table_enc);
        Write(fp, eh_frame_header_.eh_frame_ptr);
        Write(fp, eh_frame_header_.fde_count);
        for (const EHFrameHeader::FDETableEntry te : eh_frame_header_.table) {
            Write(fp, te.initial_loc);
            Write(fp, te.fde_ptr);
        }
    }

    uintptr_t Size() const {
        return sizeof(eh_frame_header_.version) + sizeof(eh_frame_header_.eh_frame_ptr_enc) + sizeof(eh_frame_header_.fde_count_enc) +
               sizeof(eh_frame_header_.table_enc) + sizeof(eh_frame_header_.eh_frame_ptr) + sizeof(eh_frame_header_.fde_count) +
               eh_frame_header_.fde_count *
                   (sizeof(EHFrameHeader::FDETableEntry::fde_ptr) + sizeof(EHFrameHeader::FDETableEntry::initial_loc));
    }

    void Add(const std::string& name, const EHFrameHeader& efh, const uintptr_t efh_vaddr, const uintptr_t load_offset,
             const uintptr_t new_efh_vaddr) {
        LOG(INFO) << "EHFrameBuilder" << SOLD_LOG_BITS(efh.version) << SOLD_LOG_DWEHPE(efh.eh_frame_ptr_enc)
                  << SOLD_LOG_DWEHPE(efh.fde_count_enc) << SOLD_LOG_DWEHPE(efh.table_enc) << SOLD_LOG_BITS(efh.eh_frame_ptr)
                  << SOLD_LOG_BITS(efh.fde_count);
        for (const auto te : efh.table) {
            LOG(INFO) << "EHFrameBuilder" << SOLD_LOG_BITS(te.fde_ptr) << SOLD_LOG_BITS(te.initial_loc);
        }

        eh_frame_header_.fde_count += efh.fde_count;
        for (const auto te : efh.table) {
            EHFrameHeader::FDETableEntry new_te;
            new_te.fde_ptr = static_cast<int32_t>(efh_vaddr) + te.fde_ptr + static_cast<int32_t>(load_offset) - new_efh_vaddr;
            new_te.initial_loc = static_cast<int32_t>(efh_vaddr) + te.initial_loc + static_cast<int32_t>(load_offset) - new_efh_vaddr;
            eh_frame_header_.table.emplace_back(new_te);
            LOG(INFO) << "EHFrameBuilder" << SOLD_LOG_BITS(new_te.fde_ptr) << SOLD_LOG_BITS(new_te.initial_loc);
        }
    }

    uint32_t fde_count() const { return eh_frame_header_.fde_count; }

private:
    EHFrameHeader eh_frame_header_;
};
