#include "memory/process.h"
#include "data/game.h"
#include "data/json.hpp"

#include <iostream>
#include <filesystem>
#include <fmt/color.h>
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

static DWORD get_ffxiv_process()
{
    PROCESSENTRY32 entry{};
    entry.dwSize = sizeof(PROCESSENTRY32);

    const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

    for (auto ok = Process32First(snapshot, &entry); ok; ok = Process32Next(snapshot, &entry))
    {
        if (std::string_view(entry.szExeFile) != "ffxiv_dx11.exe")
            continue;

        CloseHandle(snapshot);
        return entry.th32ProcessID;
    }

    CloseHandle(snapshot);
    return 0;
}

static void dump_data(const DWORD pid)
{
    const auto process = mem::process(pid);

    try
    {
        data::game data(process);

        data.setup_excel_sheet();
        data.setup_address();

        pastry_fish::Main pastry_fish_struct{};
        pastry_fish_struct.completed = data.get_unlocked_fishes();

        std::filesystem::path cwd           = std::filesystem::current_path();
        std::filesystem::path file_path     = cwd / "result.json";
        std::string           file_path_str = file_path.string();

        if (pastry_fish_struct.completed.empty())
        {
            std::ofstream ofs(file_path, std::ios::trunc);
            if (!ofs)
            {
                throw std::runtime_error("无法写入空 JSON 文件");
            }
            ofs << "{}";
            ofs.close();

            print(stdout, fmt::emphasis::bold | fg(fmt::color::yellow), "[!] PID: {0} 未找到数据，已写入空 JSON 对象: {1}. 请确保在游戏内运行本程序，或者对应角色有解锁部分钓鱼日志\n", pid, file_path_str);
            return;
        }

        if (glz::write_file_json(pastry_fish_struct, file_path_str, std::string{}))
        {
            throw std::runtime_error(fmt::format("写入文件时出错"));
        }

        print(stdout, fmt::emphasis::bold | fg(fmt::color::light_green), "[+] PID: {0} 的数据已写入到 {1}.\n", pid, file_path_str);
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
        const auto pid = get_ffxiv_process();
        if (pid == 0)
        {
            print(stdout, fmt::emphasis::bold | fg(fmt::color::red), "[x] 没有ffxiv_dx11.exe在运行\n");
            std::this_thread::sleep_for(std::chrono::seconds(3));

            return 1;
        }

        dump_data(pid);
        print(stdout, fmt::emphasis::bold | fg(fmt::color::light_green), "[+] 完毕, 5秒后退出程序.\n");
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    catch (std::exception& ex)
    {
        print(stdout, fmt::emphasis::bold | fg(fmt::color::red), "[x] 运行时发生异常: {}\n", ex.what());
        std::cout << std::to_string(std::stacktrace::current()) << '\n';
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}
