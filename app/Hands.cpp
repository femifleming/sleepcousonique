#include "Hands.h"
#include "Art.h"
#include "scq.h"

using namespace juce;

Hands::Hands (Host& h) : host (h)
{
    for (int i = 0; i < 4; ++i) items.push_back ({ "fader" + String (i + 1), SCQ_CTL_LEAN1 + i, {}, {}, true, 0.5f });
    for (int i = 0; i < 4; ++i) items.push_back ({ "button" + String (i + 1), SCQ_CTL_DOOR1 + i, {}, {}, false });
    items.push_back ({ "rocker.up", SCQ_CTL_LIFT, {}, {}, false, 1 });
    items.push_back ({ "rocker.down", SCQ_CTL_LIFT, {}, {}, false, -1 });
    items.push_back ({ "play", SCQ_CTL_WAKE, {}, {}, false });
    items.push_back ({ "dots", SCQ_CTL_LUCID, {}, {}, false });
    for (int i = 0; i < 4; ++i) host.control (SCQ_CTL_LEAN1 + i, 0.5f);
    startTimerHz (30);
}

void Hands::set (int ctl, float v) { host.control (ctl, v); }

bool Hands::lucidHeld() const { return keyHeld[7] || items[11].mouse; }

void Hands::sceneStep (int dir)
{
    rockerTaken = true;
    set (SCQ_CTL_LIFT, 0.0f);
    host.gotoScene (host.scene() + dir);
    if (onStatus) onStatus (dir > 0 ? "next scene  (dots + rocker)" : "previous scene  (dots + rocker)");
}

void Hands::resized()
{
    auto r = getLocalBounds().toFloat().reduced (14, 10);
    auto body = r.removeFromLeft (jmin (r.getWidth() * 0.62f, 640.0f));
    auto faders = body.removeFromLeft (body.getWidth() * 0.55f);
    float fw = faders.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
    {
        auto col = faders.withX (faders.getX() + fw * (float) i).withWidth (fw).reduced (8, 0);
        auto top = col.removeFromTop (16);
        leds[(size_t) i] = Rectangle<float> (9, 9).withCentre (top.getCentre());
        auto btn = col.removeFromBottom (26);
        items[(size_t) (4 + i)].r = btn.reduced (4, 2);
        items[(size_t) (4 + i)].loop = {};
        col.removeFromBottom (14);
        items[(size_t) i].r = col.reduced (jmax (0.0f, col.getWidth() * 0.5f - 13), 6);
        items[(size_t) i].loop = Rectangle<float> (14, 14).withCentre ({ col.getRight() - 6, col.getY() + 10 });
    }
    // the side: play-row LEDs, rocker, PLAY, dots
    auto side = body.reduced (12, 0);
    auto ledRow = side.removeFromTop (16);
    for (int i = 0; i < 4; ++i) leds[(size_t) (4 + i)] = Rectangle<float> (9, 9).withCentre ({ ledRow.getX() + 12 + (float) i * 18, ledRow.getCentreY() });
    side.removeFromTop (6);
    auto rocker = side.removeFromLeft (52);
    items[8].r = rocker.removeFromTop (rocker.getHeight() * 0.5f).reduced (4, 3);
    items[9].r = rocker.reduced (4, 3);
    side.removeFromLeft (10);
    side = side.withWidth (jmin (side.getWidth(), 150.0f));
    items[10].r = side.removeFromTop (side.getHeight() * 0.5f).reduced (2, 4);
    items[11].r = side.reduced (2, 4);
    for (int i = 8; i < 12; ++i) items[(size_t) i].loop = {};

    // what the hands are patched to
    r.removeFromLeft (20);
    mapArea = r;
}

