#include <iostream>
#include <string>
#include <sstream>

#include <boost/format.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <curl/curl.h>

#include <X11/Xlib.h>
#include <X11/xpm.h>
#include <X11/extensions/shape.h>
#include <X11/Xatom.h>

#include "curl_request.h"

#include "mask.xbm"
#include "font_white.xpm"
#include "font_green.xpm"
#include "font_red.xpm"

class quote_fetcher
{
public:
    quote_fetcher(const std::string& symbol);

    bool fetch();

    const std::string& symbol() const { return m_symbol; }
    double last() const { return m_last; }
    double change_percent() const { return m_change_percent; }
    double change() const { return m_change; }

private:
    curl_request m_request;

    std::string m_symbol;
    double m_last;
    double m_change_percent;
    double m_change;
};

quote_fetcher::quote_fetcher(const std::string& symbol)
    : m_symbol { symbol }
    , m_last { 0.0 }
    , m_change_percent { 0.0 }
    , m_change { 0.0 }
{
    m_request.set_url(std::string("http://exame.abril.com.br/coletor/quote/") + m_symbol);
}

bool quote_fetcher::fetch()
{
    if (!m_request.fetch())
        return false;

    // {"trdprc_1":9.01,"low_1":9.0,"trdtim_1":"20:08","high_1":9.06,"acvol_1":5811900,"yrlow":6.8548,"trdvol_1":900,"name":"ITSA4","pctchng":-0.11,"hst_close":9.02,"trade_time":"2017-07-04T20:08:00Z","cf_name":"ITAUSA PN","netchng_1":-0.01,"open_prc":9.0,"yrhigh":10.4867,"historical_close_date":"2017-07-03","trade_date":"2017-07-04"}

    std::stringstream response { m_request.buffer() };

    boost::property_tree::ptree tree;
    boost::property_tree::read_json(response, tree);

    m_last = boost::lexical_cast<double>(tree.get("trdprc_1", "0"));
    m_change_percent = boost::lexical_cast<double>(tree.get("pctchng", "0"));
    m_change = boost::lexical_cast<double>(tree.get("netchng_1", "0"));

    // XXX catch bad_lexical_cast

    return true;
}

class wm_window : private boost::noncopyable
{
public:
    wm_window();
    ~wm_window();

    bool initialize(int argc, char *argv[]);
    void add_quote(const std::string& symbol);
    void run();

private:
    bool init_window(int argc, char *argv[]);
    bool init_pixmaps();
    Pixel get_color(const char *name);

    void redraw_window();
    void draw_string(Pixmap font_pixmap, const std::string& text, int x, int y);
    void draw_quote(const std::string& symbol, const std::string& last,
                    const std::string& change, const std::string& percent_change);

    void fetch_quotes();

    Display *m_display = nullptr;
    int m_screen;
    Window m_root_window;
    Pixel m_black_pixel, m_white_pixel;
    Window m_window;
    Window m_icon_window;
    GC m_normal_gc;
    Pixmap m_visible_pixmap;
    Pixmap m_white_font_pixmap;
    Pixmap m_green_font_pixmap;
    Pixmap m_red_font_pixmap;

    std::vector<std::unique_ptr<quote_fetcher>> m_quotes;
    size_t m_cur_quote = 0;

    static const int WINDOW_SIZE = 64;
    static const int CHAR_WIDTH = 8;
    static const int CHAR_HEIGHT = 12;
};

wm_window::wm_window()
{
    curl_global_init(CURL_GLOBAL_ALL);
}

wm_window::~wm_window()
{
    if (m_display)
        XCloseDisplay(m_display);

    curl_global_cleanup();
}

bool wm_window::initialize(int argc, char *argv[])
{
    m_display = XOpenDisplay(nullptr);
    if (!m_display) {
        std::cerr << "failed to open display\n";
        return false;
    }

    m_screen = DefaultScreen(m_display);
    m_root_window = RootWindow(m_display, m_screen);

    m_white_pixel = get_color("white");
    m_black_pixel = get_color("black");

    if (!init_pixmaps())
        return false;

    if (!init_window(argc, argv))
        return false;

    return true;
}

void wm_window::add_quote(const std::string& symbol)
{
    m_quotes.emplace_back(new quote_fetcher(symbol));
}

