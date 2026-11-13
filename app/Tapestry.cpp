#include "Tapestry.h"
#include "Art.h"

using namespace juce;
using namespace scq;

static const char* HAND_ROWS[] = { "lean1", "lean2", "lean3", "lean4", "door1", "door2", "door3", "door4",
                                   "lift", "lift.up", "lift.down", "wake", "lucid" };
static const char* EAR_ROWS[]  = { "left", "right", "ears", "lamp", "lamp.track", "lamp.play",
                                   "journey.next", "journey.back", "journey.miss" };
static constexpr float BW = 168, HEAD = 46, ROW = 17;

static int rowIndex (const char* const* rows, int n, const String& t)
{
    for (int i = 0; i < n; ++i) if (t == rows[i]) return i;
    return -1;
}

Tapestry::Tapestry (Document& d, Host& h) : doc (d), host (h)
{
    setWantsKeyboardFocus (true);
    canvasImg = Image (Image::RGB, 1400, 1000, true);
    Graphics g (canvasImg);
    art::paintCanvas (g, { 0, 0, 1400, 1000 }, 11);
    startTimerHz (15);
}

void Tapestry::resized() {}

void Tapestry::refresh()
{
    layout();
    selectedCord = -1;
    hoverCord = -1;
    std::set<int> keep;
    for (int b : selected) if (b < (int) boards.size()) keep.insert (b);
    selected = keep;
    repaint();
}

void Tapestry::edited (bool recompile)
{
    if (onTextEdited) onTextEdited (recompile);
}

String Tapestry::arrow (int kind) const
{
    switch (kind) { case 1: return "~>"; case 2: return "=>"; case 3: return ">|"; default: return "->"; }
}

// ------------------------------------------------------------------------------------------ layout

Point<float> Tapestry::defaultPos (const NodeInfo& n, int index)
{
    auto* p = findPlace (n.place);
    int fam = p ? (int) p->family : 0;
    int col = fam == Current ? 0 : (fam == Source ? 1 : (fam == Shape ? 2 : 3));
    return { 250.0f + (float) col * 220.0f, 40.0f + (float) (index % 6) * 150.0f + (float) col * 12.0f };
}

void Tapestry::layout()
{
    boards.clear();
    auto pos = [this] (const String& key, Point<float> def)
    {
        auto it = doc.positions.find (doc.key (doc.scene, key));
        return it != doc.positions.end() ? it->second : def;
    };

    Board hands;
    hands.node = -1; hands.key = "@hands"; hands.title = "SP-1"; hands.sub = "hands"; hands.family = Hands;
    hands.help = "the SP-1's faders (lean), track buttons (door), rocker (lift), PLAY (wake) and dots (lucid)";
    for (auto* r : HAND_ROWS)
    {
        String t (r);
        int sig = t.startsWith ("lean") || t == "lift" ? Flow : Pulse;
        hands.outs.push_back ({ t, t, sig });
    }
    hands.r = { 0, 0, 120, HEAD + ROW * (float) hands.outs.size() + 8 };
    hands.r.setPosition (pos ("@hands", { 30, 40 }));
    boards.push_back (hands);

    int perCol[4] = { 0, 0, 0, 0 };
    float maxX = 900;
    for (auto& n : doc.nodes)
    {
        auto* p = findPlace (n.place);
        if (! p || p->family == Hands || p->family == Ears) continue;
        Board b;
        b.node = n.id; b.key = n.name; b.title = n.place; b.family = p->family; b.help = p->help;
        b.sub = (n.name != n.place ? n.name + "  " : String()) + (p->nature != 0 || n.nature != 0 ? String::fromUTF8 ("\xc2\xb7 ") + fmt (n.nature) : String());
        for (int i = 0; i < p->ins.size(); ++i)
            b.ins.push_back ({ p->ins[i], n.name + (i > 0 ? "." + p->ins[i] : String()), 0 });
        for (int i = 0; i < p->outs.size(); ++i)
            b.outs.push_back ({ p->outs[i], n.name + (i > 0 ? "." + p->outs[i] : String()), (int) outletSignal (*p, i) });
        int rows = jmax ((int) b.ins.size(), (int) b.outs.size(), 1);
        b.r = { 0, 0, BW, HEAD + ROW * (float) rows + 8 };
        int col = p->family == Current ? 0 : (p->family == Source ? 1 : (p->family == Shape ? 2 : 3));
        Point<float> def (250.0f + (float) col * 220.0f, 40.0f + (float) perCol[col]++ * 0.0f);
        def.y = 40.0f;
        for (auto& o : boards)    // stack under whatever already sits in this column
            if (o.node >= 0 && std::abs (o.r.getX() - def.x) < 1 && o.r.getBottom() + 24 > def.y) def.y = o.r.getBottom() + 24;
        b.r.setPosition (pos (n.name, def));
        maxX = jmax (maxX, b.r.getRight());
        boards.push_back (b);
    }

    Board ears;
    ears.node = -2; ears.key = "@ears"; ears.title = "SP-1"; ears.sub = "ears, lamp, journey"; ears.family = Ears;
    ears.help = "where sound leaves (left, right, ears), the eight LEDs (lamp) and travel between scenes (journey)";
    for (auto* r : EAR_ROWS) ears.ins.push_back ({ r, r, 0 });
    ears.r = { 0, 0, 132, HEAD + ROW * (float) ears.ins.size() + 8 };
    ears.r.setPosition (pos ("@ears", { maxX + 90, 40 }));
    boards.push_back (ears);
}

