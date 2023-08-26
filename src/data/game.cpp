#include "game.h"
#include "../memory/pattern.h"
#include "config.h"
#include "network.h"

#include <fmt/color.h>

void data::game::setup()
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

    print(fmt::emphasis::bold | fg(fmt::color::light_green), "[+] 所需地址已找到\n");
}

std::vector<std::uint32_t> data::game::get_unlocked_fishes() const
{
    print(fmt::emphasis::bold, "[-] 导出数据中...\n");

    std::vector<std::uint32_t> result{};
    for (auto fishlog_map = network.get_fishlog_map(); const auto& [param_id, item_id] : fishlog_map)
    {
        if (!is_fish_unlocked(param_id))
            continue;

        result.push_back(item_id);
    }

    for (auto spear_fishlog_map = network.get_spear_fishlog_map(); const auto& [param_id, item_id] : spear_fishlog_map)
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
