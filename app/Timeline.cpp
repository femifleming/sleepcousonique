#include "Timeline.h"
#include "Art.h"

using namespace juce;

static const uint32 SNAP_COLS[12] = { 0xffd22a26, 0xff2a7fd0, 0xfff2c400, 0xfff7931e, 0xff1f8a3c, 0xffa88a1e,
                                      0xff7b6fb8, 0xffe58fbf, 0xff48a8c8, 0xff8a5a2a, 0xff6f9c78, 0xff1a1a1a };

Timeline::Timeline() { setWantsKeyboardFocus (true); startTimerHz (30); }

// ------------------------------------------------------------------------------------------ the dream's ";~" lines

String Timeline::toText() const
{
    String out;
    for (int i = 0; i < SLOTS; ++i)
    {
        if (! snaps[i].used) continue;
        out << ";~snap " << (i + 1) << " scene=" << snaps[i].scene;
        for (auto& [k, v] : snaps[i].v) out << " " << k << "=" << String (v, 4);
        out << "\n";
    }
    if (! segs.empty() || ! loop || ! glideRecall || glideSecs != 2)
    {
        out << ";~timeline loop=" << (loop ? 1 : 0) << " recall=" << (glideRecall ? "glide" : "cut") << " glide=" << glideSecs;
        for (auto& s : segs) out << " " << (s.snap + 1) << "," << s.secs << "," << (s.glide ? "g" : "c");
        out << "\n";
    }
    return out;
}

String Timeline::without (const String& dream)
{
    auto ls = StringArray::fromLines (dream);
    for (int i = ls.size(); --i >= 0;) if (ls[i].startsWith (";~")) ls.remove (i);
    while (ls.size() > 0 && ls[ls.size() - 1].trim().isEmpty()) ls.remove (ls.size() - 1);
    return ls.joinIntoString ("\n") + "\n";
}

void Timeline::fromText (const String& dream)
{
    for (auto& s : snaps) s = {};
    segs.clear();
    for (auto l : StringArray::fromLines (dream))
    {
        auto t = StringArray::fromTokens (l, " ", "");
        if (t.size() >= 2 && t[0] == ";~snap")
        {
            int i = t[1].getIntValue() - 1;
            if (i < 0 || i >= SLOTS) continue;
            auto& s = snaps[i];
            s.used = true;
            for (int k = 2; k < t.size(); ++k)
            {
                auto key = t[k].upToLastOccurrenceOf ("=", false, false), val = t[k].fromLastOccurrenceOf ("=", false, false);
                if (key == "scene") s.scene = val.getIntValue();
                else if (key.isNotEmpty()) s.v[key] = val.getFloatValue();
            }
        }
        else if (t.size() >= 1 && t[0] == ";~timeline")
        {
            for (int k = 1; k < t.size(); ++k)
            {
                if (t[k].startsWith ("loop=")) loop = t[k].getLastCharacter() == '1';
                else if (t[k].startsWith ("recall=")) glideRecall = t[k].endsWith ("glide");
                else if (t[k].startsWith ("glide=")) glideSecs = jmax (0.1f, t[k].fromFirstOccurrenceOf ("=", false, false).getFloatValue());
                else
                {
                    auto p = StringArray::fromTokens (t[k], ",", "");
                    if (p.size() == 3) segs.push_back ({ jlimit (0, SLOTS - 1, p[0].getIntValue() - 1), jmax (0.5f, p[1].getFloatValue()), p[2] == "g" });
                }
            }
        }
    }
    repaint();
}

void Timeline::changed() { if (onChanged) onChanged(); repaint(); }

// ------------------------------------------------------------------------------------------ playing

Timeline::Values Timeline::lerp (const Values& a, const Values& b, float x)
{
    Values out = a;
    for (auto& [k, vb] : b)
    {
        auto it = a.find (k);
        out[k] = it == a.end() ? vb : it->second + (vb - it->second) * x;
    }
    return out;
}