Point<float> Tapestry::inletPos (const Board& b, int i) const { return { b.r.getX(), b.r.getY() + HEAD + ROW * (float) i + 8 }; }
Point<float> Tapestry::outletPos (const Board& b, int i) const { return { b.r.getRight(), b.r.getY() + HEAD + ROW * (float) i + 8 }; }

bool Tapestry::endpoint (int nodeId, int port, bool out, int& board, int& row) const
{
    auto* n = doc.node (nodeId);
    auto* p = n ? findPlace (n->place) : nullptr;
    if (! p) return false;
    if (p->family == Hands)
    {
        String t = n->name;
        if (p->name == "lift" && port > 0) t = port == 1 ? "lift.up" : "lift.down";
        board = 0;
        row = rowIndex (HAND_ROWS, 13, t);
        return row >= 0;
    }
    if (p->family == Ears)
    {
        String t = n->name;
        if (p->name == "lamp" && port > 0) t = port == 1 ? "lamp.track" : "lamp.play";
        if (p->name == "journey") t = port == 0 ? "journey.next" : (port == 1 ? "journey.back" : "journey.miss");
        board = (int) boards.size() - 1;
        row = rowIndex (EAR_ROWS, 9, t);
        return row >= 0 && ! out;
    }
    for (int i = 0; i < (int) boards.size(); ++i)
        if (boards[(size_t) i].node == nodeId) { board = i; row = port; return true; }
    return false;
}

// ------------------------------------------------------------------------------------------ hits

Tapestry::Hit Tapestry::portAt (Point<float> d, float slop) const
{
    for (int i = (int) boards.size(); --i >= 0;)
    {
        auto& b = boards[(size_t) i];
        for (int k = 0; k < (int) b.ins.size(); ++k)
            if (inletPos (b, k).getDistanceFrom (d) < slop) return { i, false, k };
        for (int k = 0; k < (int) b.outs.size(); ++k)
            if (outletPos (b, k).getDistanceFrom (d) < slop) return { i, true, k };
    }
    return {};
}

int Tapestry::boardAt (Point<float> d) const
{
    for (int i = (int) boards.size(); --i >= 0;)
        if (boards[(size_t) i].r.contains (d)) return i;
    return -1;
}

Rectangle<float> Tapestry::bottleRect (int ci) const
{
    auto& c = doc.cords[(size_t) ci];
    int b, row;
    if (! endpoint (c.dst, c.dport, false, b, row)) return {};
    int nth = 0;
    for (int k = 0; k < ci; ++k)
        if (doc.cords[(size_t) k].src < 0 && doc.cords[(size_t) k].dst == c.dst && doc.cords[(size_t) k].dport == c.dport) ++nth;
    auto p = inletPos (boards[(size_t) b], row);
    return Rectangle<float> (40, 14).withRightX (p.x - 9 - (float) nth * 44).withCentre ({ p.x - 29 - (float) nth * 44, p.y });
}

