#include <vector>

#include <curl/curl.h>

#include "wm_window.h"

int
main(int argc, char *argv[])
{
    // TODO: read these from configuration file
    const std::vector<std::string> quotes { "ITSA4", "POSI3", "OIBR4", "ITUB4", "ABEV3" };

    curl_global_init(CURL_GLOBAL_ALL);

    wm_window window;

    if (window.initialize(argc, argv)) {
        for (const auto &quote : quotes)
            window.add_quote(quote);

        window.run();
    }

    curl_global_cleanup();
}
