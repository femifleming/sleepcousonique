#include "Serial.h"

#if JUCE_MAC || JUCE_LINUX
 #include <fcntl.h>
 #include <termios.h>
 #include <unistd.h>
 #include <poll.h>
#endif

namespace serial
{
juce::StringArray candidates()
{
    juce::StringArray out;
   #if JUCE_MAC
    for (auto& f : juce::File ("/dev").findChildFiles (juce::File::findFiles, false, "cu.usbmodem*")) out.add (f.getFullPathName());
   #elif JUCE_LINUX
    for (auto& f : juce::File ("/dev").findChildFiles (juce::File::findFiles, false, "ttyACM*")) out.add (f.getFullPathName());
   #endif
    return out;
}

#if JUCE_MAC || JUCE_LINUX
static juce::String readLine (int fd, int timeoutMs, const juce::StringArray& wanted = {})
{
    if (! wanted.isEmpty())
    {
        auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
        while (juce::Time::getMillisecondCounter() < end)
        {
            auto l = readLine (fd, (int) (end - juce::Time::getMillisecondCounter()));
            for (auto& w : wanted) if (l.startsWith (w)) return l;
        }
        return {};
    }
    juce::String line;
    auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
    while (juce::Time::getMillisecondCounter() < end)
    {
        pollfd p { fd, POLLIN, 0 };
        if (poll (&p, 1, 50) <= 0) continue;
        char c;
        if (read (fd, &c, 1) != 1) break;
        if (c == '\n') return line.trim();
        line << juce::String::charToString (c);
    }
    return line.trim();
}

static int openPort (const juce::String& path)
{
    int fd = open (path.toRawUTF8(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return -1;
    termios t {};
    tcgetattr (fd, &t);
    cfmakeraw (&t);
    cfsetspeed (&t, B115200);
    t.c_cflag |= CLOCAL | CREAD;
    tcsetattr (fd, TCSANOW, &t);
    tcflush (fd, TCIOFLUSH);
    return fd;
}

static bool writeAll (int fd, const char* d, size_t n)
{
    while (n > 0)
    {
        auto w = write (fd, d, n);
        if (w < 0) { if (errno == EAGAIN) { juce::Thread::sleep (2); continue; } return false; }
        d += w; n -= (size_t) w;
    }
    return true;
}
#endif

juce::String send (const juce::String& text, int slot, juce::String& portUsed)
{
   #if JUCE_MAC || JUCE_LINUX
    auto ports = candidates();
    if (ports.isEmpty())
        return "No SP-1 is plugged in (no USB serial port found). Connect it with the Sleepcousonique firmware running.";
    for (auto& path : ports)
    {
        int fd = openPort (path);
        if (fd < 0) continue;
        writeAll (fd, "SCQ HELLO\n", 10);
        auto hello = readLine (fd, 1200, { "SLEEPCOUSONIQUE" });
        if (! hello.startsWith ("SLEEPCOUSONIQUE")) { close (fd); continue; }
        auto body = text.toStdString();
        auto head = "SCQ LOAD " + std::to_string (body.size()) + " " + std::to_string (slot) + "\n";
        writeAll (fd, head.data(), head.size());
        writeAll (fd, body.data(), body.size());
        auto reply = readLine (fd, 6000, { "OK", "ERR" });
        close (fd);
        portUsed = path;
        if (reply.startsWith ("OK")) return {};
        return reply.isEmpty() ? "The SP-1 didn't answer." : "The SP-1 said: " + reply;
    }
    return "Found " + ports.joinIntoString (", ") + " but none answered as Sleepcousonique. Flash the firmware first.";
   #else
    juce::ignoreUnused (text, slot, portUsed);
    return "Sending to the SP-1 from Windows isn't written yet.";
   #endif
}
}