int Tapestry::bottleAt (Point<float> d) const
{
    for (int k = 0; k < (int) doc.cords.size(); ++k)
        if (doc.cords[(size_t) k].src < 0 && bottleRect (k).expanded (2).contains (d)) return k;
    return -1;
}

int Tapestry::cordAt (Point<float> d) const
{
    for (int k = (int) doc.cords.size(); --k >= 0;)
    {
        auto& c = doc.cords[(size_t) k];
        if (c.src < 0) continue;
        int sb, sr, db, dr;
        if (! endpoint (c.src, c.sport, true, sb, sr) || ! endpoint (c.dst, c.dport, false, db, dr)) continue;
        if (art::distanceToCable (outletPos (boards[(size_t) sb], sr), inletPos (boards[(size_t) db], dr), d) < 6.0f / zoom + 2) return k;
    }
    return -1;
}

// ------------------------------------------------------------------------------------------ paint

void Tapestry::paint (Graphics& g)
{
    // canvas tiles
    {
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::translation (-pan.x, -pan.y).scaled (zoom));
        auto vis = Rectangle<float> (pan.x, pan.y, (float) getWidth() / zoom, (float) getHeight() / zoom);
        const float tw = (float) canvasImg.getWidth(), th = (float) canvasImg.getHeight();
        for (float y = std::floor (vis.getY() / th) * th; y < vis.getBottom(); y += th)
            for (float x = std::floor (vis.getX() / tw) * tw; x < vis.getRight(); x += tw)
                g.drawImageAt (canvasImg, (int) x, (int) y);

        // boards
        for (int i = 0; i < (int) boards.size(); ++i)
        {
            auto& b = boards[(size_t) i];
            auto r = b.r;
            bool sp1 = b.node < 0;
            g.setColour (Colour (0x40000000));
            g.fillRoundedRectangle (r.translated (3, 6), 5);
            g.setColour (sp1 ? Colour (0xffd6d4cc) : art::cream);
            g.fillRoundedRectangle (r, 5);
            g.setColour (art::familyColour (b.family).withAlpha (sp1 ? 0.5f : 0.85f));
            g.fillRoundedRectangle (r.withHeight (6), 3);
            g.setColour (selected.count (i) ? art::gold : Colour (0x55000000));
            g.drawRoundedRectangle (r, 5, selected.count (i) ? 2.4f : 1.0f);
            auto label = r.reduced (10, 0).withY (r.getY() + 11).withHeight (17);
            art::drawDymo (g, label.withWidth (jmin (label.getWidth(), 12.0f + 8.6f * (float) b.title.length())), b.title);
            g.setColour (art::ink.withAlpha (0.75f));
            g.setFont (art::hand (12));
            g.drawText (b.sub, r.reduced (10, 0).withY (r.getY() + 29).withHeight (15), Justification::left);
            g.setFont (art::mono (10.5f));
            for (int k = 0; k < (int) b.ins.size(); ++k)
            {
                auto p = inletPos (b, k);
                art::drawLoop (g, p, hoverPort.board == i && ! hoverPort.out && hoverPort.port == k, art::ink);
                g.setColour (art::ink);
                g.drawText (b.ins[(size_t) k].label, Rectangle<float> (p.x + 9, p.y - 7, r.getWidth() * 0.55f, 14), Justification::left);
            }
            for (int k = 0; k < (int) b.outs.size(); ++k)
            {
                auto p = outletPos (b, k);
                art::drawLoop (g, p, hoverPort.board == i && hoverPort.out && hoverPort.port == k, art::signalColour (b.outs[(size_t) k].signal));
                g.setColour (art::ink);
                g.drawText (b.outs[(size_t) k].label, Rectangle<float> (p.x - 9 - r.getWidth() * 0.5f, p.y - 7, r.getWidth() * 0.5f, 14), Justification::right);
            }
            if (b.node == -2)   // the lamp's eight LEDs, live
                for (int k = 0; k < 8; ++k)
                {
                    float v = host.lamp (k);
                    auto c = Rectangle<float> (7, 7).withCentre ({ r.getX() + 66 + (float) (k % 4) * 12, r.getY() + HEAD + ROW * 3 + 2 + (float) (k / 4) * 10 });
                    g.setColour (Colour (0xff2a2a2a)); g.fillEllipse (c.expanded (1));
                    g.setColour (Colour (0xffff3020).interpolatedWith (Colours::white, v).withAlpha (0.15f + 0.85f * v));
                    g.fillEllipse (c);
                }
        }

        // cords
        for (int k = 0; k < (int) doc.cords.size(); ++k)
        {
            auto& c = doc.cords[(size_t) k];
            int db, dr;
            if (! endpoint (c.dst, c.dport, false, db, dr)) continue;
            if (c.src < 0)
            {
                auto br = bottleRect (k);
                auto ip = inletPos (boards[(size_t) db], dr);
                g.setColour (art::ink.withAlpha (0.6f));
                g.drawLine (br.getRight(), br.getCentreY(), ip.x - 5, ip.y, 1.4f);
                g.setColour (selectedCord == k ? art::gold : Colour (0xffeef3f6));
                g.fillRoundedRectangle (br, 6);
                g.setColour (art::blue);
                g.drawRoundedRectangle (br, 6, 1.2f);
                g.setFont (art::mono (10.5f));
                g.drawText (fmt (k == valueCord ? valueNow : c.amt), br, Justification::centred);
                continue;
            }
            int sb, sr;
            if (! endpoint (c.src, c.sport, true, sb, sr)) continue;
            auto& from = boards[(size_t) sb];
            auto col = art::signalColour (from.outs[(size_t) sr].signal);
            art::drawCable (g, outletPos (from, sr), inletPos (boards[(size_t) db], dr), col, c.kind == scq::Range ? 4 : c.kind,
                            k == hoverCord || k == selectedCord, 1.0f);
            if ((c.kind != Hall || std::abs (c.amt - 100) > 0.01f) && c.kind != Door)
            {
                auto m = art::cableMid (outletPos (from, sr), inletPos (boards[(size_t) db], dr));
                g.setFont (art::mono (9.5f));
                g.setColour (art::ink.withAlpha (0.8f));
                String t = c.kind == scq::Range ? fmt (c.amt) + ".." + fmt (c.amt2) : (c.kind == Water ? fmt (c.amt) + "ms" : fmt (c.amt) + "%");
                g.drawText (t, Rectangle<float> (m.x - 30, m.y + 7, 60, 12), Justification::centred);
            }
        }

        // the cord in your hand
        if (mode == Mode::Cable && cableFrom.board >= 0)
        {
            auto& b = boards[(size_t) cableFrom.board];
            auto a = cableFrom.out ? outletPos (b, cableFrom.port) : inletPos (b, cableFrom.port);
            auto col = cableFrom.out ? art::signalColour (b.outs[(size_t) cableFrom.port].signal) : art::umber;
            if (cableFrom.out) art::drawCable (g, a, dragPos, col, handKind, false, 0.85f);
            else art::drawCable (g, dragPos, a, col, handKind, false, 0.85f);
        }
        if (dropPos.x >= 0)
        {
            g.setColour (art::gold);
            g.drawEllipse (Rectangle<float> (22, 22).withCentre (dropPos), 2.0f);
        }
    }

    if (doc.nodes.empty() || boards.size() <= 2)
    {
        g.setColour (art::ink.withAlpha (0.7f));
        g.setFont (art::hand (18));
        g.drawText ("right-click to add a place, or write in the journal", getLocalBounds().removeFromBottom (60), Justification::centred);
    }
}

