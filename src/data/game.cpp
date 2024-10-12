#include "game.h"
#include "../memory/pattern.h"
#include "config.h"

#include <xivres/installation.h>
#include <xivres/excel.h>
#include <fmt/color.h>

void data::game::setup_address()
{
    const auto [fishlog_sig,
        spear_fishlog_sig,
        object_table_sig,
        current_fishing_bite_sig,
        localplayer_name_sig,
        localplayer_content_id] = config.signatures();

    _fishlog_address = _process.find_pattern(pattern::make(fishlog_sig), true);
    if (!_fishlog_address)
        throw std::exception("找不到捕鱼日志的地址, 更新下signature");

    _spear_fishlog_address = _process.find_pattern(pattern::make(spear_fishlog_sig), true);
    if (!_spear_fishlog_address)
    {
        print(stdout,
              fmt::emphasis::bold | fg(fmt::color::yellow),
              "[!] 刺鱼日志的signature失效,用另外一种方法获取地址.如果两种方法都无效,或得出的结果有异常,请打开 \"config.toml\" 然后更新spear_fishlog的signature\n");

        const auto current_fishing_bite_address = _process.find_pattern(pattern::make(current_fishing_bite_sig), true, 2);

        _spear_fishlog_address = current_fishing_bite_address + 4 /*skip current field*/ + (_spearfish_notebook_size >> 3);
    }

    _object_table = _process.find_pattern(pattern::make(object_table_sig), true);
    if (!_object_table)
        throw std::exception("找不到object table的地址, 更新下signature");

    _local_player_name = _process.find_pattern(pattern::make(localplayer_name_sig), true);
    if (!_local_player_name)
        throw std::exception("找不到local_player_name的地址, 更新下signature");

    _local_player_content_id = _process.find_pattern(pattern::make(localplayer_content_id), true);
    if (!_local_player_content_id)
        throw std::exception("找不到local_player_content_id的地址, 更新下signature");

    print(stdout, fmt::emphasis::bold | fg(fmt::color::light_green), "[+] PID: {}, 所需地址已找到\n", _process.get_pid());
}

void data::game::setup_excel_sheet()
{
    const std::wstring path = _process.get_process_path();
    const xivres::installation game_reader(path);

    std::size_t inlog_index = 0;
    print(stdout, fmt::emphasis::bold, "[-] PID: {}, 正在获取钓鱼的数据\n", _process.get_pid());

    const auto fish_param_sheet = game_reader.get_excel("FishParameter");
    for (std::size_t i = 0; i < fish_param_sheet.get_exh_reader().get_pages().size(); i++)
    {
        for (const auto& row : fish_param_sheet.get_exd_reader(i))
        {
            for (const auto& subrow : row)
            {
                const auto item_id = subrow[4].int32;
                const auto in_log  = subrow[12].boolean;

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

    const auto spearfishing_notebook = game_reader.get_excel("SpearfishingNotebook");
    for (std::size_t i = 0; i < spearfishing_notebook.get_exh_reader().get_pages().size(); i++)
    {
        for (const auto& _ : spearfishing_notebook.get_exd_reader(i))
        {
            _spearfish_notebook_size++;
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

bool data::game::is_valid()
{
    const auto localplayer = _process.read<std::uintptr_t>(_object_table);
    if (!localplayer)
        throw std::exception("无法获取本地玩家地址. 可能因为没有管理员运行或者杀软误报");

    return *localplayer != 0;
}

std::string data::game::get_localplayer_name()
{
    const auto buffer = _process.read<char, 32>(_local_player_name).value();

    return buffer;
}

std::uint64_t data::game::get_localplayer_content_id()
{
    const auto val = _process.read<std::uintptr_t>(_local_player_content_id);
    if (!val)
        throw std::exception("无法获取本地玩家的content id. 可能因为没有管理员运行或者杀软误报");

    return *val;
}
