#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Host.h"

// The SP-1's hands, on screen: four faders, four track buttons, the rocker, PLAY, the dots,
// the eight LEDs and the four stem slots. Drag a fader's copper loop onto an inlet to map it.
// Keys: 1-4 doors, up/down the rocker, space PLAY, period the dots (when the journal isn't typing).
class Hands : public juce::Component, private juce::Timer
{
public:
    explicit Hands (Host&);
    std::function<bool()> typing;   // true while the journal has the keyboard
    std::function<void (const juce::String&)> onStatus;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void set (int ctl, float v);
    struct Item { juce::String name; int ctl; juce::Rectangle<float> r, loop; bool fader = false; float value = 0; bool mouse = false; };
    int itemAt (juce::Point<float>) const;
    int loopAt (juce::Point<float>) const;

    Host& host;
    std::vector<Item> items;
    std::array<juce::Rectangle<float>, 4> stemRects;
    std::array<juce::Rectangle<float>, 8> leds;
    int dragging = -1;
    bool keyHeld[8] {};
    std::unique_ptr<juce::FileChooser> chooser;
};