// ------------------------------------------------------------------------------------------ mouse

void Tapestry::timerCallback()
{
    if (hoverPort.board >= 0 && hoverPort.out && onStatus)
    {
        auto& b = boards[(size_t) hoverPort.board];
        if (b.node >= 0)
            onStatus (b.outs[(size_t) hoverPort.port].text + "   " + String (host.probe (b.node, hoverPort.port), 3));
    }
    repaint (getLocalBounds().withWidth (getWidth()));   // lamp + probes
}

void Tapestry::mouseMove (const MouseEvent& e)
{
    auto d = toDesign (e.position);
    lastMouse = d;
    auto hp = portAt (d);
    int hc = hp.board < 0 ? cordAt (d) : -1;
    int hb = boardAt (d);
    if (hp.board != hoverPort.board || hp.port != hoverPort.port || hp.out != hoverPort.out || hc != hoverCord || hb != hoverBoard)
    {
        hoverPort = hp; hoverCord = hc; hoverBoard = hb;
        if (onStatus)
        {
            if (hp.board >= 0)
            {
                auto& b = boards[(size_t) hp.board];
                onStatus ((hp.out ? b.outs : b.ins)[(size_t) hp.port].text + (hp.out ? "   drag to an inlet" : "   drag to an outlet"));
            }
            else if (hc >= 0) onStatus (doc.cordText (doc.cords[(size_t) hc]) + "      (right-click to change)");
            else if (hb >= 0) onStatus (boards[(size_t) hb].title + ": " + boards[(size_t) hb].help);
            else onStatus ({});
        }
        repaint();
    }
    setMouseCursor (hp.board >= 0 ? MouseCursor::CrosshairCursor : (bottleAt (d) >= 0 ? MouseCursor::UpDownResizeCursor : MouseCursor::NormalCursor));
}

