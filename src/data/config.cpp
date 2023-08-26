#include "config.h"
#include <toml++/toml.h>

void data::config::setup()
{
    auto config = toml::parse_file("config.toml");

    _signatures.fishlog       = config["signatures"]["fishlog"].value_or("");
    _signatures.spear_fishlog = config["signatures"]["spear_fishlog"].value_or("");
    _signatures.object_table  = config["signatures"]["object_table"].value_or("");

    _network.use_proxy         = config["network"]["use_proxy"].value_or(false);
    _network.proxy_url         = config["network"]["proxy_url"].value_or("");
    _network.fishlog_url       = config["network"]["fishlog_url"].value_or("");
    _network.spear_fishlog_url = config["network"]["spear_fishlog_url"].value_or("");
}
