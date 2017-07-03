#include <iostream>
#include <string>
#include <sstream>

#include <boost/format.hpp>
#include <boost/algorithm/string/replace.hpp>
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
    const std::string& date() const { return m_date; }
    const std::string& last() const { return m_last; }
    const std::string& change_percent() const { return m_change_percent; }
    const std::string& change() const { return m_change; }

private:
    curl_request m_request;

    std::string m_symbol;
    std::string m_date;
    std::string m_last;
    std::string m_change_percent;
    std::string m_change;
};

quote_fetcher::quote_fetcher(const std::string& symbol)
    : m_symbol { symbol }
{
    auto url = boost::format("http://g1.globo.com/indicadorg1/quote/.%1%/resumo_bolsa/") % m_symbol;
    m_request.set_url(url.str());
}

bool quote_fetcher::fetch()
{
    if (!m_request.fetch())
        return false;

    std::stringstream response { m_request.buffer() };

    boost::property_tree::ptree tree;
    boost::property_tree::read_json(response, tree);

    const auto replace_char = [](std::string& str, char c0, char c1)
                              {
                                  auto pos = str.find(c0);
                                  if (pos != std::string::npos)
                                      str.replace(pos, 1, 1, c1);
                              };

    m_date = tree.get("TIME_NOW", "");

    m_last = tree.get("CF_LAST", "");
    replace_char(m_last, '.', ',');

    m_change_percent = tree.get("PCTCHNG", "");
    replace_char(m_change_percent, ',', '.');

    m_change = tree.get("CF_NETCHNG", "");
    replace_char(m_change, ',', '.');

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
    Pixmap m_white_font_pixmap, m_white_font_mask;
    Pixmap m_green_font_pixmap, m_green_font_mask;
    Pixmap m_red_font_pixmap, m_red_font_mask;

    std::vector<std::unique_ptr<quote_fetcher>> m_quotes;
    size_t m_cur_quote = 0;
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

bool wm_window::init_window(int argc, char *argv[])
{
    // mostly cargo-culted from the asbeats dockapp

    // create window

    const auto get_color = [=](const char *name)
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
                           };

    m_white_pixel = get_color("white");
    m_black_pixel = get_color("black");

    XSizeHints size_hints;
    size_hints.flags = USSize|USPosition;
    size_hints.x = size_hints.y = 0;

    const unsigned border_width = 1;
    int gravity;
    XWMGeometry(m_display, m_screen, nullptr, nullptr, border_width, &size_hints,
                &size_hints.x, &size_hints.y, &size_hints.width, &size_hints.height, &gravity);

    size_hints.width = 64;
    size_hints.height = 64;

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
    m_visible_pixmap = XCreatePixmap(m_display, m_root_window, 64, 64, DefaultDepth(m_display, DefaultScreen(m_display)));

    XpmAttributes xpm_attribs;
    xpm_attribs.valuemask = XpmReturnPixels | XpmReturnExtensions | XpmExactColors | XpmCloseness;
    xpm_attribs.exactColors = False;
    xpm_attribs.closeness = 40000;

    XpmCreatePixmapFromData(m_display, m_root_window, font_white, &m_white_font_pixmap, &m_white_font_mask, &xpm_attribs);
    XpmCreatePixmapFromData(m_display, m_root_window, font_green, &m_green_font_pixmap, &m_green_font_mask, &xpm_attribs);
    XpmCreatePixmapFromData(m_display, m_root_window, font_red, &m_red_font_pixmap, &m_red_font_mask, &xpm_attribs);

    return true;
}

void wm_window::redraw_window()
{
    XSetForeground(m_display, m_normal_gc, m_black_pixel);
    XFillRectangle(m_display, m_visible_pixmap, m_normal_gc, 0, 0, 64, 64);

    if (m_cur_quote < m_quotes.size()) {
        const auto& quote = m_quotes[m_cur_quote];
        draw_quote(quote->symbol(), quote->last(), quote->change(), quote->change_percent());
    }

    XEvent event;
    while (XCheckTypedWindowEvent(m_display, m_icon_window, Expose, &event))
        ;
    XCopyArea(m_display, m_visible_pixmap, m_icon_window, m_normal_gc, 0, 0, 64, 64, 0, 0);

    while (XCheckTypedWindowEvent(m_display, m_window, Expose, &event))
        ;
    XCopyArea(m_display, m_visible_pixmap, m_window, m_normal_gc, 0, 0, 64, 64, 0, 0);
}

void wm_window::draw_string(Pixmap font_pixmap, const std::string& text, int x, int y)
{
    static const int char_width = 8;
    static const int char_height = 12;

    static const std::string font_chars = " !\"#$%&'()*+,-./0123456789:;<=>?"
                                          "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
                                          "`abcdefghijklmnopqrstuvwxyz{|}~*";

    for (auto ch : text) {
        auto pos = font_chars.find(ch);

        if (pos != std::string::npos) {
            static const int chars_per_row = 32;

            const int char_col = pos%chars_per_row;
            const int char_row = pos/chars_per_row;

            XCopyArea(m_display, font_pixmap, m_visible_pixmap, m_normal_gc,
                      char_col*char_width, char_row*char_height,
                      char_width, char_height,
                      x, y);
        }

        x += char_width;
    }
}

void wm_window::draw_quote(const std::string& symbol, const std::string& last,
                           const std::string& change, const std::string& percent_change)
{
    const int base_y = (64 - 4*12)/2;

    draw_string(m_white_font_pixmap, symbol, (64 - symbol.size()*8)/2, base_y);
    draw_string(m_white_font_pixmap, last, (64 - last.size()*8)/2, base_y + 12);

    auto change_font = change[0] == '+' ? m_green_font_pixmap : m_red_font_pixmap;
    draw_string(change_font, change, (64 - change.size()*8)/2, base_y + 24);
    draw_string(change_font, percent_change, (64 - percent_change.size()*8)/2, base_y + 36);
}

void wm_window::run()
{
    static const time_t update_interval = 30;

    time_t last_update = time(0);
    fetch_quotes();

    while (true) {
        const time_t now = time(0);
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
#if 0
    const std::vector<std::string> quotes
        { "BVSP", "IEE", "INDX", "ICON",
          "IFNC", "IMOB", "IVBX", "ITAG",
          "MLCX", "SMLL", "ISE", "IGCT",
          "IBX50", "IBX", "ICO2" };
#else
    const std::vector<std::string> quotes { "BVSP", "IFNC" };
#endif

    wm_window window;

    if (window.initialize(argc, argv)) {
        for (const auto &quote : quotes)
            window.add_quote(quote);

        window.run();
    }
}
