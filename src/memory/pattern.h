#pragma once
#include <optional>
#include <ranges>
#include <stdexcept>
#include <fmt/core.h>
#include <vector>

namespace pattern
{
    namespace impl
    {
        using hex_data = std::optional<std::uint8_t>;

        [[nodiscard]] static constexpr std::optional<uint8_t> hex_char_to_byte(char c) noexcept
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;

            return std::nullopt;
        }

        template <char Wildcard = '?'>
        [[nodiscard]] static constexpr std::optional<std::uint8_t> parse_hex(std::string_view str) noexcept
        {
            if (str.size() == 1 && str.front() == Wildcard)
                return std::nullopt;

            if (str.size() != 2)
                return std::nullopt;

            const auto high = hex_char_to_byte(str[0]);
            const auto low  = hex_char_to_byte(str[1]);

            // if both are not wildcard, e.g AA
            if (high.has_value() && low.has_value())
            {
                return (high.value() << 4) | low.value();
            }

            return std::nullopt;
        }

        template <char Delimiter = ' ', char Wildcard = '?'>
        [[nodiscard]] static constexpr std::vector<hex_data> parse_pattern(std::string_view pattern) noexcept
        {
            std::vector<hex_data> result{};
            for (auto&& str : pattern | std::ranges::views::split(Delimiter))
            {
                const std::string_view token(str);
                result.push_back(parse_hex<Wildcard>(token));
            }
            return result;
        }

        // https://stackoverflow.com/a/73014828
        template <auto N>
        static constexpr auto str(const char (&cstr)[N]) noexcept
        {
            std::array<char, N> arr;
            for (std::size_t i = 0; i < N; ++i)
                arr[i]         = cstr[i];
            return arr;
        }

        template <auto Str>
        constexpr auto make_pattern() noexcept
        {
            const auto sig      = impl::parse_pattern(Str.data());
            constexpr auto size = impl::parse_pattern(Str.data()).size();
            std::array<hex_data, size> arr{};
            for (std::size_t i = 0; i < size; i++)
                arr[i]         = sig[i];
            return arr;
        }

        template <char Delimiter = ' ', char Wildcard = '?'>
        struct make
        {
            explicit constexpr make(std::string_view pattern)
            {
                bytes = parse_pattern<Delimiter, Wildcard>(pattern);
                if (bytes.empty())
                    throw std::invalid_argument(fmt::format("无效signature. Delimiter: \"{}\", Wildcard: \"{}\".", Delimiter, Wildcard));

                if (!bytes.front().has_value())
                    throw std::invalid_argument(fmt::format("signature不能以wildcard\"{}\"开头.", Wildcard));
            }

            [[nodiscard]] constexpr std::size_t size() const noexcept
            {
                return bytes.size();
            }

            constexpr const hex_data& operator[](std::size_t index) const noexcept
            {
                return bytes[index];
            }

            std::vector<hex_data> bytes;
        };
    }

    std::uintptr_t find_std(std::uint8_t* data, std::size_t size, std::span<impl::hex_data> pattern) noexcept;
    using make = impl::make<' ', '?'>;
}
