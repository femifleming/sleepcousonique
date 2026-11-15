#pragma once
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <set>
#include <map>

// What the engine says about a .dream file, and the text edits the tapestry makes.
namespace scq
{
enum Family { Source, Shape, Space, Current, Hands, Ears };
enum Kind { Hall, Fog, Water, Door, Range, Bottle, Maybe };
enum Signal { Sound, Flow, Pulse };   // umber, deep blue, gold

struct OptSpec          // a switch (its positions; the first is the default) or a numeric param
{
    juce::String name;
    juce::StringArray positions;   // empty for a param
    juce::String def;
};

struct PlaceInfo
{
    std::vector<OptSpec> opts;
    juce::String name, help;
    Family family = Source;
    int cost = 0;
    float nature = 1;
    juce::StringArray ins, outs;
    bool att = false;    // an attenuverter knob on every inlet (sources, and places with a pitch inlet)
    std::vector<float> defs;   // each inlet's default
    bool tune = false;   // a base pitch knob (places with a pitch inlet)
};

struct NodeInfo
{
    int id = 0;
    juce::String place, name;
    float nature = 1;
    int line = 0;
    std::map<juce::String, juce::String> opts;   // switches and params written in the dream
};

struct CordInfo
{
    int src = -1, sport = 0, dst = 0, dport = 0, kind = 0;
    float amt = 0, amt2 = 0;
    int line = 0;
};

struct SceneInfo { juce::String title; bool current = false; };

struct Message { bool error = false; int line = 0; juce::String text; };

const std::vector<PlaceInfo>& catalog();
const PlaceInfo* findPlace (const juce::String& name);
Signal outletSignal (const PlaceInfo&, int port);
// Does inlet i get a panel knob (as it would on an analog module), and over what range?
// Audio and pulse inlets don't; pitch doesn't where the place has a tune knob.
bool knobRange (const PlaceInfo&, int inlet, float& lo, float& hi);
float knobDefault (const PlaceInfo&, int inlet);

// The text, kept as the one source of truth. Every visual edit is a text edit.
class Document
{
public:
    juce::String text;
    juce::File file;
    bool dirty = false;

    // filled after each compile
    std::vector<NodeInfo> nodes;
    std::vector<CordInfo> cords;
    std::vector<SceneInfo> scenes;
    std::vector<Message> messages;
    int scene = 0;
    int poolUsed = 0, poolTotal = 1;
    float load = 0;
    int errors = 0;

    void readDescription (const char* describe, const char* messages);

    const NodeInfo* node (int id) const;
    const NodeInfo* nodeNamed (const juce::String&) const;
    const PlaceInfo* placeOf (int id) const;

    // positions on the tapestry, per scene: ";@ scene name x y" lines at the end of the file
    std::map<juce::String, juce::Point<float>> positions;   // key "scene:name"
    void readPositions();
    void setPosition (int scene, const juce::String& name, juce::Point<float>);
    void writePositions();
    juce::String key (int sceneIdx, const juce::String& name) const { return juce::String (sceneIdx) + ":" + name; }

    // edits
    juce::String cordText (const CordInfo&) const;
    void addLine (const juce::String& line);                   // at the end of the playing scene
    void addLines (const juce::StringArray& lines);
    juce::String addPlace (const juce::String& place);         // returns the new name
    void setOption (int id, const juce::String& key, const juce::String& value);   // a switch or param
    void addCord (int src, int sport, int dst, int dport, Kind);
    void addControl (const juce::String& control, int dst, int dport);
    void removeCord (int cordIndex);
    void removeCords (const std::set<int>& cordIndices);   // several at once (one edit)
    void removeNode (int id) { removeNodes ({ id }); }
    void removeNodes (const std::set<int>& ids);
    int cordsOnLine (int line) const;
    void setCordAmount (int cordIndex, float amount, float amount2 = 0);
    void setCordKind (int cordIndex, Kind);
    juce::String copyNodes (const std::vector<int>& ids) const;   // their lines, as text

    // scenes in the text
    struct Span { int header = -1, first = 0, last = 0; };    // 0-based line indices, inclusive body
    std::vector<Span> sceneSpans() const;
    int sceneOfLine (int line0) const;                        // 0-based line -> scene index (-1 prelude)

private:
    juce::StringArray lines() const;
    void setLines (const juce::StringArray&);
    void expandLineExcept (juce::StringArray& ls, int line1, int dropCord, std::function<void (CordInfo&)> change = {}, int changeCord = -1, const std::set<int>* drops = nullptr) const;
    juce::String uniqueName (const juce::String& place) const;
};

juce::String fmt (float v);
}
