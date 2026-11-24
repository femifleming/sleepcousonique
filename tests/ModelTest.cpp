// Checks that every visual edit leaves a journal the engine still understands.
#include "../app/Scales.h"
#include <juce_core/juce_core.h>
#include "../app/Model.h"
#include "../app/Radio.h"
#include "scq.h"
#include <cmath>
#include <cstring>

static int failures = 0;
#define CHECK(c, msg) do { if (! (c)) { ++failures; std::printf ("FAIL %s  (%s:%d)\n", msg, __FILE__, __LINE__); } else std::printf ("ok   %s\n", msg); } while (0)

static int compile (scq::Document& d)
{
    auto s = d.text.toStdString();
    int e = scq_load (s.data(), (int) s.size(), 0);
    d.readDescription (scq_describe(), scq_messages());
    return e;
}

// the engine for a while; what outlet `port` of place 0 ends on, and how many pulses it made
static float run (double seconds, int port = 0, int* pulses = nullptr)
{
    float l[SCQ_BLOCK], r[SCQ_BLOCK], v = 0, was = 0;
    for (int b = 0; b < (int) (seconds * SCQ_RATE / SCQ_BLOCK); ++b)
    {
        scq_render (l, r, SCQ_BLOCK);
        v = scq_probe (0, port);
        if (pulses && v > 0.5f && was <= 0.5f) ++*pulses;
        was = v;
    }
    return v;
}

// ScqTests --listen 8: the real radio for eight seconds (the Mac's Bluetooth; macOS may ask),
// printing the room once a second. Not part of the ordinary run.
static int listenForReal (int seconds)
{
    radio::Radio radio;
    radio.setWanted (true);
    int total = 0;
    for (int t = 0; t < seconds * 10; ++t)
    {
        juce::Thread::sleep (100);
        radio.room.tick (juce::Time::getMillisecondCounterHiRes() * 0.001);
        for (auto& e : radio.room.takeEvents()) if (e.kind == 0) ++total;
        if (t % 10 == 9)
        {
            auto w = radio.room.weather();
            std::printf ("%2d s  state %d  %s  (near %.2f, strength %.2f, %d calls heard so far)\n", t / 10 + 1, (int) radio.state(),
                         radio.describe().toRawUTF8(), (double) w.near, (double) w.strength, total);
            std::fflush (stdout);
        }
    }
    auto st = radio.state();
    radio.setWanted (false);
    return st == radio::State::listening ? 0 : 3;
}

