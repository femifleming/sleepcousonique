// The editor's USB link (app/Serial.cpp) against a pretend SP-1 (tests/fakesp1.c), on every
// platform: greeting, sending, the replies, and everything that can go wrong without hanging.
// On macOS and Linux it then does it all again through a real pseudo-terminal, so the port code
// itself (open, termios, poll, write) is in the test too.
#include <juce_core/juce_core.h>
#include "../app/Serial.h"
#include "fakesp1.h"
#include "scq.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>

#if JUCE_MAC || JUCE_LINUX
 #include <fcntl.h>
 #include <termios.h>
 #include <unistd.h>
 #include <poll.h>
 #include <cstdlib>
#endif

static int failures = 0;
#define CHECK(c, msg) do { if (! (c)) { ++failures; std::printf ("FAIL %s  (%s:%d)\n", juce::String (msg).toRawUTF8(), __FILE__, __LINE__); } \
                           else std::printf ("ok   %s\n", juce::String (msg).toRawUTF8()); std::fflush (stdout); } while (0)

static juce::uint32 now() { return juce::Time::getMillisecondCounter(); }

// ------------------------------------------------------------------------------------------ in process

struct Device
{
    std::unique_ptr<fake_sp1> f { new fake_sp1() };
    juce::CriticalSection lock;
    juce::uint32 last = now();
    bool unplugged = false;
    int logLines = 30;
    int opens = 0;
    explicit Device (int mode, const char* version = "1.4.0", bool wingie = false) { fake_init (f.get(), mode, version); if (wingie) fake_wingie (f.get()); }
    void tick() { auto t = now(); fake_tick (f.get(), t - last); last = t; }
};

struct FakeWire : serial::Wire
{
    Device& d;
    explicit FakeWire (Device& x) : d (x) { const juce::ScopedLock sl (d.lock); d.tick(); ++d.opens; fake_open (d.f.get(), d.logLines); }
    int read (char* buf, int n, int timeoutMs) override
    {
        auto until = now() + (juce::uint32) timeoutMs;
        for (;;)
        {
            {
                const juce::ScopedLock sl (d.lock);
                if (d.unplugged) return -1;
                d.tick();
                int got = fake_take (d.f.get(), buf, juce::jmin (n, 200));   // USB packets, more or less
                if (got > 0) return got;
            }
            if (now() >= until) return 0;
            juce::Thread::sleep (2);
        }
    }
    int write (const char* p, int n, int timeoutMs) override
    {
        auto until = now() + (juce::uint32) timeoutMs;
        for (;;)
        {
            {
                const juce::ScopedLock sl (d.lock);
                if (d.unplugged) return -1;
                d.tick();
                int k = fake_feed (d.f.get(), p, n);
                if (k > 0 || n == 0) return k;
            }
            if (now() >= until) return 0;
            juce::Thread::sleep (2);
        }
    }
    void flushInput() override { const juce::ScopedLock sl (d.lock); char junk[4096]; while (fake_take (d.f.get(), junk, sizeof junk) > 0) {} }
};

// ports named after devices; a port with no device is busy (another program holds it)
static void use (std::map<juce::String, Device*> ports)
{
    juce::StringArray names;
    for (auto& p : ports) names.add (p.first);
    serial::setTestPorts (names, [ports] (const juce::String& name) -> std::unique_ptr<serial::Wire>
    {
        auto it = ports.find (name);
        if (it == ports.end() || it->second == nullptr) return nullptr;
        return std::make_unique<FakeWire> (*it->second);
    });
}

static juce::String firstNight()
{
    return "2026-10-08\n----------\nthe route is seq 8: 0 7 12 15 12 7 3 -5\nthe tick is slope\nthe voice is sine 2\n"
           "the hall is delay 9\n\ntick.end -> tick.trig route.next\nroute ~70> voice.pitch\nvoice -> hall -> out\nfader1 -> hall.feedback\n";
}

// a dream that sounds the same, padded with notes to exactly `bytes` bytes
static juce::String padded (int bytes)
{
    auto s = firstNight().toStdString();
    int k = 0;
    while ((int) s.size() < bytes)
    {
        std::string note = "; a note to fill the slot " + std::to_string (k++) + " ......................................\n";
        s += note.substr (0, (size_t) juce::jmin ((int) note.size(), bytes - (int) s.size()));
    }
    if (s.back() != '\n') s.back() = '\n';
    return juce::String::fromUTF8 (s.data(), (int) s.size());
}

