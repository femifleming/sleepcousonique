#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Radio.h"
#include <atomic>

// Runs the C engine on the computer's sound card, exactly as the SP-1 will run it.
class Host : private juce::AudioIODeviceCallback, private juce::MidiInputCallback, private juce::Timer
{
public:
    Host();
    ~Host() override;

    struct Result { juce::String describe, messages; int errors = 0; std::vector<float> sceneLoads; };
    Result load (const juce::String& text, juce::uint32 nights);   // keeps the scene that's playing
    Result describe();
    Result check (const juce::String& text, juce::uint32 nights);   // doesn't disturb the sound
    void setAmount (int cord, float amount);
    void setTune (int node, float octaves);
    void setAtt (int node, int inlet, float amount);
    void setParam (int node, int k, float value);
    void setKnob (int node, int inlet, float value);
    void gotoScene (int);
    int scene();
    void control (int which, float value);
    float lamp (int led);
    float probe (int node, int port);


    juce::AudioDeviceManager devices;
    float level = 0;   // output meter
    float levelR = 0;  // the right side's

    // The Wingie's microphones, played by the computer's: while a dream for the Wingie plays and
    // listening is on, the audio input feeds the mic place (scq_input), a mono input on both
    // sides. Off at the start (a microphone near the speakers feeds back): setListening opens the
    // input device (the one chosen in the sound settings, else the computer's microphone) and
    // closes it again.
    void setInstrument (int i) { instrument = i; }
    juce::String setListening (bool on);       // "" or why it couldn't
    bool listening() const { return listen.load(); }
    juce::String inputName() const;            // the input device listened to ("" when not listening)
    float inLevel[2] {};                       // input peaks, 0..1
    bool inputAllowed = true;                  // false (--shot): never opens an input
    juce::StringArray midiInputs() const;   // names of the MIDI inputs the dream hears

    // The room (the crowd and visitors places): the Bluetooth devices near the computer. The
    // radio is only listened to while the dream that's loaded has one of those places.
    radio::Radio radio;
    juce::String roomNote() const { return radio.wanted() || radio.pretending() ? radio.describe() : juce::String(); }
    juce::uint32 healed() const;            // places the engine has had to start again
    bool roomAllowed = true;                // false: never touch Bluetooth (a test mode)


private:
    void audioDeviceIOCallbackWithContext (const float* const*, int, float* const* out, int numOut, int frames,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override {}

    // MIDI in: every input is heard (midi / midicc / midiclock places); bytes cross to the
    // audio thread through a lock-free ring and reach the engine just before it renders
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void openMidiInputs();
    juce::MidiDeviceListConnection midiList;
    // the room crosses to the audio thread the same way: the weather as plain numbers, each
    // arrival, leaving and call through a ring (one a block, so every call is its own pulse)
    void timerCallback() override;
    std::atomic<int> roomLive { 0 }, roomCount { 0 };
    std::atomic<float> roomNear { 0 }, roomStrength { 0 };
    juce::AbstractFifo roomFifo { 512 };
    std::vector<radio::Room::Event> roomRing = std::vector<radio::Room::Event> (512);

    juce::AbstractFifo midiFifo { 4096 };
    std::vector<juce::uint8> midiRing = std::vector<juce::uint8> (4096);

    void noteRoom (const juce::String& described);
    bool wantRoom = false;                  // the scene that plays has a crowd or visitors
    juce::CriticalSection lock;
    double rate = 48000;
    juce::LagrangeInterpolator interp[2];
    std::vector<float> fifo[2];
    // sound in: at the device's rate (raw), then at the engine's 48 kHz, a block at a time
    std::atomic<bool> listen { false };
    std::atomic<int> instrument { 0 };
    juce::LagrangeInterpolator inInterp[2];
    std::vector<float> inRaw[2], in48[2];
    juce::String inputWanted;                  // the input chosen last, opened again when listening starts
};