void wm_window::fetch_quotes()
{
    // TODO make this multi-threaded or non-blocking
    for (auto& quote : m_quotes) {
        std::cout << "fetching " << quote->symbol() << "\n";
        quote->fetch();
    }
}

Pixel wm_window::get_color(const char *name)
{
    XWindowAttributes attribs;
    XGetWindowAttributes(m_display, m_root_window, &attribs);

    XColor color;
    color.pixel = 0;

    if (!XParseColor(m_display, attribs.colormap, name, &color))
        std::cerr << "failed to parse color " << name << "\n";

    if (!XAllocColor(m_display, attribs.colormap, &color))
        std::cerr << "failed to alloc color " << name << "\n";

    return color.pixel;
}

bool wm_window::init_window(int argc, char *argv[])
{
    // mostly cargo-culted from the asbeats dockapp

    // create window

    XSizeHints size_hints;
    size_hints.flags = USSize | USPosition;
    size_hints.x = size_hints.y = 0;

    const unsigned border_width = 1;
    int gravity;
    XWMGeometry(m_display, m_screen, nullptr, nullptr, border_width, &size_hints,
                &size_hints.x, &size_hints.y, &size_hints.width, &size_hints.height, &gravity);

    size_hints.width = size_hints.height = WINDOW_SIZE;

    m_window = XCreateSimpleWindow(m_display, m_root_window, size_hints.x, size_hints.y,
                                   size_hints.width, size_hints.height, border_width,
                                   m_black_pixel, m_white_pixel);

    m_icon_window = XCreateSimpleWindow(m_display, m_root_window, size_hints.x, size_hints.y,
                                        size_hints.width, size_hints.height, border_width,
                                        m_black_pixel, m_white_pixel);

    // activate hints

    XSetWMNormalHints(m_display, m_window, &size_hints);

    XClassHint class_hint;
    class_hint.res_name = class_hint.res_class = "wmibov";
    XSetClassHint(m_display, m_window, &class_hint);

    // event masks

    const auto event_mask = ExposureMask | ButtonPressMask | StructureNotifyMask;
    XSelectInput(m_display, m_window, event_mask);
    XSelectInput(m_display, m_icon_window, event_mask);

    XSetCommand(m_display, m_window, argv, argc);

    char *window_name = "wmibov";
    XTextProperty name_property;

    if (XStringListToTextProperty(&window_name, 1, &name_property) == 0) {
        std::cerr << "failed to allocate window name\n";
        return false;
    }

    XSetWMName(m_display, m_window, &name_property);

    // graphics context

    XGCValues gcv;
    gcv.foreground = m_black_pixel;
    gcv.background = m_white_pixel;
    gcv.graphics_exposures = 0;
    m_normal_gc = XCreateGC(m_display, m_root_window, GCForeground | GCBackground | GCGraphicsExposures, &gcv);

    // masks

    Pixmap mask_pixmap = XCreateBitmapFromData(m_display, m_window, reinterpret_cast<const char *>(mask_bits), mask_width, mask_height);
    XShapeCombineMask(m_display, m_window, ShapeBounding, 0, 0, mask_pixmap, ShapeSet);
    XShapeCombineMask(m_display, m_icon_window, ShapeBounding, 0, 0, mask_pixmap, ShapeSet);

    // wm hints

    XWMHints wm_hints;
    wm_hints.initial_state = WithdrawnState;
    wm_hints.icon_window = m_icon_window;
    wm_hints.icon_x = size_hints.x;
    wm_hints.icon_y = size_hints.y;
    wm_hints.window_group = m_window;
    wm_hints.flags = StateHint | IconWindowHint | IconPositionHint | WindowGroupHint;
    XSetWMHints(m_display, m_window, &wm_hints);

    // map it

    XMapWindow(m_display, m_window);

    return true;
}

bool wm_window::init_pixmaps()
{
    m_visible_pixmap = XCreatePixmap(m_display, m_root_window, WINDOW_SIZE, WINDOW_SIZE, DefaultDepth(m_display, DefaultScreen(m_display)));

    XpmAttributes xpm_attribs;
    xpm_attribs.valuemask = XpmReturnPixels | XpmReturnExtensions | XpmExactColors | XpmCloseness;
    xpm_attribs.exactColors = False;
    xpm_attribs.closeness = 40000;

    XpmCreatePixmapFromData(m_display, m_root_window, font_white, &m_white_font_pixmap, nullptr, &xpm_attribs);
    XpmCreatePixmapFromData(m_display, m_root_window, font_green, &m_green_font_pixmap, nullptr, &xpm_attribs);
    XpmCreatePixmapFromData(m_display, m_root_window, font_red, &m_red_font_pixmap, nullptr, &xpm_attribs);

    return true;
}