static bool same (const fake_sp1& f, const juce::String& text)
{
    auto s = text.toStdString();
    return f.last_len == (int) s.size() && std::memcmp (f.staging, s.data(), s.size()) == 0;
}

static void protocol()
{
    std::printf ("\n-- the protocol, in process --\n");
    {
        Device d (FAKE_ON, "1.4.0");
        FakeWire w (d);
        auto h = serial::greet (w);
        CHECK (h.what == serial::Hello::sleepcousonique && h.version == "1.4.0", "an SP-1 that is on answers HELLO with its version (" + h.version + ")");
        CHECK (serial::explain (h, "/dev/cu.usbmodem101").isEmpty(), "and needs no explaining");
    }
    {
        Device d (FAKE_STANDBY);
        FakeWire w (d);
        auto t0 = now();
        auto h = serial::greet (w);
        CHECK (h.what == serial::Hello::standby, "an SP-1 that is off (standby, as after flashing) is recognised as off");
        CHECK (serial::explain (h, "/dev/cu.usbmodem101").contains ("switched off"), "and the person is told to switch it on, not to flash it");
        CHECK (! serial::explain (h, "/dev/cu.usbmodem101").contains ("Flash"), "  (nothing about flashing)");
        std::printf ("     (took %u ms)\n", now() - t0);
    }
    {
        Device d (FAKE_OTHER);
        FakeWire w (d);
        auto h = serial::greet (w);
        CHECK (h.what == serial::Hello::otherFirmware, "an SP-1 with other firmware talks but never answers HELLO");
        CHECK (serial::explain (h, "COM5").contains ("Flash"), "and the person is told to flash sleepcousonique-sp1.bin");
    }
    {
        Device d (FAKE_SILENT);
        FakeWire w (d);
        auto h = serial::greet (w);
        CHECK (h.what == serial::Hello::silent, "a port that says nothing is silent");
    }
    {
        Device d (FAKE_ON);
        d.logLines = 400;                                   // a long stored log, with an old [STANDBY] line in it
        FakeWire w (d);
        auto h = serial::greet (w);
        CHECK (h.what == serial::Hello::sleepcousonique, "a long log replay (with an old STANDBY line in it) still ends in a HELLO");
    }
}

