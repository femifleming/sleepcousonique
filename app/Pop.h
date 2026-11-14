#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Model.h"
#include "Host.h"

// The openprocess look's own parts (after the poppsphere bundle's browser and parameter list).

// The place browser: four grids of colour squares, sources / shapes / spaces / currents.
// Hover names a place; click puts it on the tapestry.
class PlaceBrowser : public juce::Component
{
public:
    PlaceBrowser();
    std::function<void (const juce::String& place)> onAdd;
    std::function<void (const juce::String& text)> onHover;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct Cell { juce::String name, help; int family = 0, index = 0; juce::Rectangle<float> r; };
    std::vector<Cell> cells;
    juce::Rectangle<float> grids[4];
    int hover = -1;
    int cellAt (juce::Point<float>) const;
};

// The settings column: the selected place's tune, params, switches and attenuverters, each a
// gradient bar with a red marker. Drag a bar (it plays as you drag); click a switch to step it.
class Inspector : public juce::Component
{
public:
    Inspector (scq::Document&, Host&);
    std::function<void (bool recompile)> onEdited;     // the text changed
    std::function<void (const juce::String&)> onStatus;
    void show (int node);                              // -1: nothing selected
    void refresh();                                    // the dream was reread
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    enum Kind { Tune, Att, Param, Switch, Panel };
    struct Row
    {
        Kind kind; juce::String key, label;
        int index = 0;                                 // the inlet (Att) or the option (Param, Switch)
        float value = 0, lo = 0, hi = 1, def = 0;
        juce::StringArray positions; int pos = 0;
        juce::Rectangle<float> bar, area;
    };
    scq::Document& doc;
    Host& host;
    int node = -1;
    std::vector<Row> rows;
    int dragging = -1;
    void layoutRows();
    void apply (Row&, bool live);
};