void Tapestry::mouseDown (const MouseEvent& e)
{
    grabKeyboardFocus();
    auto d = toDesign (e.position);
    anchor = e.position;
    dragPos = d;
    if (e.mods.isPopupMenu())
    {
        int ci = cordAt (d), bi = boardAt (d), bo = bottleAt (d);
        if (bo >= 0) cordMenu (bo);
        else if (ci >= 0) cordMenu (ci);
        else if (bi >= 0) boardMenu (bi);
        else canvasMenu (d);
        return;
    }
    auto hp = portAt (d);
    if (hp.board >= 0) { mode = Mode::Cable; cableFrom = hp; return; }
    int bo = bottleAt (d);
    if (bo >= 0) { mode = Mode::Value; valueCord = bo; valueStart = valueNow = doc.cords[(size_t) bo].amt; selectedCord = bo; return; }
    int ci = cordAt (d);
    if (ci >= 0) { selectedCord = ci; selected.clear(); repaint(); return; }
    int bi = boardAt (d);
    if (bi >= 0)
    {
        if (e.mods.isShiftDown()) { if (selected.count (bi)) selected.erase (bi); else selected.insert (bi); }
        else if (! selected.count (bi)) { selected.clear(); selected.insert (bi); }
        selectedCord = -1;
        mode = Mode::Move;
        moving = bi;
        moveStart = d;
        repaint();
        return;
    }
    selected.clear();
    selectedCord = -1;
    mode = Mode::Pan;
    repaint();
}

void Tapestry::mouseDrag (const MouseEvent& e)
{
    auto d = toDesign (e.position);
    switch (mode)
    {
        case Mode::Pan:
            pan -= (e.position - anchor) / zoom;
            anchor = e.position;
            repaint();
            break;
        case Mode::Move:
        {
            auto delta = d - moveStart;
            moveStart = d;
            for (int b : selected)
            {
                boards[(size_t) b].r.translate (delta.x, delta.y);
                doc.setPosition (doc.scene, boards[(size_t) b].key, boards[(size_t) b].r.getPosition());
            }
            repaint();
            break;
        }
        case Mode::Cable:
            dragPos = d;
            hoverPort = portAt (d, 14);
            repaint();
            break;
        case Mode::Value:
        {
            float step = e.mods.isShiftDown() ? 0.1f : 1.0f;
            valueNow = valueStart + std::round ((anchor.y - e.position.y) / 2.0f) * step;
            host.setAmount (valueCord, valueNow);
            if (onStatus) onStatus ("bottle " + fmt (valueNow));
            repaint();
            break;
        }
        default: break;
    }
}

void Tapestry::connect (const Port& from, const Port& to)
{
    doc.addLine (from.text + " " + arrow (handKind) + " " + to.text);
    edited (true);
}

