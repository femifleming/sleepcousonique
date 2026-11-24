#include "Model.h"
#include "scq.h"

namespace scq
{
juce::String fmt (float v)
{
    if (std::abs (v - std::round (v)) < 0.0005f) return juce::String ((int) std::round (v));
    auto s = juce::String (v, 3);
    while (s.endsWithChar ('0')) s = s.dropLastCharacters (1);
    if (s.endsWithChar ('.')) s = s.dropLastCharacters (1);
    return s;
}

static Family familyFrom (const juce::String& f)
{
    if (f == "shape") return Shape;
    if (f == "space") return Space;
    if (f == "current") return Current;
    if (f == "hands") return Hands;
    if (f == "ears") return Ears;
    return Source;
}

const std::vector<PlaceInfo>& catalog()
{
    static std::vector<PlaceInfo> places = []
    {
        std::vector<PlaceInfo> v;
        auto lines = juce::StringArray::fromLines (juce::String::fromUTF8 (scq_catalog()));
        for (auto& l : lines)
        {
            if (l.startsWith ("D ") && ! v.empty())
            {
                auto t = juce::StringArray::fromTokens (l, " ", "");
                for (int i = 2; i < t.size(); ++i) v.back().defs.push_back (t[i].getFloatValue());
                continue;
            }
            if (l.startsWith ("O ") && ! v.empty())
            {
                auto t = juce::StringArray::fromTokens (l, " ", "");
                for (int i = 2; i < t.size(); ++i)
                {
                    OptSpec o;
                    o.name = t[i].upToFirstOccurrenceOf ("=", false, false);
                    auto vals = t[i].fromFirstOccurrenceOf ("=", false, false);
                    if (juce::CharacterFunctions::isLetter (vals[0])) o.positions = juce::StringArray::fromTokens (vals, "|", "");
                    o.def = o.positions.isEmpty() ? vals : o.positions[0];
                    v.back().opts.push_back (o);
                }
                continue;
            }
            if (l.startsWith ("I "))       // hands only one instrument has
            {
                auto t = juce::StringArray::fromTokens (l, " ", "");
                for (auto& q : v)
                    if (t.size() >= 3 && q.name == t[1]) q.only = t[2] == "wingie" ? 1 : 0;
                continue;
            }
            if (! l.startsWith ("P ")) continue;
            auto parts = juce::StringArray::fromTokens (l, "|", "");
            if (parts.size() < 3) continue;
            auto head = juce::StringArray::fromTokens (parts[0].trim(), " ", "");
            PlaceInfo p;
            p.name = head[1];
            p.family = familyFrom (head[2]);
            p.cost = head[3].getIntValue();
            p.nature = head[4].getFloatValue();
            if (head.size() > 5) p.ins = juce::StringArray::fromTokens (head[5], ",", "");
            p.outs = juce::StringArray::fromTokens (parts[1].trim(), ",", "");
            p.help = parts[2].trim();
            for (int i = 3; i < parts.size(); ++i) p.help << "|" << parts[i];
            p.tune = p.ins.contains ("pitch");
            p.att = p.family == Source || p.tune;     // same rule as place_has_att() in the engine
            v.push_back (p);
        }
        return v;
    }();
    return places;
}

float knobDefault (const PlaceInfo& p, int i) { return i >= 0 && i < (int) p.defs.size() ? p.defs[(size_t) i] : 0.0f; }

bool knobRange (const PlaceInfo& p, int i, float& lo, float& hi)
{
    if (i < 0 || i >= p.ins.size() || p.family == Hands || p.family == Ears) return false;
    auto n = p.ins[i];
    static const juce::StringArray none { "in", "in1", "in2", "in3", "in4", "a", "b", "mod", "trig", "trig1", "trig2", "strike",
                                          "pluck", "gate", "clock", "reset", "sync", "next", "back", "learn", "follow", "s1", "s2",
                                          "s3", "s4", "s5", "s6", "s7", "s8", "x", "y" };
    if (none.contains (n)) return false;                // sound and pulses come from cords, not knobs
    if (n == "pitch")
    {
        if (p.tune) return false;                       // the tune knob is its pitch knob
        lo = -4; hi = 4; return true;
    }
    struct Bi { const char* name; float r; };
    static const Bi bi[] = { { "fm", 1 }, { "width", 1 }, { "root", 1 }, { "cutoff", 3 }, { "shift", 2 }, { "pan", 1 },
                             { "spread", 1 }, { "rate", 4 }, { "ratio", 2 } };
    for (auto& b : bi) if (n == b.name) { lo = -b.r; hi = b.r; if (n == "rate" || n == "ratio") { lo = 0; } return true; }
    float d = knobDefault (p, i);
    lo = juce::jmin (0.0f, d); hi = juce::jmax (1.0f, d * 2);
    return true;
}

const PlaceInfo* findPlace (const juce::String& name)
{
    for (auto& p : catalog()) if (p.name == name) return &p;
    return nullptr;
}

Signal outletSignal (const PlaceInfo& p, int port)
{
    auto o = p.outs[port];
    if (o == "pulse" || o == "gate" || o == "trig" || o == "end" || o == "up" || o == "down" || o == "arrive" || o == "leave") return Pulse;
    if (p.name == "button" || p.name == "play" || p.name == "dots") return Pulse;
    if (p.family == Current || p.family == Hands) return Flow;
    if (p.name == "leds") return Flow;
    return Sound;
}

// ------------------------------------------------------------------------------------------

void Document::readDescription (const char* describe, const char* msgs)
{
    nodes.clear(); cords.clear(); scenes.clear(); messages.clear();
    errors = 0;
    for (auto& l : juce::StringArray::fromLines (juce::String::fromUTF8 (describe)))
    {
        auto t = juce::StringArray::fromTokens (l, " ", "");
        if (t.isEmpty()) continue;
        if (t[0] == "S" && t.size() >= 4)
        {
            SceneInfo s;
            s.current = t[3] == "1";
            s.title = l.fromFirstOccurrenceOf (" ", false, false).fromFirstOccurrenceOf (" ", false, false)
                       .fromFirstOccurrenceOf (" ", false, false).fromFirstOccurrenceOf (" ", false, false).trim();
            if (s.current) scene = (int) scenes.size();
            scenes.push_back (s);
        }
        else if (t[0] == "N" && t.size() >= 6)
        {
            NodeInfo n { t[1].getIntValue(), t[2], t[4], t[3].getFloatValue(), t[5].getIntValue(), {} };
            if (t.size() > 6)
                for (auto& kv : juce::StringArray::fromTokens (t[6], ",", ""))
                    n.opts[kv.upToFirstOccurrenceOf ("=", false, false)] = kv.fromFirstOccurrenceOf ("=", false, false);
            nodes.push_back (n);
        }
        else if (t[0] == "C" && t.size() >= 9)
            cords.push_back ({ t[1].getIntValue(), t[2].getIntValue(), t[3].getIntValue(), t[4].getIntValue(),
                               t[5].getIntValue(), t[6].getFloatValue(), t[7].getFloatValue(), t[8].getIntValue() });
        else if (t[0] == "R" && t.size() >= 7)
        {
            places = t[1].getIntValue(); placesMost = juce::jmax (1, t[2].getIntValue());
            cordsIn = t[3].getIntValue(); cordsMost = juce::jmax (1, t[4].getIntValue());
            textBytes = t[5].getIntValue(); textMost = juce::jmax (1, t[6].getIntValue());
        }
        else if (t[0] == "M" && t.size() >= 4)
        {
            poolUsed = t[1].getIntValue();
            poolTotal = juce::jmax (1, t[2].getIntValue());
            load = t[3].getFloatValue();
        }
    }
    for (auto& l : juce::StringArray::fromLines (juce::String::fromUTF8 (msgs)))
    {
        if (l.length() < 4) continue;
        Message m;
        m.error = l[0] == 'E';
        m.line = l.substring (2).getIntValue();
        m.text = l.substring (2).fromFirstOccurrenceOf (" ", false, false);
        if (m.error) ++errors;
        messages.push_back (m);
    }
}

float Document::room (juce::String* what) const
{
    struct Limit { float use; juce::String say; };
    const Limit all[] = {
        { (float) poolUsed / (float) juce::jmax (1, poolTotal), "memory " + juce::String (juce::roundToInt ((float) poolUsed / 1024.0f)) + " of " + juce::String (juce::roundToInt ((float) poolTotal / 1024.0f)) + " KB" },
        { (float) places / (float) placesMost, "places " + juce::String (places) + " of " + juce::String (placesMost) },
        { (float) cordsIn / (float) cordsMost, "cords " + juce::String (cordsIn) + " of " + juce::String (cordsMost) },
        { (float) textBytes / (float) textMost, "text " + juce::String (juce::roundToInt ((float) textBytes / 1024.0f)) + " of " + juce::String (juce::roundToInt ((float) textMost / 1024.0f)) + " KB" } };
    const Limit* worst = &all[0];
    for (auto& l : all) if (l.use > worst->use) worst = &l;
    if (what) *what = worst->say;
    return worst->use;
}

const NodeInfo* Document::node (int id) const
{
    for (auto& n : nodes) if (n.id == id) return &n;
    return nullptr;
}

const NodeInfo* Document::nodeNamed (const juce::String& name) const
{
    for (auto& n : nodes) if (n.name == name) return &n;
    return nullptr;
}

const PlaceInfo* Document::placeOf (int id) const
{
    auto* n = node (id);
    return n ? findPlace (n->place) : nullptr;
}

// ------------------------------------------------------------------------------------------ text

juce::StringArray Document::lines() const
{
    juce::StringArray ls;
    ls.addLines (text);
    if (text.endsWithChar ('\n')) ls.add ({});
    return ls;
}

void Document::setLines (const juce::StringArray& ls)
{
    text = ls.joinIntoString ("\n");
    dirty = true;
}

static bool isDashes (const juce::String& s)
{
    auto t = s.trim();
    return t.length() >= 3 && t.containsOnly ("-");
}

static bool isBlank (const juce::String& s)
{
    auto t = s.trim();
    return t.isEmpty() || t.startsWithChar (';');
}

std::vector<Document::Span> Document::sceneSpans() const
{
    auto ls = lines();
    std::vector<Span> v;
    for (int i = 0; i + 1 < ls.size(); ++i)
        if (! isBlank (ls[i]) && isDashes (ls[i + 1]))
        {
            if (! v.empty()) v.back().last = i - 1;
            v.push_back ({ i, i + 2, ls.size() - 1 });
            ++i;
        }
    if (v.empty()) v.push_back ({ -1, 0, ls.size() - 1 });
    return v;
}

int Document::sceneOfLine (int line0) const
{
    auto sp = sceneSpans();
    for (int k = (int) sp.size(); --k >= 0;)
        if (line0 >= sp[(size_t) k].header && sp[(size_t) k].header >= 0) return k;
    return sp.front().header < 0 ? 0 : -1;
}

void Document::addLines (const juce::StringArray& add)
{
    auto ls = lines();
    auto sp = sceneSpans();
    auto s = sp[(size_t) juce::jlimit (0, (int) sp.size() - 1, scene)];
    int at = s.first;
    for (int i = s.first; i <= s.last && i < ls.size(); ++i)
        if (ls[i].trim().isNotEmpty() && ! ls[i].startsWith (";@")) at = i + 1;
    for (int k = 0; k < add.size(); ++k) ls.insert (at + k, add[k]);
    setLines (ls);
}

void Document::addLine (const juce::String& line) { addLines (juce::StringArray (line)); }

juce::String Document::uniqueName (const juce::String& place) const
{
    auto ok = [this] (const juce::String& n)
    {
        if (nodeNamed (n)) return false;
        if (findPlace (n)) return false;
        return true;
    };
    if (ok (place) || (! nodeNamed (place))) return place;
    for (char c = 'b'; c <= 'z'; ++c)
        if (ok (place + juce::String::charToString (c))) return place + juce::String::charToString (c);
    return place + "x";
}

juce::String Document::addPlace (const juce::String& place)
{
    auto name = uniqueName (place);
    addLine ("the " + name + " is " + place);
    return name;
}

void Document::setOption (int id, const juce::String& key, const juce::String& value)
{
    auto* n = node (id);
    auto* p = n ? findPlace (n->place) : nullptr;
    if (! p || n->line < 1) return;
    auto opts = n->opts;
    opts[key] = value;
    for (auto& m : nodes) if (m.id == id) m.opts = opts;   // the tapestry and the settings column draw from this until the dream is next read
    juce::String written;
    for (auto& o : p->opts)                       // in the place's own order, defaults left out
    {
        auto it = opts.find (o.name);
        if (it != opts.end() && it->second != o.def) written << " " << o.name << "=" << it->second;
    }
    for (int i = 0; i < p->ins.size(); ++i)       // the panel knobs, in inlet order, defaults left out
    {
        auto& in = p->ins[i];
        bool isOpt = false;
        for (auto& o : p->opts) if (o.name == in) isOpt = true;
        auto it = opts.find (in);
        if (! isOpt && it != opts.end() && std::abs (it->second.getFloatValue() - knobDefault (*p, i)) > 1.0e-4f)
            written << " " << in << "=" << it->second;
    }
    for (auto& in : p->ins)                       // then the attenuverters, in inlet order, 1 left out
    {
        auto it = opts.find ("att." + in);
        if (it != opts.end() && it->second.getFloatValue() != 1.0f) written << " att." << in << "=" << it->second;
    }
    auto ls = lines();
    auto& l = ls.getReference (n->line - 1);
    auto code = l.upToFirstOccurrenceOf (";", false, false), note = l.fromFirstOccurrenceOf (";", true, false);
    auto words = juce::StringArray::fromTokens (code.upToFirstOccurrenceOf (":", false, false), " \t", "");
    auto steps = code.fromFirstOccurrenceOf (":", true, false).trim();
    words.removeEmptyStrings();
    bool decl = words.size() >= 4 && (words[0] == "the" || words[0] == "The") && (words[2] == "is" || words[2] == "are");
    if (decl)
    {
        juce::String head = words[0] + " " + words[1] + " " + words[2] + " " + words[3];
        if (words.size() > 4 && words[4].isNotEmpty() && words[4].containsOnly ("0123456789.-")) head << " " << words[4];
        l = head + written + (steps.isNotEmpty() ? juce::String (steps.startsWith (":") ? "" : " ") + steps : juce::String())
            + (note.isNotEmpty() ? "   " + note : juce::String());
    }
    else                                          // used without a name: name it here
    {
        juce::String nat = n->name.fromFirstOccurrenceOf (n->place, false, false);
        ls.insert (n->line - 1, "the " + n->name + " is " + n->place + (nat.isNotEmpty() ? " " + nat : juce::String()) + written);
    }
    setLines (ls);
}

static juce::String portName (const juce::StringArray& ports, int i)
{
    return i > 0 && i < ports.size() ? "." + ports[i] : juce::String();
}

juce::String Document::cordText (const CordInfo& c) const
{
    auto* dn = node (c.dst);
    auto* dp = placeOf (c.dst);
    if (! dn || ! dp) return {};
    auto dst = dn->name + portName (dp->ins, c.dport);
    if (c.src < 0) return "value " + fmt (c.amt) + " -> " + dst;
    auto* sn = node (c.src);
    auto* sp = placeOf (c.src);
    if (! sn || ! sp) return {};
    auto src = sn->name + portName (sp->outs, c.sport);
    if (c.kind == Range) return "lucid " + src + ": " + dst + " " + fmt (c.amt) + ".." + fmt (c.amt2);
    juce::String arrow;
    switch (c.kind)
    {
        case Hall:  arrow = std::abs (c.amt - 100) < 0.01f ? "->" : "-" + fmt (c.amt) + ">"; break;
        case Fog:   arrow = std::abs (c.amt - 50) < 0.01f ? "~>" : "~" + fmt (c.amt) + ">"; break;
        case Water: arrow = std::abs (c.amt - 60) < 0.01f ? "=>" : "=" + fmt (c.amt) + ">"; break;
        case Door:  arrow = ">|"; break;
        case Maybe: arrow = std::abs (c.amt - 50) < 0.01f ? "?>" : "?" + fmt (c.amt) + ">"; break;
        default:    arrow = "->"; break;
    }
    return src + " " + arrow + " " + dst;
}

void Document::expandLineExcept (juce::StringArray& ls, int line1, int dropCord, std::function<void (CordInfo&)> change, int changeCord, const std::set<int>* drops) const
{
    int i0 = line1 - 1;
    if (i0 < 0 || i0 >= ls.size()) return;
    auto original = ls[i0];
    auto indent = original.substring (0, original.length() - original.trimStart().length());
    auto comment = original.fromFirstOccurrenceOf (";", true, false);
    juce::StringArray out;
    for (int k = 0; k < (int) cords.size(); ++k)
    {
        if (cords[(size_t) k].line != line1 || k == dropCord || (drops && drops->count (k))) continue;
        auto c = cords[(size_t) k];
        if (k == changeCord && change) change (c);
        out.add (indent + cordText (c));
    }
    if (comment.isNotEmpty())
    {
        if (out.isEmpty()) out.add (indent + comment);
        else out.set (0, out[0] + "   " + comment);
    }
    ls.remove (i0);
    for (int k = 0; k < out.size(); ++k) ls.insert (i0 + k, out[k]);
}

void Document::addCord (int src, int sport, int dst, int dport, Kind kind)
{
    CordInfo c { src, sport, dst, dport, (int) kind, kind == Fog ? 50.f : (kind == Water ? 60.f : 100.f), 0, 0 };
    auto t = cordText (c);
    if (t.isNotEmpty()) addLine (t);
}

void Document::addControl (const juce::String& control, int dst, int dport)
{
    auto* dn = node (dst);
    auto* dp = placeOf (dst);
    if (! dn || ! dp) return;
    addLine (control + " -> " + dn->name + portName (dp->ins, dport));
}

void Document::removeCord (int ci)
{
    if (ci < 0 || ci >= (int) cords.size()) return;
    auto ls = lines();
    expandLineExcept (ls, cords[(size_t) ci].line, ci);
    setLines (ls);
}

void Document::removeCords (const std::set<int>& drop)
{
    std::set<int, std::greater<int>> touched;          // bottom line first, so the lines above keep their numbers
    for (int k : drop) if (k >= 0 && k < (int) cords.size()) touched.insert (cords[(size_t) k].line);
    if (touched.empty()) return;
    auto ls = lines();
    for (int line1 : touched) expandLineExcept (ls, line1, -1, {}, -1, &drop);
    setLines (ls);
}

void Document::setCordAmount (int ci, float a, float a2)
{
    if (ci < 0 || ci >= (int) cords.size()) return;
    auto ls = lines();
    // only what the language can write: a cord passes 0 % or more (the attenuverter is what
    // inverts), a maybe-cord's chance is 0 to 100, a water cord takes a millisecond or more
    auto& was = cords[(size_t) ci];
    if (was.src >= 0)
    {
        if (was.kind == Hall || was.kind == Fog) a = juce::jmax (0.0f, a);
        else if (was.kind == Maybe) a = juce::jlimit (0.0f, 100.0f, a);
        else if (was.kind == Water) a = juce::jmax (1.0f, a);
    }
    expandLineExcept (ls, was.line, -1, [a, a2] (CordInfo& c) { c.amt = a; if (c.kind == Range) c.amt2 = a2; }, ci);
    setLines (ls);
}

void Document::setCordKind (int ci, Kind k)
{
    if (ci < 0 || ci >= (int) cords.size()) return;
    auto ls = lines();
    expandLineExcept (ls, cords[(size_t) ci].line, -1, [k] (CordInfo& c)
    {
        c.kind = k;
        c.amt = k == Fog ? 50.f : (k == Water ? 60.f : 100.f);
    }, ci);
    setLines (ls);
}

int Document::cordsOnLine (int line) const
{
    int n = 0;
    for (auto& c : cords) if (c.line == line) ++n;
    return n;
}

void Document::removeNodes (const std::set<int>& ids)
{
    auto ls = lines();
    std::set<int> touched, decls;
    for (auto& c : cords) if (ids.count (c.src) || ids.count (c.dst)) touched.insert (c.line);
    for (int id : ids)
        if (auto* n = node (id))
            if (n->line > 0 && n->line <= ls.size() && ls[n->line - 1].trimStart().startsWithIgnoreCase ("the "))
                decls.insert (n->line);
    std::set<int> all = touched;
    all.insert (decls.begin(), decls.end());
    for (auto it = all.rbegin(); it != all.rend(); ++it)
    {
        int line1 = *it, i0 = line1 - 1;
        if (decls.count (line1) && ! touched.count (line1)) { ls.remove (i0); continue; }
        auto original = ls[i0];
        auto indent = original.substring (0, original.length() - original.trimStart().length());
        juce::StringArray out;
        for (auto& c : cords)
            if (c.line == line1 && ! ids.count (c.src) && ! ids.count (c.dst)) out.add (indent + cordText (c));
        ls.remove (i0);
        for (int k = 0; k < out.size(); ++k) ls.insert (i0 + k, out[k]);
    }
    setLines (ls);
}

juce::String Document::copyNodes (const std::vector<int>& ids) const
{
    auto ls = lines();
    std::set<int> set (ids.begin(), ids.end());
    juce::StringArray out;
    for (int id : ids)
    {
        auto* n = node (id);
        if (n && n->line > 0 && n->line <= ls.size() && ls[n->line - 1].trimStart().startsWithIgnoreCase ("the "))
            out.add (ls[n->line - 1].trim());
    }
    for (auto& c : cords)
        if (set.count (c.dst) && (c.src < 0 || set.count (c.src))) out.add (cordText (c));
    return out.joinIntoString ("\n");
}

static bool wordChar (juce::juce_wchar c) { return juce::CharacterFunctions::isLetterOrDigit (c) || c == '_'; }

// `from` -> `to` wherever it stands as a whole word, from column `start` on
static juce::String renameWord (const juce::String& line, const juce::String& from, const juce::String& to, int start = 0)
{
    juce::String out = line.substring (0, start);
    int i = start;
    while (i < line.length())
    {
        // a whole word, and not a port (the "decay" of "env.decay" is an inlet, not the place called decay)
        if (line.substring (i).startsWith (from) && (i == 0 || (! wordChar (line[i - 1]) && line[i - 1] != '.'))
            && (i + from.length() >= line.length() || ! wordChar (line[i + from.length()])))
        {
            out << to;
            i += from.length();
        }
        else out << line[i++];
    }
    return out;
}

// "the NAME is PLACE ...": where the name and the place stand in the line (false if it isn't one)
static bool declaration (const juce::String& line, juce::Range<int>& name, juce::Range<int>& place)
{
    int i = 0;
    juce::Range<int> w[4];
    for (int k = 0; k < 4; ++k)
    {
        while (i < line.length() && juce::CharacterFunctions::isWhitespace (line[i])) ++i;
        int a = i;
        while (i < line.length() && ! juce::CharacterFunctions::isWhitespace (line[i]) && line[i] != ';') ++i;
        if (i == a) return false;
        w[k] = { a, i };
    }
    auto word = [&line, &w] (int k) { return line.substring (w[k].getStart(), w[k].getEnd()); };
    if (! word (0).equalsIgnoreCase ("the") || (word (2) != "is" && word (2) != "are")) return false;
    name = w[1]; place = w[3];
    return true;
}

juce::StringArray Document::renamedForPaste (const juce::StringArray& pasted, juce::StringArray& newNames) const
{
    auto lines = pasted;
    std::set<juce::String> taken;
    for (auto& n : nodes) taken.insert (n.name);
    for (int i = 0; i < lines.size(); ++i)
    {
        juce::Range<int> nameAt, placeAt;
        if (! declaration (lines[i], nameAt, placeAt)) continue;
        auto name = lines[i].substring (nameAt.getStart(), nameAt.getEnd());
        if (taken.count (name))
        {
            juce::String fresh;
            for (int n = 2; fresh.isEmpty(); ++n)                 // comb -> combb, combc ... then comb27
            {
                auto tryName = n <= 26 ? name + juce::String::charToString ((juce::juce_wchar) ('a' + n - 1)) : name + juce::String (n);
                if (! taken.count (tryName) && ! findPlace (tryName)) fresh = tryName;
            }
            // The name changes wherever the pasted cords use it. In a declaration only the name
            // itself does: the place it is (the word after "is") and its switches and params stay,
            // even when they are spelled the same. "the comb is comb" becomes "the combb is comb"
            // (there is no place called combb), "the decay is decay decay=0.9" keeps its decay=.
            for (int k = 0; k < lines.size(); ++k)
            {
                juce::Range<int> nk, pk;
                auto& l = lines.getReference (k);
                if (! declaration (l, nk, pk)) l = renameWord (l, name, fresh);
                else if (l.substring (nk.getStart(), nk.getEnd()) == name && k == i)
                    l = l.substring (0, nk.getStart()) + fresh + l.substring (nk.getEnd());
            }
            name = fresh;
        }
        taken.insert (name);
        newNames.add (name);
    }
    return lines;
}

// ------------------------------------------------------------------------------------------ positions

void Document::readPositions()
{
    positions.clear();
    for (auto& l : lines())
        if (l.startsWith (";@"))
        {
            auto t = juce::StringArray::fromTokens (l.substring (2).trim(), " ", "");
            if (t.size() >= 4) positions[t[0] + ":" + t[1]] = { t[2].getFloatValue(), t[3].getFloatValue() };
        }
}

void Document::setPosition (int sceneIdx, const juce::String& name, juce::Point<float> p)
{
    positions[key (sceneIdx, name)] = p;
}

void Document::writePositions()
{
    auto ls = lines();
    for (int i = ls.size(); --i >= 0;)
        if (ls[i].startsWith (";@")) ls.remove (i);
    while (ls.size() > 0 && ls[ls.size() - 1].trim().isEmpty()) ls.remove (ls.size() - 1);
    if (! positions.empty())
    {
        ls.add ({});
        for (auto& [k, p] : positions)
            ls.add (";@ " + k.upToFirstOccurrenceOf (":", false, false) + " " + k.fromFirstOccurrenceOf (":", false, false)
                    + " " + juce::String ((int) p.x) + " " + juce::String ((int) p.y));
    }
    ls.add ({});
    setLines (ls);
}

// ------------------------------------------------------------------------------------------ instruments

// the word after `instrument` on this line (outside a note), or "" if it isn't an instrument line
static juce::String instrumentWord (const juce::String& line)
{
    auto code = line.upToFirstOccurrenceOf (";", false, false).trim();
    if (! code.startsWith ("instrument")) return {};
    auto rest = code.substring (10);
    if (rest.isEmpty() || ! (rest[0] == ' ' || rest[0] == '\t')) return {};
    return rest.trim().upToFirstOccurrenceOf (" ", false, false).toLowerCase();
}

int instrumentOf (const juce::String& text)
{
    for (auto& l : juce::StringArray::fromLines (text))
    {
        auto w = instrumentWord (l);
        if (w == "wingie") return SCQ_WINGIE;
        if (w == "sp1" || w == "sp-1") return SCQ_SP1;
    }
    return SCQ_SP1;
}

juce::String withInstrument (const juce::String& text, int instrument)
{
    juce::StringArray ls;
    ls.addLines (text);
    const bool endsNl = text.endsWithChar ('\n');
    for (int i = ls.size(); --i >= 0;)
        if (instrumentWord (ls[i]).isNotEmpty()) ls.remove (i);
    if (instrument == SCQ_WINGIE)
    {
        int at = 0;                 // after the notes the dream opens with (its title); never among the positions
        while (at < ls.size() && ls[at].trimStart().startsWithChar (';') && ! ls[at].startsWith (";@")) ++at;
        ls.insert (at, "instrument wingie");
    }
    auto out = ls.joinIntoString ("\n");
    if (endsNl && ! out.endsWithChar ('\n')) out << "\n";
    return out;
}

juce::String starterDream (int instrument, const juce::String& date)
{
    auto rule = juce::String::repeatedString ("-", date.length());
    if (instrument != SCQ_WINGIE)
        return "; a new dream\n\n" + date + "\n" + rule + "\nthe voice is sine 3\nvoice -> out\n";
    return "; a new dream for the Wingie: what the microphones hear rings in a resonator that the\n"
           "; left keys tune; Mix blends it with the room, Decay lets it ring, Volume\n"
           "instrument wingie\n\n"
           + date + "\n" + rule + "\n"
           "the ring is comb 2\n"
           "the blend is crossfade\n"
           "the level is vca\n"
           "\n"
           "mic.both -> ring\n"
           "mic.both -> blend.a\n"
           "ring -> blend.b\n"
           "keys1.pitch -> ring.pitch\n"
           "fader1 -> blend.mix\n"
           "fader2 -> ring.decay\n"
           "blend -> level -> out\n"
           "fader3 -> level.gain\n"
           "mic.both -> lights.left\n"
           "ring -> lights.right\n";
}
}