void Hands::paint (Graphics& g)
{
    g.fillAll (Colour (0xffd9d6cc));
    g.setColour (Colour (0x22000000));
    g.drawHorizontalLine (0, 0, (float) getWidth());
    // LEDs
    for (int i = 0; i < 8; ++i)
    {
        float v = live ? hwLamp[i] : host.lamp (i);
        g.setColour (Colour (0xff3a3a38)); g.fillEllipse (leds[(size_t) i].expanded (1.5f));
        g.setColour (Colour (0xffffffff).withAlpha (0.12f + 0.88f * v));
        g.fillEllipse (leds[(size_t) i]);
    }
    g.setFont (art::mono (10.5f));
    for (int i = 0; i < (int) items.size(); ++i)
    {
        auto& it = items[(size_t) i];
        if (it.fader)
        {
            auto slot = it.r.withSizeKeepingCentre (4, it.r.getHeight());
            g.setColour (Colour (0xff2a2a28)); g.fillRoundedRectangle (slot, 2);
            float y = it.r.getBottom() - it.value * it.r.getHeight();
            auto cap = Rectangle<float> (it.r.getWidth(), 14).withCentre ({ it.r.getCentreX(), y });
            g.setColour (Colour (0x40000000)); g.fillRoundedRectangle (cap.translated (0, 2), 3);
            g.setColour (Colour (0xfff4f2ec)); g.fillRoundedRectangle (cap, 3);
            g.setColour (Colour (0xff8a8880)); g.drawHorizontalLine ((int) cap.getCentreY(), cap.getX() + 3, cap.getRight() - 3);
            g.setColour (art::ink);
            g.drawText (it.name, it.r.withY (it.r.getBottom() + 1).withHeight (12).expanded (20, 0), Justification::centred);
            art::drawLoop (g, it.loop.getCentre(), false, art::blue);
        }
        else
        {
            static const int hwBit[] = { 0, 1, 2, 3, 6, 7, 4, 5 };   // buttons, rocker up/down, play, dots
            bool down = it.mouse || keyHeld[i - 4] || (live && ((hwButtons >> hwBit[i - 4]) & 1u));
            auto r = it.r;
            g.setColour (Colour (0x40000000)); g.fillRoundedRectangle (r.translated (0, 2), 5);
            g.setColour (down ? Colour (0xffb8b4a8) : Colour (0xfff4f2ec)); g.fillRoundedRectangle (r, 5);
            g.setColour (Colour (0x55000000)); g.drawRoundedRectangle (r, 5, 1);
            g.setColour (art::ink);
            String label = it.name == "rocker.up" ? String ("up  ^") : it.name == "rocker.down" ? String ("down  v") : it.name == "dots" ? String::fromUTF8 ("dots  \xe2\x80\xa2\xe2\x80\xa2") : it.name == "play" ? String ("PLAY") : it.name;
            g.drawText (label, r, Justification::centred);
        }
    }
    // the map: every control, and where this scene sends it
    mapRows.clear();
    auto area = mapArea;
    g.setColour (art::umber);
    g.setFont (art::hand (13));
    auto head = area.removeFromTop (18);
    g.drawText ("what your hands do in this scene", head, Justification::left);
    if (live)
    {
        g.setColour (Colour (0xff3d7a4e));
        g.fillEllipse (Rectangle<float> (8, 8).withCentre ({ head.getRight() - 140, head.getCentreY() }));
        g.setFont (art::mono (11));
        g.drawText ("SP-1 live", head.withTrimmedLeft (head.getWidth() - 130), Justification::left);
        g.setFont (art::hand (13));
        g.setColour (art::umber);
    }
    static const char* controls[] = { "fader1", "fader2", "fader3", "fader4", "button1", "button2", "button3", "button4",
                                      "rocker", "rocker.up", "rocker.down", "play", "dots" };
    const float rowH = jmax (11.0f, jmin (15.0f, (area.getHeight() - 2) / 7.0f));
    auto colW = area.getWidth() / 2;
    int n = 0;
    g.setFont (art::mono (jmin (11.0f, rowH - 2)));
    for (auto* c : controls)
    {
        auto to = mappingFor ? mappingFor (c) : StringArray();
        auto row = Rectangle<float> (area.getX() + (float) (n / 7) * colW, area.getY() + (float) (n % 7) * rowH, colW - 8, rowH);
        ++n;
        g.setColour (to.isEmpty() ? Colour (0xff9a9488) : art::ink);
        g.drawText (String (c).paddedRight (' ', 10), row.withWidth (78), Justification::left);
        g.setColour (to.isEmpty() ? Colour (0xffb5afa2) : art::blue);
        g.drawText (to.isEmpty() ? String ("-") : to.joinIntoString (", "), row.withTrimmedLeft (78), Justification::left, true);
        mapRows.push_back ({ c, row });
    }
}

int Hands::itemAt (Point<float> p) const
{
    for (int i = 0; i < (int) items.size(); ++i)
        if (items[(size_t) i].r.expanded (items[(size_t) i].fader ? 8.0f : 0.0f, 8.0f).contains (p)) return i;
    return -1;
}

int Hands::loopAt (Point<float> p) const
{
    for (int i = 0; i < (int) items.size(); ++i)
        if (! items[(size_t) i].loop.isEmpty() && items[(size_t) i].loop.expanded (4).contains (p)) return i;
    return -1;
}

