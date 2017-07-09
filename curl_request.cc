#include "curl_request.h"

curl_request::curl_request()
    : m_curl { curl_easy_init() }
{
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, static_write_callback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);
}

curl_request::~curl_request()
{
    curl_easy_cleanup(m_curl);
}

size_t curl_request::static_write_callback(char *buffer, size_t size, size_t nmemb, void *userp)
{
    auto self = reinterpret_cast<curl_request *>(userp);
    return self->write_callback(buffer, size, nmemb);
}

size_t curl_request::write_callback(char *buffer, size_t size, size_t nmemb)
{
    const size_t bytes { size*nmemb };
    m_buffer.append(buffer, bytes);
    return bytes;
}

void curl_request::set_url(const std::string& url)
{
    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
}

bool curl_request::fetch()
{
    m_buffer.clear();
    m_response_code = 0;

    if (curl_easy_perform(m_curl) != CURLE_OK)
        return false;

    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &m_response_code);

    return true;
}

const std::string& curl_request::buffer() const
{
    return m_buffer;
}

long curl_request::response_code() const
{
    return m_response_code;
}
