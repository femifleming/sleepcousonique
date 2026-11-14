#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>
#include <map>

// Every SP-1 in Bluetooth mode the computer can see, as one fleet: the dream you're patching
// goes to all of them, and every live change (cords, knobs, settings, the hands, scenes)
// goes to all of them as you make it. Each SP-1 is a Bluetooth MIDI port called
// "SP-1 SCQ xxxx"; the messages are SysEx F0 7D 'S' 'Q' ... F7 (see firmware sp1_scq.c).
class Fleet : private juce::MultiTimer, private juce::MidiInputCallback
{
public:
    Fleet();
    ~Fleet() override;

    struct Info { juce::String id, name, version; bool answered = false, on = true; juce::String last; };
    std::vector<Info> list() const;
    void setOn (const juce::String& id, bool on);
    int playing() const;                               // SP-1s that answered and are on

    bool live = true;                                  // every good edit goes out at once (unsaved)
    std::function<void()> onChange;                    // the list or a status changed (message thread)

    // a dream: slot 0-4 keeps it, 126 keeps it in the slot that's playing, 127 plays it unsaved
    void sendDream (const juce::String& text, int slot);
    void setAmount (int cord, float v)              { set ('a', cord, 0, v); }
    void setTune (int node, float v)                { set ('t', node, 0, v); }
    void setAtt (int node, int inlet, float v)      { set ('v', node, inlet, v); }
    void setParam (int node, int k, float v)        { set ('p', node, k, v); }
    void setKnob (int node, int inlet, float v)     { set ('k', node, inlet, v); }
    void control (int ctl, float v);
    void gotoScene (int scene);

private:
    struct Sp1
    {
        juce::String id, name, version, last;
        std::unique_ptr<juce::MidiOutput> out;
        std::unique_ptr<juce::MidiInput> in;
        bool answered = false, on = true;
        juce::uint32 pinged = 0;
    };
    void timerCallback (int id) override;           // 1: look for SP-1s, 2: send what changed
    void scan();
    void flush();
    std::map<juce::int64, float> pending;            // (kind, a, b) -> the latest value, sent ~20x a second
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void set (char kind, int a, int b, float v);
    void sysex (const std::vector<juce::uint8>& payload, bool onlyAnswered = true);
    void changed();

    mutable juce::CriticalSection lock;
    std::vector<std::unique_ptr<Sp1>> sp1s;
    float lastCtl[16];
};
