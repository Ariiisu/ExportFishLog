#include "pattern.h"

std::uintptr_t pattern::find_std(std::uint8_t* data, std::size_t size, std::span<impl::hex_data> pattern) noexcept
{
    const auto pattern_size = pattern.size();
    std::uint8_t* end       = data + size - pattern_size;
    const auto first_byte   = pattern[0].value();

    for (std::uint8_t* current = data; current <= end; ++current)
    {
        current = std::find(current, end, first_byte);

        if (current == end)
        {
            break;
        }

        if (std::equal(pattern.begin() + 1,
                       pattern.end(),
                       current + 1,
                       [](auto opt, auto byte)
                       {
                           return !opt.has_value() || *opt == byte;
                       }))
        {
            return current - data;
        }
    }
    return {};
}
