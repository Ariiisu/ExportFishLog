#pragma once

#include <cstdint>
#include <unordered_map>
#include "../memory/process.h"

namespace data
{
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
        [[nodiscard]] bool is_valid();
        std::string get_localplayer_name();
        std::uint64_t get_localplayer_content_id();

    private:
        [[nodiscard]] bool is_fish_unlocked(std::uint32_t fish_id);
        [[nodiscard]] bool is_spear_fish_unlocked(std::uint32_t fish_id);

        std::uintptr_t _fishlog_address{};
        std::uintptr_t _spear_fishlog_address{};
        std::uintptr_t _object_table{};
        std::uintptr_t _local_player_name{};
        std::uintptr_t _local_player_content_id{};
        mem::process _process{};

        std::unordered_map<std::uint32_t, std::uint32_t> _fishlog_map{};
        std::unordered_map<std::uint32_t, std::uint32_t> _spear_fishlog_map{};
        std::size_t _spearfish_notebook_size{};
    };
}
