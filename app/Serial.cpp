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

// A tiny serial port layer: POSIX on the Mac and Linux, Win32 on Windows. A port is an
// integer handle (a file descriptor, or a Windows HANDLE); -1 when closed.
namespace serial
{
using Port = intptr_t;
static constexpr Port NONE = -1;

juce::StringArray candidates()
{
    juce::StringArray out;
   #if JUCE_MAC
    for (auto& f : juce::File ("/dev").findChildFiles (juce::File::findFiles, false, "cu.usbmodem*")) out.add (f.getFullPathName());
   #elif JUCE_LINUX
    for (auto& f : juce::File ("/dev").findChildFiles (juce::File::findFiles, false, "ttyACM*")) out.add (f.getFullPathName());
   #elif JUCE_WINDOWS
    // the SP-1 running Sleepcousonique is USB 2FE3:5C51; Windows keeps its COM port name under Enum\USB
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
                wchar_t name[64]; DWORD sz = sizeof (name), type = 0;
                if (RegQueryValueExW (pk, L"PortName", nullptr, &type, (LPBYTE) name, &sz) == ERROR_SUCCESS && type == REG_SZ)
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

static Port openPort (const juce::String& path)
{
   #if JUCE_MAC || JUCE_LINUX
    int fd = open (path.toRawUTF8(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return NONE;
    termios t {};
    tcgetattr (fd, &t);
    cfmakeraw (&t);
    cfsetspeed (&t, B115200);
    t.c_cflag |= CLOCAL | CREAD;
    tcsetattr (fd, TCSANOW, &t);
    tcflush (fd, TCIOFLUSH);
    return fd;
   #elif JUCE_WINDOWS
    auto full = "\\\\.\\" + path;
    HANDLE h = CreateFileW (full.toWideCharPointer(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return NONE;
    DCB dcb {}; dcb.DCBlength = sizeof (dcb);
    GetCommState (h, &dcb);
    dcb.BaudRate = 115200; dcb.ByteSize = 8; dcb.Parity = NOPARITY; dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE; dcb.fDtrControl = DTR_CONTROL_ENABLE; dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fOutxCtsFlow = FALSE; dcb.fOutxDsrFlow = FALSE; dcb.fOutX = FALSE; dcb.fInX = FALSE;
    SetCommState (h, &dcb);
    PurgeComm (h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return (Port) h;
   #else
    juce::ignoreUnused (path);
    return NONE;
   #endif
}

static void closePort (Port p)
{
    if (p == NONE) return;
   #if JUCE_MAC || JUCE_LINUX
    close ((int) p);
   #elif JUCE_WINDOWS
    CloseHandle ((HANDLE) p);
   #endif
}

static void flushInput (Port p)
{
   #if JUCE_MAC || JUCE_LINUX
    tcflush ((int) p, TCIFLUSH);
   #elif JUCE_WINDOWS
    PurgeComm ((HANDLE) p, PURGE_RXCLEAR);
   #else
    juce::ignoreUnused (p);
   #endif
}

// up to n bytes within timeoutMs: the count, 0 if nothing came, -1 if the port is gone
static int readSome (Port p, char* buf, int n, int timeoutMs)
{
   #if JUCE_MAC || JUCE_LINUX
    pollfd pf { (int) p, POLLIN, 0 };
    int r = poll (&pf, 1, timeoutMs);
    if (r < 0 || (pf.revents & (POLLERR | POLLHUP | POLLNVAL))) return -1;
    if (r == 0) return 0;
    auto got = read ((int) p, buf, (size_t) n);
    if (got < 0) return errno == EAGAIN ? 0 : -1;
    return got == 0 ? -1 : (int) got;
   #elif JUCE_WINDOWS
    COMMTIMEOUTS to {};
    to.ReadIntervalTimeout = MAXDWORD; to.ReadTotalTimeoutMultiplier = MAXDWORD; to.ReadTotalTimeoutConstant = (DWORD) timeoutMs;
    to.WriteTotalTimeoutConstant = 2000;
    SetCommTimeouts ((HANDLE) p, &to);
    DWORD got = 0;
    if (! ReadFile ((HANDLE) p, buf, (DWORD) n, &got, nullptr)) return -1;
    return (int) got;
   #else
    juce::ignoreUnused (p, buf, n, timeoutMs);
    return -1;
   #endif
}

static bool writeAll (Port p, const char* d, size_t n)
{
    while (n > 0)
    {
       #if JUCE_MAC || JUCE_LINUX
        auto w = write ((int) p, d, n);
        if (w < 0) { if (errno == EAGAIN) { juce::Thread::sleep (2); continue; } return false; }
       #elif JUCE_WINDOWS
        DWORD w = 0;
        if (! WriteFile ((HANDLE) p, d, (DWORD) n, &w, nullptr)) return false;
       #else
        juce::ignoreUnused (p, d); return false;
        long w = 0;
       #endif
        d += w; n -= (size_t) w;
    }
    return true;
}

static juce::String readLine (Port p, int timeoutMs, const juce::StringArray& wanted = {})
{
    if (! wanted.isEmpty())
    {
        auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
        while (juce::Time::getMillisecondCounter() < end)
        {
            auto l = readLine (p, (int) (end - juce::Time::getMillisecondCounter()));
            for (auto& w : wanted) if (l.startsWith (w)) return l;
        }
        return {};
    }
    juce::String line;
    auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
    while (juce::Time::getMillisecondCounter() < end)
    {
        char c;
        int r = readSome (p, &c, 1, 50);
        if (r < 0) break;
        if (r == 0) continue;
        if (c == '\n') return line.trim();
        line << juce::String::charToString (c);
    }
    return line.trim();
}

juce::String send (const juce::String& text, int slot, juce::String& portUsed)
{
    auto ports = candidates();
    if (ports.isEmpty())
        return "No SP-1 is plugged in (no USB serial port found). Connect it with the Sleepcousonique firmware running.";
    for (auto& path : ports)
    {
        Port fd = openPort (path);
        if (fd == NONE) continue;
        // opening the port makes the SP-1 replay its stored log: let it finish first
        auto quietUntil = juce::Time::getMillisecondCounter() + 4000;
        while (juce::Time::getMillisecondCounter() < quietUntil)
        {
            auto l = readLine (fd, 400);
            if (l.isEmpty() || l.contains ("STORED LOG: end")) break;
        }
        flushInput (fd);
        juce::String hello;
        for (int attempt = 0; attempt < 3 && ! hello.startsWith ("SLEEPCOUSONIQUE"); ++attempt)
        {
            writeAll (fd, "SCQ HELLO\n", 10);
            hello = readLine (fd, 1500, { "SLEEPCOUSONIQUE" });
        }
        if (! hello.startsWith ("SLEEPCOUSONIQUE")) { closePort (fd); continue; }
        auto body = text.toStdString();
        auto head = "SCQ LOAD " + std::to_string (body.size()) + " " + std::to_string (slot) + "\n";
        writeAll (fd, head.data(), head.size());
        writeAll (fd, body.data(), body.size());
        auto reply = readLine (fd, 10000, { "OK", "ERR" });   // saving pauses and restarts the audio
        closePort (fd);
        portUsed = path;
        if (reply.startsWith ("OK")) return {};
        return reply.isEmpty() ? "The SP-1 didn't answer." : "The SP-1 said: " + reply;
    }
    return "Found " + ports.joinIntoString (", ") + " but none answered as Sleepcousonique. Flash the firmware first.";
}

// ------------------------------------------------------------------------------------------ the live link

Link::Link() : juce::Thread ("SP-1 link") { startThread(); }

Link::~Link()
{
    signalThreadShouldExit();
    stopThread (2000);
    closePort ((Port) fd);
}

juce::String Link::port() const { const juce::ScopedLock sl (lock); return portName; }
Link::Hands Link::hands() const { const juce::ScopedLock sl (lock); return latest; }

void Link::drop()
{
    closePort ((Port) fd);
    fd = -1;
    isConnected = false;
    const juce::ScopedLock sl (sendLock);
    if (awaitingReply) { sendResult = "The SP-1 went away."; awaitingReply = false; sendDone.signal(); }
}

bool Link::connect()
{
    for (auto& path : candidates())
    {
        Port f = openPort (path);
        if (f == NONE) continue;
        auto quietUntil = juce::Time::getMillisecondCounter() + 4000;      // let the log replay finish
        while (juce::Time::getMillisecondCounter() < quietUntil && ! threadShouldExit())
        {
            auto l = readLine (f, 400);
            if (l.isEmpty() || l.contains ("STORED LOG: end")) break;
        }
        flushInput (f);
        juce::String hello;
        for (int attempt = 0; attempt < 3 && ! hello.startsWith ("SLEEPCOUSONIQUE"); ++attempt)
        {
            writeAll (f, "SCQ HELLO\n", 10);
            hello = readLine (f, 1500, { "SLEEPCOUSONIQUE" });
        }
        if (! hello.startsWith ("SLEEPCOUSONIQUE")) { closePort (f); continue; }
        writeAll (f, "SCQ WATCH 1\n", 12);
        fd = (intptr_t) f;
        { const juce::ScopedLock sl (lock); portName = path; }
        isConnected = true;
        return true;
    }
    return false;
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
        m.when = juce::Time::getMillisecondCounter();
        const juce::ScopedLock sl (lock);
        meas = m;
        return;
    }
    if (line.contains ("SCQ engine avg="))
    {
        auto t = line.fromFirstOccurrenceOf ("SCQ engine avg=", false, false);
        Measured m;
        m.avg = t.getIntValue();
        m.peak = t.fromFirstOccurrenceOf ("max=", false, false).getIntValue();
        m.over = t.contains ("OVER BUDGET");
        m.when = juce::Time::getMillisecondCounter();
        const juce::ScopedLock sl (lock);
        meas = m;
        return;
    }
    const juce::ScopedLock sl (sendLock);
    if (awaitingReply && (line.startsWith ("OK") || line.startsWith ("ERR")))
    {
        sendResult = line.startsWith ("OK") ? juce::String() : "The SP-1 said: " + line;
        awaitingReply = false;
        sendDone.signal();
    }
}

Link::Measured Link::measured() const { const juce::ScopedLock sl (lock); return meas; }

juce::String Link::send (const juce::String& text, int slot)
{
    if (! isConnected) return "not connected";
    {
        const juce::ScopedLock sl (sendLock);
        pendingText = text;
        pendingSlot = slot;
        sendRequested = true;
        sendDone.reset();
    }
    if (! sendDone.wait (14000)) return "The SP-1 didn't answer.";
    const juce::ScopedLock sl (sendLock);
    return sendResult;
}

void Link::run()
{
    while (! threadShouldExit())
    {
        if (fd < 0)
        {
            if (! connect()) { wait (1500); continue; }
        }
        // a dream to send?
        {
            const juce::ScopedLock sl (sendLock);
            if (sendRequested)
            {
                sendRequested = false;
                auto body = pendingText.toStdString();
                auto head = "SCQ LOAD " + std::to_string (body.size()) + " " + std::to_string (pendingSlot) + "\n";
                if (! writeAll ((Port) fd, head.data(), head.size()) || ! writeAll ((Port) fd, body.data(), body.size()))
                {
                    sendResult = "Couldn't write to the SP-1.";
                    sendDone.signal();
                }
                else
                {
                    awaitingReply = true;
                    replyDeadline = juce::Time::getMillisecondCounter() + 12000;   // saving pauses and restarts the audio
                }
            }
            if (awaitingReply && juce::Time::getMillisecondCounter() > replyDeadline)
            {
                awaitingReply = false;
                sendResult = "The SP-1 didn't answer.";
                sendDone.signal();
            }
        }
        // read whatever has arrived
        char buf[512];
        int n = readSome ((Port) fd, buf, (int) sizeof buf, 30);
        if (n < 0) { drop(); continue; }  // unplugged
        for (int i = 0; i < n; ++i)
        {
            if (buf[i] == '\n') { handle (partial.trim()); partial.clear(); }
            else if (buf[i] != '\r' && partial.length() < 400) partial << juce::String::charToString (buf[i]);
        }
    }
}
}
