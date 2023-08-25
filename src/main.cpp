#include "data/config.h"
#include "memory/process.h"
#include "data/game.h"
#include "data/network.h"

#include <fmt/color.h>
#include <fmt/core.h>

void enable_color()
{
    // enable color support
    const auto std_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    // its ok if teh handle is valid
    if (std_handle == INVALID_HANDLE_VALUE)
        return;

    DWORD mode;
    if (!GetConsoleMode(std_handle, &mode))
        return;

    // already enabled
    if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        return;

    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(std_handle, mode);
}

int main()
{
    {
        // https://stackoverflow.com/a/45622802
        // Set console code page to UTF-8 so console known how to interpret string data
        SetConsoleOutputCP(CP_UTF8);

        // Enable buffering to prevent VS from chopping up UTF-8 byte sequences
        setvbuf(stdout, nullptr, _IOFBF, 1000);
        setvbuf(stderr, nullptr, _IOFBF, 1000);
    }
    enable_color();

    try
    {
        data::config.setup();
        data::network.setup();

        const auto process = std::make_shared<mem::process>("ffxiv_dx11.exe");

        data::game data(process);
        data.setup();

        std::once_flag flag;

        while (!data.is_valid())
        {
            std::call_once(flag,
                           []
                           {
                               print(fmt::emphasis::bold | fg(fmt::color::red), "检测不到本地玩家...数据将会在检测到后导出");
                           });

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        std::string id_string;
        for (std::vector<std::uint32_t> unlocked_fishes = data.get_unlocked_fishes(); auto&& item_id : unlocked_fishes)
        {
            id_string += fmt::format("{},", item_id);
        }
        std::ofstream file("result.json", std::ios::trunc);
        file
            << R"({"filters":{"completion":"uncaught","patch":[2,2.1,2.2,2.3,2.4,2.5,3,3.1,3.2,3.3,3.4,3.5,4,4.1,4.2,4.3,4.4,4.5,5,5.1,5.2,5.3,5.4,5.5,6]},"pinned":[],"upcomingWindowFormat":"fromPrevClose","sortingType":"windowPeriods","theme":"dark",)"
            + fmt::format("\"completed\":[{}]", id_string.substr(0, id_string.size() - 1)) + "}"
            << std::endl;

        print(fmt::emphasis::bold | fg(fmt::color::light_green), "[+] 结果已保存到 \"result.json\", 5秒后退出程序.");

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    catch (std::exception& ex)
    {
        print(fmt::emphasis::bold | fg(fmt::color::red), "[x] 运行时发生异常: {}\n", ex.what());
        system("pause");
    }

    return 0;
}
