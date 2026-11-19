#include "Radio.h"

namespace radio
{

void Room::heard (juce::uint64 who, int rssi, double now)
{
    const std::lock_guard<std::mutex> g (lock);
    const bool known = rssi != 127 && rssi < 20 && rssi > -128;      // 127: the radio couldn't tell
    Device* d = nullptr;
    for (auto& x : devices) if (x.who == who) { d = &x; break; }
    if (d == nullptr)
    {
        if (devices.size() >= 2000) return;                          // a stadium: enough
        devices.push_back ({ who, known ? (float) rssi : -90.0f, now });
        if (events.size() < 512) events.push_back ({ 1, voiceOf (who) });
    }
    else
    {
        d->last = now;
        if (known) d->rssi += 0.3f * ((float) rssi - d->rssi);       // signals flutter: lean towards the new one
    }
    if (events.size() < 512) events.push_back ({ 0, voiceOf (who) });
}

void Room::tick (double now)
{
    const std::lock_guard<std::mutex> g (lock);
    for (size_t i = devices.size(); i-- > 0;)
        if (now - devices[i].last > forgetAfter)
        {
            if (events.size() < 512) events.push_back ({ 2, voiceOf (devices[i].who) });
            devices.erase (devices.begin() + (std::ptrdiff_t) i);
        }
}

Room::Weather Room::weather() const
{
    const std::lock_guard<std::mutex> g (lock);
    Weather w;
    w.count = (int) devices.size();
    float sum = 0;
    for (auto& d : devices)
    {
        const float n = nearness (d.rssi);
        w.near = juce::jmax (w.near, n);
        sum += n;
    }
    if (w.count > 0) w.strength = sum / (float) w.count;
    return w;
}

std::vector<Room::Event> Room::takeEvents()
{
    const std::lock_guard<std::mutex> g (lock);
    std::vector<Event> out;
    out.swap (events);
    return out;
}

void Room::clear()
{
    const std::lock_guard<std::mutex> g (lock);
    devices.clear();
    events.clear();
}

void Radio::pretend (int n)
{
    fake = juce::jlimit (0, 1000, n);
    if (fake == 0) room.clear();
}

// made-up devices: each calls out about once a second, at its own distance
void Radio::pretendTick (double now)
{
    for (int k = 0; k < fake; ++k)
        if (rng.nextFloat() < 0.25f)
            room.heard (0xFA4E0000ull + (juce::uint64) k, -45 - (k * 37) % 50 + rng.nextInt (5), now);
}

juce::String Radio::describe() const
{
    if (fake > 0) { auto w = room.weather(); return "the room (pretending): " + juce::String (w.count) + (w.count == 1 ? " device" : " devices"); }
    switch (state())
    {
        case State::off:          return {};
        case State::asking:       return "the room: waiting for Bluetooth (macOS may be asking you to allow it)";
        case State::bluetoothOff: return "the room: Bluetooth is off, so the crowd is at rest";
        case State::notAllowed:   return "the room: Bluetooth isn't allowed for Sleepcousonique (System Settings > Privacy & Security > Bluetooth), so the crowd is at rest";
        case State::noRadio:      return "the room: no Bluetooth radio to listen with here, so the crowd is at rest";
        case State::listening:    break;
    }
    auto w = room.weather();
    return "the room: " + juce::String (w.count) + (w.count == 1 ? " device nearby" : " devices nearby")
         + (w.count > 0 ? ", the nearest at " + juce::String (juce::roundToInt (w.near * 100.0f)) + "%" : juce::String());
}

#if ! JUCE_MAC
// No listening here yet (Windows, Linux): the crowd holds its rest= count, or pretend.
struct Radio::Native {};
Radio::Radio() {}
Radio::~Radio() {}
void Radio::setWanted (bool w) { want = w; }
State Radio::state() const { return want ? State::noRadio : State::off; }
#endif

}
