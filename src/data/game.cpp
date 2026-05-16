#include "game.h"
#include "../memory/heuristic.h"

#include <xivres/installation.h>
#include <xivres/excel.h>
#include <fmt/color.h>

void data::game::setup_address()
{
    const auto fishlog_pair = mem::find_fishlog_globals(_process);
    if (!fishlog_pair)
        throw std::exception("启发式搜索失败: 找不到捕鱼日志/刺鱼日志的地址");

    _fishlog_address       = fishlog_pair->fishlog;
    _spear_fishlog_address = fishlog_pair->spear_fishlog;

    print(stdout, fmt::emphasis::bold | fg(fmt::color::light_green),
          "[+] PID: {}, 所需地址已找到 (fishlog=ffxiv_dx11.exe+0x{:X}, spear=ffxiv_dx11.exe+0x{:X})\n",
          _process.get_pid(), _fishlog_address - _process.base_address(), _spear_fishlog_address - _process.base_address());
}

static std::once_flag once_flag{};

void data::game::setup_excel_sheet()
{
    const std::wstring path = _process.get_process_path();
    const xivres::installation game_reader(path);

    int inlog_index   = -1;
    int item_id_index = -1;

    print(stdout, fmt::emphasis::bold, "[-] PID: {}, 正在获取钓鱼的数据\n", _process.get_pid());

    const auto fish_param_sheet = game_reader.get_excel("FishParameter");
    for (std::size_t i = 0; i < fish_param_sheet.get_exh_reader().get_pages().size(); i++)
    {
        for (const auto& row : fish_param_sheet.get_exd_reader(i))
        {
            std::call_once(once_flag,
                           [row, &inlog_index, &item_id_index]
                           {
                               std::vector<xivres::excel::cell_type> types{};

                               for (const auto& j : row[0])
                               {
                                   types.emplace_back(j.Type);
                               }

                               if (auto item_it = std::ranges::find(types, xivres::excel::cell_type::Int32); item_it != types.end())
                               {
                                   item_id_index = std::distance(types.begin(), item_it);
                               }

                               if (auto inlog_it = std::ranges::find(types, xivres::excel::cell_type::PackedBool1); inlog_it != types.end())
                               {
                                   inlog_index = std::distance(types.begin(), inlog_it);
                               }

                               if (inlog_index == -1)
                                   throw std::exception("找不到 inlog 的index");
                               if (item_id_index == -1)
                                   throw std::exception("找不到 itemid 的index");
                           });
            
            for (const auto& subrow : row)
            {
                const auto item_id = subrow[item_id_index].int32;
                const auto in_log  = subrow[inlog_index].boolean;

                if (item_id == 0 || !in_log)
                    continue;

                _fishlog_map[row.row_id()] = item_id;
            }
        }
    }

    print(stdout, fmt::emphasis::bold, "[-] PID: {}, 正在获取刺鱼的数据\n", _process.get_pid());

    const auto spear_fish_sheet = game_reader.get_excel("SpearfishingItem");
    for (std::size_t i = 0; i < spear_fish_sheet.get_exh_reader().get_pages().size(); i++)
    {
        for (const auto& row : spear_fish_sheet.get_exd_reader(i))
        {
            for (const auto& subrow : row)
            {
                const auto item_id = subrow[1].int32;
                if (item_id == 0)
                    continue;

                _spear_fishlog_map[row.row_id()] = item_id;
            }
        }
    }

    print(stdout, fmt::emphasis::bold | fg(fmt::color::light_green), "[+] PID: {}, 已获取所需csv文件的内容\n", _process.get_pid());
}

std::vector<std::uint32_t> data::game::get_unlocked_fishes()
{
    print(stdout, fmt::emphasis::bold, "[-] PID: {}, 导出数据中...\n", _process.get_pid());

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

bool data::game::is_fish_unlocked(std::uint32_t fish_id)
{
    const auto offset = fish_id / 8;
    const auto bit    = static_cast<std::uint8_t>(fish_id) % 8;

    const auto addr = _process.read<std::uint8_t>(_fishlog_address + offset);
    if (!addr.has_value())
        throw std::exception("无法获取钓鱼日志. 可能因为没有管理员运行或者杀软误报");

    return ((*addr >> bit) & 1) == 1;
}

bool data::game::is_spear_fish_unlocked(std::uint32_t fish_id)
{
    constexpr int spear_fishlog_offset = 20000;

    fish_id -= spear_fishlog_offset;
    const auto offset = fish_id / 8;
    const auto bit    = static_cast<std::uint8_t>(fish_id) % 8;

    const auto addr = _process.read<std::uint8_t>(_spear_fishlog_address + offset);
    if (!addr.has_value())
        throw std::exception("无法获取刺鱼日志. 可能因为没有管理员运行或者杀软误报");

    return ((*addr >> bit) & 1) == 1;
}