void wm_window::redraw_window()
{
    XSetForeground(m_display, m_normal_gc, m_black_pixel);
    XFillRectangle(m_display, m_visible_pixmap, m_normal_gc, 0, 0, WINDOW_SIZE, WINDOW_SIZE);

    if (m_cur_quote < m_quotes.size()) {
        const auto& quote = m_quotes[m_cur_quote];
        auto last = boost::format("%.2f") % quote->last();
        auto change = boost::format("%+.2f") % quote->change();
        auto change_percent = boost::format("%+.2f%%") % quote->change_percent();
        draw_quote(quote->symbol(), last.str(), change.str(), change_percent.str());
    }

    XEvent event;
    while (XCheckTypedWindowEvent(m_display, m_icon_window, Expose, &event))
        ;
    XCopyArea(m_display, m_visible_pixmap, m_icon_window, m_normal_gc, 0, 0, WINDOW_SIZE, WINDOW_SIZE, 0, 0);

    while (XCheckTypedWindowEvent(m_display, m_window, Expose, &event))
        ;
    XCopyArea(m_display, m_visible_pixmap, m_window, m_normal_gc, 0, 0, WINDOW_SIZE, WINDOW_SIZE, 0, 0);
}

void wm_window::draw_string(Pixmap font_pixmap, const std::string& text, int x, int y)
{
    static const std::string font_chars = " !\"#$%&'()*+,-./0123456789:;<=>?"
                                          "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
                                          "`abcdefghijklmnopqrstuvwxyz{|}~*";
    static const int chars_per_row = 32;

    for (auto ch : text) {
        auto pos = font_chars.find(ch);

        if (pos != std::string::npos) {

            const int char_col = pos%chars_per_row;
            const int char_row = pos/chars_per_row;

            XCopyArea(m_display, font_pixmap, m_visible_pixmap, m_normal_gc,
                      char_col*CHAR_WIDTH, char_row*CHAR_HEIGHT,
                      CHAR_WIDTH, CHAR_HEIGHT,
                      x, y);
        }

        x += CHAR_WIDTH;
    }
}

void wm_window::draw_quote(const std::string& symbol, const std::string& last,
                           const std::string& change, const std::string& percent_change)
{
    int base_y = (WINDOW_SIZE - 4*CHAR_HEIGHT)/2;

    const auto draw_string_centered = [&](Pixmap font_pixmap, const std::string& str)
                                      {
                                          draw_string(font_pixmap, str, (WINDOW_SIZE - str.size()*CHAR_WIDTH)/2, base_y);
                                          base_y += CHAR_HEIGHT;
                                      };

    draw_string_centered(m_white_font_pixmap, symbol);
    draw_string_centered(m_white_font_pixmap, last);

    auto change_font = change[0] == '+' ? m_green_font_pixmap : m_red_font_pixmap;
    draw_string_centered(change_font, change);
    draw_string_centered(change_font, percent_change);
}

void wm_window::run()
{
    static const time_t update_interval = 30;

    time_t last_update = time(nullptr);
    fetch_quotes();

    while (true) {
        const time_t now = time(nullptr);
        if (now - last_update >= update_interval) {
            last_update = now;
            fetch_quotes();
            redraw_window();
        }

        while (XPending(m_display)) {
            XEvent event;
            XNextEvent(m_display, &event);

            switch (event.type) {
            case Expose:
                redraw_window();
                break;

            case ButtonPress:
                if (++m_cur_quote == m_quotes.size())
                    m_cur_quote = 0;
                redraw_window();
                break;

            case DestroyNotify:
                return;

            default:
                break;
            }
        }

        XFlush(m_display);

        usleep(50000l);
    }
}

int
main(int argc, char *argv[])
{
    // TODO: read these from configuration file
    const std::vector<std::string> quotes { "ITSA4" };

    wm_window window;

    if (window.initialize(argc, argv)) {
        for (const auto &quote : quotes)
            window.add_quote(quote);

        window.run();
    }
}
