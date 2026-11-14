#include "Pop.h"
#include "Art.h"

using namespace juce;
using namespace scq;

// ------------------------------------------------------------------------------------------ browser

static const uint32 CELL_COLS[8] = { 0xffd22a26, 0xff2a7fd0, 0xfff2c400, 0xfff7931e, 0xff1f8a3c, 0xffa88a1e, 0xff7b6fb8, 0xffe58fbf };

PlaceBrowser::PlaceBrowser()
{
    int counts[4] = { 0, 0, 0, 0 };
    for (auto& p : catalog())
    {
        if (p.family >= Hands) continue;
        auto what = p.help.upToFirstOccurrenceOf (" (after", false, false);
        cells.push_back ({ p.name, what, (int) p.family, counts[(int) p.family]++, {} });
    }
}

void PlaceBrowser::resized()
{
    int counts[4] = { 0, 0, 0, 0 };
    for (auto& c : cells) counts[c.family]++;
    auto r = getLocalBounds().toFloat();
    const float gap = 10;
    const int rows = 4;
    // each grid as wide as its places need, all four filling the width
    float cell = r.getHeight() / (float) rows;
    float need = 0;
    for (int f = 0; f < 4; ++f) need += std::ceil ((float) counts[f] / rows) * cell;
    float scale = jmin (1.0f, (r.getWidth() - gap * 3) / need);
    cell *= scale;
    float x = r.getX();
    for (int f = 0; f < 4; ++f)
    {
        int cols = (int) std::ceil ((float) counts[f] / rows);
        grids[f] = { x, r.getY(), cols * cell, rows * cell };
        x += cols * cell + gap;
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
    const char* names[] = { "sources", "shapes", "spaces", "currents" };
    for (int f = 0; f < 4; ++f)
    {
        g.setColour (t.dark ? t.surface : Colours::white);
        g.fillRect (grids[f].expanded (3));
        g.setColour (t.chromeDim);
        g.setFont (art::label (8.5f));
        g.drawText (names[f], grids[f].withTop (grids[f].getBottom() + 3).withHeight (11), Justification::right);
    }
    for (int i = 0; i < (int) cells.size(); ++i)
    {
        auto& c = cells[(size_t) i];
        auto col = Colour (CELL_COLS[(c.index + c.family * 3 + c.index / 4 * 3) % 8]);
        bool lit = i == hover;
        auto r = c.r.reduced (c.r.getWidth() * 0.12f);
        g.setColour (col.withAlpha (lit ? 1.0f : 0.42f));
        g.drawRect (r, jmax (1.5f, r.getWidth() * 0.12f));
        auto m = r.reduced (r.getWidth() * 0.3f);
        auto cc = m.getCentre();
        auto ink = t.dark ? Colour (0xffe6e6ee) : Colour (0xff1a1a1a);
        switch (c.family)                 // a mark per family: square, slash, diamond, cross
        {
            case 0: g.setColour (col.withAlpha (lit ? 1.0f : 0.45f)); g.fillRect (m); break;
            case 1: g.setColour (ink.withAlpha (lit ? 1.0f : 0.4f)); g.drawLine (m.getX(), m.getBottom(), m.getRight(), m.getY(), 2.0f); break;
            case 2:
            {
                Path d; d.addQuadrilateral (cc.x, m.getY(), m.getRight(), cc.y, cc.x, m.getBottom(), m.getX(), cc.y);
                g.setColour (col.withAlpha (lit ? 1.0f : 0.45f)); g.fillPath (d); break;
            }
            default:
                g.setColour (ink.withAlpha (lit ? 1.0f : 0.4f));
                g.drawLine (m.getX(), m.getY(), m.getRight(), m.getBottom(), 1.8f);
                g.drawLine (m.getRight(), m.getY(), m.getX(), m.getBottom(), 1.8f);
        }
        if (lit) { g.setColour (ink); g.drawRect (c.r, 2.0f); }
    }
}

int PlaceBrowser::cellAt (Point<float> p) const
{
    for (int i = 0; i < (int) cells.size(); ++i) if (cells[(size_t) i].r.contains (p)) return i;
    return -1;
}

void PlaceBrowser::mouseMove (const MouseEvent& e)
{
    int h = cellAt (e.position);
    if (h == hover) return;
    hover = h;
    if (onHover) onHover (h >= 0 ? cells[(size_t) h].name + ": " + cells[(size_t) h].help + "   (click to add it)" : String());
    repaint();
}

void PlaceBrowser::mouseExit (const MouseEvent&) { hover = -1; repaint(); }

void PlaceBrowser::mouseDown (const MouseEvent& e)
{
    int h = cellAt (e.position);
    if (h >= 0 && onAdd) onAdd (cells[(size_t) h].name);
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
    if (! p || p->family >= Hands) { node = -1; repaint(); return; }
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
    auto tab1 = head.removeFromLeft (head.getWidth() * 0.45f);
    art::fillChecker (g, tab1, 7);
    g.setColour (Colour (0xff1a1a1a));
    g.setFont (art::label (11.5f));
    g.drawText (nd->name, tab1, Justification::centred);
    head.removeFromLeft (6);
    g.setColour (t.button); g.fillRect (head);
    g.setColour (t.buttonText);
    g.drawText (nd->place, head, Justification::centred);

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
                if (k == row.pos) art::fillChecker (g, s, 5);
                else { g.setColour (t.rail); g.fillRect (s); }
            }
            continue;
        }
        ColourGradient grad (i % 2 ? Colour (0xff1a1a1a) : Colours::white, b.getX(), 0, i % 2 ? Colours::white : Colour (0xff1a1a1a), b.getRight(), 0, false);
        if (t.dark) { grad.setColour (i % 2 ? 1 : 0, Colour (0xffb8b8c8)); }
        g.setGradientFill (grad);
        g.fillRect (b);
        float x = b.getX() + (row.value - row.lo) / (row.hi - row.lo) * b.getWidth();
        Path tri; tri.addTriangle (x - 6, b.getY() - 8, x + 6, b.getY() - 8, x, b.getY() - 1);
        g.setColour (Colour (0xffd22a26).withAlpha (i == dragging ? 1.0f : 0.85f));
        g.fillPath (tri);
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