static void oneShot()
{
    std::printf ("\n-- serial::send (no live link: --no-link) --\n");
    juce::String port;
    {
        Device d (FAKE_ON);
        use ({ { "COM3", &d } });
        auto text = firstNight();
        auto err = serial::send (text, -1, port);
        CHECK (err.isEmpty(), "a dream goes to an SP-1 that is on: " + (err.isEmpty() ? juce::String ("OK") : err));
        CHECK (port == "COM3", "on its port");
        CHECK (same (*d.f, text) && d.f->loads == 1 && d.f->last_slot == 0, "and arrives byte for byte, into the playing slot");
    }
    {
        Device d (FAKE_ON);
        use ({ { "COM3", &d } });
        auto text = padded (SCQ_SLOT_TEXT);                  // the most a slot keeps
        auto err = serial::send (text, -1, port);
        CHECK (err.isEmpty() && same (*d.f, text), "the longest dream a slot keeps (" + juce::String (SCQ_SLOT_TEXT) + " bytes) arrives whole and is kept: " + err);
    }
    {
        Device d (FAKE_ON);
        use ({ { "COM3", &d } });
        auto err = serial::send (padded (SCQ_MAX_TEXT), -1, port);
        CHECK (err.contains ("no room"), "a dream too long for the SP-1 is refused with a reason: " + err);
    }
    {
        Device d (FAKE_ON, "1.2.0");
        d.f->old_mistakes = 3;
        use ({ { "COM3", &d } });
        auto err = serial::send (firstNight(), -1, port);
        CHECK (err.contains ("3 mistakes") && err.contains ("1.2.0") && err.contains ("Flash"),
               "an SP-1 on older firmware that can't read a newer dream: told to update it: " + err);
    }
    {
        Device d (FAKE_ON, "1.5.0");   // the firmware this editor comes with (SCQ_FIRMWARE_VERSION)
        d.f->old_mistakes = 1;
        use ({ { "COM3", &d } });
        auto err = serial::send (firstNight(), -1, port);
        CHECK (err.contains ("1 mistake") && ! err.contains ("Flash"), "the same firmware finding a mistake isn't blamed on its age: " + err);
    }
    {
        Device d (FAKE_STANDBY);
        use ({ { "/dev/cu.usbmodem101", &d } });
        auto err = serial::send (firstNight(), -1, port);
        CHECK (err.contains ("switched off") && d.f->dreams == 0, "sending to an SP-1 in standby says to turn it on: " + err);
    }
    {
        Device d (FAKE_STALL);
        use ({ { "COM3", &d } });
        auto t0 = now();
        auto err = serial::send (padded (6000), -1, port);
        auto took = now() - t0;
        CHECK (err.contains ("stopped taking"), "an SP-1 that stops reading halfway is reported: " + err);
        CHECK (took < (juce::uint32) (serial::timing.logMost + serial::timing.writeMost + 2000), "  and the send gives up in time (" + juce::String ((int) took) + " ms), never hangs");
    }
    {
        use ({});
        auto err = serial::send (firstNight(), -1, port);
        CHECK (err.contains ("No SP-1 is plugged in"), "nothing plugged in: " + err);
    }
    {
        Device other (FAKE_OTHER), sp1 (FAKE_ON);
        use ({ { "COM3", &other }, { "COM4", &sp1 } });
        auto err = serial::send (firstNight(), -1, port);
        CHECK (err.isEmpty() && port == "COM4" && sp1.f->loads == 1 && other.f->dreams == 0,
               "two serial devices: the one that is an SP-1 running Sleepcousonique gets the dream");
    }
    {
        Device off (FAKE_STANDBY), other (FAKE_OTHER);
        use ({ { "COM3", &other }, { "COM4", &off }, { "COM5", nullptr } });
        auto err = serial::send (firstNight(), -1, port);
        CHECK (err.contains ("switched off"), "an SP-1 that is off, beside other devices and a busy port: the most useful reason wins: " + err);
    }
    {
        use ({ { "COM5", nullptr } });
        auto err = serial::send (firstNight(), -1, port);
        CHECK (err.contains ("in use"), "a port another program holds: " + err);
    }
}

static bool waitFor (const std::function<bool()>& c, int ms)
{
    auto until = now() + (juce::uint32) ms;
    while (now() < until) { if (c()) return true; juce::Thread::sleep (10); }
    return c();
}

