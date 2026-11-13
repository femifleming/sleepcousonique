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
        // opening the port makes the SP-1 replay its stored log: let it finish first
        auto quietUntil = juce::Time::getMillisecondCounter() + 4000;
        while (juce::Time::getMillisecondCounter() < quietUntil)
        {
            auto l = readLine (fd, 400);
            if (l.isEmpty() || l.contains ("STORED LOG: end")) break;
        }
        tcflush (fd, TCIFLUSH);
        juce::String hello;
        for (int attempt = 0; attempt < 3 && ! hello.startsWith ("SLEEPCOUSONIQUE"); ++attempt)
        {
            writeAll (fd, "SCQ HELLO\n", 10);
            hello = readLine (fd, 1500, { "SLEEPCOUSONIQUE" });
        }
        if (! hello.startsWith ("SLEEPCOUSONIQUE")) { close (fd); continue; }
        auto body = text.toStdString();
        auto head = "SCQ LOAD " + std::to_string (body.size()) + " " + std::to_string (slot) + "\n";
        writeAll (fd, head.data(), head.size());
        writeAll (fd, body.data(), body.size());
        auto reply = readLine (fd, 10000, { "OK", "ERR" });   // saving pauses and restarts the audio
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

// ------------------------------------------------------------------------------------------ the live link

Link::Link() : juce::Thread ("SP-1 link") { startThread(); }

Link::~Link()
{
    signalThreadShouldExit();
    stopThread (2000);
   #if JUCE_MAC || JUCE_LINUX
    if (fd >= 0) close (fd);
   #endif
}

juce::String Link::port() const { const juce::ScopedLock sl (lock); return portName; }
Link::Hands Link::hands() const { const juce::ScopedLock sl (lock); return latest; }

void Link::drop()
{
   #if JUCE_MAC || JUCE_LINUX
    if (fd >= 0) close (fd);
   #endif
    fd = -1;
    isConnected = false;
    const juce::ScopedLock sl (sendLock);
    if (awaitingReply) { sendResult = "The SP-1 went away."; awaitingReply = false; sendDone.signal(); }
}

bool Link::connect()
{
   #if JUCE_MAC || JUCE_LINUX
    for (auto& path : candidates())
    {
        int f = openPort (path);
        if (f < 0) continue;
        auto quietUntil = juce::Time::getMillisecondCounter() + 4000;      // let the log replay finish
        while (juce::Time::getMillisecondCounter() < quietUntil && ! threadShouldExit())
        {
            auto l = readLine (f, 400);
            if (l.isEmpty() || l.contains ("STORED LOG: end")) break;
        }
        tcflush (f, TCIFLUSH);
        juce::String hello;
        for (int attempt = 0; attempt < 3 && ! hello.startsWith ("SLEEPCOUSONIQUE"); ++attempt)
        {
            writeAll (f, "SCQ HELLO\n", 10);
            hello = readLine (f, 1500, { "SLEEPCOUSONIQUE" });
        }
        if (! hello.startsWith ("SLEEPCOUSONIQUE")) { close (f); continue; }
        writeAll (f, "SCQ WATCH 1\n", 12);
        fd = f;
        { const juce::ScopedLock sl (lock); portName = path; }
        isConnected = true;
        return true;
    }
   #endif
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
    const juce::ScopedLock sl (sendLock);
    if (awaitingReply && (line.startsWith ("OK") || line.startsWith ("ERR")))
    {
        sendResult = line.startsWith ("OK") ? juce::String() : "The SP-1 said: " + line;
        awaitingReply = false;
        sendDone.signal();
    }
}

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
   #if JUCE_MAC || JUCE_LINUX
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
                if (! writeAll (fd, head.data(), head.size()) || ! writeAll (fd, body.data(), body.size()))
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
        pollfd p { fd, POLLIN, 0 };
        int r = poll (&p, 1, 30);
        if (r < 0 || (p.revents & (POLLERR | POLLHUP | POLLNVAL))) { drop(); continue; }
        if (r == 0) continue;
        char buf[512];
        auto n = read (fd, buf, sizeof buf);
        if (n <= 0)
        {
            if (n < 0 && errno == EAGAIN) continue;
            drop();                       // unplugged
            continue;
        }
        for (ssize_t i = 0; i < n; ++i)
        {
            if (buf[i] == '\n') { handle (partial.trim()); partial.clear(); }
            else if (buf[i] != '\r' && partial.length() < 400) partial << juce::String::charToString (buf[i]);
        }
    }
   #endif
}
}