// only what moved goes out, so a held snapshot costs nothing and the SP-1s hear only changes
void Timeline::play (const Values& v, int scene)
{
    Values changes;
    for (auto& [k, x] : v)
    {
        auto it = sent.find (k);
        if (it == sent.end() || std::abs (it->second - x) > 1.0e-4f || scene != sentScene) changes[k] = x;
    }
    if (changes.empty() && scene == sentScene) return;
    for (auto& [k, x] : changes) sent[k] = x;
    sentScene = scene;
    if (onApply) onApply (changes, scene);
}

double Timeline::total() const { double s = 0; for (auto& g : segs) s += g.secs; return s; }
double Timeline::startOf (int i) const { double s = 0; for (int k = 0; k < i; ++k) s += segs[(size_t) k].secs; return s; }

void Timeline::timerCallback()
{
    auto now = Time::getMillisecondCounter();
    double dt = lastTick ? (now - lastTick) / 1000.0 : 0;
    lastTick = now;
    if (gliding)
    {
        glideAt += dt;
        float x = (float) jlimit (0.0, 1.0, glideAt / jmax (0.01, glideLen));
        x = x * x * (3 - 2 * x);                        // ease in and out
        play (lerp (from, to, x), toScene);
        if (glideAt >= glideLen) gliding = false;
        repaint();
    }
    if (! playing || segs.empty()) return;
    t += dt;
    if (t >= total())
    {
        if (! loop) { playing = false; t = total(); repaint(); if (onStatus) onStatus ("the timeline ended"); return; }
        t = std::fmod (t, total());
    }
    int i = 0;
    while (i + 1 < (int) segs.size() && t >= startOf (i + 1)) ++i;
    auto& s = segs[(size_t) i];
    auto& a = snaps[s.snap];
    int nextSeg = i + 1 < (int) segs.size() ? i + 1 : (loop ? 0 : -1);
    if (a.used)
    {
        if (s.glide && nextSeg >= 0 && snaps[segs[(size_t) nextSeg].snap].used && snaps[segs[(size_t) nextSeg].snap].scene == a.scene)
        {
            float x = (float) ((t - startOf (i)) / s.secs);
            play (lerp (a.v, snaps[segs[(size_t) nextSeg].snap].v, x), a.scene);
        }
        else play (a.v, a.scene);
        current = s.snap;
    }
    repaint();
}

void Timeline::togglePlay()
{
    playing = ! playing && ! segs.empty();
    gliding = false;
    lastTick = 0;
    sentScene = -1; sent.clear();                       // say everything once when it starts
    if (playing && t >= total()) t = 0;
    if (onStatus) onStatus (playing ? "playing the timeline" : "stopped");
    repaint();
}

void Timeline::recall (int slot)
{
    auto& s = snaps[slot];
    if (! s.used) return;
    playing = false;
    current = slot;
    sent.clear(); sentScene = -1;
    auto now = onCapture ? onCapture() : Snapshot {};
    if (glideRecall && glideSecs > 0.05f && now.scene == s.scene)
    {
        from = now.v; to = s.v; toScene = s.scene; glideAt = 0; glideLen = glideSecs; gliding = true;
        if (onStatus) onStatus ("gliding to snapshot " + String (slot + 1) + " over " + String (glideSecs, 1) + " s");
    }
    else
    {
        gliding = false;
        play (s.v, s.scene);
        if (onStatus) onStatus ("snapshot " + String (slot + 1));
    }
    repaint();
}

// ------------------------------------------------------------------------------------------ layout

