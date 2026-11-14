#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Host.h"
#include "Serial.h"

// The SP-1's hands, on screen: four faders, four track buttons, the rocker, PLAY, the dots,
// the eight LEDs, and a list of what each of them is patched to in the scene that's playing.
// Drag a fader's copper loop onto an inlet to map it.
// Keys: 1-4 doors, up/down the rocker, space PLAY, period the dots (when the journal isn't typing).
// The dots + the rocker change scene, as on the SP-1, and then the rocker never reaches lift.
class Hands : public juce::Component, private juce::Timer
{
public:
    explicit Hands (Host&);
    serial::Link* link = nullptr;   // when set and connected, the SP-1's own hands drive this panel
    std::function<bool()> typing;   // true while the journal has the keyboard
    std::function<void (const juce::String&)> onStatus;
    std::function<juce::StringArray (const juce::String& control)> mappingFor;   // where a control goes
    std::function<void (const juce::String& control)> onShowControl;            // click a mapping

    void paint (juce::Graphics&) override;
    void resized() override;
    void resizedPop();
    void showFader (int i, float v) { if (i >= 0 && i < 4) { items[(size_t) i].value = v; repaint(); } }   // a snapshot moved it
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
    juce::Rectangle<float> mapArea;
    std::vector<std::pair<juce::String, juce::Rectangle<float>>> mapRows;
    std::array<juce::Rectangle<float>, 8> leds;
    int dragging = -1;
    bool keyHeld[8] {};
    bool rockerTaken = false;
    juce::uint32 hwCount = 0, hwButtons = 0;
    float hwFader[4] { -1, -1, -1, -1 };
    int hwScene = -1;
    float hwLamp[8] {};
    bool live = false;               // following a connected SP-1 right now     // •• + rocker went to the scenes, not to lift
    bool lucidHeld() const;
    void sceneStep (int dir);
};
