#pragma once
#include <juce_core/juce_core.h>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

// Radio as weather: the Bluetooth devices near the computer, for the crowd and visitors places.
//
// Room keeps count of who has been heard lately (it knows nothing of radios: it is handed each
// call as an address-number, a signal strength and a time, so it can be tested with a made-up
// room). Radio listens with the computer's Bluetooth and fills a Room. Nothing is stored or
// sent anywhere: a device is a 64-bit number made from its identifier, kept while it is near.
namespace radio
{

class Room
{
public:
    // how long a device may stay quiet before it has left (phones call out every second or
    // two at the slowest, so this keeps the count steady without holding on to the gone)
    double forgetAfter = 8.0;

    struct Event { int kind; float voice; };   // 0 a call heard, 1 a device arrived, 2 one left (as scq_room_event)
    struct Weather { int count = 0; float near = 0, strength = 0; };

    // one call heard: who (any stable number for the device), how strong (dBm, 127 = unknown), when (seconds)
    void heard (juce::uint64 who, int rssi, double now);
    // forget whoever has gone quiet; call a few times a second
    void tick (double now);
    Weather weather() const;
    // the events since the last call, oldest first (at most a few hundred are kept)
    std::vector<Event> takeEvents();
    void clear();

    // -100 dBm (the edge of hearing) is 0, -40 dBm (within arm's reach) is 1
    static float nearness (float rssi) { return juce::jlimit (0.0f, 1.0f, (rssi + 100.0f) / 60.0f); }
    // a device's own number, 0 to 1, from its address-number
    static float voiceOf (juce::uint64 who) { return (float) ((who * 0x9E3779B97F4A7C15ull) >> 40) / 16777216.0f; }

private:
    struct Device { juce::uint64 who; float rssi; double last; };
    std::vector<Device> devices;
    std::vector<Event> events;
    mutable std::mutex lock;
};

// What the listening is doing, for the status line.
enum class State { off, asking, listening, bluetoothOff, notAllowed, noRadio };

class Radio
{
public:
    Radio();
    ~Radio();

    // Start (or stop) listening. Nothing touches Bluetooth until this is first set: macOS asks
    // for permission the first time, and a dream without a crowd never needs to ask.
    void setWanted (bool);
    bool wanted() const { return want; }
    State state() const;
    juce::String describe() const;       // one line for the status bar ("the room: 7 devices nearby")

    Room room;

    // For tests and for composing without a radio: pretend `n` devices are here (0 stops
    // pretending). They call out like real ones, from the message thread's timer.
    void pretend (int n);
    void pretendTick (double now);
    bool pretending() const { return fake > 0; }

private:
    struct Native;
    std::unique_ptr<Native> native;
    bool want = false;
    int fake = 0;
    juce::Random rng { 0x5c9 };
};

}
