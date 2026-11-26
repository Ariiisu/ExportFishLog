#include "game.h"
#include "../memory/heuristic.h"

#include <xivres/installation.h>
#include <xivres/excel.h>
#include <fmt/color.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace
{
    using item_map = std::map<std::uint32_t, std::uint32_t>;

    template <typename Fn>
    void for_each_row(const xivres::excel::reader& sheet, Fn&& fn)
    {
        const auto page_count = sheet.get_exh_reader().get_pages().size();
        for (std::size_t page = 0; page < page_count; ++page)
        {
            for (const auto& row : sheet.get_exd_reader(page))
            {
                if (row.size() != 0)
                    fn(row.row_id(), row[0]);
            }
        }
    }

    // SE inserts columns between patches, so Item has no fixed index. The game
    // ships <sheet>Reverse keyed by exactly the item ids the sheet references, so
    // Item is the one integer column whose non-zero values are all reverse row ids.
    std::size_t find_item_column(const xivres::excel::reader& sheet, const std::unordered_set<std::uint32_t>& reverse_row_ids)
    {
        struct candidate
        {
            std::size_t column;
            std::size_t non_zero = 0;
            bool all_in_reverse  = true;
        };

        std::vector<candidate> candidates;
        const auto& columns = sheet.get_exh_reader().get_columns();
        for (std::size_t i = 0; i < columns.size(); ++i)
        {
            const auto type = *columns[i].Type;
            if (type == xivres::excel::cell_type::Int32 || type == xivres::excel::cell_type::UInt32)
                candidates.push_back({.column = i});
        }

        for_each_row(sheet, [&](std::uint32_t, const xivres::excel::exd::row::reader& cells)
        {
            for (auto& c : candidates)
            {
                if (!c.all_in_reverse)
                    continue;

                const auto value = cells[c.column].uint32;
                if (value == 0)
                    continue;

                ++c.non_zero;
                c.all_in_reverse = reverse_row_ids.contains(value);
            }
        });

        std::vector<std::size_t> found;
        for (const auto& c : candidates)
        {
            if (c.all_in_reverse && c.non_zero != 0)
                found.push_back(c.column);
        }

        if (found.size() != 1)
            throw std::runtime_error(fmt::format("{} 中有 {} 列符合 Item 列的特征 ([{}]), 无法确定 Item 列",
                                                 sheet.get_exh_reader().name(), found.size(), fmt::join(found, ", ")));

        return found.front();
    }

    // Rows that are not part of a log sit thousands of ids away from it
    // (FishParameter 1528 -> 15000, SpearfishingItem 20304 -> 30000); inside a log
    // there are only a few placeholder rows without an Item.
    constexpr std::uint32_t max_row_id_gap = 1024;

    // End of the run of ids starting at first_row_id, before the first large gap.
    template <typename It, typename Id>
    It run_end(It it, It end, std::uint32_t first_row_id, Id id)
    {
        for (auto previous = first_row_id; it != end && id(*it) - previous <= max_row_id_gap; ++it)
            previous = id(*it);
        return it;
    }

    // Item of every row of the log's sheet, and the size of the log's block.
    void load_log(const xivres::installation& game, data::fish_log& log)
    {
        const std::string sheet_name(log.sheet);
        const auto reverse = game.get_excel(sheet_name + "Reverse");
        std::unordered_set<std::uint32_t> reverse_row_ids;
        for (std::size_t page = 0; page < reverse.get_exh_reader().get_pages().size(); ++page)
        {
            const auto& row_ids = reverse.get_exd_reader(page).get_row_ids();
            reverse_row_ids.insert(row_ids.begin(), row_ids.end());
        }

        const auto sheet  = game.get_excel(sheet_name);
        const auto column = find_item_column(sheet, reverse_row_ids);

        std::vector<std::uint32_t> row_ids;
        log.items.clear();
        for_each_row(sheet, [&](std::uint32_t row_id, const xivres::excel::exd::row::reader& cells)
        {
            row_ids.push_back(row_id);
            if (const auto item_id = cells[column].uint32; item_id != 0)
                log.items.emplace(row_id, item_id);
        });

        // The bitfield covers [first_row_id, last row id of the block): the block
        // ends with a placeholder row (FishParameter 0-1528 -> 1528 bits,
        // SpearfishingItem 20000-20304 -> 304 bits), and the game's loops stop there.
        std::ranges::sort(row_ids);
        const auto begin = std::ranges::lower_bound(row_ids, log.first_row_id);
        const auto end   = run_end(begin, row_ids.end(), log.first_row_id, [](std::uint32_t id) { return id; });
        log.block_bits   = begin == end ? 0 : *std::prev(end) - log.first_row_id;
    }

    // The game decides which rows belong to a log by range, it never reads
    // IsInLog / IsVisible: it walks the block and skips rows without an Item.
    // The log is therefore the leading run of rows with an Item.
    std::ranges::subrange<item_map::const_iterator> log_rows(const data::fish_log& log)
    {
        const auto begin = log.items.lower_bound(log.first_row_id);
        const auto end   = run_end(begin, log.items.end(), log.first_row_id, [](const auto& row) { return row.first; });
        if (begin == end)
            throw std::runtime_error(fmt::format("{}: {} 中找不到从第 {} 行开始的记录", log.name, log.sheet, log.first_row_id));

        return {begin, end};
    }
}

