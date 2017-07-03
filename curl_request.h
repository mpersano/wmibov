#pragma once

#include <string>
#include <curl/curl.h>
#include <boost/core/noncopyable.hpp>

class curl_request : private boost::noncopyable
{
public:
    curl_request();
    ~curl_request();

    void set_url(const std::string& url);
    bool fetch();
    const std::string& buffer() const;

private:
    static size_t static_write_callback(char *buffer, size_t size, size_t nmemb, void *userp);
    size_t write_callback(char *buffer, size_t size, size_t nmemb);

    std::string m_buffer;
    CURL *m_curl;
};
