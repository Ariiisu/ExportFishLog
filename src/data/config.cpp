#include "config.h"
#include <toml++/toml.h>
#include <fmt/core.h>

void data::config::setup()
{
    try
    {
        auto config = toml::parse_file("config.toml");

        _signatures.fishlog              = config["signatures"]["fishlog"].value_or("");
        _signatures.spear_fishlog        = config["signatures"]["spear_fishlog"].value_or("");
        _signatures.object_table         = config["signatures"]["object_table"].value_or("");
        _signatures.current_fishing_bite = config["signatures"]["current_fishing_bite"].value_or("");
        _signatures.localplayer_name     = config["signatures"]["localplayer_name"].value_or("");
        _signatures.localplayer_content_id = config["signatures"]["localplayer_content_id"].value_or("");
    }
    catch (std::exception& ex)
    {
        throw std::runtime_error(fmt::format("读取 config.toml 时发生异常. 原因: {}", ex.what()));
    }
}
