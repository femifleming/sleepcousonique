#include "Pop.h"
#include "Art.h"

using namespace juce;
using namespace scq;

// ------------------------------------------------------------------------------------------ browser

static const char* const familyNames[5] = { "sources", "shapes", "spaces", "currents", "hands" };
// hands that are always part of the instrument: used by their names, never placed
static const StringArray alwaysThere { "fader", "keys", "mode", "octave", "source", "mic", "line", "button", "rocker", "play", "dots" };

PlaceBrowser::PlaceBrowser()
{
    search.setTextToShowWhenEmpty ("find a place", Colours::grey);
    search.setJustification (Justification::centredLeft);
    search.setSelectAllWhenFocused (true);
    search.onTextChange = [this] { filter = search.getText().trim().toLowerCase(); hover = -1; repaint(); };
    search.onReturnKey = [this]
    {
        for (auto& c : cells) if (matches (c)) { pick (c); break; }
    };
    search.onEscapeKey = [this] { search.clear(); filter.clear(); repaint(); };
    addAndMakeVisible (search);
    build();
}

void PlaceBrowser::setInstrument (int i)
{
    if (i == instrument && ! cells.empty()) return;
    instrument = i;
    build();
    resized();
    repaint();
}

void PlaceBrowser::build()
{
    cells.clear();
    int counts[5] = { 0, 0, 0, 0, 0 };
    for (auto& p : catalog())
    {
        const bool midi = p.name.startsWith ("midi");          // MIDI to CV: placed like any module
        if (p.family == Ears) continue;
        if (p.family == Hands && ! midi && ! p.on (instrument)) continue;   // the other instrument's hands
        auto what = p.help.upToFirstOccurrenceOf (" (after", false, false);
        const int col = midi ? (int) Current : (int) p.family;
        cells.push_back ({ p.name, what, col, counts[col]++, {} });
    }
}

bool PlaceBrowser::matches (const Cell& c) const
{
    return filter.isEmpty() || c.name.contains (filter) || c.help.toLowerCase().contains (filter);
}

void PlaceBrowser::pick (const Cell& c)
{
    if (c.family == 4 && alwaysThere.contains (c.name))
    {
        if (onHover) onHover (c.name + " is always there on the " + String (instrument == 1 ? "Wingie" : "SP-1")
                              + ": drag its loop from the hands panel onto an inlet, or write  "
                              + (c.name == "keys" ? "keys1.pitch" : c.name == "mic" ? "mic.both" : c.name == "line" ? "line.both" : c.name == "fader" || c.name == "button" || c.name == "mode" || c.name == "octave" ? c.name + "1" : c.name)
                              + " -> place.inlet");
        return;
    }
    if (onAdd) onAdd (c.name);
}

void PlaceBrowser::resized()
{
    int counts[5] = { 0, 0, 0, 0, 0 };
    for (auto& c : cells) counts[c.family]++;
    auto r = getLocalBounds().toFloat();
    auto side = r.removeFromLeft (112);
    search.setBounds (side.withHeight (24).reduced (0, 1).withTrimmedRight (10).toNearestInt());
    countArea = side.withTrimmedTop (28).withHeight (14).withTrimmedRight (10);
    const float gap = 10;
    const int rows = 4;
    // each grid as wide as its places need, all five filling the width
    float cell = r.getHeight() / (float) rows;
    float need = 0;
    int grids_n = 0;
    for (int f = 0; f < 5; ++f) if (counts[f] > 0) { need += std::ceil ((float) counts[f] / rows) * cell; ++grids_n; }
    float scale = jmin (1.0f, (r.getWidth() - gap * (float) jmax (0, grids_n - 1)) / jmax (1.0f, need));
    cell *= scale;
    float x = r.getX();
    for (int f = 0; f < 5; ++f)
    {
        int cols = (int) std::ceil ((float) counts[f] / rows);
        grids[f] = { x, r.getY(), (float) cols * cell, (float) rows * cell };
        if (cols > 0) x += (float) cols * cell + gap;
    }
    for (auto& c : cells)
    {
        int col = c.index / 4, row = c.index % 4;
        c.r = { grids[c.family].getX() + (float) col * cell, grids[c.family].getY() + (float) row * cell, cell, cell };
    }
}