void data::game::setup_address()
{
    const auto globals = mem::find_fishlog_globals(_process, {.fish = _fish.block_bits, .spear = _spear.block_bits});
    if (!globals)
        throw std::exception("启发式搜索失败: 找不到捕鱼日志/刺鱼日志的地址");

    print(stdout, fmt::emphasis::bold | fg(fmt::color::light_green),
          "[+] PID: {}, 所需地址已找到 (fishlog=ffxiv_dx11.exe+0x{:X}, spear=ffxiv_dx11.exe+0x{:X})\n",
          _process.get_pid(), globals->fish.address - _process.base_address(), globals->spear.address - _process.base_address());

    for (const auto& [log, found] : {std::pair{&_fish, globals->fish}, std::pair{&_spear, globals->spear}})
    {
        log->address  = found.address;
        log->max_size = found.size;
        if (!found.anchored)
            print(stdout, fmt::emphasis::bold | fg(fmt::color::yellow),
                  "[!] PID: {}, 没有找到与 {} 行数对应的循环上界, {}日志的地址是按内存布局推断的, 结果可能有误\n", _process.get_pid(), log->sheet,
                  log->name);
    }
}

void data::game::setup_excel_sheet()
{
    const xivres::installation game_reader(_process.get_process_path());

    for (auto* log : {&_fish, &_spear})
    {
        print(stdout, fmt::emphasis::bold, "[-] PID: {}, 正在获取{}的数据\n", _process.get_pid(), log->name);
        load_log(game_reader, *log);
    }

    print(stdout, fmt::emphasis::bold | fg(fmt::color::light_green), "[+] PID: {}, 已获取所需csv文件的内容\n", _process.get_pid());
}

std::vector<std::uint32_t> data::game::get_unlocked_fishes()
{
    print(stdout, fmt::emphasis::bold, "[-] PID: {}, 导出数据中...\n", _process.get_pid());

    std::vector<std::uint32_t> result{};
    collect_unlocked(_fish, result);
    collect_unlocked(_spear, result);
    return result;
}

void data::game::collect_unlocked(const fish_log& log, std::vector<std::uint32_t>& out) const
{
    const auto rows        = log_rows(log);
    const auto last_row_id = std::prev(rows.end())->first;
    if (const auto limit = log.max_size ? log.first_row_id + std::uint64_t{*log.max_size} * 8 : UINT64_MAX; last_row_id >= limit)
        throw std::runtime_error(fmt::format("{}: {} 第 {} 行超出了位图范围 ({} 字节), 地址或表结构可能已变化", log.name, log.sheet,
                                             log.items.lower_bound(static_cast<std::uint32_t>(limit))->first, *log.max_size));

    std::vector<std::uint8_t> bitfield((last_row_id - log.first_row_id) / 8 + 1);
    if (!_process.read_bytes(log.address, bitfield))
        throw std::runtime_error(fmt::format("无法获取{}日志. 可能因为没有管理员运行或者杀软误报", log.name));

    std::size_t total    = 0;
    std::size_t unlocked = 0;
    for (const auto& [row_id, item_id] : rows)
    {
        ++total;
        const auto bit = row_id - log.first_row_id;
        if (((bitfield[bit / 8] >> (bit % 8)) & 1) == 0)
            continue;

        out.push_back(item_id);
        ++unlocked;
    }

    print(stdout, fmt::emphasis::bold | fg(fmt::color::light_green), "[+] PID: {}, {}日志: {} 第 {}~{} 行共 {} 条, 已解锁 {} 条\n",
          _process.get_pid(), log.name, log.sheet, rows.begin()->first, last_row_id, total, unlocked);
}
