#pragma once

#include <cstdint>
#include <optional>

#include "process.h"

namespace mem
{
    // Size of each log's block of rows in the game's Excel data: the block's
    // last row id (trailing placeholder rows included) - first row id
    // (FishParameter from 0, SpearfishingItem from 20000). The game sizes the
    // bitfield and bounds the loops that build the log with it.
    struct log_bits
    {
        std::uint32_t fish;
        std::uint32_t spear;
    };

    struct bitfield
    {
        std::uintptr_t address;
        // Bytes. Exact when anchored; otherwise the distance to the next detected
        // table, an upper bound that includes whatever PlayerState keeps in
        // between, or nullopt when no table follows.
        std::optional<std::uint32_t> size;
        // Found through the game's own loop bound for the data's block size, not
        // just through PlayerState layout or spearfishing constants.
        bool anchored;
    };

    struct fishlog_globals
    {
        bitfield fish;
        bitfield spear;
    };

    // Find the caught-fish and caught-spearfish bitfields in PlayerState without
    // byte signatures, independent of how the game was compiled.
    //
    // Detection
    // ---------
    // Every executable byte is decoded with Zydis: functions from .pdata (cold
    // fragments attributed to their parent), code without unwind info as
    // standalone regions. A linear register tracker follows addresses of
    // writable data loaded by rip-relative LEA, or the image base for
    // [image_base + index + table_rva] addressing, through copies, constant
    // offsets and added indices. A bit table is a data address accessed one byte
    // at a variable index and tested against a variable bit:
    //   - load + TEST/AND/OR with another register (a shifted mask),
    //   - load + BT with a bit index register, or SHR/SAR/SHRX by a register,
    //   - TEST/AND/OR/XOR/BT/BTS/BTR/BTC directly on memory.
    // Unrelated instructions may sit between LEA, load and test, and the LEA
    // may be hoisted out of the loop: pre-7.0 builds do both.
    //
    // Identification
    // --------------
    // Anchor: the code building each log walks its rows up to a constant, the
    // bitfield's bit count (fish: cmp x, 5F8h; spear: cmp x, 20000 + 130h),
    // which equals the log's block size in the same Excel sheet the tool reads.
    // A function comparing against exactly that number (or one less, for <=)
    // and reading a byte of a bit table marks that table as the log's
    // bitfield; bits / 8 rounded up is its exact size. Tables the bitfield
    // would not fit are skipped; more than one candidate is no anchor, and
    // neither is a fish block under 64 bytes. Both bitfields live in
    // PlayerState: two anchors more than 1 KiB apart cannot both be right, and
    // neither is used.
    //
    // Fallback for a log without an anchor:
    //   spear = the table accessed by the most functions that contain a
    //           spearfishing constant: ±20000 (spearfishing ids start at 20000)
    //           or a loop bound 20000 + bit count.
    //   fish  = the nearest table of at least 64 bytes before the spear table:
    //           PlayerState keeps fish bitfield, caught counters, the small
    //           spearfishing notebook bitfield, then the spearfish bitfield
    //           (unchanged from 2021 through 7.5).
    //
    // Returns nullopt if either table cannot be identified unambiguously.
    std::optional<fishlog_globals> find_fishlog_globals(const process& proc, const log_bits& needed);
}
