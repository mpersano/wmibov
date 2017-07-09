#include <iostream>
#include <cmath>

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

#include "wm_window.h"
#include "quote_fetcher.h"

#include "mask.xbm"
#include "font_white.xpm"
#include "font_green.xpm"
#include "font_red.xpm"
#include "font_yellow.xpm"

wm_window::wm_window()
    : m_quote_fetcher { new quote_fetcher(*this) }
{
}

wm_window::~wm_window()
{
    if (m_display)
        XCloseDisplay(m_display);
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

    m_delete_window = XInternAtom(m_display, "WM_DELETE_WINDOW", False);

    if (!init_pixmaps())
        return false;

    if (!init_window(argc, argv))
        return false;

    return true;
}

void wm_window::add_quote(const std::string& symbol)
{
    quote_state quote;
    quote.symbol = symbol;
    m_quotes.push_back(quote);
}

void wm_window::set_update_interval(time_t update_interval)
{
    m_update_interval = update_interval;
}

void wm_window::set_retry_interval(time_t retry_interval)
{
    m_retry_interval = retry_interval;
}

void wm_window::set_max_retries(int max_retries)
{
    m_max_retries = max_retries;
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
    XSetWMProtocols(m_display, m_window, &m_delete_window, 1);

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
    XpmCreatePixmapFromData(m_display, m_root_window, font_yellow, &m_yellow_font_pixmap, nullptr, &xpm_attribs);

    return true;
}

void wm_window::redraw_window()
{
    XSetForeground(m_display, m_normal_gc, m_black_pixel);
    XFillRectangle(m_display, m_visible_pixmap, m_normal_gc, 0, 0, WINDOW_SIZE, WINDOW_SIZE);

    if (m_cur_quote < m_quotes.size()) {
        quote_state quote;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            quote = m_quotes[m_cur_quote];
        }
        if (quote.last_update == static_cast<time_t>(0) || quote.state == quote_state::WAITING)
            draw_wait(quote.symbol);
        else if (quote.state == quote_state::ERROR)
            draw_error(quote.symbol);
        else
            draw_quote(quote.symbol, quote.last, quote.change, quote.percent_change);
    }

    XEvent event;
    while (XCheckTypedWindowEvent(m_display, m_icon_window, Expose, &event))
        ;
    XCopyArea(m_display, m_visible_pixmap, m_icon_window, m_normal_gc, 0, 0, WINDOW_SIZE, WINDOW_SIZE, 0, 0);

    while (XCheckTypedWindowEvent(m_display, m_window, Expose, &event))
        ;
    XCopyArea(m_display, m_visible_pixmap, m_window, m_normal_gc, 0, 0, WINDOW_SIZE, WINDOW_SIZE, 0, 0);
}