void Tapestry::mouseUp (const MouseEvent& e)
{
    auto d = toDesign (e.position);
    if (mode == Mode::Cable)
    {
        auto to = portAt (d, 14);
        if (to.board >= 0 && to.out != cableFrom.out)
        {
            auto& a = boards[(size_t) (cableFrom.out ? cableFrom.board : to.board)];
            auto& b = boards[(size_t) (cableFrom.out ? to.board : cableFrom.board)];
            connect (a.outs[(size_t) (cableFrom.out ? cableFrom.port : to.port)], b.ins[(size_t) (cableFrom.out ? to.port : cableFrom.port)]);
        }
        cableFrom = {};
    }
    else if (mode == Mode::Move && e.getDistanceFromDragStart() > 2)
    {
        doc.writePositions();
        edited (false);
    }
    else if (mode == Mode::Value && valueCord >= 0)
    {
        bool alone = doc.cordsOnLine (doc.cords[(size_t) valueCord].line) == 1;
        doc.setCordAmount (valueCord, valueNow);
        doc.cords[(size_t) valueCord].amt = valueNow;
        edited (! alone);
        valueCord = -1;
    }
    mode = Mode::None;
    repaint();
}

void Tapestry::mouseDoubleClick (const MouseEvent& e)
{
    auto d = toDesign (e.position);
    int ci = cordAt (d);
    if (ci < 0) ci = bottleAt (d);
    if (ci >= 0) { askAmount (ci); return; }
    int bi = boardAt (d);
    if (bi >= 0 && boards[(size_t) bi].node >= 0)
        if (auto* n = doc.node (boards[(size_t) bi].node)) if (onShowLine) onShowLine (n->line);
}

void Tapestry::zoomAbout (float z, Point<float> screen)
{
    z = jlimit (0.3f, 2.5f, z);
    auto before = toDesign (screen);
    zoom = z;
    pan = before - screen / zoom;
    repaint();
}

void Tapestry::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
        zoomAbout (zoom * (1.0f + w.deltaY * 0.6f), e.position);
    else
    {
        pan -= Point<float> (w.deltaX, w.deltaY) * 220.0f / zoom;
        repaint();
    }
}

void Tapestry::mouseMagnify (const MouseEvent& e, float s) { zoomAbout (zoom * s, e.position); }

void Tapestry::fitAll()
{
    if (boards.empty()) return;
    Rectangle<float> all = boards.front().r;
    for (auto& b : boards) all = all.getUnion (b.r);
    all = all.expanded (60);
    zoom = jlimit (0.3f, 1.4f, jmin ((float) getWidth() / all.getWidth(), (float) getHeight() / all.getHeight()));
    pan = all.getPosition() - Point<float> (((float) getWidth() / zoom - all.getWidth()) * 0.5f, 0);
    repaint();
}

bool Tapestry::keyPressed (const KeyPress& k)
{
    if (k == KeyPress::deleteKey || k == KeyPress::backspaceKey) { removeSelection(); return true; }
    if (k == KeyPress ('c', ModifierKeys::commandModifier, 0)) { SystemClipboard::copyTextToClipboard (copySelection()); return true; }
    if (k == KeyPress ('v', ModifierKeys::commandModifier, 0)) { paste (SystemClipboard::getTextFromClipboard()); return true; }
    if (k == KeyPress ('x', ModifierKeys::commandModifier, 0)) { SystemClipboard::copyTextToClipboard (copySelection()); removeSelection(); return true; }
    if (k == KeyPress ('0', ModifierKeys::commandModifier, 0)) { fitAll(); return true; }
    if (k == KeyPress ('=', ModifierKeys::commandModifier, 0)) { zoomAbout (zoom * 1.2f, getLocalBounds().getCentre().toFloat()); return true; }
    if (k == KeyPress ('-', ModifierKeys::commandModifier, 0)) { zoomAbout (zoom / 1.2f, getLocalBounds().getCentre().toFloat()); return true; }
    return false;
}

// ------------------------------------------------------------------------------------------ editing

String Tapestry::copySelection() const
{
    if (selectedCord >= 0 && selectedCord < (int) doc.cords.size()) return doc.cordText (doc.cords[(size_t) selectedCord]);
    std::vector<int> ids;
    for (int b : selected) if (boards[(size_t) b].node >= 0) ids.push_back (boards[(size_t) b].node);
    return doc.copyNodes (ids);
}

