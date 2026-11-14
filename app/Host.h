#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

// Runs the C engine on the computer's sound card, exactly as the SP-1 will run it.
class Fleet;

class Host : private juce::AudioIODeviceCallback
{
public:
    Fleet* fleet = nullptr;          // SP-1s over Bluetooth: every live change goes to them too
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
    void gotoScene (int);
    int scene();
    void control (int which, float value);
    float lamp (int led);
    float probe (int node, int port);


    juce::AudioDeviceManager devices;
    float level = 0;   // output meter

private:
    void audioDeviceIOCallbackWithContext (const float* const*, int, float* const* out, int numOut, int frames,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override {}

    juce::CriticalSection lock;
    double rate = 48000;
    juce::LagrangeInterpolator interp[2];
    std::vector<float> fifo[2];
};