Rectangle<float> Timeline::slotRect (int i) const
{
    const float h = (float) getHeight() - 16, sz = jmin (26.0f, (h - 4) / 2);
    return { 10 + (float) (i % 6) * (sz + 4), 8 + (float) (i / 6) * (sz + 4), sz, sz };
}
Rectangle<float> Timeline::playRect() const { return { slotRect (5).getRight() + 18, 8, 30, 30 }; }
Rectangle<float> Timeline::loopRect() const { return { playRect().getRight() + 6, 8, 44, 14 }; }
Rectangle<float> Timeline::modeRect() const { return { playRect().getRight() + 6, 24, 44, 14 }; }
Rectangle<float> Timeline::secsRect() const { return { modeRect().getRight() + 4, 24, 34, 14 }; }
Rectangle<float> Timeline::laneRect() const
{
    auto x = secsRect().getRight() + 14;
    return { x, 10, (float) getWidth() - x - 12, (float) getHeight() - 24 };
}
float Timeline::pxPerSec() const { return laneRect().getWidth() / (float) jmax (20.0, total() * 1.15); }
Rectangle<float> Timeline::segRect (int i) const
{
    auto l = laneRect();
    return { l.getX() + (float) startOf (i) * pxPerSec(), l.getY() + 6, segs[(size_t) i].secs * pxPerSec(), l.getHeight() - 10 };
}
int Timeline::segAt (Point<float> p) const
{
    for (int i = 0; i < (int) segs.size(); ++i) if (segRect (i).contains (p)) return i;
    return -1;
}

// ------------------------------------------------------------------------------------------ paint

void Timeline::paint (Graphics& g)
{
    auto& th = art::theme();
    g.fillAll (th.pop ? th.window : th.rail);
    g.setColour (th.line);
    g.drawHorizontalLine (0, 0, (float) getWidth());
    const auto ink = th.pop ? th.chromeText : art::ink;

    for (int i = 0; i < SLOTS; ++i)                    // the snapshot squares
    {
        auto r = slotRect (i);
        auto col = Colour (SNAP_COLS[i]);
        if (snaps[i].used)
        {
            g.setColour (col); g.drawRect (r, 3.0f);
            g.fillRect (r.reduced (r.getWidth() * 0.3f));
        }
        else
        {
            g.setColour (ink.withAlpha (0.25f));
            Path p; p.addRectangle (r.reduced (1)); Path d;
            const float dash[] = { 2, 2 };
            PathStrokeType (1.0f).createDashedStroke (d, p, dash, 2);
            g.fillPath (d);
        }
        if (i == current) { g.setColour (ink); g.drawRect (r.expanded (2.5f), 1.5f); }
        g.setColour (ink.withAlpha (snaps[i].used ? 0.9f : 0.35f));
        g.setFont (art::label (8));
        g.drawText (String (i + 1), r.translated (0, r.getHeight() - 9).withHeight (9).withX (r.getRight() - 9).withWidth (9), Justification::centred);
    }

    // transport: the red flag, loop, cut / glide and the glide time
    {
        auto p = playRect();
        g.setColour (Colour (0xffd42a24));
        if (playing) g.fillRect (p.reduced (8));
        else { Path tri; tri.addTriangle (p.getX() + 9, p.getY() + 6, p.getRight() - 6, p.getCentreY(), p.getX() + 9, p.getBottom() - 6); g.fillPath (tri); }
        g.setColour (ink.withAlpha (0.5f)); g.drawRect (p, 1.0f);
        auto toggle = [&] (Rectangle<float> r, const String& text, bool on)
        {
            if (on) art::fillChecker (g, r, 4); else { g.setColour (th.button); g.fillRect (r); }
            g.setColour (on ? Colour (0xff1a1a1a) : th.buttonText);
            g.setFont (art::label (9));
            g.drawText (text, r, Justification::centred);
        };
        toggle (loopRect(), "loop", loop);
        toggle (modeRect(), glideRecall ? "glide" : "cut", glideRecall);
        g.setColour (ink); g.setFont (art::label (9));
        g.drawText (String (glideSecs, glideSecs < 1 ? 1 : 0) + " s", secsRect(), Justification::centred);
    }

    // the lane: segments, the ruler, the playhead
    auto l = laneRect();
    g.setColour (th.pop ? th.surface : art::paper);
    g.fillRect (l);
    g.setColour (ink.withAlpha (0.25f));
    for (int s = 0; s <= (int) (l.getWidth() / pxPerSec()); ++s)
    {
        float x = l.getX() + (float) s * pxPerSec();
        g.drawVerticalLine ((int) x, l.getBottom() - (s % 10 == 0 ? 8.0f : 4.0f), l.getBottom());
        if (s % 10 == 0) { g.setFont (art::label (8)); g.drawText (String (s) + "s", Rectangle<float> (x + 2, l.getBottom() - 10, 30, 10), Justification::left); }
    }
    if (segs.empty())
    {
        g.setColour (ink.withAlpha (0.5f));
        g.setFont (art::label (10));
        g.drawText ("click an empty square to keep a snapshot, click a kept one to go to it; drag one here to put it on the timeline",
                    l.reduced (10, 0), Justification::centredLeft, true);
    }
    for (int i = 0; i < (int) segs.size(); ++i)
    {
        auto& s = segs[(size_t) i];
        auto r = segRect (i);
        auto col = Colour (SNAP_COLS[s.snap]);
        g.setColour (col.withAlpha (snaps[s.snap].used ? 0.85f : 0.3f));
        g.fillRect (r.reduced (1, 0));
        if (s.glide)                                    // a glide: a ramp into the next
        {
            Path ramp; ramp.startNewSubPath (r.getX() + 2, r.getBottom() - 2); ramp.lineTo (r.getRight() - 2, r.getY() + 2);
            g.setColour (Colours::white.withAlpha (0.8f)); g.strokePath (ramp, PathStrokeType (1.6f));
        }
        g.setColour (i == selected ? ink : Colours::black.withAlpha (0.4f));
        g.drawRect (r.reduced (1, 0), i == selected ? 2.0f : 1.0f);
        g.setColour (Colours::white);
        g.setFont (art::label (10));
        g.drawText (String (s.snap + 1) + "  " + String (s.secs, s.secs < 10 ? 1 : 0) + "s", r.reduced (5, 2), Justification::topLeft, true);
    }
    if (dragSlot >= 0 && l.contains (dragNow))         // where a dragged snapshot would land
    {
        g.setColour (Colour (SNAP_COLS[dragSlot]));
        g.fillRect (Rectangle<float> (dragNow.x - 1.5f, l.getY(), 3, l.getHeight()));
    }
    if (! segs.empty())
    {
        float x = l.getX() + (float) t * pxPerSec();
        g.setColour (Colour (0xffd42a24));
        g.fillRect (Rectangle<float> (x - 1, l.getY() - 4, 2, l.getHeight() + 4));
        g.fillRect (Rectangle<float> (x - 1, l.getY() - 6, 9, 6));
        g.setColour (ink); g.setFont (art::label (9));
        g.drawText (String (t, 1) + " / " + String (total(), 1) + " s", l.withTrimmedRight (4).withHeight (12), Justification::right);
    }
}

