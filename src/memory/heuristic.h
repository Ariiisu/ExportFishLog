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
    // Strategy
    // --------
    // Every fishlog/spearfish unlock check is a packed-byte bit-test. We walk the
    // PE exception directory (.pdata) to enumerate all functions, decode each
    // with Zydis, and look for the bit-test idiom anchored on
    //
    //   LEA   r64, [rip+disp32]                  ; target lives in .data
    //   MOVZX r32, byte ptr [reg + lea_reg + d]  ; effective table = lea_target + d
    //   <bit-test consumer>
    //
    // The consumer can be any compiler-equivalent shape:
    //   - TEST/AND/OR  byte_reg, byte_reg
    //   - BT           reg, reg
    //   - SHL/SHR ...,cl  →  TEST byte_reg, byte_reg
    //
    // For every distinct table, we count callsites and check whether each
    // callsite's enclosing function applies the magic constant -20000 to a
    // register before reaching the LEA. That constant comes from FFXIV game
    // logic (spearfishing IDs start at 20000) and is independent of compiler
    // codegen, so it survives recompilation.
    //
    // Classification
    // --------------
    //   spear  = the table where every callsite applies -20000.
    //   fish   = the largest table (by neighbour-gap) that
    //              · is not spear,
    //              · has at least 2 callsites,
    //              · has a known forward gap ≥ 64 bytes.
    //
    // The fishlog bitfield is by a wide margin the largest packed-byte table in
    // PlayerState (≈190 bytes for 1500+ FishParameter rows; the next biggest is
    // ≈56 bytes), so picking by size is robust even when the table grows.
    //
    // Returns nullopt if neither table can be identified.
    std::optional<fishlog_globals> find_fishlog_globals(const process& proc);
}
