// Checks that every visual edit leaves a journal the engine still understands.
#include <juce_core/juce_core.h>
#include "../app/Model.h"
#include "scq.h"

static int failures = 0;
#define CHECK(c, msg) do { if (! (c)) { ++failures; std::printf ("FAIL %s  (%s:%d)\n", msg, __FILE__, __LINE__); } else std::printf ("ok   %s\n", msg); } while (0)

static int compile (scq::Document& d)
{
    auto s = d.text.toStdString();
    int e = scq_load (s.data(), (int) s.size(), 0);
    d.readDescription (scq_describe(), scq_messages());
    return e;
}

int main()
{
    scq::Document d;
    d.text = "drift 0\n\n2026-11-13\n----------\nthe voice is aira 3\nthe hall is hallway 4\n"
             "voice -> hall left right   ; three at once\nlean1 -40> hall.feedback\nbottle 20 -> hall.length\n";
    CHECK (compile (d) == 0, "the starting dream compiles");
    int cords = (int) d.cords.size();
    CHECK (cords == 5, "five passages");

    // remove the cord voice -> left from a three-way line
    int ci = -1;
    for (int k = 0; k < cords; ++k)
        if (d.cordText (d.cords[(size_t) k]) == "voice -> left") ci = k;
    CHECK (ci >= 0, "found voice -> left");
    d.removeCord (ci);
    CHECK (compile (d) == 0, "after removing a cord it still compiles");
    CHECK ((int) d.cords.size() == cords - 1, "one fewer passage");
    CHECK (d.text.contains ("voice -> hall") && d.text.contains ("voice -> right") && ! d.text.contains ("voice -> left"), "the others stayed");
    CHECK (d.text.contains ("; three at once"), "the note on the line stayed");

    // change an amount and a kind
    for (int k = 0; k < (int) d.cords.size(); ++k)
        if (d.cords[(size_t) k].src < 0) { d.setCordAmount (k, 55); break; }
    CHECK (compile (d) == 0 && d.text.contains ("bottle 55 -> hall.length"), "a bottle changed");
    for (int k = 0; k < (int) d.cords.size(); ++k)
        if (d.cordText (d.cords[(size_t) k]) == "lean1 -40> hall.feedback") { d.setCordKind (k, scq::Water); break; }
    CHECK (compile (d) == 0 && d.text.contains ("lean1 => hall.feedback"), "hall became water");

    // add a place and a cord
    auto name = d.addPlace ("moss");
    CHECK (compile (d) == 0 && d.nodeNamed (name) != nullptr, "a moss was added");
    d.addLine ("hall -> " + name);
    CHECK (compile (d) == 0, "and patched");

    // remove the hall: its declaration and all its cords go
    d.removeNode (d.nodeNamed ("hall")->id);
    CHECK (compile (d) == 0, "after removing the hall it still compiles");
    CHECK (d.nodeNamed ("hall") == nullptr && ! d.text.contains ("hall"), "no hall anywhere");

    // positions survive and don't change the sound
    d.setPosition (0, "voice", { 100, 200 });
    d.writePositions();
    CHECK (compile (d) == 0 && d.text.contains (";@ 0 voice 100 200"), "positions are kept as notes");
    d.readPositions();
    CHECK (d.positions.size() == 1, "and read back");

    // copy two places as text and paste them into a fresh dream
    auto copied = d.copyNodes ({ d.nodeNamed ("voice")->id, d.nodeNamed (name)->id });
    CHECK (copied.contains ("the voice is aira 3"), "copy has the declaration");
    scq::Document e;
    e.text = "fresh\n-----\n";
    compile (e);
    e.addLines (juce::StringArray::fromLines (copied));
    CHECK (compile (e) == 0, "pasted text compiles in another dream");

    // every example compiles
    for (auto f : juce::File (juce::File::getCurrentWorkingDirectory().getChildFile ("examples")).findChildFiles (juce::File::findFiles, false, "*.dream"))
    {
        scq::Document x;
        x.text = f.loadFileAsString();
        CHECK (compile (x) == 0, ("example " + f.getFileName()).toRawUTF8());
    }
    std::printf (failures ? "\n%d FAILED\n" : "\nall good\n", failures);
    return failures ? 1 : 0;
}
