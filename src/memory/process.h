#pragma once

#include <Windows.h>
#include <string>
#include <cstdint>
#include <optional>
#include <vector>

#include "pattern.h"

#include <memory>

namespace mem
{
    class process : public std::enable_shared_from_this<process>
    {
    public:
        process() = default;
        explicit process(std::string_view process_name);

    private:
        void look_for_proess();
        void setup_base_address();

        std::uintptr_t _base_address{};
        std::vector<std::uint8_t> _process_bytes{};

        std::uintptr_t _text_section_start{};
        std::uintptr_t _text_section_end{};

        std::string _process_name{};
        std::uint32_t _pid{};
        HANDLE _handle{};
        std::string _process_path;

        bool read_impl(std::uintptr_t address, void* buffer, std::size_t size) const;
        bool write_impl(std::uintptr_t address, const void* buffer, std::size_t size) const;

    public:
        template <typename T>
        std::optional<T> read(std::uintptr_t address)
        {
            T res{};
            if (!read_impl(address, &res, sizeof(T)))
                return std::nullopt;

            return res;
        }

        template <typename T>
        bool write(std::uintptr_t address, T value)
        {
            return write_impl(address, &value, sizeof(T));
        }

        std::shared_ptr<process> ptr()
        {
            return shared_from_this();
        }

        std::uintptr_t find_pattern(pattern::impl::make<> pattern, bool rel = false, std::uint8_t rel_offset = 3);
        std::uintptr_t find_pattern(const std::span<pattern::impl::hex_data>& pattern, bool rel = false, std::uint8_t rel_offset = 3);

        std::vector<std::uintptr_t> find_pattern_multi(pattern::impl::make<> pattern, bool rel = false, std::uint8_t rel_offset = 3);
        std::vector<std::uintptr_t> find_pattern_multi(const std::span<pattern::impl::hex_data>& pattern, bool rel = false, std::uint8_t rel_offset = 3);

        std::string get_process_path()
        {
            return _process_path;
        }
    };
}
