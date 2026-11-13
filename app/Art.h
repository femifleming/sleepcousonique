#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace art
{
// colours from the journal
const juce::Colour canvas   { 0xffe9e3d4 };   // duck canvas
const juce::Colour cream    { 0xfff3ead2 };   // the Victorian cream manor
const juce::Colour ink      { 0xff1d1b19 };
const juce::Colour umber    { 0xff7a4a2a };   // sound: dark umber mountains
const juce::Colour blue     { 0xff1f3f8f };   // current: deep blue light
const juce::Colour gold     { 0xffd2a531 };   // pulse: golden glitter
const juce::Colour copper   { 0xffb8733b };
const juce::Colour paper    { 0xfffbf8f0 };
const juce::Colour night    { 0xff15161c };

juce::Colour familyColour (int family);
juce::Colour signalColour (int signal);

void paintCanvas (juce::Graphics&, juce::Rectangle<float>, int seed);
juce::Path cablePath (juce::Point<float> a, juce::Point<float> b);
float distanceToCable (juce::Point<float> a, juce::Point<float> b, juce::Point<float> p);
juce::Point<float> cableMid (juce::Point<float> a, juce::Point<float> b);
void drawCable (juce::Graphics&, juce::Point<float> a, juce::Point<float> b, juce::Colour, int kind, bool hover, float alpha);
void drawLoop (juce::Graphics&, juce::Point<float> c, bool hot, juce::Colour signal);
void drawDymo (juce::Graphics&, juce::Rectangle<float> r, const juce::String& text);
juce::Font mono (float h);
juce::Font hand (float h);
}