static void link()
{
    std::printf ("\n-- the live link (what the window's send to SP-1 uses) --\n");
    {
        Device d (FAKE_ON, "1.4.0");
        use ({ { "/dev/cu.usbmodem101", &d } });
        serial::Link l;
        CHECK (waitFor ([&] { return l.connected(); }, 4000), "the link finds an SP-1 that is on");
        CHECK (l.version() == "1.4.0" && l.port() == "/dev/cu.usbmodem101", "and knows its port and firmware");
        CHECK (waitFor ([&] { return l.hands().count > 3; }, 2000), "and follows its hands");
        auto h = l.hands();
        CHECK (h.fader[0] > 0.99f && h.scene == 2 && h.slot == 3 && h.buttons == 0x11 && h.lamp[0] > 0.99f, "faders, buttons, scene, slot and LEDs read right");
        auto text = padded (12000);
        auto t0 = now();
        auto err = l.send (text, -1);
        CHECK (err.isEmpty() && same (*d.f, text) && d.f->loads == 1, "a 12 KB dream goes over the link, byte for byte, while its hands keep coming (" + juce::String ((int) (now() - t0)) + " ms): " + err);
        auto before = l.hands().count;
        CHECK (waitFor ([&] { return l.hands().count > before + 3; }, 2000), "and the hands go on after it");
        err = l.send (firstNight(), 2);
        CHECK (err.isEmpty() && d.f->last_slot == 2 && d.f->slot_len[2] > 0, "a dream into another slot");
        {
            const juce::ScopedLock sl (d.lock);
            d.f->old_mistakes = 2;
        }
        err = l.send (firstNight(), -1);
        CHECK (err.contains ("2 mistakes"), "mistakes the SP-1 finds come back as words: " + err);
        {
            const juce::ScopedLock sl (d.lock);
            d.unplugged = true;
        }
        CHECK (waitFor ([&] { return ! l.connected(); }, 2000), "unplugged: the link lets go");
        err = l.send (firstNight(), -1, 200);
        CHECK (err.isNotEmpty(), "and a send then says why it can't: " + err);
    }
    {
        Device d (FAKE_STANDBY);
        use ({ { "/dev/cu.usbmodem101", &d } });
        serial::Link l;
        CHECK (waitFor ([&] { return l.note().contains ("switched off"); }, 6000), "the link sees an SP-1 that is off and says so: " + l.note());
        auto t0 = now();
        auto err = l.send (firstNight(), -1, 500);
        CHECK (err.contains ("switched off") && now() - t0 < 2000, "a send to it says to turn it on, at once");
        { const juce::ScopedLock sl (d.lock); d.f->mode = FAKE_ON; }     // someone holds ••
        CHECK (waitFor ([&] { return l.connected(); }, 8000), "switched on: the link finds it");
        err = l.send (firstNight(), -1, 500);
        CHECK (err.isEmpty() && d.f->loads == 1, "and the dream goes");
    }
    {
        Device d (FAKE_STALL);
        use ({ { "COM3", &d } });
        serial::Link l;
        CHECK (waitFor ([&] { return l.connected(); }, 4000), "a link to an SP-1 that will stop reading");
        auto t0 = now();
        auto err = l.send (padded (8000), -1);
        auto took = now() - t0;
        CHECK (err.contains ("stopped taking") && took < (juce::uint32) (serial::timing.writeMost + 2000),
               "a stuck SP-1 is reported in " + juce::String ((int) took) + " ms; the caller is never left hanging: " + err);
    }
    {
        // the window sends from a thread of its own: two sends at once must both come back
        Device d (FAKE_ON);
        use ({ { "COM3", &d } });
        serial::Link l;
        waitFor ([&] { return l.connected(); }, 4000);
        juce::String e1 = "?", e2;
        juce::WaitableEvent done;
        juce::Thread::launch ([&] { e1 = l.send (padded (3000), -1); done.signal(); });
        e2 = l.send (padded (4000), -1);
        CHECK (done.wait (10000) && e1.isEmpty() && e2.isEmpty() && d.f->loads == 2, "two sends at once: both arrive, one after the other (" + e1 + " / " + e2 + ")");
    }
}

// ------------------------------------------------------------------------------------------ the Wingie

static juce::String wingieDream()
{
    return "instrument wingie\n2026-11-24\n----------\nthe ring is comb 2\nmic.both -> ring\nkeys1.pitch -> ring.pitch\n"
           "fader2 -> ring.decay\nring -> out\nring -> lights.right\n";
}