void PlaceBrowser::paint (Graphics& g)
{
    auto& t = art::theme();
    for (int f = 0; f < 5; ++f)
    {
        if (grids[f].isEmpty()) continue;
        g.setColour (t.chromeDim.withAlpha (0.5f));
        g.drawHorizontalLine ((int) grids[f].getBottom() + 2, grids[f].getX(), grids[f].getRight());
        g.setColour (t.chromeDim);
        g.setFont (art::label (8.5f));
        String nm = familyNames[f];
        if (f == 4) nm = instrument == 1 ? "wingie" : "sp-1";
        g.drawText (nm, grids[f].withTop (grids[f].getBottom() + 3).withHeight (11).withWidth (jmax (grids[f].getWidth(), 50.0f)), Justification::right);
    }
    int found = 0;
    for (int i = 0; i < (int) cells.size(); ++i)
    {
        auto& c = cells[(size_t) i];
        const bool match = matches (c);
        found += match ? 1 : 0;
        const float fade = match ? 1.0f : 0.18f;
        // faltung: a slanted tile in the family's colour, carrying a code (the place's first
        // letters), as the folded strips on the snd sleeves carry te1 / rlo3
        auto col = c.family == 4 ? t.cardSp1 : art::familyColour (c.family);
        bool lit = i == hover;
        if (t.modular)                    // a raised icon button with a coloured strip and the place's code
        {
            auto r = c.r.reduced (1.0f);
            g.setColour ((lit ? Colour (0xffe8e6e0) : t.button).withMultipliedAlpha (fade)); g.fillRect (r);
            if (match) art::bevel (g, r, ! lit);
            auto strip = r.reduced (3.0f).withHeight (r.getHeight() * 0.48f);
            g.setColour (col.withMultipliedAlpha (fade)); g.fillRect (strip);
            if (match) art::bevel (g, strip, true);
            g.setColour (Colours::black.withAlpha (fade)); g.setFont (art::tahoma (jmin (9.0f, r.getHeight() * 0.36f)));
            g.drawText (c.name.substring (0, 6), r.withTop (strip.getBottom()), Justification::centred);
            continue;
        }
        auto r = c.r.reduced (1.5f, c.r.getHeight() * 0.14f);
        auto p = art::slant (r, r.getHeight() * 0.35f);
        g.setColour ((lit ? col : col.withMultipliedSaturation (0.85f).withMultipliedBrightness (t.dark ? 0.85f : 1.0f)).withMultipliedAlpha (fade)); g.fillPath (p);
        if (lit || (match && filter.isNotEmpty())) { g.setColour (t.chromeText); g.strokePath (p, PathStrokeType (lit ? 1.2f : 0.8f)); }
        g.setColour (art::boardInk.withAlpha ((lit ? 1.0f : 0.8f) * fade));
        g.setFont (art::label (jmin (9.0f, r.getHeight() * 0.55f)));
        g.drawText (c.name.substring (0, 3), r, Justification::centred);
    }
    g.setColour (t.chromeDim);
    g.setFont (art::label (8.5f));
    g.drawText (filter.isEmpty() ? String ((int) cells.size()) + " places" : String (found) + " of " + String ((int) cells.size()) + (found ? "  (return adds)" : ""),
                countArea, Justification::left, true);
}

int PlaceBrowser::cellAt (Point<float> p) const
{
    for (int i = 0; i < (int) cells.size(); ++i)
        if (cells[(size_t) i].r.contains (p) && matches (cells[(size_t) i])) return i;
    return -1;
}

void PlaceBrowser::mouseMove (const MouseEvent& e)
{
    int h = cellAt (e.position);
    if (h == hover) return;
    hover = h;
    if (onHover)
    {
        if (h < 0) onHover ({});
        else
        {
            auto& c = cells[(size_t) h];
            onHover (c.name + ": " + c.help + (c.family == 4 && alwaysThere.contains (c.name) ? String ("   (always there: drag its loop from the hands panel)") : String ("   (click to add it)")));
        }
    }
    repaint();
}

void PlaceBrowser::mouseExit (const MouseEvent&) { hover = -1; repaint(); }

void PlaceBrowser::mouseDown (const MouseEvent& e)
{
    int h = cellAt (e.position);
    if (h >= 0) pick (cells[(size_t) h]);
}

// ------------------------------------------------------------------------------------------ inspector

Inspector::Inspector (Document& d, Host& h) : doc (d), host (h) {}

void Inspector::show (int n)
{
    if (n == node && ! rows.empty()) return;
    node = n;
    refresh();
}