void wm_window::draw_string(Pixmap font_pixmap, const std::string& text, int x, int y) const
{
    static const std::string font_chars = " !\"#$%&'()*+,-./0123456789:;<=>?"
                                          "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
                                          "`abcdefghijklmnopqrstuvwxyz{|}~*";
    static const int chars_per_row = 32;

    for (auto ch : text) {
        const auto pos = font_chars.find(ch);

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

void wm_window::draw_string_centered(Pixmap font_pixmap, const std::string& text, int y) const
{
      draw_string(font_pixmap, text, (WINDOW_SIZE - text.size()*CHAR_WIDTH)/2, y);
}

void wm_window::draw_quote(const std::string& symbol, double last, double change, double percent_change) const
{
    std::string last_str, change_str;

    if (last >= 100) {
        auto ilast = std::lround(last);
        auto ichange = std::lround(change);

        if (ilast >= 1000)
            last_str = (boost::format("%d,%03d") % (ilast / 1000) % (ilast % 1000)).str();
        else
            last_str = (boost::format("%d") % ilast).str();
        change_str = (boost::format("%+d") % ichange).str();
    } else {
        last_str = (boost::format("%.2f") % last).str();
        change_str = (boost::format("%.2f") % change).str();
    }
    auto percent_change_str = (boost::format("%+.2f%%") % percent_change).str();

    int base_y = (WINDOW_SIZE - 4*CHAR_HEIGHT)/2;

    draw_string_centered(m_white_font_pixmap, symbol, base_y);
    base_y += CHAR_HEIGHT;

    draw_string_centered(m_white_font_pixmap, last_str, base_y);
    base_y += CHAR_HEIGHT;

    auto change_font = change_str[0] == '+' ? m_green_font_pixmap : m_red_font_pixmap;
    draw_string_centered(change_font, change_str, base_y);
    base_y += CHAR_HEIGHT;

    draw_string_centered(change_font, percent_change_str, base_y);
}

void wm_window::draw_wait(const std::string& symbol) const
{
    int base_y = (WINDOW_SIZE - 2*CHAR_HEIGHT)/2;
    draw_string_centered(m_white_font_pixmap, symbol, base_y);
    draw_string_centered(m_yellow_font_pixmap, "WAIT", base_y + CHAR_HEIGHT);
}

void wm_window::draw_error(const std::string& symbol) const
{
    int base_y = (WINDOW_SIZE - 2*CHAR_HEIGHT)/2;
    draw_string_centered(m_white_font_pixmap, symbol, base_y);
    draw_string_centered(m_red_font_pixmap, "ERROR", base_y + CHAR_HEIGHT);
}

void wm_window::run()
{
    while (true) {
        const time_t now = time(nullptr);

        if (m_cur_quote < m_quotes.size()) {
            std::unique_lock<std::mutex> lock(m_mutex);

            auto& quote = m_quotes[m_cur_quote];

            if (quote.state != quote_state::WAITING) {
                bool update;
                if (quote.last_update == static_cast<time_t>(0)) {
                    update = true;
                } else if (quote.state == quote_state::FETCHED) {
                    update = now - quote.last_update >= m_update_interval;
                } else if (quote.state == quote_state::ERROR && quote.retries < m_max_retries) {
                    update = now - quote.last_update >= m_retry_interval;
                } else {
                    update = false;
                }

                if (update) {
                    ++quote.retries;
                    quote.state = quote_state::WAITING;
                    m_quote_fetcher->fetch(quote.symbol);
                }
            }
        }

        // XXX should only do this when we get an update
        redraw_window();

        while (XPending(m_display)) {
            XEvent event;
            XNextEvent(m_display, &event);

            switch (event.type) {
            case Expose:
                // redraw_window();
                break;

            case ButtonPress:
                if (++m_cur_quote == m_quotes.size())
                    m_cur_quote = 0;
                // redraw_window();
                break;

            case ClientMessage:
                if (event.xclient.data.l[0] == static_cast<int>(m_delete_window))
                    return;

            default:
                break;
            }
        }

        XFlush(m_display);

        usleep(50000l);
    }
}

void wm_window::set_quote_state(const std::string& symbol, double last, double change, double percent_change)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    auto it = std::find_if(std::begin(m_quotes),
                        std::end(m_quotes),
                        [&](const quote_state& quote) { return quote.symbol == symbol; });
    if (it != std::end(m_quotes)) {
        auto& quote = *it;
        quote.last = last;
        quote.change = change;
        quote.percent_change = percent_change;
        quote.state = quote_state::FETCHED;
        quote.last_update = time(nullptr);
        quote.retries = 0;
    }
}

void wm_window::set_quote_error(const std::string& symbol)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    auto it = std::find_if(std::begin(m_quotes),
                        std::end(m_quotes),
                        [&](const quote_state& quote) { return quote.symbol == symbol; });
    if (it != std::end(m_quotes)) {
        auto& quote = *it;
        quote.state = quote_state::ERROR;
        quote.last_update = time(nullptr);
    }
}
