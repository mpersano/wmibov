#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <vector>

#include <curl/curl.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "wm_window.h"

int
main(int argc, char *argv[])
{
    int update_interval;
    std::vector<std::string> quotes;

    auto rc_path = std::string { std::getenv("HOME") } + "/.wmibov";

    if (access(rc_path.c_str(), R_OK) == 0) {
        boost::property_tree::ptree tree;
        boost::property_tree::read_json(rc_path, tree);

        update_interval = tree.get<int>("interval", 30);

        const auto& symbols = tree.get_child("symbols");
        std::transform(
                std::begin(symbols),
                std::end(symbols),
                std::back_inserter(quotes),
                [](const boost::property_tree::ptree::value_type& v)
                { return v.second.data(); });
    } else {
        quotes = { "BVSP" };
        update_interval = 30;
    }

    curl_global_init(CURL_GLOBAL_ALL);

    wm_window window;

    if (window.initialize(argc, argv)) {
        window.set_update_interval(update_interval);

        for (const auto& quote : quotes)
            window.add_quote(quote);

        window.run();
    }

    curl_global_cleanup();
}
