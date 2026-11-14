#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>

// Snapshots and a timeline, after openprocess: for composing a piece for the SP-1 ensemble.
//
// A snapshot is what's sounding: every cord amount and kept value, every tune, attenuverter and
// param, the four faders, and the scene, keyed by names so it survives editing the dream.
// Recalling one cuts to it or glides there. The timeline is a row of segments, each a snapshot
// and a length; a glide segment morphs into the next snapshot over its length, a cut segment
// holds. Everything it plays goes through the editor's live path, so every SP-1 follows.
// Snapshots and the timeline are kept in the dream as ";~" lines, which the engine ignores.
class Timeline : public juce::Component, private juce::Timer
{
public:
    using Values = std::map<juce::String, float>;
    struct Snapshot { bool used = false; int scene = 0; Values v; };
    struct Segment { int snap = 0; float secs = 8; bool glide = true; };

    Timeline();
    std::function<Snapshot()> onCapture;                                   // what's sounding now
    std::function<void (const Values&, int scene)> onApply;               // play these values
    std::function<void (const juce::String&)> onStatus;
    std::function<void()> onChanged;                                       // keep it in the dream

    juce::String toText() const;                                           // the ";~" lines
    void fromText (const juce::String& dream);
    static juce::String without (const juce::String& dream);               // the dream minus them

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void togglePlay();

private:
    static constexpr int SLOTS = 12;
    Snapshot snaps[SLOTS];
    std::vector<Segment> segs;
    bool playing = false, loop = true, glideRecall = true;
    float glideSecs = 2;
    double t = 0;                                    // the playhead, seconds
    int current = -1, selected = -1;
    // a glide started by clicking a snapshot
    Values from, to; int toScene = 0; double glideAt = 0, glideLen = 0; bool gliding = false;
    Values sent; int sentScene = -1;
    juce::uint32 lastTick = 0;
    // mouse
    int dragSlot = -1, dragEdge = -1; juce::Point<float> dragNow; float edgeStart = 0;

    void timerCallback() override;
    void play (const Values& v, int scene);
    static Values lerp (const Values& a, const Values& b, float x);
    double total() const;
    double startOf (int seg) const;
    juce::Rectangle<float> slotRect (int i) const;
    juce::Rectangle<float> playRect() const, loopRect() const, modeRect() const, secsRect() const, laneRect() const;
    juce::Rectangle<float> segRect (int i) const;
    float pxPerSec() const;
    int segAt (juce::Point<float>) const;
    void recall (int slot);
    void changed();
};
