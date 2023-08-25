#include "network.h"
#include "config.h"

#include <fmt/format.h>

#include "csv.hpp"

#include <fmt/color.h>

struct spearfishing_item
{
    std::uint32_t id{}; // 0
    std::string description{}; // 1
    std::uint32_t item_id{}; // 2
    std::int32_t _gathering_item_level{}; // 3
    bool unkn{};                          // 4
    std::int32_t _fishing_record_type{};  // 5
    std::int32_t _territory_type{};       // 6
    std::uint16_t Unknown6{};             // 7
    bool _is_visible{};                   // 8
    auto tied()
    {
        return std::tie(id, description, item_id, _gathering_item_level, unkn, _fishing_record_type, _territory_type, Unknown6, _is_visible);
    }
};

void data::network::setup()
{
    fmt::print(fmt::emphasis::bold, "[-] 开始下载 FishParameter.csv 和 SpearfishingItem.csv\n");

    auto [use_proxy, proxy_url, fishlog_url, spear_fishlog_url] = config.network();
    if (use_proxy)
    {
        _session.SetProxies({ { "http", proxy_url }, { "https", proxy_url } });
    }

    {
        _session.SetUrl(cpr::Url{ fishlog_url });

        auto response = _session.Get();
        if (response.error.code != cpr::ErrorCode::OK)
        {
            throw std::exception(fmt::format("下载 FishParameter.csv 失败. {}", response.error.message).data());
        }

        std::ofstream file("FishParameter.csv", std::ios::in | std::ios::trunc);
        auto str = response.text;
        // trim CRLF
        str.erase(std::ranges::unique(str,
                                      [](const char a, const char b)
                                      {
                                          return a == '\r' && b == '\n';
                                      }).begin(),
                             str.end());
        file << str << std::endl;
        file.close();

        csv::CSVFormat format;
        format.header_row(1);
        for (csv::CSVReader reader("FishParameter.csv", format); auto&& row : reader)
        {
            if (!row["#"].is_num())
                continue;

            if (const auto in_log = row["IsInLog"].get<std::string_view>(); in_log == "False")
                continue;

            const auto item = row["Item"].get<std::uint32_t>();
            if (item == 0)
                continue;

            const auto id   = row["#"].get<std::uint32_t>();
            _fishlog_map[id] = item;
        }
    }

    {
        _session.SetUrl(cpr::Url{ spear_fishlog_url });

        auto response = _session.Get();
        if (response.error.code != cpr::ErrorCode::OK)
        {
            throw std::exception(fmt::format("下载 SpearfishingItem.csv 失败. {}", response.error.message).data());
        }

        std::ofstream file("SpearfishingItem.csv", std::ios::trunc);
        auto str = response.text;
        // trim CRLF
        str.erase(std::ranges::unique(str,
                                      [](const char a, const char b)
                                      {
                                          return a == '\r' && b == '\n';
                                      })
                    .begin(),
                  str.end());
        file << str << std::endl;
        
        file.close();
        csv::CSVFormat format;
        format.header_row(1);
        for (csv::CSVReader reader("SpearfishingItem.csv", format); auto&& row : reader)
        {
            if (!row["#"].is_num())
                continue;

            const auto item  = row["Item"].get<std::uint32_t>();
            if (item == 0)
                continue;

            const auto id    = row["#"].get<std::uint32_t>();
            _fishlog_map[id] = item;
        }
    }

    fmt::print(fmt::emphasis::bold | fg(fmt::color::light_green), "[+] 已保存 FishParameter.csv 和 SpearfishingItem.csv\n");
}