// ------------------------------------------------------------------------------------------ mouse

void Timeline::mouseDown (const MouseEvent& e)
{
    grabKeyboardFocus();
    auto p = e.position;
    dragSlot = -1; dragEdge = -1;
    for (int i = 0; i < SLOTS; ++i)
    {
        if (! slotRect (i).contains (p)) continue;
        if (e.mods.isAltDown()) { snaps[i] = {}; changed(); if (onStatus) onStatus ("snapshot " + String (i + 1) + " cleared"); return; }
        if (! snaps[i].used || e.mods.isShiftDown())
        {
            snaps[i] = onCapture ? onCapture() : Snapshot {};
            snaps[i].used = true;
            current = i;
            changed();
            if (onStatus) onStatus ("kept snapshot " + String (i + 1) + "   (click: go there, shift-click: keep again, option-click: clear, drag onto the timeline)");
            return;
        }
        dragSlot = i; dragNow = p;                       // a click recalls it (on mouse up); a drag places it
        return;
    }
    if (playRect().contains (p)) { togglePlay(); return; }
    if (loopRect().contains (p)) { loop = ! loop; changed(); return; }
    if (modeRect().contains (p)) { glideRecall = ! glideRecall; changed(); return; }
    if (secsRect().contains (p))
    {
        const float steps[] = { 0.5f, 1, 2, 4, 8, 16, 30 };   // click: the next glide time, round again after 30 s
        int k = 0;
        while (k < 7 && steps[k] <= glideSecs + 0.01f) ++k;
        glideSecs = steps[k % 7];
        changed(); return;
    }
    int si = segAt (p);
    if (si >= 0)
    {
        selected = si;
        if (e.mods.isPopupMenu())
        {
            PopupMenu m;
            m.addItem (1, "cut: hold the snapshot", true, ! segs[(size_t) si].glide);
            m.addItem (2, "glide: morph into the next", true, segs[(size_t) si].glide);
            PopupMenu len;
            const float lens[] = { 1, 2, 4, 8, 16, 30, 60 };
            for (int k = 0; k < 7; ++k) len.addItem (10 + k, String (lens[k], 0) + " s", true, std::abs (segs[(size_t) si].secs - lens[k]) < 0.01f);
            m.addSubMenu ("length", len);
            m.addSeparator();
            m.addItem (3, "remove");
            m.showMenuAsync ({}, [this, si, lens] (int r)
            {
                if (si >= (int) segs.size()) return;
                if (r == 1 || r == 2) segs[(size_t) si].glide = r == 2;
                else if (r == 3) { segs.erase (segs.begin() + si); selected = -1; }
                else if (r >= 10 && r < 17) segs[(size_t) si].secs = lens[r - 10];
                else return;
                changed();
            });
            return;
        }
        if (std::abs (p.x - segRect (si).getRight()) < 6) { dragEdge = si; edgeStart = segs[(size_t) si].secs; }
        repaint();
        return;
    }
    if (laneRect().contains (p) && ! segs.empty())       // move the playhead
    {
        t = jlimit (0.0, total(), (double) ((p.x - laneRect().getX()) / pxPerSec()));
        repaint();
    }
}

