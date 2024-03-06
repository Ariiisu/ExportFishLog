#include "data/config.h"
#include "memory/process.h"
#include "data/game.h"
#include "data/json.hpp"

#include <iostream>
#include <fmt/color.h>
#include <fmt/core.h>
#include <magic_enum.hpp>
#include <stacktrace>

void set_utf8_output()
{
    // https://stackoverflow.com/a/45622802
    // https://stackoverflow.com/a/77225440

    // console UTF-8
    std::setlocale(LC_CTYPE, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

void enable_color()
{
    // enable color support
    const auto std_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    // its ok if the handle is invalid
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

void parse_input(pastry_fish::Main& pastry_fish_struct)
{
    const auto read_error = glz::read_file_json(pastry_fish_struct, "./pastry_fish_data.json", std::string{});
    if (!read_error)
    {
        print(stdout, fmt::emphasis::bold | fg(fmt::color::light_green), "[+] 解析成功\n");
        return;
    }
    fmt::print(stdout,
               fmt::emphasis::bold | fg(fmt::color::red),
               "[x] 在读取文件 \"pastry_fish_data.json\"时出错. 原因: {}. loc:{}.\n",
               magic_enum::enum_name(read_error.ec),
               read_error.location);

    fmt::println("[-] 请输入从鱼糕上导出的数据 (可以从鱼糕页面左下角的 导入/导出 -> 点 \"鱼糕本站导入导出\" 里的导出按钮 -> 复制到剪贴板 获取内容)");
    std::string input;
    std::cin >> input;
    const auto parse_error = glz::read_json(pastry_fish_struct, input);
    if (!parse_error)
    {
        print(stdout, fmt::emphasis::bold | fg(fmt::color::light_green), "[+] 解析成功\n");
        return;
    }

    fmt::print(stdout,
               fmt::emphasis::bold | fg(fmt::color::red),
               "[x] 在解析鱼糕导出的数据时出错. 原因: {} / loc: {}. 生成出来的内容只会有钓过的鱼\n",
               magic_enum::enum_name(parse_error.ec),
               parse_error.location);
}

int main()
{
    set_utf8_output();
    enable_color();

    std::thread(
    []
    {
        while (true)
        {
            std::fflush(stdout);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }).detach();

    try
    {
        std::once_flag flag;
        data::config.setup();

        const auto process = std::make_shared<mem::process>("ffxiv_dx11.exe");

        data::game data(process);
        data.setup_excel_sheet();
        data.setup_address();

        pastry_fish::Main pastry_fish_struct;
        parse_input(pastry_fish_struct);

        while (!data.is_valid())
        {
            std::call_once(flag,
                           []
                           {
                               print(stdout, fmt::emphasis::bold | fg(fmt::color::red), "[x] 检测不到本地玩家,数据将会在检测到后导出\n");
                           });

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        pastry_fish_struct.completed = data.get_unlocked_fishes();
        if (glz::write_file_json(pastry_fish_struct, "./result.json", std::string{}))
        {
            throw std::runtime_error(fmt::format("写入文件时出错"));
        }

        print(stdout, fmt::emphasis::bold | fg(fmt::color::light_green), "[+] 结果已保存到 \"result.json\", 5秒后退出程序.\n");

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    catch (std::exception& ex)
    {
        print(stdout, fmt::emphasis::bold | fg(fmt::color::red), "[x] 运行时发生异常: {}\n{}\n", ex.what(), std::to_string(std::stacktrace::current()));
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}