void Tapestry::removeSelection()
{
    if (selectedCord >= 0) { doc.removeCord (selectedCord); selectedCord = -1; edited (true); return; }
    std::set<int> ids;
    for (int b : selected) if (boards[(size_t) b].node >= 0) ids.insert (boards[(size_t) b].node);
    if (ids.empty()) return;
    doc.removeNodes (ids);
    selected.clear();
    edited (true);
}

static bool wordChar (juce_wchar c) { return CharacterFunctions::isLetterOrDigit (c) || c == '_'; }

static String renameWord (const String& line, const String& from, const String& to)
{
    String out;
    int i = 0;
    while (i < line.length())
    {
        if (line.substring (i).startsWith (from) && (i == 0 || ! wordChar (line[i - 1]))
            && (i + from.length() >= line.length() || ! wordChar (line[i + from.length()])))
        {
            out << to;
            i += from.length();
        }
        else out << line[i++];
    }
    return out;
}

void Tapestry::paste (const String& text)
{
    auto lines = StringArray::fromLines (text.trim());
    lines.removeEmptyStrings();
    if (lines.isEmpty()) return;
    // a pasted dream keeps its own names unless they're taken here
    std::set<String> taken;
    for (auto& n : doc.nodes) taken.insert (n.name);
    StringArray newNames;
    for (int i = 0; i < lines.size(); ++i)
    {
        auto t = StringArray::fromTokens (lines[i].trim(), " ", "");
        if (t.size() >= 4 && t[0].equalsIgnoreCase ("the") && (t[2] == "is" || t[2] == "are"))
        {
            String name = t[1];
            if (taken.count (name))
            {
                String fresh;
                for (char c = 'b'; c <= 'z' && fresh.isEmpty(); ++c)
                    if (! taken.count (name + String::charToString (c))) fresh = name + String::charToString (c);
                for (int k = 0; k < lines.size(); ++k) lines.set (k, renameWord (lines[k], name, fresh));
                name = fresh;
            }
            taken.insert (name);
            newNames.add (name);
        }
    }
    // put pasted boards where the mouse is
    float x = lastMouse.x, y = lastMouse.y;
    for (auto& n : newNames) { doc.setPosition (doc.scene, n, { x, y }); x += 200; if (x > lastMouse.x + 600) { x = lastMouse.x; y += 170; } }
    doc.addLines (lines);
    if (! newNames.isEmpty()) doc.writePositions();
    edited (true);
}