static void wingie()
{
    std::printf ("\n-- the Wingie --\n");
    const juce::String wport = "/dev/cu.usbserial-1410";
    {
        // what the Wingie says, word by word
        serial::Link::Measured m;
        CHECK (serial::Link::parseReport ("SCQ engine avg=12% max=20%  places=7 scene=1 slot=1  load=12.3 peak=20.1 dropped=0 of 750 "
                                          "mem=12000/126976 core1=4% heap=123456 inL=-42 inR=-40 outL=-18 outR=-19", m)
               && m.avg == 12 && m.peak == 20 && std::abs (m.load - 12.3f) < 0.01f && std::abs (m.loadPeak - 20.1f) < 0.01f
               && m.dropped == 0 && m.blocks == 750 && m.memUsed == 12000 && m.memTotal == 126976 && m.core1 == 4
               && m.heap == 123456 && m.inL == -42 && m.inR == -40 && m.outL == -18 && m.outR == -19 && ! m.over,
               "a Wingie's report: CPU, load, dropped blocks, pool, control core, heap and the four levels");
        serial::Link::Measured s;
        CHECK (serial::Link::parseReport ("SCQ engine avg=41% max=57%  places=12 scene=1 slot=1  load=41.0 peak=57.0 dropped=3 of 7500 mem=20000/126976", s)
               && s.avg == 41 && s.peak == 57 && s.dropped == 3 && s.over && s.core1 == -1 && s.heap == -1 && s.inL == -100,
               "an SP-1's report reads as before (no Wingie extras; dropped blocks are too heavy)");
        serial::Link::WHands h;
        CHECK (serial::Link::parseWHands ("SCQ WHANDS 4095 0 2048 a05 fff 3 -1 1 1 2 12 00 ff", h)
               && h.fader[0] > 0.99f && h.fader[1] == 0.0f && std::abs (h.fader[2] - 0.5f) < 0.01f && h.keys[0] == 0xa05
               && h.keys[1] == 0xfff && h.mode == 3 && h.octave[0] == -1 && h.octave[1] == 1 && h.source == 1
               && h.scene == 2 && h.slot == 12 && h.lamp[0] == 0.0f && h.lamp[1] > 0.99f,
               "a Wingie's hands: three faders, both keyboards, Mode, octaves, source, scene, slot and its two lights");
        CHECK (! serial::Link::parseWHands ("SCQ WHANDS 4095 0", h), "  and a cut-off line is ignored");
    }
    {
        Device d (FAKE_ON, "0.1.0", true);
        FakeWire w (d);
        auto h = serial::greet (w);
        CHECK (h.what == serial::Hello::sleepcousonique && h.device == serial::wingie && h.version == "0.1.0",
               "a Wingie that has just reset (its port was opened) answers a later HELLO as a Wingie (" + h.version + ")");
    }
    juce::String port;
    {
        Device d (FAKE_ON, "0.1.0", true);
        use ({ { wport, &d } });
        auto err = serial::send (wingieDream(), 9, port, 1);
        CHECK (err.isEmpty() && same (*d.f, wingieDream()) && d.f->last_slot == 9 && port == wport,
               "a dream for the Wingie goes to a Wingie, into slot 10 of its 12: " + err);
        err = serial::send (firstNight(), -1, port, 0);
        CHECK (err.contains ("for the SP-1") && err.contains ("is a Wingie") && d.f->dreams == 1,
               "a dream for the SP-1 isn't sent to the Wingie, and says why: " + err);
        err = serial::send (firstNight(), -1, port);
        CHECK (err.contains ("No SP-1") && d.f->dreams == 1, "a send for whichever answers (the command line's --send) leaves the Wingie alone: " + err);
        err = serial::describeReply ("ERR 1 mistakes", "0.1.0", serial::wingie);
        CHECK (err.contains ("Wingie found 1 mistake") && err.contains ("SP-1"), "a dream the Wingie refuses comes back in words: " + err);
        err = serial::send (wingieDream(), -1, port, 1);
        CHECK (err.isEmpty(), "and still takes the next dream for it");
    }
    {
        Device d (FAKE_ON, "1.4.0");
        use ({ { "/dev/cu.usbmodem101", &d } });
        auto err = serial::send (wingieDream(), -1, port, 1);
        CHECK (err.contains ("for the Wingie") && err.contains ("is an SP-1") && d.f->dreams == 0,
               "a dream for the Wingie isn't sent to an SP-1: " + err);
        CHECK (serial::describeReply ("ERR 1 mistakes", "1.4.0").contains ("SP-1 found 1 mistake"), "the SP-1's own words are unchanged");
    }
    {
        Device w (FAKE_ON, "0.1.0", true);
        use ({ { wport, &w } });
        serial::Link sp1only;                    // the Array's kind of link: never opens a Wingie's port (it would reset it)
        juce::Thread::sleep (1200);
        CHECK (! sp1only.connected() && w.opens == 0, "a link for SP-1s only leaves the Wingie's port alone");
    }
    {
        Device d (FAKE_ON, "0.1.0", true);
        use ({ { wport, &d } });
        serial::Link l (true);
        CHECK (waitFor ([&] { return l.connected(); }, 5000), "the editor's link finds a Wingie");
        CHECK (l.device() == serial::wingie && l.version() == "0.1.0" && l.port() == wport, "and knows it is a Wingie");
        CHECK (waitFor ([&] { return l.whands().count > 3; }, 2000), "and follows its hands (SCQ WHANDS)");
        auto h = l.whands();
        CHECK (h.fader[0] > 0.99f && h.keys[0] == 0x011 && h.keys[1] == 0x800 && h.mode == 1 && h.octave[0] == -1
               && h.octave[1] == 1 && h.source == 0 && h.scene == 2 && h.lamp[1] > 0.99f, "faders, keys, Mode, octaves, scene and lights read right");
        CHECK (waitFor ([&] { return l.measured().count >= 2; }, 3500), "its report comes every second");
        auto m = l.measured();
        CHECK (m.avg >= 30 && m.peak > m.avg && m.core1 >= 4 && m.heap > 100000 && m.memTotal == 126976 && m.inL > -99 && m.outR == -19,
               "and the link reads its CPU, control core, heap, pool and levels");
        auto err = l.send (wingieDream(), 7, 0, 1);
        CHECK (err.isEmpty() && d.f->last_slot == 7 && same (*d.f, wingieDream()), "a dream for the Wingie goes over the link into slot 8: " + err);
        CHECK (waitFor ([&] { return l.whands().slot == 8; }, 2000), "and its hands then say slot 8");
        err = l.send (firstNight(), -1, 0, 0);
        CHECK (err.contains ("is a Wingie") && d.f->dreams == 1, "a dream for the SP-1 isn't sent over a Wingie's link: " + err);
    }
    {
        Device other (FAKE_OTHER, "0.0.0", true);
        use ({ { wport, &other } });
        serial::Link l (true);
        CHECK (waitFor ([&] { return l.note().contains ("not as Sleepcousonique"); }, 6000), "a CH340 device with other firmware is named: " + l.note());
        juce::Thread::sleep (3500);
        CHECK (other.opens == 1, "and isn't opened (reset) again and again (" + juce::String (other.opens) + " opening)");
    }
    {
        Device sp (FAKE_ON, "1.4.0"), w (FAKE_ON, "0.1.0", true);
        use ({ { "/dev/cu.usbmodem101", &sp }, { wport, &w } });
        serial::Link l (true);
        CHECK (waitFor ([&] { return l.connected(); }, 5000) && l.device() == serial::sp1, "an SP-1 and a Wingie: the SP-1 comes first, as before");
        auto err = l.send (wingieDream(), -1, 0, 1);
        CHECK (err.contains ("is an SP-1") && sp.f->dreams == 0, "and a Wingie dream isn't sent to it: " + err);
        err = l.send (firstNight(), -1, 0, 0);
        CHECK (err.isEmpty() && sp.f->loads == 1, "an SP-1 dream is");
    }
}

