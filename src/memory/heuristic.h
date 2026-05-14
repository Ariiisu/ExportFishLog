#pragma once

#include <cstdint>
#include <optional>

#include "process.h"

namespace mem
{
    struct fishlog_globals
    {
        std::uintptr_t fishlog;
        std::uintptr_t spear_fishlog;
    };

    // Find g_fishlog_data and g_spear_fishlog_data without using byte signatures.
    //
    // Walks the PE exception directory (.pdata) to enumerate every function range,
    // decodes each function with Zydis, and looks for the "bit-test on packed .data
    // byte-table" idiom that the compiler emits at every fishlog/spearfish unlock
    // check. Two arms are matched:
    //
    //   Arm A (MSVC, current shipping compiler):
    //     LEA   r64, [rip+disp32]       ; target ∈ .data
    //     MOVZX r32, byte ptr [r+lea_reg]
    //     SHL/SHR r32, cl
    //     TEST  r8,  r8
    //
    //   Arm B (clang-cl style — future-proofing):
    //     LEA   r64, [rip+disp32]       ; target ∈ .data
    //     MOVZX r32, byte ptr [r+lea_reg]
    //     BT    r32, r32                ; CF = bit-test result
    //
    // The Fisherman's Notebook persistent state lives as a cluster of ~4 adjacent
    // bit-tables in .data. Within that cluster:
    //   - g_fishlog_data       = the LARGEST table (~190 bytes for ~1500 fish IDs)
    //   - g_spear_fishlog_data = the HIGHEST-RVA table in the cluster
    //
    // Returns nullopt if the heuristic cannot identify a plausible cluster.
    std::optional<fishlog_globals> find_fishlog_globals(const process& proc);
}
