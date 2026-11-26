#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string_view>
#include <vector>
#include "../memory/process.h"

namespace data
{
    // One unlock bitfield in PlayerState and the Excel sheet whose row ids index it.
    struct fish_log
    {
        std::string_view name;      // for messages
        std::string_view sheet;
        std::uint32_t first_row_id; // row id stored in bit 0

        std::uintptr_t address{};
        std::optional<std::uint32_t> max_size{};        // bytes, from mem::find_fishlog_globals: exact when anchored, else an upper bound
        std::map<std::uint32_t, std::uint32_t> items{}; // row id -> item id, only rows with an Item
        std::uint32_t block_bits{};                     // last row id of the log's block in the sheet - first_row_id
    };

    class game
    {
    public:
        explicit game(const mem::process& process)
        {
            _process = process;
        }

        void setup_address();
        void setup_excel_sheet();

        [[nodiscard]] std::vector<std::uint32_t> get_unlocked_fishes();

    private:
        void collect_unlocked(const fish_log& log, std::vector<std::uint32_t>& out) const;

        mem::process _process{};
        fish_log _fish{"钓鱼", "FishParameter", 0};
        // Game logic: spearfishing ids start at 20000 (bit = row id - 20000).
        fish_log _spear{"刺鱼", "SpearfishingItem", 20000};
    };
}
