#include "Wingie.h"
#include "Art.h"
#include "scq.h"

using namespace juce;

Colour wingieLamp (float v)
{
    const Colour red (0xffe5251f), purple (0xffa43be0), yellow (0xfff4d23a), white (0xfffff8ee);
    v = jlimit (0.0f, 1.0f, v) * 3.0f;
    if (v < 1.0f) return red.interpolatedWith (purple, v);
    if (v < 2.0f) return purple.interpolatedWith (yellow, v - 1.0f);
    return yellow.interpolatedWith (white, v - 2.0f);
}

static const char* const noteName[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
// the computer keyboard as two piano octaves (z row keys1, the Wingie's bottom keyboard; q row keys2, the top), a and p Mode
static const int keyChar[2][12] = { { 'z', 's', 'x', 'd', 'c', 'v', 'g', 'b', 'h', 'n', 'j', 'm' },
                                    { 'q', '2', 'w', '3', 'e', 'r', '5', 't', '6', 'y', '7', 'u' } };
static const int modeChar[2] = { 'a', 'p' };

// ------------------------------------------------------------------------------------------ the panel

WingiePanel::WingiePanel (Host& h) : host (h)
{
    startTimerHz (30);
}

void WingiePanel::reset()
{
    for (int i = 0; i < 3; ++i) host.control (SCQ_CTL_LEAN1 + i, fader[i]);
    for (int s = 0; s < 2; ++s)
    {
        for (int k = 0; k < 12; ++k) host.control (SCQ_CTL_KEYS1 + s * 12 + k, keyBy[s][k] ? 1.0f : 0.0f);
        host.control (SCQ_CTL_MODE1 + s, modeBy[s] ? 1.0f : 0.0f);
        host.control (SCQ_CTL_OCTAVE1 + s, (float) octave[s]);
    }
    host.control (SCQ_CTL_SOURCE, (float) source);
    repaint();
}

void WingiePanel::setKey (int side, int k, bool down, int src)
{
    auto& by = keyBy[side][k];
    const bool was = by != 0;
    by = (juce::uint8) (down ? (by | (1u << src)) : (by & ~(1u << src)));
    if ((by != 0) != was)
    {
        host.control (SCQ_CTL_KEYS1 + side * 12 + k, by ? 1.0f : 0.0f);
        if (by && onStatus && src != 2)
            onStatus ("keys" + String (side + 1) + "  " + noteName[k] + (octave[side] != 0 ? String ("  octave ") + (octave[side] > 0 ? "+1" : "-1") : String()));
        repaint();
    }
}

void WingiePanel::setMode (int side, bool down, int src)
{
    auto& by = modeBy[side];
    const bool was = by != 0;
    by = (juce::uint8) (down ? (by | (1u << src)) : (by & ~(1u << src)));
    if ((by != 0) != was) { host.control (SCQ_CTL_MODE1 + side, by ? 1.0f : 0.0f); repaint(); }
}

void WingiePanel::setOctave (int side, int v)
{
    v = jlimit (-1, 1, v);
    if (v == octave[side]) return;
    octave[side] = v;
    host.control (SCQ_CTL_OCTAVE1 + side, (float) v);
    if (onStatus) onStatus ("octave" + String (side + 1) + "   " + (v > 0 ? "+1" : String (v)));
    repaint();
}

void WingiePanel::setSource (int v)
{
    source = v ? 1 : 0;
    host.control (SCQ_CTL_SOURCE, (float) source);
    if (onStatus) onStatus (source ? "source: line in" : "source: microphones");
    repaint();
}

void WingiePanel::setFader (int i, float v)
{
    fader[i] = jlimit (0.0f, 1.0f, v);
    host.control (SCQ_CTL_LEAN1 + i, fader[i]);
    repaint();
}

// ------------------------------------------------------------------------------------------ layout

void WingiePanel::resized()
{
    // the Wingie as it is (Meng Qi's photo of the Wingie2, in its own pixels: 780 wide, 902 high):
    // grilles and the Line In / Microphone switch at the top, L.Octave R.Octave, L.Mode R.Mode,
    // the Mix Decay Volume faders, the two lights by Volume, and the two keyboards one above the
    // other — keys2 the top, keys1 the bottom — each a row of black buttons over a row of white
    auto r = getLocalBounds().toFloat().reduced (10, 8);
    const float fh = r.getHeight(), fw = fh * 780.0f / 902.0f;
    face = r.removeFromLeft (fw);
    unit = fw / 780.0f;
    auto circle = [this] (float x, float y, float rad) { return Rectangle<float> (2 * rad * unit, 2 * rad * unit).withCentre (at (x, y)); };
    sourceSw = Rectangle<float> (26 * unit, 46 * unit).withCentre (at (171, 134));
    for (int s = 0; s < 2; ++s)
    {
        const float x = s == 0 ? 78.0f : 183.0f;
        octSw[s] = Rectangle<float> (26 * unit, 46 * unit).withCentre (at (x, 290));
        modeBtn[s] = circle (x, 442, 30);
        modeLoop[s] = Rectangle<float> (12, 12).withCentre (at (x + 54, 442));   // drag these onto an inlet: mode1 / mode2
        octLoop[s] = Rectangle<float> (12, 12).withCentre (at (x + 44, 290));     // octave1 / octave2
        lampR[s] = circle (s == 0 ? 595.0f : 702.0f, 442, 11);
        const float y = s == 1 ? 586.0f : 780.0f;        // keys2 on top, keys1 below
        keybed[s] = Rectangle<float> (at (44, y - 78), at (736, y + 78));
        board[s] = keybed[s];
        keysLoop[s] = Rectangle<float> (14, 14).withCentre ({ face.getRight() + 16, at (0, y).y + 8 });   // just outside the face, by its keyboard
    }
    for (int i = 0; i < 3; ++i)
    {
        const float x = 286.0f + 104.0f * (float) i;
        faderR[i] = Rectangle<float> (at (x - 14, 147), at (x + 14, 437));
        faderLoop[i] = Rectangle<float> (12, 12).withCentre (at (x + 30, 140));
    }
    micLoop = Rectangle<float> (14, 14).withCentre (at (100, 182));        // beside the panel's MIC
    lineLoop = Rectangle<float> (14, 14).withCentre (at (100, 92));        // beside LINE IN
    sourceLoop = Rectangle<float> (12, 12).withCentre (at (171 + 46, 134));

    r.removeFromLeft (40);                               // room for the keyboards' loops and names
    auto top = r.removeFromTop (24);
    listenBtn = top.removeFromRight (jmin (150.0f, top.getWidth() * 0.4f)).reduced (0, 2);
    mapArea = r.withTop (r.getY() - 24);
}

Rectangle<float> WingiePanel::keyRect (int side, int k) const
{
    static const float x[12] = { 78, 130, 182, 234, 286, 390, 442, 494, 546, 598, 650, 702 };
    const float y = (side == 1 ? 586.0f : 780.0f) + (isBlack (k) ? -46.0f : 46.0f);
    return Rectangle<float> (60 * unit, 60 * unit).withCentre (at (x[k], y));
}

int WingiePanel::keyAt (Point<float> p, int& side) const
{
    for (int s = 0; s < 2; ++s)
        for (int k = 0; k < 12; ++k)
        {
            auto r = keyRect (s, k);
            if (r.getCentre().getDistanceFrom (p) <= r.getWidth() * 0.5f + 2) { side = s; return k; }
        }
    return -1;
}

// ------------------------------------------------------------------------------------------ drawing

void WingiePanel::paint (Graphics& g)
{
    auto& th = art::theme();
    g.fillAll (th.pop ? th.window : th.rail);
    g.setColour (th.line);
    g.drawHorizontalLine (0, 0, (float) getWidth());
    const auto ink = th.pop ? th.chromeText : art::ink;
    const auto held = th.pop ? th.checkA : art::gold;

    // the face: the Wingie's aluminium, as it is in Meng Qi's photo
    {
        auto f = face;
        if (th.modular) { g.setColour (th.button); g.fillRect (f); art::bevel (g, f, true); }
        else if (th.pop) { g.setColour (th.chromeText.withAlpha (0.35f)); g.strokePath (art::slant (f, 12.0f), PathStrokeType (1.0f)); }
        else
        {
            g.setColour (Colour (0x30000000)); g.fillRoundedRectangle (f.translated (0, 2), 8);
            g.setColour (th.cardSp1); g.fillRoundedRectangle (f, 8);
            g.setColour (th.dark ? Colour (0x40ffffff) : Colour (0x40000000)); g.drawRoundedRectangle (f, 8, 1);
        }
        // the two grilles: a diamond of holes each
        g.setColour (th.dark || th.pop ? Colour (0xff0c0b0a) : Colour (0xff3a3632));
        for (float gx : { 80.0f, 702.0f })
            for (int row = -3; row <= 3; ++row)
                for (int col = -3; col <= 3; ++col)
                    if (std::abs (row) + std::abs (col) <= 3 && ((row + col) & 1) == 0)
                        g.fillEllipse (Rectangle<float> (7 * unit, 7 * unit).withCentre (at (gx + 21.0f * (float) col, 137 + 21.0f * (float) row)));
    }
    auto silk = [&] (const String& t, float x, float y, float w = 120)   // the panel's printed words
    {
        g.setColour (th.pop ? th.chromeDim : ink.withAlpha (0.8f));
        g.setFont (th.pop ? art::label (jmax (7.5f, 17 * unit)) : art::mono (jmax (7.5f, 18 * unit)));
        g.drawText (t, Rectangle<float> (w * unit, 22 * unit).withCentre (at (x, y)), Justification::centred, false);
    };
    // a toggle: a dark collar and a lever up, centred or down (pos +1, 0, -1)
    auto toggle = [&] (Rectangle<float> r, int pos)
    {
        auto collar = Rectangle<float> (r.getWidth(), r.getWidth()).withCentre (r.getCentre());
        g.setColour (Colour (0xff2b2826)); g.fillEllipse (collar);
        g.setColour (Colour (0xff6e6a64)); g.drawEllipse (collar, 1.0f);
        auto c = r.getCentre();
        const auto tip = c.translated (0, -(float) pos * r.getHeight() * 0.42f + (pos == 0 ? -r.getHeight() * 0.05f : 0.0f));
        g.setColour (Colour (0xffdcdad4)); g.drawLine ({ c, tip }, jmax (2.0f, r.getWidth() * 0.18f));
        g.fillEllipse (Rectangle<float> (r.getWidth() * 0.32f, r.getWidth() * 0.32f).withCentre (tip));
    };
    silk ("LINE IN", 171, 92); silk ("MIC", 171, 182);
    toggle (sourceSw, source == 1 ? 1 : -1);
    art::drawLoop (g, sourceLoop.getCentre(), false, art::blue);
    art::drawLoop (g, lineLoop.getCentre(), false, art::umber);

    // the lights, in their holes by Volume: red underneath, then purple, yellow, white
    for (int s = 0; s < 2; ++s)
    {
        const float v = following ? hwLamp[s] : host.lamp (s);
        auto l = lampR[s];
        auto col = wingieLamp (v);
        g.setColour (col.withAlpha (0.25f + 0.25f * v)); g.fillEllipse (l.expanded (4 + 3 * v));
        g.setColour (th.ledOff); g.fillEllipse (l.expanded (1.5f));
        g.setColour (col); g.fillEllipse (l);
        g.setColour (Colour (0xffe5251f).withAlpha (0.8f)); g.drawEllipse (l, 1.2f);   // the red underneath
        g.setColour (Colours::white.withAlpha (0.35f)); g.fillEllipse (l.reduced (l.getWidth() * 0.3f).translated (-1, -1));
    }

    // L.Octave R.Octave (up is an octave up) and L.Mode R.Mode, round, as on the panel
    for (int s = 0; s < 2; ++s)
    {
        const float x = s == 0 ? 78.0f : 183.0f;
        silk (s == 0 ? "L.OCT" : "R.OCT", x, 250);
        toggle (octSw[s], octave[s]);
        art::drawLoop (g, octLoop[s].getCentre(), false, art::blue);
        art::drawLoop (g, modeLoop[s].getCentre(), false, art::blue);
        silk (s == 0 ? "L.MODE" : "R.MODE", x, 398);
        auto r = modeBtn[s];
        const bool down = modeBy[s] != 0;
        if (th.modular) { g.setColour (down ? th.checkA : th.button); g.fillEllipse (r); g.setColour (Colours::black); g.drawEllipse (r, 1.0f); }
        else
        {
            g.setColour (Colour (0x40000000)); g.fillEllipse (r.translated (0, 1.5f));
            g.setColour (down ? held : Colour (0xffd0cdc6)); g.fillEllipse (r);
            g.setColour (Colour (0x60000000)); g.drawEllipse (r, 1.0f);
        }
    }

    // the two keyboards: round buttons, the black row over the white, keys2 on top, keys1 below
    for (int s = 0; s < 2; ++s)
    {
        for (int k = 0; k < 12; ++k)
        {
            auto r = keyRect (s, k);
            const bool down = keyBy[s][k] != 0, blk = isBlack (k);
            if (th.modular)
            {
                g.setColour (down ? th.checkA : (blk ? Colours::black : Colours::white)); g.fillEllipse (r);
                g.setColour (Colours::black); g.drawEllipse (r, 1.0f);
            }
            else
            {
                g.setColour (Colour (0x38000000)); g.fillEllipse (r.translated (0, 1.5f));
                g.setColour (down ? held : (blk ? Colour (0xff121110) : (th.dark ? Colour (0xffe6e3dc) : Colour (0xffefece6)))); g.fillEllipse (r);
                g.setColour (Colour (0x55000000)); g.drawEllipse (r, 0.8f);
            }
            if (! blk && unit * 60 >= 18)
            {
                g.setColour (down ? Colours::black.withAlpha (0.6f) : Colour (0xff1d1b19).withAlpha (0.45f));
                g.setFont (th.pop ? art::label (jmax (7.0f, 15 * unit)) : art::mono (jmax (7.0f, 15 * unit)));
                g.drawText (String (noteName[k]), r, Justification::centred, false);
            }
        }
        // which keyboard is which: written in the margin, by the loop you drag it from
        auto lb = keysLoop[s];
        g.setColour (th.pop ? th.chromeDim : ink.withAlpha (0.7f));
        g.setFont (th.pop ? art::label (jmax (7.5f, 16 * unit)) : art::mono (jmax (7.5f, 16 * unit)));
        g.setFont (th.pop ? art::label (9) : art::mono (9));
        g.drawText (s == 0 ? "keys1" : "keys2", Rectangle<float> (40, 12).withCentre (lb.getCentre().translated (2, -15)), Justification::centred, false);
        art::drawLoop (g, keysLoop[s].getCentre(), false, art::blue);
    }

    // Mix, Decay, Volume: long slots, as on the panel
    static const char* const fname[3] = { "MIX", "DECAY", "VOLUME" };
    for (int i = 0; i < 3; ++i)
    {
        auto r = faderR[i];
        const float y = r.getBottom() - fader[i] * r.getHeight();
        auto slot = r.withSizeKeepingCentre (jmax (4.0f, 16 * unit), r.getHeight());
        g.setColour (Colour (0xff121110)); g.fillRoundedRectangle (slot, slot.getWidth() * 0.5f);
        auto cap = Rectangle<float> (26 * unit, 40 * unit).withCentre ({ r.getCentreX(), y });
        if (th.modular) { g.setColour (th.button); g.fillRect (cap); art::bevel (g, cap, true); }
        else
        {
            g.setColour (Colour (0x40000000)); g.fillEllipse (cap.translated (0, 2));
            g.setColour (th.pop ? th.checkA : Colour (0xfff4f1ea)); g.fillEllipse (cap);
            g.setColour (Colour (0x50000000)); g.drawEllipse (cap, 0.8f);
        }
        silk (fname[i], 286.0f + 104.0f * (float) i, 462);
        art::drawLoop (g, faderLoop[i].getCentre(), false, art::blue);
    }

    // listen: the computer's microphone as the Wingie's, with its level
    {
        auto r = listenBtn;
        const bool on = host.listening();
        if (th.modular) { g.setColour (on ? th.checkA : th.button); g.fillRect (r); g.setColour (Colours::black); g.drawRect (r, 1.0f); art::bevel (g, r.reduced (1), ! on); }
        else if (th.pop)
        {
            auto p = art::slant (r, 7.0f);
            if (on) { g.setColour (th.sound); g.fillPath (p); }
            g.setColour (on ? th.sound : ink.withAlpha (0.8f)); g.strokePath (p, PathStrokeType (1.0f));
        }
        else
        {
            g.setColour (on ? Colour (0xffc0392b) : th.button); g.fillRoundedRectangle (r, 4);
            g.setColour (th.dark ? Colour (0x55ffffff) : Colour (0x55000000)); g.drawRoundedRectangle (r, 4, 1);
        }
        g.setColour (on ? (th.modular ? Colours::black : Colours::white) : ink);
        g.setFont (th.pop ? art::label (9.5f) : art::mono (10));
        g.drawText (on ? "listening" : "listen: off", r.withTrimmedRight (14), Justification::centred);
        for (int c = 0; c < 2; ++c)                       // the input's two sides
        {
            auto m = Rectangle<float> (r.getRight() - 11 + (float) c * 5, r.getY() + 3, 3, r.getHeight() - 6);
            g.setColour (Colours::black.withAlpha (0.25f)); g.fillRect (m);
            const float v = jlimit (0.0f, 1.0f, host.inLevel[c]);
            g.setColour (v > 0.9f ? Colour (0xffff4a3a) : (on ? Colour (0xff5fcf7a) : th.chromeDim));
            g.fillRect (m.withTop (m.getBottom() - v * m.getHeight()));
        }
        art::drawLoop (g, micLoop.getCentre(), false, art::umber);
    }

    // what the hands are patched to in this scene
    mapRows.clear();
    auto area = mapArea;
    g.setColour (th.pop ? th.chromeText : art::umber);
    g.setFont (th.pop ? art::sans (12.5f, true) : art::hand (13));
    auto head = area.removeFromTop (24).withTrimmedRight (listenBtn.getWidth() + 10);
    g.drawText ("what your hands do in this scene", head, Justification::left);
    if (following)
    {
        g.setColour (th.hand);
        g.fillEllipse (Rectangle<float> (8, 8).withCentre ({ head.getRight() - 96, head.getCentreY() }));
        g.setFont (art::mono (11));
        g.drawText ("Wingie live", head.withTrimmedLeft (head.getWidth() - 86), Justification::left);
    }
    static const char* const controls[] = { "fader1", "fader2", "fader3", "keys1", "keys1.gate", "keys1.trig", "keys1.key",
                                            "keys2", "keys2.gate", "keys2.trig", "keys2.key", "mode1", "mode1.tap", "mode2",
                                            "mode2.tap", "octave1", "octave2", "source", "mic", "mic.right", "mic.both", "line", "line.right", "line.both" };
    StringArray patched, idle;
    std::map<String, StringArray> where;
    for (auto* c : controls)
    {
        auto to = mappingFor ? mappingFor (c) : StringArray();
        if (to.isEmpty()) idle.add (c); else { patched.add (c); where[c] = to; }
    }
    const int rowsPer = 7, cols = area.getWidth() > 420 ? 3 : 2;
    const float rowH = jmax (11.0f, jmin (15.0f, (area.getHeight() - 2) / (float) rowsPer));
    const float colW = area.getWidth() / (float) cols;
    g.setFont (th.pop ? art::label (jmin (10.5f, rowH - 2)) : art::mono (jmin (11.0f, rowH - 2)));
    int n = 0;
    StringArray shown (patched);
    shown.addArray (idle);
    for (auto& c : shown)
    {
        if (n >= rowsPer * cols) break;
        auto to = where.count (c) ? where[c] : StringArray();
        auto row = Rectangle<float> (area.getX() + (float) (n / rowsPer) * colW, area.getY() + (float) (n % rowsPer) * rowH, colW - 8, rowH);
        ++n;
        g.setColour (to.isEmpty() ? th.chromeDim : ink);
        g.drawText (c, row.withWidth (74), Justification::left);
        g.setColour (to.isEmpty() ? th.chromeDim.withAlpha (0.7f) : (th.pop ? th.word : art::blue));
        g.drawText (to.isEmpty() ? String ("-") : to.joinIntoString (", "), row.withTrimmedLeft (74), Justification::left, true);
        mapRows.push_back ({ c, row });
    }
}

// ------------------------------------------------------------------------------------------ hands

void WingiePanel::mouseDown (const MouseEvent& e)
{
    auto p = e.position;
    auto dragHand = [this] (const String& control)
    {
        if (auto* dnd = DragAndDropContainer::findParentDragContainerFor (this)) dnd->startDragging ("hand:" + control, this);
    };
    for (int i = 0; i < 3; ++i) if (faderLoop[i].expanded (4).contains (p)) { dragHand ("fader" + String (i + 1)); return; }
    for (int s = 0; s < 2; ++s) if (keysLoop[s].expanded (4).contains (p)) { dragHand ("keys" + String (s + 1)); return; }
    if (micLoop.expanded (4).contains (p)) { dragHand ("mic.both"); return; }
    if (lineLoop.expanded (4).contains (p)) { dragHand ("line.both"); return; }
    if (sourceLoop.expanded (3).contains (p)) { dragHand ("source"); return; }
    for (int s = 0; s < 2; ++s)
    {
        if (octLoop[s].expanded (3).contains (p)) { dragHand ("octave" + String (s + 1)); return; }
        if (modeLoop[s].expanded (3).contains (p)) { dragHand ("mode" + String (s + 1)); return; }
    }
    for (auto& [c, row] : mapRows)
        if (row.contains (p)) { if (onShowControl) onShowControl (c); return; }

    int side = 0;
    int k = keyAt (p, side);
    if (k >= 0) { drag = Drag::Key; dragSide = side; dragIndex = k; setKey (side, k, true, 0); return; }
    for (int s = 0; s < 2; ++s)
    {
        if (modeBtn[s].contains (p)) { drag = Drag::Mode; dragSide = s; setMode (s, true, 0); return; }
        if (octSw[s].expanded (6, 4).contains (p))      // a toggle: click above its middle to move it up, below to move it down
        {
            setOctave (s, jlimit (-1, 1, octave[s] + (p.y < octSw[s].getCentreY() ? 1 : -1)));
            return;
        }
    }
    if (sourceSw.expanded (6, 4).contains (p)) { setSource (p.y < sourceSw.getCentreY() ? 1 : 0); return; }   // up: line in, down: microphones
    if (listenBtn.contains (p)) { if (onListen) onListen (! host.listening()); repaint(); return; }
    for (int i = 0; i < 3; ++i)
        if (faderR[i].expanded (10, 8).contains (p)) { drag = Drag::Fader; dragIndex = i; mouseDrag (e); return; }
}

void WingiePanel::mouseDrag (const MouseEvent& e)
{
    if (drag == Drag::Fader && dragIndex >= 0)
    {
        auto r = faderR[dragIndex];
        setFader (dragIndex, (r.getBottom() - e.position.y) / r.getHeight());
        static const char* const fname[3] = { "Mix", "Decay", "Volume" };
        if (onStatus) onStatus ("fader" + String (dragIndex + 1) + " (" + fname[dragIndex] + ")   " + String (fader[dragIndex], 2));
    }
    else if (drag == Drag::Key)
    {
        int side = 0;
        int k = keyAt (e.position, side);
        if (k >= 0 && (k != dragIndex || side != dragSide))   // glide along the keys
        {
            setKey (dragSide, dragIndex, false, 0);
            dragSide = side; dragIndex = k;
            setKey (side, k, true, 0);
        }
    }
}

void WingiePanel::mouseUp (const MouseEvent&)
{
    if (drag == Drag::Key && dragIndex >= 0) setKey (dragSide, dragIndex, false, 0);
    if (drag == Drag::Mode) setMode (dragSide, false, 0);
    drag = Drag::None;
    dragIndex = -1;
}

void WingiePanel::timerCallback()
{
    // a connected Wingie: its hands move these; a control follows it only when it moves there
    const bool was = following;
    following = link != nullptr && link->device() == serial::wingie;
    if (following)
    {
        auto h = link->whands();
        if (h.count != hwCount)
        {
            hwCount = h.count;
            for (int i = 0; i < 3; ++i)
                if (std::abs (h.fader[i] - hwFader[i]) > 0.004f) { hwFader[i] = h.fader[i]; setFader (i, h.fader[i]); }
            for (int s = 0; s < 2; ++s)
            {
                auto changed = h.keys[s] ^ hwKeys[s];
                hwKeys[s] = h.keys[s];
                for (int k = 0; k < 12; ++k) if (changed & (1u << k)) setKey (s, k, (h.keys[s] >> k) & 1u, 2);
                if (((h.mode ^ hwMode) >> s) & 1u) setMode (s, (h.mode >> s) & 1u, 2);
                if (h.octave[s] != hwOct[s]) { hwOct[s] = h.octave[s]; setOctave (s, h.octave[s]); }
                hwLamp[s] = h.lamp[s];
            }
            hwMode = h.mode;
            if (h.source != hwSource) { hwSource = h.source; setSource (h.source); }
            if (h.scene - 1 != hwScene)
            {
                if (hwScene >= 0 && h.scene - 1 != host.scene()) host.gotoScene (h.scene - 1);
                hwScene = h.scene - 1;
            }
        }
    }
    else if (was)
    {   // let go of whatever the Wingie was holding
        for (int s = 0; s < 2; ++s) { for (int k = 0; k < 12; ++k) setKey (s, k, false, 2); setMode (s, false, 2); hwKeys[s] = 0; hwOct[s] = 9; }
        hwMode = 0; hwSource = 9; hwScene = -1; hwCount = 0;
        for (auto& f : hwFader) f = -1;
    }
    if (following != was && onStatus)
        onStatus (following ? "Wingie connected on " + link->port() + ": following its hands" : "Wingie unplugged");

    // the computer keyboard as the two keyboards, while you aren't writing
    const bool free = isShowing() && ! (typing && typing());
    for (int s = 0; s < 2; ++s)
    {
        for (int k = 0; k < 12; ++k)
        {
            const bool down = free && KeyPress::isKeyCurrentlyDown (keyChar[s][k]);
            if (down != keyDown[s * 12 + k]) { keyDown[s * 12 + k] = down; setKey (s, k, down, 1); }
        }
        const bool m = free && KeyPress::isKeyCurrentlyDown (modeChar[s]);
        if (m != keyDown[24 + s]) { keyDown[24 + s] = m; setMode (s, m, 1); }
    }
    if (isShowing()) repaint();
}

// ------------------------------------------------------------------------------------------ meters

WingieMeters::WingieMeters (Host& h) : host (h) { startTimerHz (10); }

void WingieMeters::timerCallback()
{
    if (link != nullptr && link->device() == serial::wingie)
    {
        auto m = link->measured();
        if (m.count != seen && m.avg >= 0)
        {
            seen = m.count;
            last = m;

            Sample s;
            s.avg = m.load >= 0 ? m.load / 100.0f : (float) m.avg / 100.0f;
            s.peak = m.loadPeak >= 0 ? m.loadPeak / 100.0f : (float) m.peak / 100.0f;
            s.estimate = estimate ? estimate() : 0.0f;
            s.dropped = m.dropped > 0;
            history.push_back (s);
            while (history.size() > 60) history.pop_front();
        }
    }
    if (isShowing()) repaint();
}

void WingieMeters::paint (Graphics& g)
{
    auto& th = art::theme();
    g.fillAll (th.pop ? th.window : th.rail);
    g.setColour (th.line);
    g.drawHorizontalLine (0, 0, (float) getWidth());
    g.drawVerticalLine (0, 0, (float) getHeight());
    const auto ink = th.pop ? th.chromeText : art::ink;
    const Colour okC = th.dark ? Colour (0xff5fcf7a) : Colour (0xff1f8a3c), tightC (0xfff7931e), overC (0xffff4a3a);
    const Colour accent = th.pop ? th.checkA : art::blue, peakC = th.pop ? th.sound : art::copper;
    auto judge = [&] (float v) { return v > 1.0f ? overC : (v > 0.85f ? tightC : okC); };
    auto word = [] (float v) { return v > 1.0f ? String ("too heavy") : (v > 0.85f ? String ("tight") : String ("ok")); };

    const bool connected = link != nullptr && link->device() == serial::wingie;
    const bool fresh = (connected && last.avg >= 0 && Time::getMillisecondCounter() - last.when < 3500);
    const float est = estimate ? estimate() : 0.0f;

    auto r = getLocalBounds().toFloat().reduced (12, 8);
    auto head = r.removeFromTop (16);
    g.setColour (ink);
    g.setFont (th.pop ? art::sans (12.5f, true) : art::label (11.5f));
    g.drawText ("WINGIE  cpu and load", head, Justification::left);
    g.setColour (fresh ? th.hand : th.chromeDim);
    g.setFont (th.pop ? art::label (9.5f) : art::mono (10));
    g.drawText (
                connected ? (fresh ? "measured on " + link->port().fromLastOccurrenceOf ("/", false, false) : String ("connected, waiting for its report..."))
                : String ("estimate only: plug in a Wingie to measure"), head, Justification::right);
    r.removeFromTop (4);

    auto left = r.removeFromLeft (r.getWidth() * 0.47f);
    r.removeFromLeft (12);
    auto right = r;

    auto bar = [&] (Rectangle<float> b, float v, float v2, Colour c, Colour c2)
    {
        g.setColour (th.pop ? th.rail : (th.dark ? Colour (0x22ffffff) : Colour (0x22000000)));
        g.fillRect (b);
        const float full = b.getWidth() / 1.3f;               // to 130 %; the tick is 100 %
        if (v2 > v) { g.setColour (c2.withAlpha (0.55f)); g.fillRect (b.withWidth (jlimit (0.0f, b.getWidth(), full * v2))); }
        g.setColour (c); g.fillRect (b.withWidth (jlimit (0.0f, b.getWidth(), full * v)));
        g.setColour (ink); g.fillRect (Rectangle<float> (b.getX() + full - 1, b.getY() - 2, 2, b.getHeight() + 4));
    };
    auto small = [&] () { return th.pop ? art::label (9.5f) : art::mono (10); };

    // the estimate
    {
        auto a = left.removeFromTop (38);
        g.setColour (th.chromeDim); g.setFont (small());
        g.drawText ("LOAD (estimate, this scene)", a.removeFromTop (12), Justification::left);
        auto num = a.removeFromTop (18);
        g.setColour (judge (est)); g.setFont (art::label (17));
        g.drawText (String (roundToInt (est * 100)) + "%", num.withWidth (56), Justification::left);
        g.setFont (small());
        g.drawText (word (est) + "   of the Wingie's processor", num.withTrimmedLeft (58), Justification::centredLeft, true);
        bar (a.removeFromTop (6).withTrimmedTop (1), est, 0, judge (est), judge (est));
    }
    left.removeFromTop (4);
    // what the Wingie measures
    {
        auto a = left.removeFromTop (38);
        const float avg = fresh ? (last.load >= 0 ? last.load / 100.0f : (float) last.avg / 100.0f) : 0.0f;
        const float pk = fresh ? (last.loadPeak >= 0 ? last.loadPeak / 100.0f : (float) last.peak / 100.0f) : 0.0f;
        g.setColour (th.chromeDim); g.setFont (small());
        g.drawText ("CPU (measured: average, peak)", a.removeFromTop (12), Justification::left);
        auto num = a.removeFromTop (18);
        g.setColour (fresh ? judge (pk) : th.chromeDim); g.setFont (art::label (17));
        g.drawText (fresh ? String (roundToInt (avg * 100)) + "%" : String ("--"), num.withWidth (56), Justification::left);
        g.setFont (small());
        g.drawText (fresh ? "peak " + String (roundToInt (pk * 100)) + "%   " + word (pk) : String ("not connected"), num.withTrimmedLeft (58), Justification::centredLeft, true);
        bar (a.removeFromTop (6).withTrimmedTop (1), avg, pk, fresh ? judge (avg) : th.chromeDim, peakC);
    }
    left.removeFromTop (5);
    // the rest of the report, two by two
    {
        const float lh = jmin (14.0f, left.getHeight() / 3);
        g.setFont (small());
        auto cell = [&] (int i, const String& name, const String& value, Colour c)
        {
            auto row = Rectangle<float> (left.getX() + (float) (i % 2) * left.getWidth() * 0.5f, left.getY() + (float) (i / 2) * lh, left.getWidth() * 0.5f - 4, lh);
            auto nameArea = row.removeFromLeft (jmin (row.getWidth() * 0.42f, 70.0f));   // the value never runs under the name
            g.setColour (th.chromeDim); g.drawText (name, nameArea, Justification::left, true);
            g.setColour (c); g.drawFittedText (value, row.toNearestInt(), Justification::right, 1, 0.8f);
        };
        const String dash ("--");
        cell (0, "control core", fresh && last.core1 >= 0 ? String (last.core1) + "%" : dash, fresh && last.core1 > 85 ? overC : ink);
        cell (1, "heap free", fresh && last.heap >= 0 ? String (roundToInt ((float) last.heap / 1024.0f)) + " KB" : dash,
              fresh && last.heap >= 0 && last.heap < 16384 ? overC : ink);
        String poolSay = poolWord ? poolWord() : String();
        cell (2, "pool", fresh && last.memTotal > 0 ? String (roundToInt ((float) last.memUsed / 1024.0f)) + " of " + String (roundToInt ((float) last.memTotal / 1024.0f)) + " KB"
                                                    : poolSay.isNotEmpty() ? "~" + poolSay : dash,
              fresh && last.memTotal > 0 && last.memUsed > last.memTotal ? overC : ink);
        const bool drops = fresh && last.dropped > 0;
        cell (3, "dropped", fresh && last.dropped >= 0 ? String (last.dropped) + (last.blocks > 0 ? " of " + String (last.blocks) : String()) : dash, drops ? overC : ink);
        if (drops)
        {
            auto warn = Rectangle<float> (left.getX(), left.getY() + 2 * lh, left.getWidth(), lh + 1);
            g.setColour (overC.withAlpha (0.18f)); g.fillRect (warn);
            g.setColour (overC);
            g.drawText (String::fromUTF8 ("\xe2\x96\xb2 blocks dropped: too heavy, it stutters"), warn.reduced (4, 0), Justification::left, true);
        }
    }

    // the last minute: average filled, peaks as a line, the estimate dashed, 85 % and 100 % marked
    auto graph = right.removeFromTop (right.getHeight() - 30);
    {
        g.setColour (th.pop ? th.rail.withAlpha (0.35f) : (th.dark ? Colour (0x16ffffff) : Colour (0x12000000)));
        g.fillRect (graph);
        g.setColour (th.line); g.drawRect (graph, 1.0f);
        const float top = 1.3f;                                   // the graph runs to 130 %
        auto yOf = [&] (float v) { return graph.getBottom() - jlimit (0.0f, top, v) / top * graph.getHeight(); };
        g.setColour (tightC.withAlpha (0.6f));
        { float y = yOf (0.85f); float dl[] = { 3, 3 }; g.drawDashedLine ({ graph.getX(), y, graph.getRight(), y }, dl, 2, 1.0f); }
        g.setColour (overC.withAlpha (0.8f)); g.drawHorizontalLine ((int) yOf (1.0f), graph.getX(), graph.getRight());
        g.setFont (art::mono (8.5f)); g.setColour (th.chromeDim);
        g.drawText ("100%", Rectangle<float> (graph.getX() + 2, yOf (1.0f) - 11, 40, 10), Justification::left);
        const float dx = graph.getWidth() / 59.0f;
        const int n = (int) history.size();
        auto xOf = [&] (int i) { return graph.getRight() - (float) (n - 1 - i) * dx; };
        if (n > 0)
        {
            Path area, pk;
            area.startNewSubPath (xOf (0), graph.getBottom());
            for (int i = 0; i < n; ++i) area.lineTo (xOf (i), yOf (history[(size_t) i].avg));
            area.lineTo (xOf (n - 1), graph.getBottom());
            area.closeSubPath();
            g.setColour (accent.withAlpha (0.45f)); g.fillPath (area);
            for (int i = 0; i < n; ++i)
            {
                if (i == 0) pk.startNewSubPath (xOf (i), yOf (history[(size_t) i].peak));
                else pk.lineTo (xOf (i), yOf (history[(size_t) i].peak));
                if (history[(size_t) i].dropped)
                {
                    g.setColour (overC); g.fillRect (Rectangle<float> (xOf (i) - 1.5f, graph.getY(), 3, graph.getHeight()));
                }
            }
            g.setColour (peakC); g.strokePath (pk, PathStrokeType (1.4f));
        }
        g.setColour (ink.withAlpha (0.7f));
        { float y = yOf (est); float dl[] = { 5, 3 }; g.drawDashedLine ({ graph.getX(), y, graph.getRight(), y }, dl, 2, 1.2f); }
        g.setColour (th.chromeDim); g.setFont (art::mono (8.5f));
        g.drawText (n > 0 ? String ("1 min: avg, peak, est.") : String ("a minute of reports"),
                    graph.reduced (4, 2), Justification::topRight, true);
    }
    right.removeFromTop (6);
    // levels: the Wingie's ins and outs (dBFS over the last second), or the computer's
    {
        auto dbOf = [] (float lin) { return lin <= 0.00001f ? -99 : jmax (-99, roundToInt (20.0f * std::log10 (lin))); };
        int lv[4];
        const bool hw = fresh && last.inL > -100;
        if (hw) { lv[0] = last.inL; lv[1] = last.inR; lv[2] = last.outL; lv[3] = last.outR; }
        else { lv[0] = dbOf (host.inLevel[0]); lv[1] = dbOf (host.inLevel[1]); lv[2] = dbOf (host.level); lv[3] = dbOf (host.levelR); }
        const char* names[4] = { "in L", "in R", "out L", "out R" };
        const float cw = right.getWidth() / 4;
        g.setFont (art::mono (8.5f));
        for (int i = 0; i < 4; ++i)
        {
            auto c = Rectangle<float> (right.getX() + cw * (float) i, right.getY(), cw - 6, right.getHeight());
            g.setColour (th.chromeDim);
            g.drawText (String (names[i]) + (hw ? "" : (i < 2 ? " mic" : " sim")), c.removeFromTop (11), Justification::left, true);   // the name on its own line,
            g.drawText (lv[i] <= -99 ? String ("-inf") : String (lv[i]) + " dB", c.removeFromTop (11), Justification::left, true);   // the reading under it
            auto b = c.removeFromTop (6).withTrimmedTop (1);
            g.setColour (th.pop ? th.rail : (th.dark ? Colour (0x22ffffff) : Colour (0x22000000))); g.fillRect (b);
            const float v = jlimit (0.0f, 1.0f, ((float) lv[i] + 60.0f) / 60.0f);   // -60 .. 0 dBFS
            g.setColour (lv[i] > -1 ? overC : (lv[i] > -6 ? tightC : okC));
            g.fillRect (b.withWidth (b.getWidth() * v));
        }
    }
}
