#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Model.h"
#include "Host.h"

// The visual editor: boards on a splatter-painted canvas, joined by cords with parts in them.
// Every change here is a change to the text; the text stays the truth.
class Tapestry : public juce::Component,
                 public juce::DragAndDropTarget,
                 private juce::Timer
{
public:
    Tapestry (scq::Document&, Host&);

    std::function<void (bool recompile)> onTextEdited;   // the document's text changed
    std::function<void (const juce::String&)> onStatus;
    std::function<void (int line)> onShowLine;           // reveal a line in the journal

    int handKind = 0;          // passage for new cords: 0 hall, 1 fog, 2 water, 3 door
    void refresh();            // after a compile
    void paste (const juce::String& text);
    juce::String copySelection() const;
    void removeSelection();
    void fitAll();
    void themeChanged();
    struct FleetChip { juce::String id, name; bool answered = false, on = true; };
    void setFleet (const std::vector<FleetChip>& chips) { fleet = chips; repaint(); }
    std::function<void (const juce::String& id, bool on)> onFleetToggle;   // click a chip
    int selectedNode() const;                // the one selected place, or -1
    juce::Point<float> viewCentre() const { return toDesign (getLocalBounds().getCentre().toFloat()); }                     // the look changed: repaint the canvas
    void setRouting (bool on);              // cables around the boards, like Max
    void setLight (float roomLight);        // 1 = daylight .. 0.1 = dark, cables and LEDs glow
    void setHighlight (int firstLine, int lastLine);   // lines selected in the journal (1-based), or an empty range
    bool routing = false;
    float light = 1.0f;
    void centreOn (int board);              // bring one board to the middle and select it
    juce::Rectangle<int> findButton() const;
    juce::Rectangle<int> chipRect (int i) const;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float) override;
    bool keyPressed (const juce::KeyPress&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

private:
    struct Port { juce::String label, text; int signal = 0; };   // text: how the journal names it
    struct Board
    {
        int node = -1;                // -1 hands, -2 ears
        juce::String key, title, sub, help;
        int family = 0;
        juce::Rectangle<float> r;
        std::vector<Port> ins, outs;
        bool hasTune = false, hasAtt = false;
        float tune = 0;               // the base pitch knob, octaves
        std::vector<float> att;       // an attenuverter per inlet
        struct Param { juce::String name; int k = 0; float value = 0, def = 0, lo = 0, hi = 1; };
        std::vector<Param> params;    // numeric params as knobs (ratio, length...)
    };
    struct Knob { int board = -1, inlet = -2, param = -1; };   // inlet -1: the tune knob; param >= 0: a param knob
    struct Hit { int board = -1; bool out = false; int port = -1; };

    void timerCallback() override;
    bool cordEnds (int k, juce::Point<float>& a, juce::Point<float>& b) const;
    juce::Path cordPath (int k) const;      // routed, or the hanging curve
    mutable std::vector<juce::Path> routes;
    mutable bool routesDirty = true;
    int hlFirst = 0, hlLast = -1;
    std::vector<FleetChip> fleet;   // the SP-1s over Bluetooth, shown top right
    bool lit (int line) const { return line >= hlFirst && line <= hlLast; }
    void layout();
    juce::Point<float> inletPos (const Board&, int) const;
    juce::Point<float> outletPos (const Board&, int) const;
    bool endpoint (int node, int port, bool out, int& board, int& row) const;
    Hit portAt (juce::Point<float> design, float slop = 9) const;
    Knob knobAt (juce::Point<float> design) const;
    juce::Point<float> tunePos (const Board&) const;
    juce::Point<float> paramPos (const Board&, int j) const;
    juce::Point<float> attPos (const Board&, int inlet) const;
    void drawKnob (juce::Graphics&, juce::Point<float> c, float radius, float value, float range, bool bipolar, bool hot) const;
    int boardAt (juce::Point<float> design) const;
    int cordAt (juce::Point<float> design) const;
    int bottleAt (juce::Point<float> design) const;
    juce::Rectangle<float> bottleRect (int cord) const;
    juce::Point<float> toDesign (juce::Point<float> screen) const { return screen / zoom + pan; }
    juce::Point<float> toScreen (juce::Point<float> d) const { return (d - pan) * zoom; }
    void zoomAbout (float z, juce::Point<float> screen);
    void edited (bool recompile);
    void boardMenu (int board);
    void cordMenu (int cord);
    void canvasMenu (juce::Point<float> design);
    void askAmount (int cord);
    void connect (const Port& from, const Port& to);
    juce::String arrow (int kind) const;
    juce::Point<float> defaultPos (const scq::NodeInfo&, int index);

    scq::Document& doc;
    Host& host;
    std::vector<Board> boards;
    juce::Image canvasImg;
    float zoom = 1.0f;
    juce::Point<float> pan { -20, -20 };

    enum class Mode { None, Pan, Move, Cable, Value, Knob } mode = Mode::None;
    juce::Point<float> anchor, dragPos, moveStart;
    Hit cableFrom;
    int moving = -1, valueCord = -1;
    float valueStart = 0, valueNow = 0;
    std::set<int> selected;        // board indices
    int selectedCord = -1, hoverCord = -1;
    Hit hoverPort;
    Knob knob, hoverKnob;
    float knobStart = 0;
    int hoverBoard = -1;
    juce::Point<float> dropPos { -1, -1 };
    juce::Point<float> lastMouse;
};
