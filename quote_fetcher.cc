#include <boost/lexical_cast.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "curl_request.h"

#include "quote_fetcher.h"

quote_fetcher::quote_fetcher(wm_window& window)
    : m_window { window }
    , m_thread {
        [this]()
        {
            curl_request request;

            while (true) {
                std::string symbol;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);

                    while (!m_done && m_queue.empty())
                        m_condition.wait(lock);

                    if (m_done)
                        break;

                    symbol = m_queue.front();
                    m_queue.pop_front();
                }

                request.set_url(std::string("http://exame.abril.com.br/coletor/quote/") + symbol);

                if (request.fetch()) {
                    const auto response_code = request.response_code();

                    if (response_code == 200) {
                        std::stringstream response { request.buffer() };

                        boost::property_tree::ptree tree;
                        boost::property_tree::read_json(response, tree);

                        double last = boost::lexical_cast<double>(tree.get("trdprc_1", "0"));
                        double change_percent = boost::lexical_cast<double>(tree.get("pctchng", "0"));
                        double change = boost::lexical_cast<double>(tree.get("netchng_1", "0"));

                        // XXX catch bad_lexical_cast

                        m_window.set_quote_state(symbol, last, change, change_percent);
                    } else {
                        m_window.set_quote_error(symbol);
                    }
                } else {
                    m_window.set_quote_error(symbol);
                }
            }
        } }
    , m_done { false }
{
}

quote_fetcher::~quote_fetcher()
{
    m_done = true;
    m_condition.notify_one();
    m_thread.join();
}

void quote_fetcher::fetch(const std::string& symbol)
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.push_back(symbol);
    }
    m_condition.notify_one();
}
