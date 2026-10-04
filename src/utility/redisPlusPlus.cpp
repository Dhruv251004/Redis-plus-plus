// welcome_screen.cpp -- Redis++ startup banner
//
// Build:  g++ -std=c++17 -O2 -Wall -Wextra welcome_screen.cpp -o welcome
//
// Colors are emitted only when stdout is a terminal. NO_COLOR disables them;
// CLICOLOR_FORCE=1 forces them (useful when piping through `less -R`).
//
// Layout adapts to the terminal width at print time:
//   >= 78 cols : full box, side-by-side SERVER | BENCHMARK
//   57..77     : box sized to the terminal, sections stacked
//   <  57      : no box, no ASCII logo, compact text

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include "redisPlusPlus.h"

namespace
{

    // Columns a string occupies on screen: skips ANSI CSI sequences and counts
    // UTF-8 code points (every glyph used by the banner is single-width).
    std::size_t displayWidth(const std::string &s)
    {
        std::size_t w = 0;
        for (std::size_t i = 0; i < s.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            if (c == 0x1B)
            {
                if (i + 1 < s.size() && s[i + 1] == '[')
                {
                    i += 2;
                    while (i < s.size() && !(s[i] >= 0x40 && s[i] <= 0x7E))
                        ++i;
                }
                continue;
            }
            if ((c & 0xC0) != 0x80)
                ++w; // skip UTF-8 continuation bytes
        }
        return w;
    }

    // Terminal width: TIOCGWINSZ, then $COLUMNS, then 80 (e.g. output to a file).
    int terminalColumns()
    {
        winsize ws{};
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
            return ws.ws_col;
        if (const char *c = std::getenv("COLUMNS"))
        {
            const int n = std::atoi(c);
            if (n > 0)
                return n;
        }
        return 80;
    }

} // namespace