void Tapestry::askAmount (int ci)
{
    auto c = doc.cords[(size_t) ci];
    if (c.kind == Door) return;
    auto* w = new AlertWindow ("amount", c.kind == Water ? "how slow, in milliseconds" : (c.kind == scq::Range ? "low..high" : "percent"), MessageBoxIconType::NoIcon);
    w->addTextEditor ("v", c.kind == scq::Range ? fmt (c.amt) + ".." + fmt (c.amt2) : fmt (c.amt));
    w->addButton ("ok", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("cancel", 0, KeyPress (KeyPress::escapeKey));
    w->enterModalState (true, ModalCallbackFunction::create ([this, w, ci, c] (int r)
    {
        if (r == 1)
        {
            auto t = w->getTextEditorContents ("v");
            if (c.kind == scq::Range) doc.setCordAmount (ci, t.upToFirstOccurrenceOf ("..", false, false).getFloatValue(), t.fromFirstOccurrenceOf ("..", false, false).getFloatValue());
            else doc.setCordAmount (ci, t.getFloatValue());
            edited (true);
        }
    }), true);
}

void Tapestry::cordMenu (int ci)
{
    selectedCord = ci;
    repaint();
    auto c = doc.cords[(size_t) ci];
    PopupMenu m;
    m.addSectionHeader (doc.cordText (c));
    if (c.src >= 0 && c.kind != scq::Range)
    {
        m.addItem (1, "hall  ->   (a plain wire)", true, c.kind == Hall);
        m.addItem (2, "fog   ~>   (a resistor: softer)", true, c.kind == Fog);
        m.addItem (3, "water =>   (a capacitor: slower)", true, c.kind == Water);
        m.addItem (4, "door  >|   (a diode: one way)", true, c.kind == Door);
        m.addSeparator();
    }
    if (c.kind != Door) m.addItem (5, "amount...");
    m.addItem (6, "show in the journal");
    m.addItem (7, "remove");
    m.showMenuAsync ({}, [this, ci, c] (int r)
    {
        if (r >= 1 && r <= 4) { doc.setCordKind (ci, (Kind) (r - 1)); edited (true); }
        else if (r == 5) askAmount (ci);
        else if (r == 6 && onShowLine) onShowLine (c.line);
        else if (r == 7) { doc.removeCord (ci); edited (true); }
    });
}

void Tapestry::boardMenu (int bi)
{
    auto& b = boards[(size_t) bi];
    if (b.node < 0) return;
    if (! selected.count (bi)) { selected.clear(); selected.insert (bi); repaint(); }
    PopupMenu m;
    m.addSectionHeader (b.title + (b.key != b.title ? "  (" + b.key + ")" : String()));
    m.addItem (1, "show in the journal");
    m.addItem (2, "copy as text");
    m.addItem (3, "remove");
    auto line = doc.node (b.node) ? doc.node (b.node)->line : 0;
    m.showMenuAsync ({}, [this, line] (int r)
    {
        if (r == 1 && onShowLine) onShowLine (line);
        else if (r == 2) SystemClipboard::copyTextToClipboard (copySelection());
        else if (r == 3) removeSelection();
    });
}

void Tapestry::canvasMenu (Point<float> d)
{
    PopupMenu m, fams[4];
    const char* names[] = { "sources", "shapes", "spaces", "currents" };
    int id = 1;
    std::vector<String> ids;
    for (auto& p : catalog())
    {
        if (p.family >= Hands) continue;
        // what it does (after the dream image), and where it comes from
        auto what = p.help.fromFirstOccurrenceOf (": ", false, false).upToFirstOccurrenceOf (" (after", false, false);
        auto from = p.help.fromFirstOccurrenceOf ("(after ", false, false).upToLastOccurrenceOf (")", false, false);
        if (what.length() > 58) what = what.substring (0, 56) + "...";
        fams[p.family].addItem (id++, p.name.paddedRight (' ', 12) + what + (from.isNotEmpty() ? "   [" + from.upToFirstOccurrenceOf (";", false, false) + "]" : String()));
        ids.push_back (p.name);
    }
    for (int f = 0; f < 4; ++f) m.addSubMenu (names[f], fams[f]);
    m.addSeparator();
    m.addItem (1000, "paste", SystemClipboard::getTextFromClipboard().isNotEmpty());
    m.addItem (1001, "fit everything in view");
    lastMouse = d;
    m.showMenuAsync ({}, [this, ids, d] (int r)
    {
        if (r >= 1 && r <= (int) ids.size())
        {
            auto name = doc.addPlace (ids[(size_t) r - 1]);
            doc.setPosition (doc.scene, name, d);
            doc.writePositions();
            edited (true);
        }
        else if (r == 1000) paste (SystemClipboard::getTextFromClipboard());
        else if (r == 1001) fitAll();
    });
}

// ------------------------------------------------------------------------------------------ drops from the hands panel

bool Tapestry::isInterestedInDragSource (const SourceDetails& s) { return s.description.toString().startsWith ("hand:"); }

void Tapestry::itemDragMove (const SourceDetails& s)
{
    auto d = toDesign (s.localPosition.toFloat());
    auto hp = portAt (d, 16);
    dropPos = (hp.board >= 0 && ! hp.out) ? inletPos (boards[(size_t) hp.board], hp.port) : Point<float> (-1, -1);
    repaint();
}

void Tapestry::itemDragExit (const SourceDetails&) { dropPos = { -1, -1 }; repaint(); }

void Tapestry::itemDropped (const SourceDetails& s)
{
    dropPos = { -1, -1 };
    auto d = toDesign (s.localPosition.toFloat());
    auto hp = portAt (d, 16);
    if (hp.board >= 0 && ! hp.out)
    {
        Port from { {}, s.description.toString().fromFirstOccurrenceOf ("hand:", false, false), 1 };
        connect (from, boards[(size_t) hp.board].ins[(size_t) hp.port]);
    }
    repaint();
}
