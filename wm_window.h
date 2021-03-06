#pragma once

#include <X11/Xlib.h>
#include <X11/xpm.h>
#include <X11/extensions/shape.h>
#include <X11/Xatom.h>

#include <string>
#include <mutex>
#include <memory>

#include <boost/core/noncopyable.hpp>

class quote_fetcher;

class wm_window : private boost::noncopyable
{
public:
    wm_window();
    ~wm_window();

    bool initialize(int argc, char *argv[]);
    void add_quote(const std::string& symbol);
    void set_update_interval(time_t update_interval);
    void set_retry_interval(time_t retry_interval);
    void set_max_retries(int max_retries);

    void run();

    void set_quote_state(const std::string& symbol, double last, double change, double percent_change);
    void set_quote_error(const std::string& symbol);

private:
    bool init_window(int argc, char *argv[]);
    bool init_pixmaps();
    Pixel get_color(const char *name);

    void redraw_window();
    void draw_string(Pixmap font_pixmap, const std::string& text, int x, int y) const;
    void draw_string_centered(Pixmap font_pixmap, const std::string& text, int y) const;
    void draw_quote(const std::string& symbol, double last, double change, double percent_change) const;
    void draw_wait(const std::string& symbol) const;
    void draw_error(const std::string& symbol) const;

    struct quote_state
    {
        std::string symbol;
        time_t last_update = static_cast<time_t>(0);
        enum { NONE, WAITING, FETCHED, ERROR } state = NONE;
        int retries = 0;
        double last;
        double change;
        double percent_change;
    };

    std::mutex m_mutex;
    std::vector<quote_state> m_quotes;

    std::unique_ptr<quote_fetcher> m_quote_fetcher;

    Display *m_display = nullptr;
    int m_screen;
    Window m_root_window;
    Pixel m_black_pixel, m_white_pixel;
    Atom m_delete_window;
    Window m_window;
    Window m_icon_window;
    GC m_normal_gc;
    Pixmap m_visible_pixmap;
    Pixmap m_white_font_pixmap;
    Pixmap m_green_font_pixmap;
    Pixmap m_red_font_pixmap;
    Pixmap m_yellow_font_pixmap;

    size_t m_cur_quote = 0;
    time_t m_update_interval = 60;
    time_t m_retry_interval = 5;
    int m_max_retries = 3;

    static const int WINDOW_SIZE = 64;
    static const int CHAR_WIDTH = 8;
    static const int CHAR_HEIGHT = 12;
};
