#pragma once

#include <string>

namespace data
{
    struct config
    {
        struct signatures
        {
            std::string fishlog {};
            std::string spear_fishlog {};
            std::string object_table {};
        };

        struct network
        {
            bool use_proxy {};
            std::string proxy_url {};
            std::string fishlog_url {};
            std::string spear_fishlog_url {};
        };

        void setup();

    private:
        signatures _signatures {};
        network _network {};

    public:
        signatures signatures()
        {
            return _signatures;
        }

        network network()
        {
            return _network;
        }
    };

    inline config config{};
}