void Hands::mouseDown (const MouseEvent& e)
{
    auto p = e.position;
    int lp = loopAt (p);
    if (lp >= 0)
    {
        if (auto* dnd = DragAndDropContainer::findParentDragContainerFor (this))
            dnd->startDragging ("hand:" + items[(size_t) lp].name, this);
        return;
    }
    for (auto& [c, row] : mapRows)
        if (row.contains (p)) { if (onShowControl) onShowControl (c); return; }
    dragging = itemAt (p);
    if (dragging < 0) return;
    auto& it = items[(size_t) dragging];
    if (it.fader) mouseDrag (e);
    else
    {
        it.mouse = true;
        if (it.ctl == SCQ_CTL_LIFT && lucidHeld()) sceneStep (it.value > 0 ? 1 : -1);
        else if (! (it.ctl == SCQ_CTL_LIFT && rockerTaken)) set (it.ctl, it.ctl == SCQ_CTL_LIFT ? it.value : 1.0f);
    }
    repaint();
}

void Hands::mouseDrag (const MouseEvent& e)
{
    if (dragging < 0) return;
    auto& it = items[(size_t) dragging];
    if (! it.fader) return;
    it.value = jlimit (0.0f, 1.0f, (it.r.getBottom() - e.position.y) / it.r.getHeight());
    set (it.ctl, it.value);
    if (onStatus) onStatus (it.name + "   " + String (it.value, 2));
    repaint();
}

void Hands::mouseUp (const MouseEvent&)
{
    if (dragging >= 0)
    {
        auto& it = items[(size_t) dragging];
        if (! it.fader) { it.mouse = false; set (it.ctl, 0.0f); }
        if (it.ctl == SCQ_CTL_LIFT && ! keyHeld[4] && ! keyHeld[5]) rockerTaken = false;
    }
    dragging = -1;
    repaint();
}

void Hands::timerCallback()
{
    // a connected SP-1: its faders, buttons, scene and LEDs, live. A hardware control only
    // takes over when it actually moves, so the mouse and keys keep working too.
    bool wasLive = live;
    live = link != nullptr && link->connected();
    if (live)
    {
        auto h = link->hands();
        if (h.count != hwCount)
        {
            hwCount = h.count;
            for (int i = 0; i < 4; ++i)
                if (std::abs (h.fader[i] - hwFader[i]) > 0.004f)
                {
                    hwFader[i] = h.fader[i];
                    items[(size_t) i].value = h.fader[i];
                    set (SCQ_CTL_LEAN1 + i, h.fader[i]);
                }
            auto changed = h.buttons ^ hwButtons;
            hwButtons = h.buttons;
            auto bit = [&] (int b) { return ((h.buttons >> b) & 1u) != 0; };
            for (int d = 0; d < 4; ++d)
                if (changed & (1u << d)) set (SCQ_CTL_DOOR1 + d, bit (d) ? 1.0f : 0.0f);
            if (changed & (1u << 4)) set (SCQ_CTL_WAKE, bit (4) ? 1.0f : 0.0f);
            if (changed & (1u << 5)) set (SCQ_CTL_LUCID, bit (5) ? 1.0f : 0.0f);
            if (changed & ((1u << 6) | (1u << 7)) && ! bit (5))       // •• + rocker is the scene, not lift
                set (SCQ_CTL_LIFT, bit (6) ? 1.0f : (bit (7) ? -1.0f : 0.0f));
            if (h.scene - 1 != hwScene)
            {
                if (hwScene >= 0 && h.scene - 1 != host.scene()) host.gotoScene (h.scene - 1);
                hwScene = h.scene - 1;
            }
            for (int k = 0; k < 8; ++k) hwLamp[k] = h.lamp[k];
        }
    }
    if (live != wasLive && onStatus)
        onStatus (live ? "SP-1 connected on " + link->port() + ": following its hands" : "SP-1 unplugged");

    // the computer keyboard as the SP-1's buttons, while you aren't writing
    bool free = ! (typing && typing());
    const int keys[8] = { '1', '2', '3', '4', KeyPress::upKey, KeyPress::downKey, KeyPress::spaceKey, '.' };
    for (int k = 0; k < 8; ++k)
    {
        bool down = free && KeyPress::isKeyCurrentlyDown (keys[k]);
        if (down == keyHeld[k]) continue;
        keyHeld[k] = down;
        if (k < 4) set (SCQ_CTL_DOOR1 + k, down ? 1.0f : 0.0f);
        else if ((k == 4 || k == 5) && down && lucidHeld()) sceneStep (k == 4 ? 1 : -1);
        else if (k == 4 || k == 5)
        {
            if (rockerTaken) { if (! keyHeld[4] && ! keyHeld[5]) rockerTaken = false; continue; }
            if (k == 4) set (SCQ_CTL_LIFT, down ? 1.0f : (keyHeld[5] ? -1.0f : 0.0f));
            else set (SCQ_CTL_LIFT, down ? -1.0f : (keyHeld[4] ? 1.0f : 0.0f));
        }
        else if (k == 6) set (SCQ_CTL_WAKE, down ? 1.0f : 0.0f);
        else set (SCQ_CTL_LUCID, down ? 1.0f : 0.0f);
    }
    repaint();
}
