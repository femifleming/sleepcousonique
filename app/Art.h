#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace art
{
// The look. Two styles, each light or dark:
//   journal     - the original: splatter canvas, cream boards, Dymo labels, braided loops
//   openprocess - the openprocess bundle's: flat ground, dotted window, grey-headed cards,
//                 rings for ports, checker for what's on, Verdana bold
struct Theme
{
    bool pop = true, dark = true;
    bool modular = false;   // the 2004 modular editor look (the faltung layout, drawn as GDI)
    juce::Colour window, bar, chromeText, chromeDim, line;   // the frame around everything
    juce::Colour surface, card, cardSp1, cardHead, cardHeadText;
    juce::Colour button, buttonText, rail;
    juce::Colour checkA, checkB;                              // "on": checker yellow / orange
    juce::Colour sound, current, pulse;                       // signal colours
    juce::Colour fam[6];
    juce::Colour ledOff, faderSlot, faderCap;
    juce::Colour note, date, word, place, passage, number, map, hand, gutter, gutterText;
};
Theme& theme();
void setTheme (bool openprocessStyle, bool dark, bool modular = false);

// colours everything reads; setTheme() rewrites them
inline juce::Colour canvas   { 0xffe9e3d4 };   // the tapestry's ground
inline juce::Colour cream    { 0xfff3ead2 };   // a board
inline juce::Colour ink      { 0xff1d1b19 };   // text on the content
inline juce::Colour umber    { 0xff7a4a2a };   // sound
inline juce::Colour blue     { 0xff1f3f8f };   // current
inline juce::Colour gold     { 0xffd2a531 };   // pulse, selection
inline juce::Colour copper   { 0xffb8733b };
inline juce::Colour paper    { 0xfffbf8f0 };   // the journal's page
inline juce::Colour night    { 0xff15161c };
inline juce::Colour boardInk { 0xff13222c };   // text on a board (the faltung look's pastel blocks)

// the faltung look (openprocess layout): Inter for names, IBM Plex Mono for everything else
juce::Font sans (float h, bool medium = false);
// a slanted parallelogram (the faltung control), and an isometric folded block
juce::Path slant (juce::Rectangle<float> r, float k = -1.0f);
void drawBlock (juce::Graphics&, juce::Rectangle<float> front, juce::Colour face, float depth, bool selected);
void drawCube (juce::Graphics&, juce::Point<float> c, float r);
// the modular look: Windows 3D edges, a module strip, a jack (round in, square out), Tahoma
void bevel (juce::Graphics&, juce::Rectangle<float> r, bool raised);
void drawModule (juce::Graphics&, juce::Rectangle<float> r, juce::Colour, bool selected, juce::Colour icon);
void drawJack (juce::Graphics&, juce::Point<float> c, bool out, bool hot, juce::Colour signal);
juce::Font tahoma (float h, bool bold = false);

juce::Font label (float h);                      // titles and buttons: Verdana bold (openprocess) or mono
void fillChecker (juce::Graphics&, juce::Rectangle<float>, float cell = 6.0f);   // the "on" checker

juce::Colour familyColour (int family);
juce::Colour signalColour (int signal);

void paintCanvas (juce::Graphics&, juce::Rectangle<float>, int seed);
// how cables look, set from the View menu
struct CableStyle
{
    float opacity = 1.0f;     // 0.1 .. 1
    float slack = 1.0f;       // 0 tight .. 3 very loose (elasticity)
    float glow = 0.0f;        // 0 .. 1: the lights are down and cables glow
};
CableStyle& cableStyle();

juce::Path cablePath (juce::Point<float> a, juce::Point<float> b, float heavy = 1.0f);   // heavy > 1 hangs lower
// a cable along any path (curved, or routed around the boards); mid and angle place its part
// Two kinds of cable, as on a modular where the audio leads are fat and the CV leads thin:
// signal 0 (sound) is a thick braided cable; 1 (current) is thin and plain; 2 (pulses) thin
// and beaded. -1 works it out from the colour.
void drawCablePath (juce::Graphics&, const juce::Path&, juce::Colour, int kind, bool hover, float alpha, int signal = -1);
float distanceToCable (juce::Point<float> a, juce::Point<float> b, juce::Point<float> p);
juce::Point<float> cableMid (juce::Point<float> a, juce::Point<float> b);
void drawCable (juce::Graphics&, juce::Point<float> a, juce::Point<float> b, juce::Colour, int kind, bool hover, float alpha, int signal = -1);
void drawLoop (juce::Graphics&, juce::Point<float> c, bool hot, juce::Colour signal);
void drawDymo (juce::Graphics&, juce::Rectangle<float> r, const juce::String& text);
juce::Font mono (float h);
juce::Font hand (float h);
}
