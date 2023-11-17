#include "game.h"
#include "../memory/pattern.h"
#include "config.h"

#include <xivres/installation.h>
#include <xivres/excel.h>

#include <fmt/color.h>

void data::game::setup_address()
{
    const auto [fishlog_sig, spear_fishlog_sig, object_table_sig] = config.signatures();

    _fishlog_address = _process->find_pattern(pattern::make(fishlog_sig), true);
    if (!_fishlog_address)
        throw std::exception("找不到捕鱼日志的地址, 更新下signature");

    _spear_fishlog_address = _process->find_pattern(pattern::make(spear_fishlog_sig), true);
    if (!_spear_fishlog_address)
        throw std::exception("找不到刺鱼日志的地址, 更新下signature");

    _object_table = _process->find_pattern(pattern::make(object_table_sig), true);
    if (!_object_table)
        throw std::exception("找不到object table的地址, 更新下signature");

    print(stdout, fmt::emphasis::bold | fg(fmt::color::light_green), "[+] 所需地址已找到\n");
}

void data::game::setup_excel_sheet()
{
    const xivres::installation game_reader(_process->get_process_path());

    const auto fish_param_sheet = game_reader.get_excel("FishParameter");
    for (std::size_t i = 0; i < fish_param_sheet.get_exh_reader().get_pages().size(); i++)
    {
        for (const auto& row : fish_param_sheet.get_exd_reader(i))
        {
            for (const auto& subrow : row)
            {
                if (const auto is_in_log = subrow[8].boolean; !is_in_log)
                    continue;

                if (const auto description = subrow[0].String.repr(); description.empty())
                    continue;

                _fishlog_map[row.row_id()] = subrow[1].int32; /*item id*/
            }
        }
    }

    const auto spear_fish_sheet = game_reader.get_excel("SpearfishingItem");
    for (std::size_t i = 0; i < spear_fish_sheet.get_exh_reader().get_pages().size(); i++)
    {
        for (const auto& row : spear_fish_sheet.get_exd_reader(i))
        {
            for (const auto& subrow : row)
            {
                if (const auto description = subrow[0].String.repr(); description.empty())
                    continue;

                _spear_fishlog_map[row.row_id()] = subrow[1].int32; /*item id*/
            }
        }
    }

    print(stdout, fmt::emphasis::bold | fg(fmt::color::light_green), "[+] 已获取所需csv文件的内容\n");
}

std::vector<std::uint32_t> data::game::get_unlocked_fishes() const
{
    print(stdout, fmt::emphasis::bold, "[-] 导出数据中...\n");

    std::vector<std::uint32_t> result{};
    for (const auto& [param_id, item_id] : _fishlog_map)
    {
        if (!is_fish_unlocked(param_id))
            continue;

        result.push_back(item_id);
    }

    for (const auto& [param_id, item_id] : _spear_fishlog_map)
    {
        if (!is_spear_fish_unlocked(param_id))
            continue;

        result.push_back(item_id);
    }
    return result;
}

bool data::game::is_fish_unlocked(std::uint32_t fish_id) const
{
    const auto offset = fish_id / 8;
    const auto bit    = static_cast<std::uint8_t>(fish_id) % 8;

    const auto addr = _process->read<std::uint8_t>(_fishlog_address + offset);
    if (!addr.has_value())
        throw std::exception("无法获取钓鱼日志. 可能因为没有管理员运行或者杀软误报");

    return ((*addr >> bit) & 1) == 1;
}

bool data::game::is_spear_fish_unlocked(std::uint32_t fish_id) const
{
    constexpr int spear_fishlog_offset = 20000;

    fish_id -= spear_fishlog_offset;
    const auto offset = fish_id / 8;
    const auto bit    = static_cast<std::uint8_t>(fish_id) % 8;

    const auto addr = _process->read<std::uint8_t>(_spear_fishlog_address + offset);
    if (!addr.has_value())
        throw std::exception("无法获取刺鱼日志. 可能因为没有管理员运行或者杀软误报");

    return ((*addr >> bit) & 1) == 1;
}

bool data::game::is_valid() const
{
    const auto localplayer = _process->read<std::uintptr_t>(_object_table);
    if (!localplayer)
        throw std::exception("无法获取本地玩家地址. 可能因为没有管理员运行或者杀软误报");

    return *localplayer != 0;
}