void Timeline::mouseDrag (const MouseEvent& e)
{
    if (dragSlot >= 0) { dragNow = e.position; repaint(); return; }
    if (dragEdge >= 0)
    {
        float secs = edgeStart + (float) e.getDistanceFromDragStartX() / pxPerSec();
        segs[(size_t) dragEdge].secs = jmax (0.5f, std::round (secs * 2) / 2);
        if (onStatus) onStatus ("segment " + String (dragEdge + 1) + ": " + String (segs[(size_t) dragEdge].secs, 1) + " s");
        repaint();
    }
}

void Timeline::mouseUp (const MouseEvent& e)
{
    if (dragSlot >= 0)
    {
        int slot = dragSlot;
        dragSlot = -1;
        if (laneRect().contains (e.position) && e.getDistanceFromDragStart() > 4)
        {
            int at = (int) segs.size();                  // before the segment it was dropped on, or at the end
            for (int i = 0; i < (int) segs.size(); ++i) if (e.position.x < segRect (i).getCentreX()) { at = i; break; }
            segs.insert (segs.begin() + at, { slot, 8.0f, true });
            changed();
            if (onStatus) onStatus ("snapshot " + String (slot + 1) + " on the timeline: drag its right edge for its length, right-click for cut / glide");
        }
        else if (e.getDistanceFromDragStart() <= 4) recall (slot);
        repaint();
        return;
    }
    if (dragEdge >= 0) { dragEdge = -1; changed(); }
}

void Timeline::mouseDoubleClick (const MouseEvent& e)
{
    int si = segAt (e.position);
    if (si >= 0) recall (segs[(size_t) si].snap);
}

bool Timeline::keyPressed (const KeyPress& k)
{
    if ((k == KeyPress::deleteKey || k == KeyPress::backspaceKey) && selected >= 0 && selected < (int) segs.size())
    {
        segs.erase (segs.begin() + selected);
        selected = -1;
        changed();
        return true;
    }
    if (k == KeyPress::spaceKey && ! segs.empty()) { togglePlay(); return true; }
    return false;
}