#if JUCE_MAC || JUCE_LINUX
// the same pretend SP-1 behind a pseudo-terminal: the editor opens it as it would /dev/cu.usbmodem101
struct Pty : juce::Thread
{
    Device d;
    int m = -1, keep = -1;
    juce::String name;
    explicit Pty (int mode) : juce::Thread ("pty"), d (mode)
    {
        m = posix_openpt (O_RDWR | O_NOCTTY);
        grantpt (m); unlockpt (m);
        name = ptsname (m);
        keep = ::open (name.toRawUTF8(), O_RDWR | O_NOCTTY);
        termios t {}; tcgetattr (keep, &t); cfmakeraw (&t); tcsetattr (keep, TCSANOW, &t);
        fcntl (m, F_SETFL, fcntl (m, F_GETFL) | O_NONBLOCK);
        { const juce::ScopedLock sl (d.lock); fake_open (d.f.get(), 30); }
        startThread();
    }
    ~Pty() override { stopThread (2000); ::close (keep); ::close (m); }
    void run() override
    {
        char in[2048], out[1024]; int inN = 0, outN = 0;
        while (! threadShouldExit())
        {
            pollfd p { m, POLLIN, 0 }; poll (&p, 1, 5);
            const juce::ScopedLock sl (d.lock);
            d.tick();
            if (inN == 0) { auto n = ::read (m, in, sizeof in); if (n > 0) inN = (int) n; }
            if (inN > 0) { int k = fake_feed (d.f.get(), in, inN); std::memmove (in, in + k, (size_t) (inN - k)); inN -= k; }
            if (outN == 0) outN = fake_take (d.f.get(), out, sizeof out);
            if (outN > 0) { auto w = ::write (m, out, (size_t) outN); if (w > 0) { std::memmove (out, out + w, (size_t) (outN - w)); outN -= (int) w; } }
        }
    }
};