void Inspector::refresh()
{
    if (dragging >= 0) return;                         // don't pull the bar out from under the mouse
    rows.clear();
    auto* nd = doc.node (node);
    auto* p = nd ? findPlace (nd->place) : nullptr;
    if (! p || (p->family >= Hands && ! p->name.startsWith ("midi"))) { node = -1; repaint(); return; }
    auto opt = [&] (const String& k, const String& def) { return nd->opts.count (k) ? nd->opts.at (k) : def; };
    if (p->tune) rows.push_back ({ Tune, "tune", "tune", -1, opt ("tune", "0").getFloatValue(), -12, 12, 0 });
    for (int k = 0; k < (int) p->opts.size(); ++k)
    {
        auto& o = p->opts[(size_t) k];
        if (o.name == "tune") continue;
        if (o.positions.isEmpty())
        {
            Row r { Param, o.name, o.name, k, opt (o.name, o.def).getFloatValue(), 0, 1, o.def.getFloatValue() };
            if (o.name == "ratio") { r.lo = 1; r.hi = 3; } else r.hi = jmax (1.0f, r.def * 4);
            rows.push_back (r);
        }
        else
        {
            Row r { Switch, o.name, o.name, k };
            r.positions = o.positions;
            r.pos = jmax (0, o.positions.indexOf (opt (o.name, o.def)));
            rows.push_back (r);
        }
    }
    for (int i = 0; i < p->ins.size(); ++i)            // the inlets' own knobs
    {
        float lo, hi;
        bool has = knobRange (*p, i, lo, hi);
        for (auto& o : p->opts) if (o.name == p->ins[i]) has = false;
        if (! has) continue;
        float def = knobDefault (*p, i);
        rows.push_back ({ Panel, p->ins[i], p->ins[i], i, opt (p->ins[i], String (def)).getFloatValue(), lo, hi, def });
    }
    if (p->att)
        for (int i = 0; i < p->ins.size(); ++i)
        {
            float range = p->ins[i] == "pitch" ? 10.0f : 2.0f;
            rows.push_back ({ Att, "att." + p->ins[i], "att." + p->ins[i], i, opt ("att." + p->ins[i], "1").getFloatValue(), -range, range, 1 });
        }
    layoutRows();
    repaint();
}

void Inspector::layoutRows()
{
    auto r = getLocalBounds().toFloat().reduced (10, 0);
    r.removeFromTop (44);
    const float h = jmin (52.0f, jmax (30.0f, r.getHeight() / (float) jmax (1, (int) rows.size())));
    for (auto& row : rows)
    {
        row.area = r.removeFromTop (h);
        row.bar = row.area.withTop (row.area.getY() + 20).withHeight (jmin (14.0f, h - 24));
    }
}

static String comma (float v, int places = 2) { return String (v, places).replaceCharacter ('.', ','); }