int main (int argc, char** argv)
{
    if (argc > 2 && juce::String (argv[1]) == "--listen") return listenForReal (juce::String (argv[2]).getIntValue());

    scq::Document d;
    d.text = "drift 0\n\n2026-11-13\n----------\nthe voice is sine 3\nthe hall is delay 4\n"
             "voice -> hall left right   ; three at once\nfader1 -40> hall.feedback\nvalue 20 -> hall.time\n";
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
    CHECK (compile (d) == 0 && d.text.contains ("value 55 -> hall.time"), "a value changed");
    for (int k = 0; k < (int) d.cords.size(); ++k)
        if (d.cordText (d.cords[(size_t) k]) == "fader1 -40> hall.feedback") { d.setCordKind (k, scq::Water); break; }
    CHECK (compile (d) == 0 && d.text.contains ("fader1 => hall.feedback"), "hall became water");

    // add a place and a cord
    auto name = d.addPlace ("granular");
    CHECK (compile (d) == 0 && d.nodeNamed (name) != nullptr, "a granular was added");
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

    // switches: set one from the tapestry, the declaration line changes
    {
        scq::Document f;
        f.text = "s\n-----\nthe env is dusg   ; two slopes\nenv -> out\nslope -> out\n";
        CHECK (compile (f) == 0, "a dusg compiles");
        f.setOption (f.nodeNamed ("env")->id, "cycle1", "on");
        compile (f);
        f.setOption (f.nodeNamed ("env")->id, "range", "audio");
        CHECK (compile (f) == 0 && f.text.contains ("the env is dusg cycle1=on range=audio   ; two slopes"), "switches written");
        CHECK (f.nodeNamed ("env")->opts.at ("range") == "audio", "and read back");
        f.setOption (f.nodeNamed ("env")->id, "range", "lfo");
        CHECK (compile (f) == 0 && f.text.contains ("the env is dusg cycle1=on   ;"), "a default is left out");
        f.setOption (f.nodeNamed ("slope")->id, "cycle", "on");
        CHECK (compile (f) == 0 && f.text.contains ("the slope is slope cycle=on"), "an unnamed place gets a name for its switch");
    }

    // copy two places as text and paste them into a fresh dream
    auto copied = d.copyNodes ({ d.nodeNamed ("voice")->id, d.nodeNamed (name)->id });
    CHECK (copied.contains ("the voice is sine 3"), "copy has the declaration");
    scq::Document e;
    e.text = "fresh\n-----\n";
    compile (e);
    e.addLines (juce::StringArray::fromLines (copied));
    CHECK (compile (e) == 0, "pasted text compiles in another dream");

    // paste where the names are taken: only the names change, never the place a name is, its
    // params, or a port spelled the same (the editor once wrote "the combb is combb")
    {
        scq::Document p;
        p.text = "n\n-----\nthe comb is comb\nthe decay is decay decay=0.9\nthe saw is saw\nsaw -> comb\ndecay -> comb.decay\ncomb -> out\n";
        CHECK (compile (p) == 0 && p.messages.empty(), "a comb, a decay and a saw, no notes");
        for (int round = 0; round < 3; ++round)      // paste, then paste the paste
        {
            std::vector<int> ids;
            for (auto& n : p.nodes) if (n.name.startsWith ("comb") || n.name.startsWith ("decay") || n.name.startsWith ("saw")) ids.push_back (n.id);
            juce::StringArray names;
            auto lines = p.renamedForPaste (juce::StringArray::fromLines (p.copyNodes (ids)), names);
            p.addLines (lines);
            CHECK (compile (p) == 0 && p.messages.empty(), ("pasted copies compile with no notes, round " + juce::String (round + 1)).toRawUTF8());
            CHECK (names.size() == (int) ids.size(), "every pasted place got a name");
        }
        CHECK (p.text.contains ("the combb is comb\n") && ! p.text.contains ("is combb"), "the copy of comb is still a comb");
        CHECK (p.text.contains ("the decayb is decay decay=0.9"), "a param spelled like the name is left alone");
        CHECK (p.text.contains ("decayb -> combb.decay"), "and so is a port spelled like the name");
        int combs = 0;
        for (auto& n : p.nodes) if (n.place == "comb") ++combs;
        CHECK (combs == 8, "eight combs after three rounds");
    }

    // an amount the language can't write is brought to the nearest one it can
    {
        scq::Document a;
        a.text = "the s is sine\nthe o is onepole\ns -50> o\ns ~> o.cutoff\ns => o\ns ?> o\nlucid fader1: o.cutoff 0..100\n";
        CHECK (compile (a) == 0, "five kinds of cord");
        for (int k = 0; k < (int) a.cords.size(); ++k) { a.setCordAmount (k, -25.5f, -155.96f); CHECK (compile (a) == 0, "a negative amount still compiles"); }
        CHECK (a.text.contains ("lucid fader1: o.cutoff -25.5..-155.96"), "a range may go below zero at both ends");
        CHECK (a.text.contains ("s -0> o") && a.text.contains ("s ?0> o") && a.text.contains ("s =1> o"), "cords stop at nothing, water at a millisecond");
    }

    // every example compiles
    for (auto f : juce::File (juce::File::getCurrentWorkingDirectory().getChildFile ("examples")).findChildFiles (juce::File::findFiles, false, "*.dream"))
    {
        scq::Document x;
        x.text = f.loadFileAsString();
        CHECK (compile (x) == 0, ("example " + f.getFileName()).toRawUTF8());
    }
    // option-click on a port: several cables out in one edit, even two from one chain line
    {
        scq::Document r;
        r.text = "the a is sine\nthe b is lowpass\nthe c is sine 2\na -> b -> out\nc -> out\nc -> b.cutoff\n";
        compile (r);
        std::set<int> drop;
        for (int k = 0; k < (int) r.cords.size(); ++k)
            if (r.cords[(size_t) k].dst == r.cords[(size_t) k].dst && r.node (r.cords[(size_t) k].dst) && r.node (r.cords[(size_t) k].dst)->name == "out") drop.insert (k);
        CHECK (drop.size() == 2, "two cables into out");
        r.removeCords (drop);
        CHECK (compile (r) == 0, "still compiles after removing them");
        int intoOut = 0;
        for (auto& c : r.cords) if (r.node (c.dst) && r.node (c.dst)->name == "out") ++intoOut;
        CHECK (intoOut == 0 && r.cords.size() == 2, "a -> b and c -> b.cutoff stay, nothing into out");
    }

    // a knob turned and let go keeps its new value on the board (no reload happens after it)
    {
        scq::Document k;
        k.text = "the h is delay 9\nthe s is saw\ns -> h -> out\n";
        compile (k);
        int id = -1;
        for (auto& n : k.nodes) if (n.name == "h") id = n.id;
        k.setOption (id, "feedback", "0.7");
        auto* n = k.node (id);
        CHECK (n && n->opts.count ("feedback") && n->opts.at ("feedback") == "0.7", "a turned knob keeps its value before any reload");
        CHECK (k.text.contains ("the h is delay 9 feedback=0.7"), "and it's written in the dream");
    }

    // every scale in the scales folder becomes a tuning the engine takes, and quantizes to it
    for (auto& sc : scales::names())
    {
        auto withT = scales::withTunings ("the q is quantizer scale=" + sc + "\nfader1 -> q\nq -> out\n");
        auto utf8 = withT.toStdString();
        int e = scq_load (utf8.data(), (int) utf8.size(), 0);
        CHECK (e == 0 && withT.contains ("tuning " + sc + ":"), ("scale " + sc).toRawUTF8());
    }
    // ---- the room: who has been heard lately (a made-up room; no radio is touched here)
    {
        radio::Room room;
        room.forgetAfter = 8.0;
        CHECK (room.weather().count == 0 && room.weather().near == 0.0f, "an empty room");
        room.heard (11, -40, 0.0);                   // within arm's reach
        room.heard (22, -100, 0.1);                  // at the edge of hearing
        room.heard (33, -70, 0.2);
        auto w = room.weather();
        CHECK (w.count == 3, "three devices are three, not one");
        CHECK (std::abs (w.near - 1.0f) < 0.01f, "the nearest sets near");
        CHECK (std::abs (w.strength - (1.0f + 0.0f + 0.5f) / 3.0f) < 0.01f, "strength is all of them together");
        auto ev = room.takeEvents();
        int arrive = 0, call = 0, leave = 0;
        for (auto& e : ev) { arrive += e.kind == 1; call += e.kind == 0; leave += e.kind == 2; }
        CHECK (arrive == 3 && call == 3 && leave == 0, "each new device arrives once and is heard");
        for (int k = 0; k < 20; ++k) room.heard (11, -42, 0.3 + k * 0.1);     // the same phone, again and again
        CHECK (room.weather().count == 3, "one device calling often is still one device");
        ev = room.takeEvents();
        arrive = call = 0;
        for (auto& e : ev) { arrive += e.kind == 1; call += e.kind == 0; }
        CHECK (arrive == 0 && call == 20, "it arrives once, and every call is heard");
        room.heard (11, 127, 2.4);                   // 127: the radio couldn't tell how strong
        CHECK (room.weather().near > 0.9f, "an unknown strength changes nothing");
        room.tick (5.0);
        CHECK (room.weather().count == 3, "quiet for a few seconds isn't gone");
        room.heard (11, -40, 9.0);
        room.tick (9.5);                             // 22 and 33 last called at 0.1 and 0.2
        CHECK (room.weather().count == 1, "the ones that went quiet have left");
        leave = 0;
        for (auto& e : room.takeEvents()) leave += e.kind == 2;
        CHECK (leave == 2, "and each leaving is told once");
        room.tick (30.0);
        CHECK (room.weather().count == 0 && room.weather().strength == 0.0f, "then the room is empty again");
        float v1 = radio::Room::voiceOf (11), v2 = radio::Room::voiceOf (22);
        CHECK (v1 >= 0.0f && v1 < 1.0f && v2 >= 0.0f && v2 < 1.0f && v1 != v2, "each device has its own number between 0 and 1");
        for (int k = 0; k < 40; ++k) room.heard (1000 + (juce::uint64) k, -60, 31.0);
        CHECK (room.weather().count == 40, "forty devices are forty");
    }

    // ---- the crowd and the visitors, in the engine
    {
        const char* t = "the people is crowd max=10 rest=4\npeople -> out\n";
        scq_room (0, 0, 0, 0);
        CHECK (scq_load (t, (int) std::strlen (t), 0) == 0, "a dream with a crowd loads");
        CHECK (std::abs (run (3.0) - 0.4f) < 0.01f, "with no radio the crowd is at rest (rest=4 of max=10)");
        CHECK (std::abs (run (0.1, 1) - 4.0f) < 0.001f, "and its count is the rest count");
        scq_room (1, 0, 0, 0);
        CHECK (run (3.0) < 0.01f, "a radio and an empty room: 0");
        scq_room (1, 1, 0.9f, 0.9f);
        CHECK (std::abs (run (3.0) - 0.1f) < 0.01f, "one device of ten: 0.1");
        scq_room (1, 5, 0.9f, 0.6f);
        CHECK (std::abs (run (3.0) - 0.5f) < 0.01f, "five devices of ten: 0.5 (more than one is counted)");
        CHECK (std::abs (run (0.1, 1) - 5.0f) < 0.001f, "count is the plain number, 5");
        CHECK (std::abs (run (0.1, 2) - 0.9f) < 0.01f && std::abs (run (0.1, 3) - 0.6f) < 0.01f, "near and strength come through");
        scq_room (1, 30, 1, 1);
        float soon = run (0.05);
        CHECK (soon > 0.5f && soon < 0.99f, "the crowd swells, it doesn't jump");
        CHECK (std::abs (run (3.0) - 1.0f) < 0.01f && std::abs (run (0.1, 1) - 30.0f) < 0.001f, "thirty of ten: out stops at 1, count says 30");

        const char* v = "visitors.arrive -> leds.track\nvisitors.voice -> out\n";
        CHECK (scq_load (v, (int) std::strlen (v), 0) == 0, "visitors need no naming");
        run (0.2);
        int arrive = 0, leave = 0, call = 0;
        scq_room_event (1, 0.25f); run (0.1, 0, &arrive);
        scq_room_event (0, 0.25f); run (0.1, 2, &call);
        CHECK (std::abs (run (0.01, 3) - 0.25f) < 0.001f, "voice holds the number of whoever called last");
        scq_room_event (0, 0.75f); run (0.1, 2, &call);
        scq_room_event (2, 0.25f); run (0.1, 1, &leave);
        CHECK (arrive == 1 && call == 2 && leave == 1, "one pulse for each arrival, call and leaving");
        CHECK (std::abs (run (0.01, 3) - 0.75f) < 0.001f, "and the voice moved to the next caller");
        scq_room (0, 0, 0, 0);
    }

    // ---- the instrument switch (SP-1 / Wingie): a text edit, nothing else changes
    {
        const juce::String sp1 = "; my dream\n; second note\n\n2026-11-24\n----------\nthe voice is sine 3\nvoice -> out\n;@ 0 voice 300 40\n";
        CHECK (scq::instrumentOf (sp1) == SCQ_SP1, "a dream without an instrument line is for the SP-1");
        auto w = scq::withInstrument (sp1, SCQ_WINGIE);
        CHECK (w == "; my dream\n; second note\ninstrument wingie\n\n2026-11-24\n----------\nthe voice is sine 3\nvoice -> out\n;@ 0 voice 300 40\n",
               "switching to the Wingie adds `instrument wingie` after the opening notes, every other line kept");
        CHECK (scq::instrumentOf (w) == SCQ_WINGIE, "and the dream is then for the Wingie");
        CHECK (scq::withInstrument (w, SCQ_WINGIE) == w, "switching to the Wingie again changes nothing");
        CHECK (scq::withInstrument (w, SCQ_SP1) == sp1, "switching back takes the line out: the text is as it was");
        CHECK (scq::withInstrument ("instrument sp1\nvoice -> out", SCQ_WINGIE) == "instrument wingie\nvoice -> out", "an `instrument sp1` line is replaced, not doubled");
        CHECK (scq::instrumentOf ("; instrument wingie (a note)\nvoice -> out") == SCQ_SP1, "an instrument named in a note doesn't count");
        CHECK (scq::instrumentOf ("the voice is sine\n  instrument wingie  ; the Wingie\n") == SCQ_WINGIE, "the line may be anywhere, indented, with a note after");
        scq::Document d;
        d.text = sp1;
        d.setInstrument (SCQ_WINGIE);
        CHECK (d.dirty && d.instrument() == SCQ_WINGIE, "Document::setInstrument edits the text (undoable: one text change)");
        CHECK (compile (d) == 0 && scq_instrument() == SCQ_WINGIE, "the engine reads it as a dream for the Wingie");
        d.text = d.text + "button1 -> voice.fm\n";
        CHECK (compile (d) > 0, "the SP-1's buttons are a mistake in a dream for the Wingie");
        d.setInstrument (SCQ_SP1);
        CHECK (compile (d) == 0 && scq_instrument() == SCQ_SP1, "and fine again once it is for the SP-1");

        auto starter = scq::starterDream (SCQ_WINGIE, "2026-11-24");
        d.text = starter;
        CHECK (compile (d) == 0 && scq_instrument() == SCQ_WINGIE, "the starter dream for the Wingie has no mistakes");
        bool mic = false, keys = false, faders = true, lights = false;
        for (auto* name : { "fader1", "fader2", "fader3" }) faders = faders && d.nodeNamed (name) != nullptr;
        mic = d.nodeNamed ("mic") != nullptr; keys = d.nodeNamed ("keys1") != nullptr; lights = d.nodeNamed ("lights") != nullptr;
        CHECK (mic && keys && faders && lights, "it uses the microphones, the left keys, the three faders and the lights");
        d.text = scq::starterDream (SCQ_SP1, "2026-11-24");
        CHECK (compile (d) == 0 && scq_instrument() == SCQ_SP1, "the starter dream for the SP-1 is as before");

        // the catalog's I lines: each instrument's own hands
        auto* mp = scq::findPlace ("mic"); auto* bp = scq::findPlace ("button"); auto* fp = scq::findPlace ("fader"); auto* lp = scq::findPlace ("lights");
        CHECK (mp && mp->only == SCQ_WINGIE && lp && lp->only == SCQ_WINGIE && bp && bp->only == SCQ_SP1 && fp && fp->only < 0,
               "the catalog marks the Wingie's hands, the SP-1's, and the faders both have");
        CHECK (scq::findPlace ("comb")->on (SCQ_WINGIE) && scq::findPlace ("comb")->on (SCQ_SP1), "a processing place shows for both");

        // downsampling a place in a dream for the Wingie, as in one for the SP-1: the estimate follows
        d.text = starter;
        compile (d);
        const float full = d.load;
        auto* ring = d.nodeNamed ("ring");
        CHECK (ring != nullptr, "the starter's resonator is there");
        bool hasRate = false;
        if (auto* rp = scq::findPlace ("comb")) for (auto& o : rp->opts) hasRate = hasRate || (o.name == "samplerate" && o.positions.contains ("half"));
        CHECK (hasRate, "the resonator has a samplerate switch in a Wingie dream too");
        if (ring) d.setOption (ring->id, "samplerate", "quarter");
        CHECK (d.text.contains ("the ring is comb 2 samplerate=quarter") || d.text.contains ("samplerate=quarter"), "setting it is a text edit, as for the SP-1");
        CHECK (compile (d) == 0 && d.load < full, "and the Wingie's load estimate drops");
        std::printf ("     (%.3f -> %.3f)\n", (double) full, (double) d.load);
    }

    std::printf (failures ? "\n%d FAILED\n" : "\nall good\n", failures);
    return failures ? 1 : 0;
}