static void pty()
{
    std::printf ("\n-- through a real pseudo-terminal (the port code itself) --\n");
    serial::setTestPorts ({}, nullptr);
    {
        Pty p (FAKE_ON);
        setenv ("SCQ_PORTS", p.name.toRawUTF8(), 1);
        juce::String port;
        auto text = padded (SCQ_SLOT_TEXT);
        auto err = serial::send (text, -1, port);
        bool ok; { const juce::ScopedLock sl (p.d.lock); ok = same (*p.d.f, text); }
        CHECK (err.isEmpty() && ok && port == p.name, "serial::send over a pty: the longest dream, byte for byte: " + err);
        serial::Link l;
        CHECK (waitFor ([&] { return l.connected(); }, 4000), "the link opens the pty and greets it");
        CHECK (waitFor ([&] { return l.hands().count > 3; }, 2000), "hands over the pty");
        text = padded (9000);
        err = l.send (text, -1);
        { const juce::ScopedLock sl (p.d.lock); ok = same (*p.d.f, text); }
        CHECK (err.isEmpty() && ok, "the link sends over the pty while the hands stream: " + err);
    }
    {
        Pty p (FAKE_STANDBY);
        setenv ("SCQ_PORTS", p.name.toRawUTF8(), 1);
        juce::String port;
        auto err = serial::send (firstNight(), -1, port);
        CHECK (err.contains ("switched off"), "an SP-1 in standby behind a pty: " + err);
    }
    {
        Pty p (FAKE_STALL);
        setenv ("SCQ_PORTS", p.name.toRawUTF8(), 1);
        juce::String port;
        auto t0 = now();
        auto err = serial::send (padded (12000), -1, port);
        CHECK (err.contains ("stopped taking") && now() - t0 < (juce::uint32) (serial::timing.logMost + serial::timing.writeMost + 2500),
               "a stuck SP-1 behind a pty: the write gives up instead of spinning forever: " + err);
    }
    unsetenv ("SCQ_PORTS");
}
#endif

int main (int argc, char** argv)
{
    serial::Link::enabled = true;
    // shorter waits than the real ones (which allow for a slow USB stack and a long log)
    serial::timing.logQuiet = 150;
    serial::timing.logMost = 1500;
    serial::timing.helloWait = 400;
    serial::timing.writeMost = 2000;
    serial::timing.replyWait = 3000;

    if (argc > 1 && juce::String (argv[1]) == "--ports")
    {
        // what this computer would try, the real way (Windows: the registry; macOS: /dev/cu.usbmodem*)
        for (auto& p : serial::candidates()) std::printf ("%s\n", p.toRawUTF8());
        return 0;
    }
    if (argc > 2 && juce::String (argv[1]) == "--hello")
    {
        // ScqSerialTest --hello PORT: greet a real SP-1 the way the editor does (read-only)
        serial::timing = serial::Timing();
        auto w = serial::open (argv[2]);
        if (w == nullptr) { std::printf ("can't open %s\n", argv[2]); return 1; }
        auto t0 = now();
        auto h = serial::greet (*w);
        static const char* const what[] = { "sleepcousonique", "standby", "other firmware", "silent" };
        std::printf ("%s: %s %s (%u ms)\n%s\n", argv[2], what[h.what], h.version.toRawUTF8(), now() - t0,
                     serial::explain (h, argv[2]).toRawUTF8());
        return h.what == serial::Hello::sleepcousonique ? 0 : 1;
    }
    if (argc > 4 && juce::String (argv[1]) == "--ask")
    {
        // ScqSerialTest --ask PORT "SCQ WHO" "SCQ ME": one command, one reply (what the Array does)
        auto r = serial::ask (argv[2], argv[3], { argv[4] }, 3000);
        std::printf ("%s\n", r.isEmpty() ? "(no answer)" : r.toRawUTF8());
        return r.isEmpty() ? 1 : 0;
    }
    protocol();
    oneShot();
    link();
    wingie();
   #if JUCE_MAC || JUCE_LINUX
    pty();
   #endif
    serial::setTestPorts ({}, nullptr);
    auto real = serial::candidates();                        // must not crash, whatever is (or isn't) plugged in
    std::printf ("\n(this computer's candidate ports: %s)\n", real.isEmpty() ? "none" : real.joinIntoString (", ").toRawUTF8());
    std::printf (failures ? "\n%d FAILED\n" : "\nall good\n", failures);
    return failures ? 1 : 0;
}