void printWelcomeScreen()
{
    // ------------------------------------------------------------------ config
    const char *kVersion = "1.0.0";

    struct Bench
    {
        const char *cmd;
        long qps;
    };
    const Bench kBench[] = {
        {"PING", 159443},
        {"GET", 101779},
        {"SET", 93965},
        {"EXPIRE", 94288},
        {"TTL", 117846},
    };

    // ------------------------------------------------------------------ style
    const char *forced = std::getenv("CLICOLOR_FORCE");
    const bool color = std::getenv("NO_COLOR") == nullptr &&
                       (isatty(STDOUT_FILENO) || (forced && *forced && *forced != '0'));
    auto sgr = [color](const std::string &code)
    {
        return color ? "\033[" + code + "m" : std::string();
    };

    const std::string RST = sgr("0");
    const std::string FRM = sgr("38;5;31");    // frame: steel blue
    const std::string HEAD = sgr("1;38;5;45"); // section headers: cyan
    const std::string LBL = sgr("38;5;246");   // labels: grey
    const std::string VAL = sgr("1;38;5;255"); // values: bright white
    const std::string TXT = sgr("38;5;252");   // body text
    const std::string DIM = sgr("38;5;240");
    const std::string GRN = sgr("1;38;5;42");
    const std::string BAR = sgr("38;5;39;48;5;236"); // bar fill on dark track

    // ------------------------------------------------------------ responsive
    enum class Mode
    {
        Wide,
        Stacked,
        Compact
    };
    const int cols = terminalColumns();
    const Mode mode = cols >= 78 ? Mode::Wide : cols >= 57 ? Mode::Stacked
                                                           : Mode::Compact;
    const bool boxed = mode != Mode::Compact;

    const int W = mode == Mode::Wide ? 76 : mode == Mode::Stacked ? cols - 2
                                                                  : cols;
    const int LW = 37; // split columns (wide mode only)
    const int RW = W - LW - 1;

    // ---------------------------------------------------------------- layout
    auto rep = [](const std::string &g, int n)
    {
        std::string s;
        for (int i = 0; i < n; ++i)
            s += g;
        return s;
    };
    auto pad = [](const std::string &s, int w)
    {
        const int n = w - static_cast<int>(displayWidth(s));
        return n > 0 ? s + std::string(n, ' ') : s;
    };
    auto rpad = [](const std::string &s, int w) { // right-align
        const int n = w - static_cast<int>(displayWidth(s));
        return n > 0 ? std::string(n, ' ') + s : s;
    };
    auto center = [&](const std::string &s, int w)
    {
        const int n = (w - static_cast<int>(displayWidth(s))) / 2;
        return pad(std::string(n > 0 ? n : 0, ' ') + s, w);
    };

    std::string out;
    const std::string bar = FRM + "│" + RST;

    auto border = [&](const char *l, const char *r, const char *mid = nullptr)
    {
        if (!boxed)
        { // compact: thin rule instead of a frame
            out += DIM + rep("─", W < 40 ? W : 40) + RST + "\n";
            return;
        }
        out += FRM + l;
        out += mid ? rep("─", LW) + mid + rep("─", RW) : rep("─", W);
        out += r + RST + "\n";
    };
    auto row = [&](const std::string &s)
    {
        out += boxed ? bar + pad(s, W) + bar + "\n" : s + "\n";
    };
    auto split = [&](const std::string &a, const std::string &b)
    {
        out += bar + pad(a, LW) + bar + pad(b, RW) + bar + "\n";
    };

    // Colors solid blocks with `face` and the box-drawing shadow with `shade`.
    auto paint = [&](const std::string &art, const std::string &face,
                     const std::string &shade)
    {
        std::string s;
        int state = 0; // 0 = none, 1 = face, 2 = shade
        for (std::size_t i = 0; i < art.size();)
        {
            const unsigned char c = static_cast<unsigned char>(art[i]);
            const std::size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2
                                               : (c >> 4) == 0xE   ? 3
                                                                   : 4;
            const std::string g = art.substr(i, len);
            i += len;
            const int want = (g == " ") ? state : (g == "█" ? 1 : 2);
            if (want != state)
            {
                s += (want == 1 ? face : shade);
                state = want;
            }
            s += g;
        }
        return s + RST;
    };

    // ------------------------------------------------------------- sections
    auto kv = [&](const std::string &k, const std::string &v)
    {
        return "  " + LBL + pad(k, 14) + RST + v;
    };
    const std::string server[] = {
        "  " + HEAD + "SERVER" + RST,
        kv("Version", VAL + kVersion + RST),
        kv("Protocol", VAL + "RESP" + RST),
        kv("Persistence", VAL + "AOF" + RST),
        kv("Event loop", VAL + "epoll" + RST),
        kv("PID", VAL + std::to_string(getpid()) + RST),
        kv("Status", GRN + "● ONLINE" + RST + DIM + " / " + RST + GRN + "READY" + RST),
    };

    constexpr int kCells = 12; // bar length in columns
    long peak = 0;
    for (const auto &b : kBench)
        peak = b.qps > peak ? b.qps : peak;

    auto commas = [](long v)
    {
        std::string s = std::to_string(v);
        for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3)
            s.insert(i, ",");
        return s;
    };
    auto meter = [&](long v)
    {
        static const char *kEighths[8] = {"", "▏", "▎", "▍", "▌", "▋", "▊", "▉"};
        const long eighths = (v * kCells * 8 + peak / 2) / peak;
        const int full = static_cast<int>(eighths / 8), rem = static_cast<int>(eighths % 8);
        const int used = full + (rem ? 1 : 0);
        return BAR + rep("█", full) + kEighths[rem] + std::string(kCells - used, ' ') + RST;
    };

    std::string bench[7];
    bench[0] = pad("  " + HEAD + "BENCHMARK" + RST, 2 + 8 + kCells + 2 + 7 - 5) + DIM + "req/s" + RST;
    for (int i = 0; i < 5; ++i)
    {
        bench[i + 1] = "  " + LBL + pad(kBench[i].cmd, 8) + RST + meter(kBench[i].qps) +
                       "  " + VAL + rpad(commas(kBench[i].qps), 7) + RST;
    }

    // ----------------------------------------------------------------- header
    static const char *kRedis[6] = {
        "██████╗ ███████╗██████╗ ██╗███████╗",
        "██╔══██╗██╔════╝██╔══██╗██║██╔════╝",
        "██████╔╝█████╗  ██║  ██║██║███████╗",
        "██╔══██╗██╔══╝  ██║  ██║██║╚════██║",
        "██║  ██║███████╗██████╔╝██║███████║",
        "╚═╝  ╚═╝╚══════╝╚═════╝ ╚═╝╚══════╝",
    };
    static const char *kPlus[6] = {
        "       ",
        "  ██╗  ",
        "██████╗",
        "╚═██╔═╝",
        "  ╚═╝  ",
        "       ",
    };
    static const char *kBlueRamp[6] = {"38;5;87", "38;5;81", "38;5;45", "38;5;39", "38;5;33", "38;5;33"};
    static const char *kGreenRamp[6] = {"38;5;49", "38;5;48", "38;5;42", "38;5;41", "38;5;35", "38;5;35"};

    if (boxed)
    {
        border("╭", "╮");
        row("");
        for (int r = 0; r < 6; ++r)
        {
            const std::string plus = paint(kPlus[r], sgr(kGreenRamp[r]), sgr("38;5;23"));
            row(center(paint(kRedis[r], sgr(kBlueRamp[r]), sgr("38;5;24")) + " " + plus + " " + plus, W));
        }
        row("");
        row(center(TXT + "in-memory key-value store" + DIM + "  ·  " + RST +
                       LBL + "v" + kVersion + RST,
                   W));
        row("");
    }
    else
    {
        out += "\n  " + sgr("1;38;5;45") + "REDIS" + sgr("1;38;5;42") + "++" + RST +
               "  " + LBL + "v" + kVersion + RST + "\n";
        out += "  " + TXT + "in-memory key-value store" + RST + "\n";
        border("", "");
    }

    // --------------------------------------------------- server / benchmark
    if (mode == Mode::Wide)
    {
        border("├", "┤", "┬");
        split("", "");
        for (int i = 0; i < 7; ++i)
            split(server[i], bench[i]);
        split("", "");
        border("├", "┤", "┴");
    }
    else
    {
        if (boxed)
        {
            border("├", "┤");
            row("");
        }
        for (const auto &s : server)
            row(s);
        row("");
        for (int i = 0; i < 6; ++i)
            row(bench[i]); // bench[6] is the wide-mode filler
        if (boxed)
        {
            row("");
            border("├", "┤");
        }
        else
            border("", "");
    }

    // ---------------------------------------------------------------- startup
    auto ok = [&](const std::string &msg)
    {
        return "  " + DIM + "[" + RST + GRN + " OK " + RST + DIM + "]" + RST + "  " + TXT + msg + RST;
    };
    if (boxed)
        row("");
    row("  " + HEAD + "STARTUP" + RST);
    row(ok("AOF enabled"));
    row(ok("Event loop started"));
    row(ok("Server ready"));
    if (boxed)
        row("");

    // ----------------------------------------------------------------- footer
    char stamp[32] = "";
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    if (localtime_r(&now, &tm))
        std::strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", &tm);

    const std::string hint = "  " + TXT + "Ready to accept connections." + RST;
    const std::string right = DIM + "started " + stamp + RST + "  ";
    const int need = static_cast<int>(displayWidth(hint) + displayWidth(right)) + 2;

    border("├", "┤");
    if (W >= need)
        row(pad(hint, W - static_cast<int>(displayWidth(right))) + right);
    else
        row(hint); // drop the timestamp rather than wrap
    if (boxed)
        border("╰", "╯");

    out += RST; // belt and braces: never leave the terminal styled
    std::fputs(out.c_str(), stdout);
    std::fflush(stdout);
}

