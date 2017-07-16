#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>

#include <string>
#include <list>

#include <boost/core/noncopyable.hpp>

#include "wm_window.h"

class wm_window;

class quote_fetcher : private boost::noncopyable
{
public:
    quote_fetcher(wm_window& window);
    ~quote_fetcher();

    void fetch(const std::string& quote);

private:
    wm_window& m_window;
    std::thread m_thread;
    std::list<std::string> m_queue;
    bool m_done;
    std::mutex m_mutex;
    std::condition_variable m_condition;
};
