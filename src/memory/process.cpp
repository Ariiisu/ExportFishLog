#include "process.h"
#include <tlhelp32.h>
#include <Windows.h>
#include <Psapi.h>
#include <fmt/color.h>

mem::process::process(std::string_view process_name)
{
    this->_process_name = process_name;
    look_for_proess();
}

mem::process::process(DWORD pid)
{
    _pid    = pid;
    _handle = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, false, pid);
    setup_base_address();
}

void mem::process::look_for_proess()
{
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

    for (auto ok = Process32First(snapshot, &entry); ok; ok = Process32Next(snapshot, &entry))
    {
        if (this->_process_name == entry.szExeFile)
        {
            const auto handle = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, false, entry.th32ProcessID);
            if (!handle)
            [[unlikely]]
            {
                CloseHandle(snapshot);
                return;
            }

            _pid    = static_cast<std::uint32_t>(entry.th32ProcessID);
            _handle = handle;
            setup_base_address();
            print(fmt::emphasis::bold | fg(fmt::color::light_green), "[+] 已找到 ffxiv_dx11.exe.\n");
            CloseHandle(snapshot);
            return;
        }
    }

    CloseHandle(snapshot);
    throw std::exception("找不到ffxiv_dx11.exe");
}

void mem::process::setup_base_address()
{
    // https://learn.microsoft.com/en-us/windows/win32/psapi/enumerating-all-modules-for-a-process
    HMODULE lph_module[1024];
    DWORD lpcb_needed{};

    if (!EnumProcessModules(_handle, lph_module, sizeof(lph_module), &lpcb_needed))
        throw std::exception("无法获取ffxiv_dx11.exe的module列表, 可能因为没有用管理员运行或者杀毒软件误报");

    // 第一个就是exe本体
    auto mod = lph_module[0];
    _process_path.resize(MAX_PATH);

    if (!GetModuleFileNameExW(_handle, mod, _process_path.data(), MAX_PATH))
        throw std::exception("无法获取ffxiv_dx11.exe的module名, 可能因为没有用管理员运行或者杀毒软件误报");

    MODULEINFO module_info{};
    if (!GetModuleInformation(_handle, mod, &module_info, lpcb_needed))
        throw std::exception("无法获取ffxiv_dx11.exe的module信息, 可能因为没有用管理员运行或者杀毒软件误报");

    _process_path = _process_path.substr(0, _process_path.find_last_of('\\'));

    _base_address   = reinterpret_cast<std::uintptr_t>(mod);
    const auto size = module_info.SizeOfImage;

    _process_bytes.resize(size);
    read_impl(_base_address, _process_bytes.data(), size);
}

bool mem::process::read_impl(const std::uintptr_t address, void* buffer, const std::size_t size) const
{
    SIZE_T read_bytes{};
    ReadProcessMemory(_handle, reinterpret_cast<void*>(address), buffer, size, &read_bytes);
    return read_bytes == size;
}

bool mem::process::write_impl(const std::uintptr_t address, const void* buffer, const std::size_t size) const
{
    SIZE_T written_bytes{};
    WriteProcessMemory(_handle, reinterpret_cast<void*>(address), buffer, size, &written_bytes);
    return written_bytes == size;
}

std::uintptr_t mem::process::find_pattern(pattern::impl::make<> pattern, const bool rel, const std::uint8_t rel_offset)
{
    auto res = pattern::find_std(_process_bytes.data(), _process_bytes.size(), pattern.bytes);
    if (!res)
        return 0;

    res += _base_address;

    if (rel)
    {
        const auto offset = read<std::int32_t>(res + rel_offset);
        if (!offset.has_value())
            throw std::exception("[find_pattern] 读取offset失败, 可能因为没有用管理员运行或者杀毒软件误报");

        res = res + rel_offset + sizeof(std::uint32_t) + *offset;
    }

    return res;
}

std::uintptr_t mem::process::find_pattern(const std::span<pattern::impl::hex_data>& pattern, const bool rel, const std::uint8_t rel_offset)
{
    auto res = pattern::find_std(_process_bytes.data(), _process_bytes.size(), pattern);
    if (!res)
        return 0;

    res += _base_address;

    if (rel)
    {
        const auto offset = read<std::uint32_t>(res + rel_offset);
        if (!offset.has_value())
            throw std::exception("[find_pattern] 读取offset失败, 可能因为没有用管理员运行或者杀毒软件误报");

        res = res + rel_offset + sizeof(std::uint32_t) + *offset;
    }

    return res;
}

std::vector<std::uintptr_t> mem::process::find_pattern_multi(pattern::impl::make<> pattern, bool rel, std::uint8_t rel_offset)
{
    auto res = pattern::find_multi_std(_process_bytes.data(), _process_bytes.size(), pattern.bytes);
    if (res.empty())
        return {};

    for (auto& addr : res)
    {
        addr += _base_address;

        if (rel)
        {
            const auto offset = read<std::uint32_t>(addr + rel_offset);
            if (!offset.has_value())
                throw std::exception("[find_pattern] 读取offset失败, 可能因为没有用管理员运行或者杀毒软件误报");

            addr = addr + rel_offset + sizeof(std::uint32_t) + *offset;
        }
    }

    return res;
}

std::vector<std::uintptr_t> mem::process::find_pattern_multi(const std::span<pattern::impl::hex_data>& pattern, bool rel, std::uint8_t rel_offset)
{
    auto res = pattern::find_multi_std(_process_bytes.data(), _process_bytes.size(), pattern);
    if (res.empty())
        return {};

    for (auto& addr : res)
    {
        addr += _base_address;

        if (rel)
        {
            const auto offset = read<std::uint32_t>(addr + rel_offset);
            if (!offset.has_value())
                throw std::exception("[find_pattern] 读取offset失败, 可能因为没有用管理员运行或者杀毒软件误报");

            addr = addr + rel_offset + sizeof(std::uint32_t) + *offset;
        }
    }

    return res;
}
