#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Host.h"
#include "Serial.h"
#include <deque>
#include <map>

// The Wingie's lights: always red underneath; purple, yellow and white come in as v climbs to 1.
juce::Colour wingieLamp (float v);

// The Wingie's hands, on screen, in place of the SP-1's when the dream is for the Wingie: two
// 12-key keyboards (C..B, held while pressed), each with its Mode button and 3-position octave
// switch, the Mix / Decay / Volume faders, the mic / line source switch and the two lights; the
// computer's microphone as the Wingie's (listen), and what each hand is patched to.
// Every control turns scq_control in the simulation; a connected Wingie's own hands
// (SCQ WHANDS) move them, as the SP-1's panel follows SCQ HANDS.
// Keys (while the journal isn't typing): left keyboard z s x d c v g b h n j m, right
// q 2 w 3 e r 5 t 6 y 7 u (as a piano), a and p the Mode buttons.
class WingiePanel : public juce::Component, private juce::Timer
{
public:
    explicit WingiePanel (Host&);
    serial::Link* link = nullptr;
    std::function<bool()> typing;
    std::function<void (const juce::String&)> onStatus;
    std::function<juce::StringArray (const juce::String& control)> mappingFor;
    std::function<void (const juce::String& control)> onShowControl;
    std::function<void (bool on)> onListen;          // the listen button: open / close the computer's input

    void reset();                                    // the dream changed instrument: send every control's value again
    void showFader (int i, float v) { if (i >= 0 && i < 3) { fader[i] = v; repaint(); } }
    bool live() const { return following; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void setKey (int side, int k, bool down, int source);    // source: 0 mouse, 1 keyboard, 2 the Wingie
    void setMode (int side, bool down, int source);
    void setOctave (int side, int v);
    void setSource (int v);
    void setFader (int i, float v);
    juce::Rectangle<float> keyRect (int side, int k) const;
    int keyAt (juce::Point<float>, int& side) const;
    bool isBlack (int k) const { return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; }

    Host& host;
    // what is held, and by whom (bit 0 mouse, 1 keyboard, 2 the Wingie): a key is down while anyone holds it
    juce::uint8 keyBy[2][12] {}, modeBy[2] {};
    int octave[2] {}, source = 0;
    float fader[3] { 0.5f, 0.5f, 0.5f };
    float hwLamp[2] {};
    bool following = false;                          // a Wingie is connected and its hands drive this

    juce::Rectangle<float> board[2], keybed[2], modeBtn[2], octSw[2], lampR[2], faderR[3], faderLoop[3],
                           keysLoop[2], modeLoop[2], sourceSw, listenBtn, micLoop, mapArea;
    std::vector<std::pair<juce::String, juce::Rectangle<float>>> mapRows;
    enum class Drag { None, Key, Mode, Fader } drag = Drag::None;
    int dragSide = 0, dragIndex = -1;

    // the Wingie's last report, so a control only follows it when it moves there
    juce::uint32 hwCount = 0, hwKeys[2] {}, hwMode = 0;
    float hwFader[3] { -1, -1, -1 };
    int hwOct[2] { 9, 9 }, hwSource = 9, hwScene = -1;
    bool keyDown[26] {};                             // the computer keyboard, last look
};

// The Wingie's meters: the estimate for the scene that plays (as a share of the Wingie), and when a
// Wingie is connected what it measures, once a second: CPU average and peak (bar, and a scrolling
// minute of history), its control core, free heap, the engine's pool, dropped blocks (red when any),
// and its input and output levels. Without a Wingie the levels are the computer's (the microphone
// listened to, and the sound out).
class WingieMeters : public juce::Component, private juce::Timer
{
public:
    explicit WingieMeters (Host&);
    serial::Link* link = nullptr;
    std::function<float()> estimate;                 // the playing scene's load on the Wingie, 1 = all of it
    std::function<juce::String()> poolWord;          // the estimate of the engine's pool for this scene ("12 of 124 KB")
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    Host& host;
    struct Sample { float avg = 0, peak = 0, estimate = 0; bool dropped = false; };
    std::deque<Sample> history;                      // one a second, the newest last
    juce::uint32 seen = 0;
    serial::Link::Measured last;
};