void Inspector::paint (Graphics& g)
{
    auto& t = art::theme();
    auto r = getLocalBounds().toFloat().reduced (10, 0);
    auto* nd = doc.node (node);
    if (node < 0 || ! nd)
    {
        g.setColour (t.chromeDim);
        g.setFont (art::label (11));
        g.drawFittedText ("select a place on the tapestry: its settings appear here", r.toNearestInt().withHeight (60), Justification::centredLeft, 3);
        return;
    }
    auto head = r.withHeight (30);
    auto* pl = findPlace (nd->place);
    if (t.modular)                        // a module strip: the triangle, the bold name, the place
    {
        art::drawModule (g, head, art::familyColour (pl ? (int) pl->family : 0), false, Colour (0xff4060ff));
        g.setColour (Colours::black); g.setFont (art::tahoma (12.0f, true));
        g.drawText (nd->name, head.withTrimmedLeft (20), Justification::centredLeft);
        g.setFont (art::tahoma (10.0f)); g.drawText (nd->place, head.reduced (6, 0), Justification::centredRight);
    }
    else
    {
    // faltung: the name large, the place small, a rule under them (Partials' section head)
    art::drawBlock (g, Rectangle<float> (head.getX(), head.getY() + 10, 16, 16), art::familyColour (pl ? (int) pl->family : 0), 4, false);
    g.setColour (t.chromeText);
    g.setFont (art::sans (16.0f, true));
    g.drawText (nd->name, head.withTrimmedLeft (26), Justification::centredLeft);
    g.setColour (t.chromeDim);
    g.setFont (art::label (10.0f));
    g.drawText (nd->place, head, Justification::centredRight);
    g.setColour (t.checkA);
    g.drawHorizontalLine ((int) head.getBottom() + 4, head.getX(), head.getRight());
    }

    for (int i = 0; i < (int) rows.size(); ++i)
    {
        auto& row = rows[(size_t) i];
        g.setColour (t.chromeText);
        g.setFont (art::label (11.5f));
        auto labelArea = row.area.withHeight (18);
        g.drawText (row.label, labelArea, Justification::left);
        String v = row.kind == Switch ? row.positions[row.pos]
                 : row.kind == Tune ? comma (row.value, 2) + " oct" : comma (row.value);
        g.drawText (v, labelArea, Justification::right);
        auto b = row.bar;
        if (row.kind == Switch)        // a segment per position, the chosen one checker
        {
            float w = b.getWidth() / (float) row.positions.size();
            for (int k = 0; k < row.positions.size(); ++k)
            {
                auto s = Rectangle<float> (b.getX() + w * (float) k, b.getY(), w - 2, b.getHeight());
                auto sp = art::slant (s, t.modular ? 0.0f : s.getHeight() * 0.45f);
                if (k == row.pos) { g.setColour (t.checkA); g.fillPath (sp); }
                g.setColour (k == row.pos ? t.checkA : t.chromeText.withAlpha (0.6f)); g.strokePath (sp, PathStrokeType (1.0f));
            }
            continue;
        }
        if (t.modular)                    // a dark value box beside a sunken track with a raised thumb
        {
            auto bb = b.withSizeKeepingCentre (b.getWidth(), 6.0f);
            g.setColour (Colours::white); g.fillRect (bb); art::bevel (g, bb, false);
            const float fr = jlimit (0.0f, 1.0f, (row.value - row.lo) / (row.hi - row.lo));
            auto th = Rectangle<float> (10, 16).withCentre ({ bb.getX() + fr * bb.getWidth(), bb.getCentreY() });
            g.setColour (t.button); g.fillRect (th); art::bevel (g, th, true);
            continue;
        }
        // Partials' bar: a slanted track, filled up to the value, an ink edge at the value
        auto bb = b.withSizeKeepingCentre (b.getWidth(), jmin (9.0f, b.getHeight()));
        const float k = bb.getHeight() * 0.6f, frac = jlimit (0.0f, 1.0f, (row.value - row.lo) / (row.hi - row.lo));
        g.setColour (t.rail); g.strokePath (art::slant (bb, k), PathStrokeType (1.0f));
        if (frac > 0.002f) { g.setColour (t.checkA.withAlpha (i == dragging ? 1.0f : 0.9f)); g.fillPath (art::slant (bb.withWidth (k + frac * (bb.getWidth() - k)), k)); }
        const float x = bb.getX() + k * 0.5f + frac * (bb.getWidth() - k);
        g.setColour (t.chromeText); g.drawLine (x, bb.getY() - 3, x, bb.getBottom() + 3, 1.6f);
    }
}

void Inspector::apply (Row& r, bool live)
{
    if (live)
    {
        if (r.kind == Tune) host.setTune (node, r.value);
        else if (r.kind == Att) host.setAtt (node, r.index, r.value);
        else if (r.kind == Param) host.setParam (node, r.index, r.value);
        else if (r.kind == Panel) host.setKnob (node, r.index, r.value);
        if (onStatus) onStatus (r.label + " " + comma (r.value) + "   (double-click: back to " + comma (r.def) + ")");
        repaint();
        return;
    }
    String v = r.kind == Switch ? r.positions[r.pos] : fmt (r.value);
    doc.setOption (node, r.key, v);
    if (onEdited) onEdited (r.kind == Switch);         // switches are read when the dream is read
}

void Inspector::mouseDown (const MouseEvent& e)
{
    for (int i = 0; i < (int) rows.size(); ++i)
    {
        auto& r = rows[(size_t) i];
        if (! r.area.contains (e.position)) continue;
        if (r.kind == Switch)
        {
            float w = r.bar.getWidth() / (float) r.positions.size();
            int k = r.bar.expanded (0, 6).contains (e.position) ? jlimit (0, r.positions.size() - 1, (int) ((e.position.x - r.bar.getX()) / w))
                                                                 : (r.pos + 1) % r.positions.size();
            if (k != r.pos) { r.pos = k; apply (r, false); }
            return;
        }
        if (e.getNumberOfClicks() > 1) { r.value = r.def; apply (r, true); apply (r, false); return; }
        dragging = i;
        mouseDrag (e);
        return;
    }
}

void Inspector::mouseDrag (const MouseEvent& e)
{
    if (dragging < 0) return;
    auto& r = rows[(size_t) dragging];
    float t = jlimit (0.0f, 1.0f, (e.position.x - r.bar.getX()) / r.bar.getWidth());
    r.value = r.lo + t * (r.hi - r.lo);
    r.value = r.kind == Tune ? std::round (r.value * 12.0f) / 12.0f : std::round (r.value * 100.0f) / 100.0f;
    apply (r, true);
}

void Inspector::mouseUp (const MouseEvent&)
{
    if (dragging < 0) return;
    auto i = dragging;
    dragging = -1;
    apply (rows[(size_t) i], false);
}
