#include "data/config.h"
#include "memory/process.h"
#include "data/game.h"
#include "data/json.hpp"

#include <iostream>
#include <fmt/color.h>
#include <fmt/core.h>
#include <magic_enum.hpp>
#include <stacktrace>

#include <Windows.h>
#include <tlhelp32.h>

static void set_utf8_output()
{
    // https://stackoverflow.com/a/45622802
    // https://stackoverflow.com/a/77225440

    // console UTF-8
    std::setlocale(LC_CTYPE, ".UTF8");
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

static void enable_color()
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

static void parse_input(pastry_fish::Main& pastry_fish_struct)
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
               "[x] 在解析鱼糕导出的数据时出错. 原因: {} / loc: {}. 生成出来的内容不会带有鱼糕的设置.\n",
               magic_enum::enum_name(parse_error.ec),
               parse_error.location);
}

static std::vector<DWORD> get_ffxiv_processes()
{
    std::vector<DWORD> result{};
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

    for (auto ok = Process32First(snapshot, &entry); ok; ok = Process32Next(snapshot, &entry))
    {
        if (std::string_view(entry.szExeFile) != "ffxiv_dx11.exe")
            continue;

        const auto pid = entry.th32ProcessID;
        if (std::ranges::contains(result, pid))
            continue;

        result.push_back(pid);
    }

    CloseHandle(snapshot);
    return result;
}

static void dump_data(pastry_fish::Main& pastry_fish_struct, const DWORD pid)
{
    const auto process = mem::process(pid);

    try
    {
        data::game data(process);

        data.setup_excel_sheet();
        data.setup_address();

        std::once_flag flag{};

        while (!data.is_valid())
        {
            std::call_once(flag,
                           [pid]
                           {
                               print(stdout, fmt::emphasis::bold | fg(fmt::color::red), "[x] PID: {}, 检测不到本地玩家, 数据将会在检测到后导出\n", pid);
                           });

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        pastry_fish_struct.completed = data.get_unlocked_fishes();

        const auto name = data.get_localplayer_name();
        const auto content_id = data.get_localplayer_content_id();
        const auto file_name = fmt::format("result_{}_{:x}.json", name, content_id);
        if (glz::write_file_json(pastry_fish_struct, file_name, std::string {}))
        {
            throw std::runtime_error(fmt::format("写入文件时出错"));
        }

        print(stdout, fmt::emphasis::bold | fg(fmt::color::light_green), "[+] {0} 的数据已写入到 {1} 里.\n", name, file_name);
    }
    catch (std::exception& ex)
    {
        print(stdout, fmt::emphasis::bold | fg(fmt::color::red), "[x] PID: {}, 运行时发生异常: {}\n", pid, ex.what());
    }
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
        const auto processes = get_ffxiv_processes();
        if (processes.empty())
        {
            print(stdout, fmt::emphasis::bold | fg(fmt::color::red), "[x] 没有ffxiv_dx11.exe在运行\n");
            std::this_thread::sleep_for(std::chrono::seconds(3));

            return 1;
        }

        data::config.setup();

        pastry_fish::Main pastry_fish_struct{};

        glz::pool pool;

        for (const DWORD& pid : processes)
        {
            pool.emplace_back(
            [&]
            {
                dump_data(pastry_fish_struct, pid);
            });
        }

        pool.wait();

        print(stdout, fmt::emphasis::bold | fg(fmt::color::light_green), "[+] 完毕, 5秒后退出程序.\n");

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    catch (std::exception& ex)
    {
        print(stdout, fmt::emphasis::bold | fg(fmt::color::red), "[x] 运行时发生异常: {}\n", ex.what());
        std::cout << std::to_string(std::stacktrace::current()) << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}
