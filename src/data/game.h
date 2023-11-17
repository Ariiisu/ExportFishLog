#pragma once

#include <cstdint>
#include <unordered_map>
#include "../memory/process.h"

namespace data
{
    class game
    {
    public:
        explicit game(const std::shared_ptr<mem::process>& process)
        {
            _process = process;
        }

        void setup_address();
        void setup_excel_sheet();

        [[nodiscard]] std::vector<std::uint32_t> get_unlocked_fishes() const;
        [[nodiscard]] bool is_valid() const;

    private:
        [[nodiscard]] bool is_fish_unlocked(std::uint32_t fish_id) const;
        [[nodiscard]] bool is_spear_fish_unlocked(std::uint32_t fish_id) const;

        std::uintptr_t _fishlog_address{};
        std::uintptr_t _spear_fishlog_address{};
        std::uintptr_t _object_table{};
        std::shared_ptr<mem::process> _process;

        std::unordered_map<std::uint32_t, std::uint32_t> _fishlog_map;
        std::unordered_map<std::uint32_t, std::uint32_t> _spear_fishlog_map;
    };
}
