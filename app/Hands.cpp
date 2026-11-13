#include "Hands.h"
#include "Art.h"
#include "scq.h"

using namespace juce;

Hands::Hands (Host& h) : host (h)
{
    for (int i = 0; i < 4; ++i) items.push_back ({ "lean" + String (i + 1), SCQ_CTL_LEAN1 + i, {}, {}, true, 0.5f });
    for (int i = 0; i < 4; ++i) items.push_back ({ "door" + String (i + 1), SCQ_CTL_DOOR1 + i, {}, {}, false });
    items.push_back ({ "lift.up", SCQ_CTL_LIFT, {}, {}, false, 1 });
    items.push_back ({ "lift.down", SCQ_CTL_LIFT, {}, {}, false, -1 });
    items.push_back ({ "wake", SCQ_CTL_WAKE, {}, {}, false });
    items.push_back ({ "lucid", SCQ_CTL_LUCID, {}, {}, false });
    for (int i = 0; i < 4; ++i) host.control (SCQ_CTL_LEAN1 + i, 0.5f);
    startTimerHz (30);
}

void Hands::set (int ctl, float v) { host.control (ctl, v); }

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

    // stems
    r.removeFromLeft (20);
    float sh = jmin (28.0f, r.getHeight() / 4.5f);
    r.removeFromTop (4);
    for (int i = 0; i < 4; ++i) stemRects[(size_t) i] = r.removeFromTop (sh).reduced (0, 3).withWidth (jmin (r.getWidth(), 280.0f));
}

void Hands::paint (Graphics& g)
{
    g.fillAll (Colour (0xffd9d6cc));
    g.setColour (Colour (0x22000000));
    g.drawHorizontalLine (0, 0, (float) getWidth());
    // LEDs
    for (int i = 0; i < 8; ++i)
    {
        float v = host.lamp (i);
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
            bool down = it.mouse || keyHeld[i - 4];
            auto r = it.r;
            g.setColour (Colour (0x40000000)); g.fillRoundedRectangle (r.translated (0, 2), 5);
            g.setColour (down ? Colour (0xffb8b4a8) : Colour (0xfff4f2ec)); g.fillRoundedRectangle (r, 5);
            g.setColour (Colour (0x55000000)); g.drawRoundedRectangle (r, 5, 1);
            g.setColour (art::ink);
            String label = it.name == "lift.up" ? String ("lift  ^") : it.name == "lift.down" ? String ("lift  v") : it.name == "lucid" ? String::fromUTF8 ("lucid  \xe2\x80\xa2\xe2\x80\xa2") : it.name == "wake" ? String ("wake  PLAY") : it.name;
            g.drawText (label, r, Justification::centred);
        }
    }
    g.setFont (art::mono (11));
    for (int i = 0; i < 4; ++i)
    {
        auto r = stemRects[(size_t) i];
        g.setColour (Colour (0xfff4f2ec)); g.fillRoundedRectangle (r, 4);
        g.setColour (Colour (0x44000000)); g.drawRoundedRectangle (r, 4, 1);
        auto name = host.stemName (i);
        g.setColour (name.isEmpty() ? Colour (0xff8a8578) : art::ink);
        g.drawText ("stem" + String (i + 1) + "   " + (name.isEmpty() ? String ("(click to load a sound)") : name), r.reduced (8, 0), Justification::left);
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
    for (int i = 0; i < 4; ++i)
        if (stemRects[(size_t) i].contains (p))
        {
            chooser = std::make_unique<FileChooser> ("a sound for stem" + String (i + 1), File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this, i] (const FileChooser& fc)
            {
                auto f = fc.getResult();
                if (! f.existsAsFile()) return;
                auto err = host.loadStem (i, f);
                if (onStatus) onStatus (err.isEmpty() ? "stem" + String (i + 1) + " is " + f.getFileName() : err);
                repaint();
            });
            return;
        }
    dragging = itemAt (p);
    if (dragging < 0) return;
    auto& it = items[(size_t) dragging];
    if (it.fader) mouseDrag (e);
    else
    {
        it.mouse = true;
        set (it.ctl, it.ctl == SCQ_CTL_LIFT ? it.value : 1.0f);
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
    }
    dragging = -1;
    repaint();
}

void Hands::timerCallback()
{
    // the computer keyboard as the SP-1's buttons, while you aren't writing
    bool free = ! (typing && typing());
    const int keys[8] = { '1', '2', '3', '4', KeyPress::upKey, KeyPress::downKey, KeyPress::spaceKey, '.' };
    for (int k = 0; k < 8; ++k)
    {
        bool down = free && KeyPress::isKeyCurrentlyDown (keys[k]);
        if (down == keyHeld[k]) continue;
        keyHeld[k] = down;
        if (k < 4) set (SCQ_CTL_DOOR1 + k, down ? 1.0f : 0.0f);
        else if (k == 4) set (SCQ_CTL_LIFT, down ? 1.0f : (keyHeld[5] ? -1.0f : 0.0f));
        else if (k == 5) set (SCQ_CTL_LIFT, down ? -1.0f : (keyHeld[4] ? 1.0f : 0.0f));
        else if (k == 6) set (SCQ_CTL_WAKE, down ? 1.0f : 0.0f);
        else set (SCQ_CTL_LUCID, down ? 1.0f : 0.0f);
    }
    repaint();
}
