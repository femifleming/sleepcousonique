#include "Scales.h"
#include "ScqData.h"
#include <cmath>

namespace scales
{
// a file's name as a dream can write it: lower case, letters, digits and _
static juce::String nameOf (const juce::File& f)
{
    juce::String out;
    for (auto c : f.getFileNameWithoutExtension().toLowerCase())
        out << (juce::CharacterFunctions::isLetterOrDigit (c) ? juce::String::charToString (c) : juce::String ("_"));
    if (out.isNotEmpty() && juce::CharacterFunctions::isDigit (out[0])) out = "s" + out;
    return out;
}

juce::File folder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Sleepcousonique").getChildFile ("scales");
    static bool seeded = false;
    if (! seeded)
    {
        seeded = true;
        dir.createDirectory();
        // copy in each scale that came with the app, once: one you delete stays deleted
        auto list = dir.getChildFile (".came-with-the-app");
        auto had = juce::StringArray::fromLines (list.loadFileAsString());
        for (int i = 0; i < ScqData::namedResourceListSize; ++i)
        {
            juce::String file (ScqData::originalFilenames[i]);
            if (! file.endsWithIgnoreCase (".scl") || had.contains (file)) continue;
            int size = 0;
            if (auto* data = ScqData::getNamedResource (ScqData::namedResourceList[i], size))
                if (! dir.getChildFile (file).existsAsFile())
                    dir.getChildFile (file).replaceWithData (data, (size_t) size);
            had.add (file);
        }
        had.removeEmptyStrings();
        list.replaceWithText (had.joinIntoString ("\n") + "\n");
    }
    return dir;
}

static juce::Array<juce::File> files()
{
    auto all = folder().findChildFiles (juce::File::findFiles, false, "*.scl");
    all.sort();
    return all;
}

juce::StringArray names()
{
    juce::StringArray out;
    for (auto& f : files()) out.addIfNotAlreadyThere (nameOf (f));
    return out;
}

static juce::File fileFor (const juce::String& name)
{
    for (auto& f : files()) if (nameOf (f) == name) return f;
    return {};
}

// Scala: '!' lines are comments; then a description line, the count, and that many pitches
// (cents if there's a '.', else a ratio a/b or a whole number). 1/1 is left out; the last is the period.
static bool parse (const juce::File& f, juce::String& desc, juce::Array<double>& cents)
{
    int stage = 0, count = 0;
    for (auto l : juce::StringArray::fromLines (f.loadFileAsString()))
    {
        l = l.trim();
        if (l.startsWithChar ('!')) continue;
        if (stage == 0) { desc = l; stage = 1; continue; }
        if (stage == 1) { if (l.isEmpty()) continue; count = l.getIntValue(); stage = 2; continue; }
        if (l.isEmpty()) continue;
        auto w = l.upToFirstOccurrenceOf (" ", false, false).upToFirstOccurrenceOf ("\t", false, false);
        double c;
        if (w.containsChar ('.')) c = w.getDoubleValue();
        else
        {
            double a = w.upToFirstOccurrenceOf ("/", false, false).getDoubleValue();
            double b = w.containsChar ('/') ? w.fromFirstOccurrenceOf ("/", false, false).getDoubleValue() : 1.0;
            if (a <= 0 || b <= 0) return false;
            c = 1200.0 * std::log2 (a / b);
        }
        cents.add (c);
        if (cents.size() == count) break;
    }
    return count > 0 && cents.size() == count;
}

juce::String description (const juce::String& name)
{
    juce::String d; juce::Array<double> c;
    auto f = fileFor (name);
    return f.existsAsFile() && parse (f, d, c) ? d : juce::String();
}

juce::String withTunings (const juce::String& text)
{
    juce::StringArray wanted, defined;
    for (auto l : juce::StringArray::fromLines (text))
    {
        l = l.upToFirstOccurrenceOf (";", false, false);
        auto t = juce::StringArray::fromTokens (l, " \t", "");
        if (t.size() > 2 && t[0] == "tuning" && t[1].endsWithChar (':')) defined.add (t[1].dropLastCharacters (1));
        else if (t.size() > 2 && t[0] == "tuning" && t[2] == ":") defined.add (t[1]);
        for (auto& w : t)
            if (w.startsWith ("scale=")) wanted.addIfNotAlreadyThere (w.fromFirstOccurrenceOf ("=", false, false));
    }
    juce::String extra;
    for (auto& name : wanted)
    {
        if (defined.contains (name)) continue;
        auto f = fileFor (name);
        juce::String d; juce::Array<double> cents;
        if (! f.existsAsFile() || ! parse (f, d, cents)) continue;   // a built-in scale, or the engine says it's missing
        for (int i = 0; i < cents.size(); i += 40)                  // long tunings go on over several lines
        {
            extra << "tuning " << name << ":";
            for (int j = i; j < juce::jmin (i + 40, cents.size()); ++j) extra << " " << juce::String (cents[j], 4);
            extra << "\n";
        }
    }
    if (extra.isEmpty()) return text;
    return text + (text.endsWithChar ('\n') ? "" : "\n") + "; from the scales folder\n" + extra;
}
}
