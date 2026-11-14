#include "Fleet.h"
#include <cstring>

using namespace juce;

static bool isSp1 (const String& name) { return name.startsWith ("SP-1 SCQ"); }

Fleet::Fleet()
{
    for (auto& v : lastCtl) v = -1000.0f;
    startTimer (1500);
    timerCallback();
}

Fleet::~Fleet()
{
    stopTimer();
    const ScopedLock sl (lock);
    for (auto& s : sp1s) if (s->in) s->in->stop();
}

std::vector<Fleet::Info> Fleet::list() const
{
    const ScopedLock sl (lock);
    std::vector<Info> out;
    for (auto& s : sp1s) out.push_back ({ s->id, s->name, s->version, s->answered, s->on, s->last });
    return out;
}

void Fleet::setOn (const String& id, bool on)
{
    { const ScopedLock sl (lock); for (auto& s : sp1s) if (s->id == id) s->on = on; }
    changed();
}

int Fleet::playing() const
{
    const ScopedLock sl (lock);
    int n = 0;
    for (auto& s : sp1s) if (s->answered && s->on) ++n;
    return n;
}

void Fleet::changed()
{
    MessageManager::callAsync ([this] { if (onChange) onChange(); });
}

// find SP-1s as they appear (paired and connected), let go of the ones that went, and say hello
void Fleet::timerCallback()
{
    auto outs = MidiOutput::getAvailableDevices();
    auto ins = MidiInput::getAvailableDevices();
    bool any = false;
    const ScopedLock sl (lock);
    for (auto it = sp1s.begin(); it != sp1s.end();)
    {
        bool still = false;
        for (auto& d : outs) if (d.identifier == (*it)->id) still = true;
        if (! still) { if ((*it)->in) (*it)->in->stop(); it = sp1s.erase (it); any = true; }
        else ++it;
    }
    for (auto& d : outs)
    {
        if (! isSp1 (d.name)) continue;
        bool known = false;
        for (auto& s : sp1s) if (s->id == d.identifier) known = true;
        if (known) continue;
        auto s = std::make_unique<Sp1>();
        s->id = d.identifier;
        s->name = d.name;
        s->out = MidiOutput::openDevice (d.identifier);
        for (auto& i : ins)
            if (i.name == d.name)
            {
                s->in = MidiInput::openDevice (i.identifier, this);
                if (s->in) s->in->start();
            }
        if (s->out) { sp1s.push_back (std::move (s)); any = true; }
    }
    auto now = Time::getMillisecondCounter();
    for (auto& s : sp1s)
        if (! s->answered && now - s->pinged > 1400)
        {
            s->pinged = now;
            const uint8 hello[] = { 0x7D, 'S', 'Q', 'H' };
            s->out->sendMessageNow (MidiMessage::createSysExMessage (hello, 4));
        }
    if (any) changed();
}

void Fleet::handleIncomingMidiMessage (MidiInput* source, const MidiMessage& m)
{
    if (! m.isSysEx()) return;
    auto* d = m.getSysExData();
    int n = m.getSysExDataSize();
    if (n < 4 || d[0] != 0x7D || d[1] != 'S' || d[2] != 'Q') return;
    String text = String::fromUTF8 ((const char*) d + 4, n - 4);
    {
        const ScopedLock sl (lock);
        for (auto& s : sp1s)
        {
            if (s->in.get() != source) continue;
            switch (d[3])
            {
                case 'h': s->answered = true; s->version = text.fromFirstOccurrenceOf ("SLEEPCOUSONIQUE ", false, false).upToFirstOccurrenceOf (" ", false, false); break;
                case 'o': s->answered = true; s->last = "ok"; break;
                case 'x': s->last = text; break;
                default: break;
            }
        }
    }
    changed();
}

void Fleet::sysex (const std::vector<uint8>& payload, bool onlyAnswered)
{
    auto msg = MidiMessage::createSysExMessage (payload.data(), (int) payload.size());
    const ScopedLock sl (lock);
    for (auto& s : sp1s)
        if (s->on && (s->answered || ! onlyAnswered) && s->out) s->out->sendMessageNow (msg);
}

void Fleet::sendDream (const String& text, int slot)
{
    if (playing() == 0) return;
    auto utf8 = text.toStdString();
    const int len = (int) utf8.size();
    sysex ({ 0x7D, 'S', 'Q', 'B', (uint8) (len & 0x7f), (uint8) ((len >> 7) & 0x7f), (uint8) ((len >> 14) & 0x7f), (uint8) jlimit (0, 127, slot) });
    for (int i = 0; i < len; i += 100)                 // 100 bytes a message, as nibbles
    {
        std::vector<uint8> p { 0x7D, 'S', 'Q', 'D' };
        for (int j = i; j < jmin (len, i + 100); ++j)
        {
            auto b = (uint8) utf8[(size_t) j];
            p.push_back ((uint8) (b >> 4));
            p.push_back ((uint8) (b & 0x0f));
        }
        sysex (p);
    }
    sysex ({ 0x7D, 'S', 'Q', 'E' });
}

void Fleet::set (char kind, int a, int b, float v)
{
    if (playing() == 0) return;
    uint32 u; std::memcpy (&u, &v, 4);
    sysex ({ 0x7D, 'S', 'Q', 'S', (uint8) kind, (uint8) (a & 0x7f), (uint8) ((a >> 7) & 0x7f), (uint8) (b & 0x7f),
             (uint8) (u & 0x7f), (uint8) ((u >> 7) & 0x7f), (uint8) ((u >> 14) & 0x7f), (uint8) ((u >> 21) & 0x7f), (uint8) ((u >> 28) & 0x0f) });
}

void Fleet::control (int ctl, float v)
{
    if (ctl < 0 || ctl >= 16 || std::abs (lastCtl[ctl] - v) < 0.004f) return;   // only what moved
    lastCtl[ctl] = v;
    set ('c', 0, ctl, v);
}

void Fleet::gotoScene (int scene)
{
    if (playing() == 0) return;
    sysex ({ 0x7D, 'S', 'Q', 'G', (uint8) (scene & 0x7f), (uint8) ((scene >> 7) & 0x7f) });
}
