#pragma once
#include <juce_core/juce_core.h>

// The scales folder: Scala (.scl) files the quantizer can use by name (scale=young_wtp).
// The SP-1 has no files, so the editor writes the scales a dream names into the dream itself,
// as "tuning NAME: cents ... period" lines at the end, whenever it plays, checks or sends it.
namespace scales
{
juce::File folder();                        // ~/Documents/Sleepcousonique/scales, made and seeded on first use
juce::StringArray names();                  // the scales in it, as a dream names them
juce::String description (const juce::String& name);
juce::String withTunings (const juce::String& dreamText);   // the dream, plus the tunings it names
}
