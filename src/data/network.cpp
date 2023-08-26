#include "network.h"
#include "config.h"

#include "csv.hpp"

#include <fmt/format.h>
#include <fmt/color.h>

void data::network::setup()
{
    print(fmt::emphasis::bold, "[-] 开始下载 FishParameter.csv 和 SpearfishingItem.csv\n");

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

            const auto id    = row["#"].get<std::uint32_t>();
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

            const auto item = row["Item"].get<std::uint32_t>();
            if (item == 0)
                continue;

            const auto id    = row["#"].get<std::uint32_t>();
            _spear_fishlog_map[id] = item;
        }
    }

    print(fmt::emphasis::bold | fg(fmt::color::light_green), "[+] 已保存 FishParameter.csv 和 SpearfishingItem.csv\n");
}
