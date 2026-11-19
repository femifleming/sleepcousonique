#include "Serial.h"

#if JUCE_MAC || JUCE_LINUX
 #include <fcntl.h>
 #include <termios.h>
 #include <unistd.h>
 #include <poll.h>
 #include <cerrno>
#elif JUCE_WINDOWS
 #include <windows.h>
#endif

// the firmware that ships with this editor (firmware/dist/sleepcousonique-sp1.bin), from CMakeLists.txt
#ifndef SCQ_FIRMWARE_VERSION
 #define SCQ_FIRMWARE_VERSION "0.0.0"
#endif

namespace serial
{
Timing timing;

static juce::uint32 now() { return juce::Time::getMillisecondCounter(); }
static int left (juce::uint32 until) { auto t = now(); return t >= until ? 0 : (int) (until - t); }

// ------------------------------------------------------------------------------------------ ports

#if JUCE_MAC || JUCE_LINUX
struct PortWire : Wire
{
    int fd;
    explicit PortWire (int f) : fd (f) {}
    ~PortWire() override { close (fd); }
    int read (char* buf, int n, int timeoutMs) override
    {
        pollfd pf { fd, POLLIN, 0 };
        int r = poll (&pf, 1, timeoutMs);
        if (r < 0) return errno == EINTR ? 0 : -1;
        if (r == 0) return 0;
        if (pf.revents & (POLLERR | POLLNVAL)) return -1;
        auto got = ::read (fd, buf, (size_t) n);
        if (got < 0) return errno == EAGAIN || errno == EINTR ? 0 : -1;
        return got == 0 ? -1 : (int) got;     // POLLHUP with nothing left: unplugged
    }
    int write (const char* d, int n, int timeoutMs) override
    {
        pollfd pf { fd, POLLOUT, 0 };
        int r = poll (&pf, 1, timeoutMs);
        if (r < 0) return errno == EINTR ? 0 : -1;
        if (r == 0) return 0;                 // the SP-1 isn't taking bytes just now
        if (pf.revents & (POLLERR | POLLNVAL | POLLHUP)) return -1;
        auto w = ::write (fd, d, (size_t) n);
        if (w < 0) return errno == EAGAIN || errno == EINTR ? 0 : -1;
        return (int) w;
    }
    void flushInput() override { tcflush (fd, TCIFLUSH); }
};

static std::unique_ptr<Wire> openPort (const juce::String& path)
{
    int fd = ::open (path.toRawUTF8(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return nullptr;
    termios t {};
    tcgetattr (fd, &t);
    cfmakeraw (&t);
    cfsetspeed (&t, B115200);
    t.c_cflag |= CLOCAL | CREAD;
    tcsetattr (fd, TCSANOW, &t);
    tcflush (fd, TCIOFLUSH);
    return std::make_unique<PortWire> (fd);
}
#elif JUCE_WINDOWS
struct PortWire : Wire
{
    HANDLE h;
    DWORD readTo = 0xffffffff, writeTo = 0xffffffff;
    explicit PortWire (HANDLE x) : h (x) {}
    ~PortWire() override { CloseHandle (h); }
    void timeouts (DWORD r, DWORD w)
    {
        if (r == readTo && w == writeTo) return;
        COMMTIMEOUTS to {};
        // MAXDWORD / MAXDWORD / constant: return at once with whatever has come, else wait up to
        // the constant for the first byte. MAXDWORD / 0 / 0: return at once, even with nothing.
        to.ReadIntervalTimeout = MAXDWORD;
        to.ReadTotalTimeoutMultiplier = r > 0 ? MAXDWORD : 0;
        to.ReadTotalTimeoutConstant = r;
        to.WriteTotalTimeoutMultiplier = 0;
        to.WriteTotalTimeoutConstant = juce::jmax ((DWORD) 1, w);   // 0 would mean "never time out"
        SetCommTimeouts (h, &to);
        readTo = r; writeTo = w;
    }
    int read (char* buf, int n, int timeoutMs) override
    {
        timeouts ((DWORD) juce::jmax (0, timeoutMs), writeTo == 0xffffffff ? 2000 : writeTo);
        DWORD got = 0;
        if (! ReadFile (h, buf, (DWORD) n, &got, nullptr)) return -1;
        return (int) got;
    }
    int write (const char* d, int n, int timeoutMs) override
    {
        timeouts (readTo == 0xffffffff ? 0 : readTo, (DWORD) juce::jmax (1, timeoutMs));
        DWORD w = 0;
        if (! WriteFile (h, d, (DWORD) n, &w, nullptr)) return GetLastError() == ERROR_SEM_TIMEOUT ? (int) w : -1;
        return (int) w;                       // fewer than n: it timed out
    }
    void flushInput() override { PurgeComm (h, PURGE_RXCLEAR); }
};

static std::unique_ptr<Wire> openPort (const juce::String& path)
{
    // "\\.\COM12": the only form that reaches COM10 and above
    auto full = path.startsWith ("\\\\.\\") ? path : "\\\\.\\" + path;
    HANDLE h = CreateFileW (full.toWideCharPointer(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return nullptr;     // gone, or another program has it
    DCB dcb {}; dcb.DCBlength = sizeof (dcb);
    GetCommState (h, &dcb);
    dcb.BaudRate = 115200; dcb.ByteSize = 8; dcb.Parity = NOPARITY; dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE; dcb.fDtrControl = DTR_CONTROL_ENABLE; dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fOutxCtsFlow = FALSE; dcb.fOutxDsrFlow = FALSE; dcb.fOutX = FALSE; dcb.fInX = FALSE;
    SetCommState (h, &dcb);
    PurgeComm (h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    auto w = std::make_unique<PortWire> (h);
    w->timeouts (0, 2000);
    return w;
}
#else
static std::unique_ptr<Wire> openPort (const juce::String&) { return nullptr; }
#endif

static juce::StringArray testPorts;
static Opener testOpener;
static juce::CriticalSection testLock;

void setTestPorts (const juce::StringArray& ports, Opener opener)
{
    const juce::ScopedLock sl (testLock);
    testPorts = ports;
    testOpener = std::move (opener);
}

std::unique_ptr<Wire> open (const juce::String& port)
{
    Opener o;
    { const juce::ScopedLock sl (testLock); o = testOpener; }
    return o ? o (port) : openPort (port);
}

juce::StringArray candidates()
{
    {
        const juce::ScopedLock sl (testLock);
        if (testOpener) return testPorts;
    }
    juce::StringArray out;
    auto env = juce::SystemStats::getEnvironmentVariable ("SCQ_PORTS", {});
    if (env.isNotEmpty())
    {
        out.addTokens (env, ";", "");
        out.removeEmptyStrings();
        return out;
    }
   #if JUCE_MAC
    for (auto& f : juce::File ("/dev").findChildFiles (juce::File::findFiles, false, "cu.usbmodem*")) out.add (f.getFullPathName());
    out.sort (true);
   #elif JUCE_LINUX
    for (auto& f : juce::File ("/dev").findChildFiles (juce::File::findFiles, false, "ttyACM*")) out.add (f.getFullPathName());
    out.sort (true);
   #elif JUCE_WINDOWS
    // the SP-1 running Sleepcousonique is USB 2FE3:5C51 (a composite device: its serial port is
    // interface 0, "VID_2FE3&PID_5C51&MI_00"); Windows keeps the COM port name under Enum\USB.
    // Devices seen before but not plugged in now stay listed there too: QueryDosDevice tells
    // which COM ports exist right now.
    HKEY usb;
    if (RegOpenKeyExW (HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Enum\\USB", 0, KEY_READ, &usb) == ERROR_SUCCESS)
    {
        wchar_t dev[256];
        for (DWORD i = 0; ; ++i)
        {
            DWORD n = 256;
            if (RegEnumKeyExW (usb, i, dev, &n, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
            if (! juce::String (dev).startsWithIgnoreCase ("VID_2FE3&PID_5C51")) continue;
            HKEY d;
            if (RegOpenKeyExW (usb, dev, 0, KEY_READ, &d) != ERROR_SUCCESS) continue;
            wchar_t inst[256];
            for (DWORD j = 0; ; ++j)
            {
                DWORD m = 256;
                if (RegEnumKeyExW (d, j, inst, &m, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
                juce::String key = juce::String (inst) + "\\Device Parameters";
                HKEY pk;
                if (RegOpenKeyExW (d, key.toWideCharPointer(), 0, KEY_READ, &pk) != ERROR_SUCCESS) continue;
                wchar_t name[64] {}; DWORD sz = sizeof (name) - sizeof (wchar_t), type = 0;
                wchar_t target[512];
                if (RegQueryValueExW (pk, L"PortName", nullptr, &type, (LPBYTE) name, &sz) == ERROR_SUCCESS && type == REG_SZ
                     && QueryDosDeviceW (name, target, 512) != 0)        // plugged in now (not just remembered)
                    out.addIfNotAlreadyThere (juce::String (name));
                RegCloseKey (pk);
            }
            RegCloseKey (d);
        }
        RegCloseKey (usb);
    }
   #endif
    return out;
}

// ------------------------------------------------------------------------------------------ lines

// Lines from a wire, read in chunks (not a byte at a time).
struct Lines
{
    Wire& w;
    std::string buf;
    bool gone = false;
    explicit Lines (Wire& x) : w (x) {}
    // the next line within timeoutMs: true and the line, or false (nothing whole came, or gone)
    bool next (juce::String& line, int timeoutMs)
    {
        auto until = now() + (juce::uint32) juce::jmax (0, timeoutMs);
        for (;;)
        {
            auto nl = buf.find ('\n');
            if (nl != std::string::npos)
            {
                line = juce::String::fromUTF8 (buf.data(), (int) nl).trim();
                buf.erase (0, nl + 1);
                return true;
            }
            if (gone) return false;
            char c[256];
            int n = w.read (c, (int) sizeof c, juce::jmin (50, left (until)));
            if (n < 0) { gone = true; return false; }
            buf.append (c, (size_t) n);
            if (buf.size() > 4096) buf.erase (0, buf.size() - 4096);   // no newline in 4 KB: noise
            if (n == 0 && left (until) == 0) return false;
        }
    }
};

static bool writeAll (Wire& w, const char* d, int n, int timeoutMs)
{
    auto until = now() + (juce::uint32) timeoutMs;
    while (n > 0)
    {
        int k = w.write (d, n, juce::jmax (1, juce::jmin (250, left (until))));
        if (k < 0) return false;
        d += k; n -= k;
        if (n > 0 && left (until) == 0) return false;
    }
    return true;
}

// ------------------------------------------------------------------------------------------ the protocol

Hello greet (Wire& w, const std::function<bool()>& stop)
{
    Hello h;
    bool standby = false, heard = false;
    auto look = [&] (const juce::String& l)
    {
        if (l.isEmpty()) return false;
        heard = true;
        // a live status line; replayed ones start "[+00:00:01.000]" and say what WAS
        if (l.startsWith ("[STANDBY]")) standby = true;
        if (l.startsWith ("SLEEPCOUSONIQUE"))
        {
            h.what = Hello::sleepcousonique;
            h.version = l.fromFirstOccurrenceOf ("SLEEPCOUSONIQUE", false, false).trim().upToFirstOccurrenceOf (" ", false, false);
            return true;
        }
        return false;
    };
    auto stopped = [&] { return stop && stop(); };
    Lines in (w);
    juce::String l;
    // opening the port makes the SP-1 print its banner and replay its stored log: let it finish
    auto until = now() + (juce::uint32) timing.logMost;
    while (left (until) > 0 && ! stopped() && in.next (l, timing.logQuiet))
    {
        if (look (l)) return h;
        if (l.contains ("STORED LOG: end")) break;
    }
    w.flushInput();
    in.buf.clear();
    for (int attempt = 0; attempt < timing.helloTries && ! stopped() && ! in.gone; ++attempt)
    {
        if (! writeAll (w, "SCQ HELLO\n", 10, 2000)) break;
        auto end = now() + (juce::uint32) timing.helloWait;
        while (left (end) > 0 && ! in.gone)
            if (in.next (l, left (end)) && look (l)) return h;
    }
    h.what = standby ? Hello::standby : heard ? Hello::otherFirmware : Hello::silent;
    return h;
}

juce::String explain (const Hello& h, const juce::String& port)
{
    const auto dots = juce::String::fromUTF8 ("\xe2\x80\xa2\xe2\x80\xa2");   // the SP-1's power button
    auto name = port.fromLastOccurrenceOf ("/", false, false);
    switch (h.what)
    {
        case Hello::sleepcousonique: return {};
        case Hello::standby:
            return "The SP-1 on " + name + " is switched off (standby: it is charging, and the "
                   "Sleepcousonique firmware only listens while it plays). Hold " + dots + " for about a second "
                   "to turn it on, then send again. (An SP-1 that has just been flashed, or plugged "
                   "in while off, always waits here.)";
        case Hello::otherFirmware:
            return name + " is talking, but not as Sleepcousonique: that SP-1 runs other firmware "
                   "(Wakes, the tape looper...), or this is another USB device. Flash "
                   "sleepcousonique-sp1.bin (see FLASHING.txt).";
        case Hello::silent:
        default:
            return name + " never answered. If it's the SP-1: unplug it and plug it in again, turn "
                   "it on (hold " + dots + "), and quit any other program that has its port open (a serial "
                   "monitor, tools/sp1fleet.py, another Sleepcousonique).";
    }
}

static juce::String busy (const juce::String& port)
{
   #if JUCE_MAC || JUCE_LINUX
    if (port.startsWith ("/") && ! juce::File (port).exists())
        return port.fromLastOccurrenceOf ("/", false, false) + " has gone (the SP-1 was unplugged).";
   #endif
    return port.fromLastOccurrenceOf ("/", false, false) + " is in use by another program (another Sleepcousonique, "
           "the Array, a serial monitor or tools/sp1fleet.py): quit it and send again.";
}

static const char* const noSP1 = "No SP-1 is plugged in (no USB serial port for it found). Connect it with a "
                                 "data cable (not a charge-only one) and turn it on; if this is its first time, "
                                 "flash sleepcousonique-sp1.bin first (see FLASHING.txt).";

bool writeDream (Wire& w, const std::string& body, int slot, const std::function<void (const char*, int)>& between)
{
    auto all = "SCQ LOAD " + std::to_string (body.size()) + " " + std::to_string (slot) + "\n" + body;
    auto until = now() + (juce::uint32) timing.writeMost;
    size_t pos = 0;
    while (pos < all.size())
    {
        int n = (int) juce::jmin ((size_t) 512, all.size() - pos);
        int k = w.write (all.data() + pos, n, juce::jmax (1, juce::jmin (250, left (until))));
        if (k < 0) return false;
        pos += (size_t) k;
        if (between)
        {
            // what the SP-1 says meanwhile (its hands, ~30 lines a second) is read as we go, so
            // neither side waits on the other
            char c[512];
            int got = w.read (c, (int) sizeof c, 0);
            if (got < 0) return false;
            if (got > 0) between (c, got);
        }
        if (pos < all.size() && left (until) == 0) return false;
    }
    return true;
}

static juce::Array<int> versionParts (const juce::String& v)
{
    juce::Array<int> p;
    for (auto& t : juce::StringArray::fromTokens (v.upToFirstOccurrenceOf ("-", false, false), ".", ""))
        p.add (t.getIntValue());
    while (p.size() < 3) p.add (0);
    return p;
}

// true if a is older than b ("1.2.0" < "1.4.0"; "1.4.0-beta1" counts as 1.4.0)
static bool older (const juce::String& a, const juce::String& b)
{
    auto x = versionParts (a), y = versionParts (b);
    for (int i = 0; i < 3; ++i) if (x[i] != y[i]) return x[i] < y[i];
    return false;
}

juce::String describeReply (const juce::String& reply, const juce::String& sp1Version)
{
    if (reply.startsWith ("OK")) return {};
    auto oldFirmware = sp1Version.isNotEmpty() && older (sp1Version, SCQ_FIRMWARE_VERSION)
        ? " The SP-1 is running firmware " + sp1Version + "; this editor comes with " SCQ_FIRMWARE_VERSION
          ", and places and room added since aren't on the SP-1 yet. Flash the sleepcousonique-sp1.bin that "
          "came with this editor (see FLASHING.txt) and send again."
        : juce::String();
    if (reply.isEmpty())
        return "The SP-1 didn't answer." + oldFirmware;
    if (reply.startsWith ("ERR too big"))
        return "The SP-1 has no room for a dream this long (a slot keeps 16 KB of text).";
    if (reply.startsWith ("ERR could not save"))
        return "The SP-1 is playing the dream but couldn't keep it in its slot, so it will be gone when the "
               "SP-1 is switched off. Send it again.";
    if (reply.startsWith ("ERR") && reply.contains ("mistake"))
    {
        int n = reply.fromFirstOccurrenceOf ("ERR", false, false).trim().getIntValue();
        return "The SP-1 found " + juce::String (n) + (n == 1 ? " mistake" : " mistakes") + " in this dream and kept "
               "playing the one it had." + (oldFirmware.isNotEmpty() ? oldFirmware
               : juce::String (" If the editor shows none, please report it, with the dream."));
    }
    return "The SP-1 said: " + reply + oldFirmware;
}

juce::String send (const juce::String& text, int slot, juce::String& portUsed)
{
    auto ports = candidates();
    if (ports.isEmpty()) return noSP1;
    juce::String why;
    int whyRank = -1;
    auto note = [&] (const juce::String& w, int rank) { if (rank > whyRank) { why = w; whyRank = rank; } };
    for (auto& path : ports)
    {
        auto w = open (path);
        if (w == nullptr) { note (busy (path), 1); continue; }
        auto h = greet (*w);
        if (h.what != Hello::sleepcousonique)
        {
            note (explain (h, path), h.what == Hello::standby ? 4 : h.what == Hello::otherFirmware ? 3 : 0);
            continue;
        }
        portUsed = path;
        Lines in (*w);
        if (! writeDream (*w, text.toStdString(), slot, [&] (const char* c, int n) { in.buf.append (c, (size_t) n); }))
            return "The SP-1 on " + path.fromLastOccurrenceOf ("/", false, false) + " stopped taking the dream halfway. "
                   "Unplug it, plug it in again and send again.";
        juce::String l, reply;
        auto until = now() + (juce::uint32) timing.replyWait;
        while (left (until) > 0 && ! in.gone)
            if (in.next (l, left (until)) && (l.startsWith ("OK") || l.startsWith ("ERR"))) { reply = l; break; }
        return describeReply (reply, h.version);
    }
    return why;
}

juce::String ask (const juce::String& port, const juce::String& line, const juce::StringArray& wanted, int timeoutMs)
{
    auto w = open (port);
    if (w == nullptr) return {};
    w->flushInput();
    auto out = (line + "\n").toStdString();
    if (! writeAll (*w, out.data(), (int) out.size(), 2000)) return {};
    Lines in (*w);
    juce::String l;
    auto until = now() + (juce::uint32) timeoutMs;
    while (left (until) > 0 && ! in.gone)
        if (in.next (l, left (until)))
            for (auto& x : wanted) if (l.startsWith (x)) return l;
    return {};
}

// ------------------------------------------------------------------------------------------ the live link

bool Link::enabled = true;

Link::Link() : juce::Thread ("SP-1 link") { if (enabled) startThread(); }

Link::~Link()
{
    signalThreadShouldExit();
    notify();
    stopThread (4000);
    wire.reset();
}

juce::String Link::port() const { const juce::ScopedLock sl (lock); return portName; }
juce::String Link::version() const { const juce::ScopedLock sl (lock); return sp1Version; }
juce::String Link::note() const { const juce::ScopedLock sl (lock); return isConnected ? juce::String() : why; }
Link::Hands Link::hands() const { const juce::ScopedLock sl (lock); return latest; }
Link::Measured Link::measured() const { const juce::ScopedLock sl (lock); return meas; }

void Link::finishSend (const juce::String& result)
{
    const juce::ScopedLock sl (sendLock);
    sendResult = result;
    awaitingReply = false;
    doneTicket = sendTicket;
    sendDone.signal();
}

void Link::drop()
{
    wire.reset();
    isConnected = false;
    partial.clear();
    { const juce::ScopedLock sl (lock); why = "The SP-1 went away (unplugged, or switched off)."; }
    bool waiting;
    { const juce::ScopedLock sl (sendLock); waiting = awaitingReply || sendRequested; sendRequested = false; }
    if (waiting) finishSend ("The SP-1 went away before it answered.");
}

bool Link::connect()
{
    auto ports = candidates();
    juce::String reason = ports.isEmpty() ? juce::String (noSP1) : juce::String();
    int rank = -1;
    for (auto& path : ports)
    {
        if (threadShouldExit()) return false;
        auto w = open (path);
        if (w == nullptr) { if (rank < 1) { reason = busy (path); rank = 1; } continue; }
        auto h = greet (*w, [this] { return threadShouldExit(); });
        if (h.what != Hello::sleepcousonique)
        {
            int r = h.what == Hello::standby ? 4 : h.what == Hello::otherFirmware ? 3 : 0;
            if (r > rank) { reason = explain (h, path); rank = r; }
            continue;
        }
        if (! writeAll (*w, "SCQ WATCH 1\n", 12, 2000)) continue;
        wire = std::move (w);
        partial.clear();
        {
            const juce::ScopedLock sl (lock);
            portName = path;
            sp1Version = h.version;
            why.clear();
        }
        isConnected = true;
        return true;
    }
    const juce::ScopedLock sl (lock);
    why = reason;
    return false;
}

void Link::heard (const char* buf, int n)
{
    for (int i = 0; i < n; ++i)
    {
        if (buf[i] == '\n') { handle (partial.trim()); partial.clear(); }
        else if (buf[i] != '\r' && partial.length() < 400) partial << juce::String::charToString (buf[i]);
    }
}

void Link::handle (const juce::String& line)
{
    if (line.startsWith ("SCQ HANDS "))
    {
        auto t = juce::StringArray::fromTokens (line.substring (10), " ", "");
        if (t.size() < 8) return;
        Hands h;
        for (int k = 0; k < 4; ++k) h.fader[k] = juce::jlimit (0.0f, 1.0f, (float) t[k].getIntValue() / 4095.0f);
        h.buttons = (juce::uint32) t[4].getHexValue32();
        h.scene = t[5].getIntValue();
        h.slot = t[6].getIntValue();
        for (int k = 0; k < 8 && k * 2 + 1 < t[7].length(); ++k)
            h.lamp[k] = (float) t[7].substring (k * 2, k * 2 + 2).getHexValue32() / 255.0f;
        const juce::ScopedLock sl (lock);
        h.count = latest.count + 1;
        latest = h;
        return;
    }
    if (line.contains ("SCQ engine avg="))
    {
        auto t = line.fromFirstOccurrenceOf ("SCQ engine avg=", false, false);
        Measured m;
        m.avg = t.getIntValue();
        m.peak = t.fromFirstOccurrenceOf ("max=", false, false).getIntValue();
        m.over = t.contains ("OVER BUDGET");
        m.when = now();
        const juce::ScopedLock sl (lock);
        meas = m;
        return;
    }
    bool mine;
    { const juce::ScopedLock sl (sendLock); mine = awaitingReply && (line.startsWith ("OK") || line.startsWith ("ERR")); }
    if (mine) finishSend (describeReply (line, version()));
}

juce::String Link::send (const juce::String& text, int slot, int waitForSP1Ms)
{
    if (! isThreadRunning()) return "The SP-1 link is off (--no-link).";
    const juce::ScopedLock one (oneSend);
    auto until = now() + (juce::uint32) juce::jmax (0, waitForSP1Ms);
    while (! isConnected && left (until) > 0) juce::Thread::sleep (50);
    if (! isConnected)
    {
        auto n = note();
        return n.isNotEmpty() ? n : juce::String ("Still looking for the SP-1: send again in a moment.");
    }
    juce::uint32 ticket;
    {
        const juce::ScopedLock sl (sendLock);
        pendingText = text;
        pendingSlot = slot;
        sendRequested = true;
        ticket = ++sendTicket;
        sendDone.reset();
    }
    notify();
    if (! sendDone.wait (timing.writeMost + timing.replyWait + 3000))
    {
        const juce::ScopedLock sl (sendLock);
        sendRequested = false;
        awaitingReply = false;
        return "The SP-1 didn't answer.";
    }
    const juce::ScopedLock sl (sendLock);
    return doneTicket == ticket ? sendResult : juce::String ("The SP-1 didn't answer.");
}

void Link::run()
{
    while (! threadShouldExit())
    {
        if (wire == nullptr)
        {
            if (! connect()) { wait (1500); continue; }
        }
        // a dream to send? (written outside the lock: a send never holds up the caller's lock)
        std::string body;
        int slot = -1;
        bool sending = false;
        {
            const juce::ScopedLock sl (sendLock);
            if (sendRequested) { sendRequested = false; sending = true; body = pendingText.toStdString(); slot = pendingSlot; }
        }
        if (sending)
        {
            {
                const juce::ScopedLock sl (sendLock);
                awaitingReply = true;               // an ERR can come back before the last byte goes
                replyDeadline = now() + (juce::uint32) (timing.writeMost + timing.replyWait);
            }
            if (! writeDream (*wire, body, slot, [this] (const char* c, int n) { heard (c, n); }))
            {
                finishSend ("The SP-1 stopped taking the dream halfway. Unplug it, plug it in again and send again.");
                drop();
                continue;
            }
            const juce::ScopedLock sl (sendLock);
            if (awaitingReply) replyDeadline = now() + (juce::uint32) timing.replyWait;
        }
        bool late;
        {
            const juce::ScopedLock sl (sendLock);
            late = awaitingReply && now() > replyDeadline;
        }
        if (late) finishSend (describeReply ({}, version()));
        // read whatever has arrived
        char buf[512];
        int n = wire->read (buf, (int) sizeof buf, 30);
        if (n < 0) { drop(); continue; }  // unplugged
        heard (buf, n);
    }
}
}
