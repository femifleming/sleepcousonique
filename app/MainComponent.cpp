#include "MainComponent.h"
#include "Art.h"
#include "Serial.h"
#include "ScqData.h"
#include "scq.h"
#include "Scales.h"

using namespace juce;

namespace
{
struct Look : LookAndFeel_V4
{
    Look() { refresh(); }
    void refresh()
    {
        auto& t = art::theme();
        setColour (TextButton::buttonColourId, t.button);
        setColour (TextButton::buttonOnColourId, art::ink);
        setColour (TextButton::textColourOffId, t.buttonText);
        setColour (TextButton::textColourOnId, t.pop ? (t.dark ? t.window : t.chromeText) : t.window);
        setColour (PopupMenu::backgroundColourId, t.pop ? t.card : art::paper);
        setColour (PopupMenu::textColourId, art::ink);
        setColour (PopupMenu::highlightedBackgroundColourId, t.pop ? t.checkB : art::blue);
        setColour (PopupMenu::highlightedTextColourId, t.pop ? art::boardInk : art::paper);
        setColour (PopupMenu::headerTextColourId, art::umber);
        setColour (AlertWindow::backgroundColourId, t.pop ? t.card : art::paper);
        setColour (AlertWindow::textColourId, art::ink);
        setColour (AlertWindow::outlineColourId, t.line);
        setColour (TextEditor::backgroundColourId, t.dark ? t.surface : Colours::white);
        setColour (TextEditor::textColourId, art::ink);
        setColour (TextEditor::outlineColourId, t.line);
        setColour (ResizableWindow::backgroundColourId, t.window);
        setColour (ScrollBar::thumbColourId, t.dark ? Colour (0x40ffffff) : Colour (0x40000000));
        setColour (ListBox::backgroundColourId, t.card);
        setColour (ComboBox::backgroundColourId, t.button);
        setColour (ComboBox::textColourId, t.buttonText);
        setColour (Label::textColourId, art::ink);
    }
    void drawButtonBackground (Graphics& g, Button& b, const Colour& bg, bool over, bool down) override
    {
        auto& t = art::theme();
        auto r = b.getLocalBounds().toFloat().reduced (1.5f);
        if (t.modular)                // Windows: raised grey; on is cyan and sunken
        {
            auto rr = b.getLocalBounds().toFloat().reduced (1.0f);
            const bool on = b.getToggleState() || down;
            g.setColour (on ? t.checkA : (over ? Colour (0xffe0ddd6) : t.button)); g.fillRect (rr);
            g.setColour (Colours::black); g.drawRect (rr, 1.0f);
            art::bevel (g, rr.reduced (1.0f), ! on);
            ignoreUnused (bg);
            return;
        }
        if (t.pop)                    // faltung: a slanted outline; on (or pressed) is filled
        {
            auto p = art::slant (r);
            if (b.getToggleState() || down) { g.setColour (t.checkA); g.fillPath (p); }
            else if (over) { g.setColour (t.rail); g.fillPath (p); }
            g.setColour (b.getToggleState() ? t.checkA : t.chromeText.withAlpha (0.75f));
            g.strokePath (p, PathStrokeType (1.0f));
            ignoreUnused (bg);
            return;
        }
        auto c = b.getToggleState() ? art::ink : bg;
        if (over && ! b.getToggleState()) c = c.darker (0.06f);
        if (down) c = c.darker (0.15f);
        g.setColour (c);
        g.fillRoundedRectangle (r, 4);
        g.setColour (t.dark ? Colour (0x33ffffff) : Colour (0x33000000));
        g.drawRoundedRectangle (r, 4, 1);
    }
    Font getTextButtonFont (TextButton&, int h) override { return art::label (jmin (13.0f, (float) h * 0.5f)); }
    Font getPopupMenuFont() override { return art::theme().pop ? art::label (13.0f) : art::mono (13.5f); }
};
}

MainComponent::MainComponent()
{
    art::setTheme (false, false);            // the journal, light (the original), until the settings say otherwise
    laf = std::make_unique<Look>();
    setLookAndFeel (laf.get());
    PropertiesFile::Options o;
   #if SCQ_LEGACY
    o.applicationName = "Sleepcousonique Legacy";   // its own settings: the modular look by default
   #else
    o.applicationName = "Sleepcousonique";
   #endif
    o.filenameSuffix = "settings";
    o.osxLibrarySubFolder = "Application Support";
    props.setStorageParameters (o);

    addAndMakeVisible (journal);
    addAndMakeVisible (tapestry);
    addAndMakeVisible (hands);
    addChildComponent (wingiePanel);
    addChildComponent (wingieMeters);
    const char* instNames[2] = { "SP-1", "Wingie" };
    const char* instTips[2] = { "patch for the SP-1 (a dream without an instrument line)",
                                "patch for the Meng Qi Wingie2: writes `instrument wingie` at the top of the dream" };
    for (int i = 0; i < 2; ++i)
    {
        instBtn[i].setButtonText (instNames[i]);
        instBtn[i].setTooltip (instTips[i]);
        instBtn[i].onClick = [this, i] { setInstrument (i); };
        addAndMakeVisible (instBtn[i]);
    }
    bar = std::make_unique<StretchableLayoutResizerBar> (&split, 1, true);
    addAndMakeVisible (*bar);
    split.setItemLayout (0, 260, -0.8, -0.38);
    split.setItemLayout (1, 6, 6, 6);
    split.setItemLayout (2, 300, -1.0, -0.62);

    const char* kinds[] = { "->  hall", "~>  fog", "=>  water", ">|  door", "?>  maybe" };
    const char* tips[] = { "a plain wire", "a resistor: half as loud, softened", "a capacitor: slow to fill, slow to drain", "a diode: only the positive half", "maybe: each wave or pulse gets through by chance (50%; ?30> for 30%)" };
    for (int i = 0; i < 5; ++i)
    {
        kindBtn[i].setButtonText (kinds[i]);
        kindBtn[i].setTooltip (tips[i]);
        kindBtn[i].setClickingTogglesState (true);
        kindBtn[i].setRadioGroupId (1);
        kindBtn[i].onClick = [this, i] { tapestry.handKind = i == 4 ? 6 : i; };   // 6 = maybe (4, 5 are lucid/value)
        addAndMakeVisible (kindBtn[i]);
    }
    kindBtn[0].setToggleState (true, dontSendNotification);
    const char* views[] = { "journal", "both", "tapestry" };
    for (int i = 0; i < 3; ++i)
    {
        viewBtn[i].setButtonText (views[i]);
        viewBtn[i].setClickingTogglesState (true);
        viewBtn[i].setRadioGroupId (2);
        viewBtn[i].onClick = [this, i] { setView (i); };
        addAndMakeVisible (viewBtn[i]);
    }
    viewBtn[1].setToggleState (true, dontSendNotification);
    sendBtn.onClick = [this] { sendToSP1(); };
    soundBtn.onClick = [this]
    {
        // the input is chosen here while the Wingie's microphones are listened to (listen, in its panel)
        const int ins = host.listening() ? 2 : 0;
        auto* sel = new AudioDeviceSelectorComponent (host.devices, 0, ins, 2, 2, false, false, true, false);
        sel->setSize (480, 360);
        DialogWindow::LaunchOptions lo;
        lo.content.setOwned (sel);
        lo.dialogTitle = "sound";
        lo.dialogBackgroundColour = art::paper;
        lo.launchAsync();
    };
    addSceneBtn.onClick = [this] { addScene(); };
    addAndMakeVisible (sendBtn);
    addAndMakeVisible (soundBtn);
    addAndMakeVisible (addSceneBtn);

    journal.onTyped = [this] { pendingTyping = true; typedAt = Time::getMillisecondCounter(); doc.dirty = true; updateTitle(); };
    journal.onPickMessage = [this] (int line) { journal.showLine (line); };
    addAndMakeVisible (timeline);
    timeline.onCapture = [this] { return capture(); };
    timeline.onApply = [this] (const Timeline::Values& v, int scene) { applySnapshot (v, scene); };
    timeline.onStatus = [this] (const String& t) { status (t); };
    timeline.onChanged = [this]
    {
        if (pendingTyping) { pendingTyping = false; doc.text = journal.getText(); }   // keep what's being typed
        doc.text = Timeline::without (doc.text) + timeline.toText();
        textEdited (false);
    };
    journal.onUndo = [this] (bool redo) { undoRedo (redo); };
    tapestry.onTextEdited = [this] (bool recompile) { textEdited (recompile); };
    inspector.onEdited = [this] (bool recompile) { textEdited (recompile); };
    inspector.onStatus = [this] (const String& t) { status (t); };
    browser.onHover = [this] (const String& t) { status (t); };
    browser.onAdd = [this] (const String& place)
    {
        auto name = doc.addPlace (place);
        doc.setPosition (doc.scene, name, tapestry.viewCentre() + Point<float> ((float) (Random::getSystemRandom().nextInt (60) - 30), (float) (Random::getSystemRandom().nextInt (60) - 30)));
        doc.writePositions();
        textEdited (true);
        status ("added " + name + "  (cmd-Z takes it back)");
    };
    undoBtn.onClick = [this] { undoRedo (false); };
    redoBtn.onClick = [this] { undoRedo (true); };
    addChildComponent (browser);
    addChildComponent (inspector);
    addChildComponent (undoBtn);
    addChildComponent (redoBtn);
    tapestry.onStatus = [this] (const String& s) { status (s); };
    tapestry.onShowLine = [this] (int line) { if (view == 2) setView (1); journal.showLine (line); };
    hands.typing = [this] { return journal.editorHasFocus(); };
    hands.link = &link;
    hands.onStatus = [this] (const String& s) { status (s); };
    hands.mappingFor = [this] (const String& control)
    {
        // every cord leaving this control in the playing scene, as the journal names its end
        StringArray to;
        auto name = control.upToFirstOccurrenceOf (".", false, false);
        auto port = control.fromFirstOccurrenceOf (".", false, false);
        auto* n = doc.nodeNamed (name);
        auto* p = n ? scq::findPlace (n->place) : nullptr;
        if (! p) return to;
        int sport = port.isEmpty() ? 0 : p->outs.indexOf (port);
        for (auto& c : doc.cords)
            if (c.src == n->id && c.sport == sport)
                if (auto* dn = doc.node (c.dst))
                    if (auto* dp = scq::findPlace (dn->place))
                        to.addIfNotAlreadyThere (dn->name + (c.dport > 0 || (dp->family >= scq::Hands && dp->ins.size() > 1) ? "." + dp->ins[c.dport] : String())
                                                 + (c.kind == scq::Range ? " " + scq::fmt (c.amt) + ".." + scq::fmt (c.amt2) : String()));
        return to;
    };
    hands.onShowControl = [this] (const String& control)
    {
        auto name = control.upToFirstOccurrenceOf (".", false, false);
        if (auto* n = doc.nodeNamed (name))
            for (auto& c : doc.cords)
                if (c.src == n->id) { if (view == 2) setView (1); journal.showLine (c.line); return; }
        status (control + " isn't patched in this scene: drag its loop onto an inlet, or write  " + control + " -> somewhere");
    };
    // the Wingie's panel: the same map, its own hands
    wingiePanel.typing = hands.typing;
    wingiePanel.link = &link;
    wingiePanel.onStatus = hands.onStatus;
    wingiePanel.mappingFor = hands.mappingFor;
    wingiePanel.onShowControl = hands.onShowControl;
    wingiePanel.onListen = [this] (bool on)
    {
        auto err = host.setListening (on);
        if (err.isNotEmpty()) status ("can't listen: " + err);
        else status (on ? "listening to " + host.inputName() + " as the Wingie's microphones (wear headphones, or it feeds back; the sound settings choose the input)"
                        : "not listening: the Wingie's microphones are silent in the simulation");
        wingiePanel.repaint();
    };
    wingieMeters.link = &link;
    wingieMeters.estimate = [this] { return doc.load; };
    wingieMeters.poolWord = [this] { return String (roundToInt ((float) doc.poolUsed / 1024.0f)) + " of " + String (roundToInt ((float) doc.poolTotal / 1024.0f)) + " KB"; };

    setSize (1440, 900);
    // start with the first night
    doc.text = String::fromUTF8 (ScqData::firstnight_dream, ScqData::firstnight_dreamSize);
    journal.setText (doc.text);
    compile (true);
    doc.dirty = false;
    resetHistory();
    applyLook();
    updateTitle();
   #if SCQ_LEGACY
    startTimerHz (10);
   #else
    startTimerHz (20);
   #endif
    MessageManager::callAsync ([this] { tapestry.fitAll(); });
}

MainComponent::~MainComponent()
{
    Desktop::getInstance().removeDarkModeSettingListener (this);
    setLookAndFeel (nullptr);
}

String MainComponent::codeOf (const String& text) const
{
    StringArray ls;
    ls.addLines (text);
    StringArray out;
    for (auto& l : ls) if (! l.startsWith (";@")) out.add (l);
    return out.joinIntoString ("\n").trimEnd();
}

void MainComponent::compile (bool force)
{
    auto code = codeOf (doc.text);
    doc.readPositions();
    syncInstrument();                // (an instrument line typed or taken back)
    if (! force && code == lastCode) { tapestry.refresh(); return; }
    auto chk = host.check (doc.text, nights);
    if (chk.errors > 0)
    {
        // keep the last good dream playing; show what's wrong
        scq::Document tmp;
        tmp.readDescription ("", chk.messages.toRawUTF8());
        journal.setMessages (tmp.messages, String (chk.errors) + (chk.errors == 1 ? " mistake" : " mistakes")
                                            + String::fromUTF8 (" \xe2\x80\x94 still playing the last dream that made sense"));
        lastCode = code;
        return;
    }
    afterLoad (host.load (doc.text, nights));
    lastCode = code;
}

void MainComponent::afterLoad (const Host::Result& r)
{
    struct Later { Inspector& i; ~Later() { i.refresh(); } } later { inspector };
    timeline.fromText (doc.text);
    lastMessages = r.messages;
    doc.readDescription (r.describe.toRawUTF8(), r.messages.toRawUTF8());
    journal.setMessages (doc.messages, {});
    tapestry.refresh();
    refreshScenes();
    repaint();
}

void MainComponent::refreshScenes()
{
    sceneBtns.clear();
    for (int i = 0; i < (int) doc.scenes.size(); ++i)
    {
        auto* b = sceneBtns.add (new TextButton (doc.scenes[(size_t) i].title));
        b->setToggleState (i == doc.scene, dontSendNotification);
        b->onClick = [this, i]
        {
            host.gotoScene (i);
            auto spans = doc.sceneSpans();
            if (i < (int) spans.size() && spans[(size_t) i].header >= 0) journal.showLine (spans[(size_t) i].header + 1);
        };
        addAndMakeVisible (b);
    }
    resized();
}

void MainComponent::timerCallback()
{
    if (pendingTyping && Time::getMillisecondCounter() - typedAt > 450)
    {
        pendingTyping = false;
        doc.text = journal.getText();
        remember();                  // a burst of typing is one step
        compile();
    }
    {   // the device says a dream was too heavy and what it went back to
        juce::uint32 c = 0;
        auto e = link.lastEvent (c);
        if (c != eventShown) { eventShown = c; status ((link.device() == serial::wingie ? "the Wingie: " : "the SP-1: ") + e); }
    }
    {   // the slot place: the computer has no slots, so say what the instrument would do
        int to = -1, step = 0;
        if (scq_wanted_slot (&to, &step))
            status (to >= 0 ? "the dream asks for slot " + String (to + 1) + " (on the instrument it plays that slot)"
                            : String ("the dream asks for the ") + (step > 0 ? "next" : "previous") + " slot (on the instrument it goes there)");
    }
    if (auto note = host.roomNote(); note != roomShown)   // the crowd: someone came or went
    {
        roomShown = note;
        repaint (statusArea);
        repaint (pillArea);
    }
    if (host.scene() != doc.scene)   // a journey happened (or a tab was pressed)
    {
        auto r = host.describe();
        doc.readDescription (r.describe.toRawUTF8(), lastMessages.toRawUTF8());
        tapestry.refresh();
        tapestry.fitAll();           // a new scene: show all of it
        refreshScenes();
    }
    repaint (meters);
    repaint (linkArea);
    if (art::theme().pop) { repaint (marginArea); repaint (pillArea); }
    if (art::theme().pop) inspector.show (tapestry.selectedNode());
    {   // lines selected in the journal light up in the tapestry
        auto sel = journal.selectedLines();
        if (hlShot) {}
        else if (sel.isEmpty()) tapestry.setHighlight (0, -1);
        else tapestry.setHighlight (sel.getStart(), sel.getEnd() - 1);
    }
}

void MainComponent::setView (int v)
{
    view = v;
    for (int i = 0; i < 3; ++i) viewBtn[i].setToggleState (i == v, dontSendNotification);
    resized();
}

void MainComponent::resized()
{
    bool pop = art::theme().pop;
    browser.setVisible (pop); inspector.setVisible (pop); undoBtn.setVisible (pop); redoBtn.setVisible (pop);
    {
        const char* full[] = { "->  hall", "~>  fog", "=>  water", ">|  door", "?>  maybe" };
        const char* arrow[] = { "->", "~>", "=>", ">|", "?>" };
        const char* name[] = { "hall: a plain wire", "fog: a resistor, half as loud, softened", "water: a capacitor, slow to fill, slow to drain", "door: a diode, only the positive half", "maybe: each wave or pulse gets through by chance" };
        for (int i = 0; i < 5; ++i)
        {
            auto& k = kindBtn[i];
            k.getProperties().set ("round", pop);
            k.setButtonText (pop ? arrow[i] : full[i]);
            k.setTooltip (name[i]);
            if (pop) { k.setColour (TextButton::textColourOffId, art::theme().chromeText); k.setColour (TextButton::textColourOnId, art::theme().dark ? art::theme().window : art::theme().chromeText); }
            else { k.removeColour (TextButton::textColourOffId); k.removeColour (TextButton::textColourOnId); }
        }
    }
    instBtn[0].setButtonText (pop ? "sp-1" : "SP-1");
    instBtn[1].setButtonText (pop ? "wingie" : "Wingie");
    sendBtn.setButtonText ("send to " + deviceText (shownInstrument == 1 ? 1 : 0));
    if (pop) { resizedPop(); return; }
    pillArea = marginArea = stripArea = contentArea = helpArea = {};
    auto r = getLocalBounds();
    auto top = r.removeFromTop (46).reduced (10, 8);
    const bool narrow = top.getWidth() < 1240;          // room for the instrument switch: the passages as arrows only
    sendBtn.setBounds (top.removeFromLeft (128));
    top.removeFromLeft (10);
    instBtn[0].setBounds (top.removeFromLeft (58));      // SP-1 | Wingie
    instBtn[1].setBounds (top.removeFromLeft (66));
    top.removeFromLeft (12);
    const char* arrowOnly[] = { "->", "~>", "=>", ">|", "?>" };
    for (int i = 0; i < 5; ++i)
    {
        if (narrow) kindBtn[i].setButtonText (arrowOnly[i]);
        kindBtn[i].setBounds (top.removeFromLeft (narrow ? 46 : 92));
    }
    top.removeFromLeft (16);
    for (auto& v : viewBtn) v.setBounds (top.removeFromLeft (84));
    soundBtn.setBounds (top.removeFromRight (70));
    top.removeFromRight (10);
    meters = top.removeFromRight (jmin (300, top.getWidth())).withTrimmedTop (-8).withTrimmedBottom (-8);

    sceneArea = r.removeFromTop (34);
    auto sa = sceneArea.reduced (10, 4);
    for (auto* b : sceneBtns) b->setBounds (sa.removeFromLeft (jmin (170, 30 + 8 * b->getButtonText().length())).withTrimmedRight (6));
    addSceneBtn.setBounds (sa.removeFromLeft (84));
    linkArea = sceneArea.reduced (10, 0).removeFromRight (360);

    statusArea = r.removeFromBottom (24);
    {
        auto handsRow = r.removeFromBottom (shownInstrument == SCQ_WINGIE ? 300 : 176);   // the Wingie's face is nearly square
        hands.setBounds (handsRow);
        wingieMeters.setBounds (handsRow.removeFromRight (jmin (470, handsRow.getWidth() * 2 / 5)));
        wingiePanel.setBounds (handsRow);
    }
    timeline.setVisible (showTimeline);
    if (showTimeline) timeline.setBounds (r.removeFromBottom (70));

    journal.setVisible (view != 2);
    tapestry.setVisible (view != 0);
    bar->setVisible (view == 1);
    if (view == 1)
    {
        Component* comps[] = { &journal, bar.get(), &tapestry };
        split.layOutComponents (comps, 3, r.getX(), r.getY(), r.getWidth(), r.getHeight(), false, true);
    }
    else if (view == 0) journal.setBounds (r);
    else tapestry.setBounds (r);
}

// The openprocess look (after the poppsphere bundle): the browser grids along the top, the
// status pill and round passage buttons under them, a margin with the brace, the bracket
// meters and the wordmark, the settings column on the right, the hands as rails and rings,
// and the crescent and bars along the bottom.
void MainComponent::resizedPop()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (120).reduced (0, 8);
    auto btns = top.removeFromRight (262).reduced (8, 0);
    {   // send / journal / tapestry, both / sound, and the scene add
        auto row1 = btns.removeFromTop (34); btns.removeFromTop (6);
        auto row2 = btns.removeFromTop (34); btns.removeFromTop (6);
        sendBtn.setBounds (row1.removeFromLeft (70)); row1.removeFromLeft (6);
        viewBtn[0].setBounds (row1.removeFromLeft (row1.getWidth() / 2 - 3)); row1.removeFromLeft (6);
        viewBtn[2].setBounds (row1);
        viewBtn[1].setBounds (row2.removeFromLeft (70)); row2.removeFromLeft (6);
        soundBtn.setBounds (row2);
        auto row3 = btns.removeFromTop (30);           // sp-1 | wingie, and the scene add
        instBtn[0].setBounds (row3.removeFromLeft (58)); row3.removeFromLeft (4);
        instBtn[1].setBounds (row3.removeFromLeft (70)); row3.removeFromLeft (6);
        addSceneBtn.setBounds (row3);
    }
    top.removeFromLeft (104);
    browser.setBounds (top.reduced (0, 2).withTrimmedBottom (12));

    auto row = r.removeFromTop (44).reduced (0, 4);
    row.removeFromLeft (104);
    pillArea = row.removeFromLeft (jmin (460, row.getWidth() / 3));
    row.removeFromLeft (12);
    for (auto& k : kindBtn) { k.setBounds (row.removeFromLeft (36)); row.removeFromLeft (4); }
    row.removeFromLeft (12);
    undoBtn.setBounds (row.removeFromLeft (62)); row.removeFromLeft (4);
    redoBtn.setBounds (row.removeFromLeft (62)); row.removeFromLeft (14);
    linkArea = row.removeFromLeft (jmin (330, row.getWidth() - 270));
    sceneArea = row.withTrimmedLeft (12);
    auto sa = sceneArea;
    for (auto* b : sceneBtns) { b->setBounds (sa.removeFromLeft (jmin (150, 30 + 8 * b->getButtonText().length()))); sa.removeFromLeft (4); }

    stripArea = r.removeFromBottom (60);
    meters = stripArea;
    statusArea = {};
    auto handsArea = r.removeFromBottom (shownInstrument == SCQ_WINGIE ? 290 : 150);
    auto tlArea = showTimeline ? r.removeFromBottom (70) : Rectangle<int>();
    timeline.setVisible (showTimeline);
    if (showTimeline) timeline.setBounds (tlArea.withTrimmedLeft (104));
    marginArea = r.withTrimmedBottom (-(handsArea.getHeight() + tlArea.getHeight())).removeFromLeft (104);
    helpArea = marginArea.withTop (marginArea.getBottom() - 70).withHeight (24).withTrimmedLeft (12).withWidth (80);
    handsArea.removeFromLeft (104);
    hands.setBounds (handsArea);
    wingieMeters.setBounds (handsArea.removeFromRight (jmin (470, handsArea.getWidth() * 2 / 5)));
    wingiePanel.setBounds (handsArea);
    r.removeFromLeft (104);
    inspector.setBounds (r.removeFromRight (250).withTrimmedTop (10).reduced (0, 4));
    r.removeFromRight (16);                           // the right rail
    contentArea = r;
    auto c = r.withTrimmedTop (12);                   // under the top rail
    journal.setVisible (view != 2);
    tapestry.setVisible (view != 0);
    bar->setVisible (view == 1);
    if (view == 1)
    {
        Component* comps[] = { &journal, bar.get(), &tapestry };
        split.layOutComponents (comps, 3, c.getX(), c.getY(), c.getWidth(), c.getHeight(), false, true);
    }
    else if (view == 0) journal.setBounds (c);
    else tapestry.setBounds (c);
}

void MainComponent::paintPop (Graphics& g)
{
    auto& th = art::theme();
    g.fillAll (th.window);
    const auto ink = th.chromeText;

    // rails round the work area
    g.setColour (th.rail);
    g.fillRect (contentArea.withHeight (10).withTrimmedLeft (90));
    g.fillRect (Rectangle<int> (contentArea.getRight(), contentArea.getY(), 12, contentArea.getHeight() + 10));
    g.fillRect (Rectangle<int> (contentArea.getX() - 12, contentArea.getY() + 60, 10, 50));

    // the status line: a slanted strip, as the faltung tracklists
    {
        auto p = pillArea.toFloat().reduced (0, 3);
        if (th.modular) { g.setColour (Colours::white); g.fillRect (p); art::bevel (g, p, false); }
        else
        {
            g.setColour (th.chromeText.withAlpha (0.6f)); g.strokePath (art::slant (p, p.getHeight() * 0.5f), PathStrokeType (1.0f));
            g.setColour (th.checkA); g.fillPath (art::slant (p.withWidth (18), p.getHeight() * 0.5f));
        }
        g.setColour (ink);
        g.setFont (art::label (11));
        auto t = statusText.isNotEmpty() ? statusText : (doc.file.existsAsFile() ? doc.file.getFileName() + " is loaded..." : String ("the first night is loaded..."));
        if (roomShown.isNotEmpty()) t = roomShown + String::fromUTF8 ("   \xc2\xb7   ") + t;                 // who is in the room, when a crowd listens
        g.drawText (t, pillArea.reduced (16, 0).withTrimmedLeft (14), Justification::centredLeft, true);
    }

    // the SP-1 bar, gold (or the Wingie's)
    {
        auto ln = linkNow();
        const bool open = ln.open, live = ln.live;
        struct { int slot, scene; } h { ln.slot, ln.scene };
        const int dreamFor = shownInstrument == 1 ? 1 : 0;
        const bool other = open && ln.device != dreamFor;       // connected, but the other instrument
        auto b = linkArea.toFloat().reduced (0, 2);
        auto shape = art::slant (b, th.modular ? 0.0f : b.getHeight() * 0.45f);
        if (th.modular) { g.setColour (th.button); g.fillRect (b); art::bevel (g, b, true); }
        if (! th.modular)
        {
            if (live) { g.setColour (th.checkA); g.fillPath (shape); }
            g.setColour (live ? th.checkA : ink.withAlpha (0.75f)); g.strokePath (shape, PathStrokeType (1.0f));
        }
        auto sq = Rectangle<float> (12, 12).withCentre ({ b.getX() + 24, b.getCentreY() });
        if (th.modular) { g.setColour (Colours::black); g.fillRect (sq.reduced (2)); g.setColour (live ? Colour (0xff30ff30) : Colour (0xff0a4a0a)); g.fillRect (sq.reduced (3)); }
        else { g.setColour (live ? th.window : (open ? th.checkA : th.chromeDim)); g.fillPath (art::slant (sq, 4)); }
        g.setColour (live && th.dark ? th.window : ink);
        g.setFont (art::label (12.5f));
        String a = deviceText (open ? ln.device : dreamFor).toLowerCase() + " ", c = ! open ? "not connected" : (live ? "connected" : "waiting...");
        if (other) c << "  (dream: " << deviceText (dreamFor).toLowerCase() << ")";
        g.drawText (a + "/ " + c, b.withTrimmedLeft (36), Justification::centredLeft, true);
        if (live && ! other)
        {
            g.setFont (art::label (10));
            g.drawText ("slot " + String (h.slot) + " · scene " + String (h.scene), b.reduced (10, 0), Justification::centredRight);
        }
    }

    // the margin: the pixel brace, the bracket meters, the wordmark, help
    {
        auto m = marginArea.toFloat();
        float x = m.getX() + 30, y = m.getY() + 30;
        // the yellow cube, as on the faltung sleeves (the modular look: the app's "e" on a raised tile)
        if (th.modular)
        {
            auto tile = Rectangle<float> (x - 4, y, 40, 40);
            g.setColour (Colour (0xff1f2a5a)); g.fillRect (tile); art::bevel (g, tile, true);
            g.setColour (Colour (0xfff4efe4)); g.setFont (art::tahoma (24.0f, true)); g.drawText ("e", tile, Justification::centred);
        }
        else art::drawCube (g, { x + 18, y + 22 }, 20);
        // the left / right meters: two tall slanted columns
        float my = y + 70, mh = jmax (60.0f, m.getHeight() - 300);
        for (int ch = 0; ch < 2; ++ch)
        {
            const float bx = x + (float) ch * 26, bw = 15, k = 6;
            if (th.modular)            // an LED column: green, yellow, red
            {
                auto col = Rectangle<float> (bx, my, bw, mh);
                g.setColour (Colours::black); g.fillRect (col);
                const int n = (int) (mh / 5); const float v = jlimit (0.0f, 1.0f, host.level);
                for (int q = 0; q < n; ++q)
                {
                    const float fr = (float) q / (float) n;
                    g.setColour (fr < v ? (fr > 0.85f ? Colour (0xffff3030) : (fr > 0.7f ? Colour (0xffffe030) : Colour (0xff30e030))) : Colour (0xff0a3a0a));
                    g.fillRect (bx + 2, my + mh - 5.0f * (float) (q + 1) + 1, bw - 4, 3.0f);
                }
                g.setColour (ink); g.setFont (art::label (9));
                g.drawText (ch ? "r" : "l", Rectangle<float> (bx, my + mh + 4, bw, 12), Justification::centred);
                continue;
            }
            Path track; track.startNewSubPath (bx + k, my); track.lineTo (bx + k + bw, my); track.lineTo (bx + bw, my + mh); track.lineTo (bx, my + mh); track.closeSubPath();
            g.setColour (th.rail); g.strokePath (track, PathStrokeType (1.0f));
            const float v = jlimit (0.0f, 1.0f, host.level), fh = v * mh, kk = k * v;
            Path fillp; fillp.startNewSubPath (bx + kk, my + mh - fh); fillp.lineTo (bx + kk + bw, my + mh - fh); fillp.lineTo (bx + bw, my + mh); fillp.lineTo (bx, my + mh); fillp.closeSubPath();
            g.setColour (v > 0.95f ? th.sound : th.checkA); g.fillPath (fillp);
            g.setColour (ink); g.setFont (art::label (9));
            g.drawText (ch ? "r" : "l", Rectangle<float> (bx, my + mh + 4, bw + k, 12), Justification::centred);
        }
        // the wordmark, in two lines
        g.setFont (art::sans (17.0f, true));
        auto wm = Rectangle<float> (m.getX() + 8, (float) helpArea.getY() - 52, m.getWidth() - 8, 20);
        g.setColour (ink); g.drawText ("sleepcou-", wm, Justification::left);
        g.drawText ("sonique", wm.translated (0, 20), Justification::left);
        // ? ? ?
        for (int k = 0; k < 3; ++k)
        {
            auto q = Rectangle<float> ((float) helpArea.getX() + (float) k * 26, (float) helpArea.getY(), 22, 18);
            auto qp = art::slant (q, th.modular ? 0.0f : 6.0f);
            if (k == 0) { g.setColour (th.checkA); g.fillPath (qp); }
            g.setColour (k == 0 ? th.checkA : ink.withAlpha (0.6f)); g.strokePath (qp, PathStrokeType (1.0f));
            g.setColour (k == 0 && th.dark ? th.window : ink);
            g.setFont (art::label (11));
            g.drawText ("?", q, Justification::centred);
        }
        g.setColour (ink); g.setFont (art::label (11));
        g.drawText ("help", helpArea.translated (0, 24), Justification::left);
    }

    // along the bottom, plain to read: LOAD (the editor's estimate for the SP-1), SP-1 CPU (what
    // the SP-1 measures, when it's plugged in), MEMORY; each a big number, a bar with its 100 %
    // mark, and ok / tight / too heavy in green, orange, red
    {
        auto s = stripArea.toFloat().reduced (0, 5);
        // the crescent, as the mark of the look
        if (! th.modular)
        {   // a wireframe polyhedron with one white face, as the mark of the look
            const float cx = s.getX() + 34, cy = s.getCentreY(), r = s.getHeight() * 0.42f;
            Point<float> a (cx - r, cy + r * 0.3f), b (cx - r * 0.1f, cy - r), c2 (cx + r, cy - r * 0.2f), d (cx + r * 0.2f, cy + r * 0.8f);
            Path face; face.startNewSubPath (a); face.lineTo (c2); face.lineTo (d); face.closeSubPath();
            g.setColour (th.dark ? Colours::white.withAlpha (0.85f) : Colours::white); g.fillPath (face);
            g.setColour (ink);
            for (auto e : { std::pair (a, b), std::pair (b, c2), std::pair (c2, d), std::pair (d, a), std::pair (a, c2), std::pair (b, d) })
                g.drawLine ({ e.first, e.second }, 0.9f);
        }

        const Colour okC = th.dark ? Colour (0xff5fcf7a) : Colour (0xff1f8a3c), tightC (0xfff7931e), overC (0xffff4a3a);
        auto readout = [&] (Rectangle<float> r, const String& name, float v, bool known, const String& extra)
        {
            auto col = ! known ? th.chromeDim : (v > 1.0f ? overC : (v > 0.85f ? tightC : okC));
            String word = ! known ? String() : (v > 1.0f ? (name == "ROOM" ? "full" : "too heavy") : (v > 0.85f ? "tight" : "ok"));
            g.setColour (th.chromeDim);
            g.setFont (art::label (10));
            g.drawText (name, r.withHeight (12), Justification::left);
            g.setColour (col);
            g.setFont (art::label (22));
            auto num = r.withTrimmedTop (12).withHeight (24);
            g.drawText (known ? String (roundToInt (v * 100)) + "%" : String ("--"), num, Justification::left);
            g.setFont (art::label (11));
            g.drawText (known ? word + (extra.isNotEmpty() ? "  " + extra : String()) : extra, num.withTrimmedLeft (70), Justification::centredLeft, true);
            auto bar = Rectangle<float> (r.getX(), r.getBottom() - 6, r.getWidth() - 10, 6);
            g.setColour (th.rail); g.fillRect (bar);
            const float full = bar.getWidth() / 1.3f;               // the bar runs to 130 %; the white tick is 100 %
            g.setColour (col); g.fillRect (bar.withWidth (jlimit (0.0f, bar.getWidth(), full * (known ? v : 0.0f))));
            g.setColour (ink); g.fillRect (Rectangle<float> (bar.getX() + full - 1, bar.getY() - 3, 2, bar.getHeight() + 6));
        };
        const int dreamFor = shownInstrument == 1 ? 1 : 0;                 // the meters are for the instrument the dream is for
        auto meas = measuredFor (dreamFor);
        bool fresh = meas.avg >= 0 && Time::getMillisecondCounter() - meas.when < 12000;
        auto area = s.withTrimmedLeft (78).withTrimmedRight (170);
        float w = area.getWidth() / 3;
        readout (area.removeFromLeft (w), "LOAD (estimate)", doc.load, true, {});
        readout (area.removeFromLeft (w), deviceText (dreamFor).toUpperCase() + " CPU (measured)", fresh ? (float) meas.peak / 100.0f : 0.0f, fresh,
                 fresh ? "avg " + String (meas.avg) + "%" + (meas.dropped > 0 ? "  dropped " + String (meas.dropped) : String()) : "plug in the " + deviceText (dreamFor));
        {
            // room on the SP-1: whichever runs out first (the memory pool, 96 places, 320 cords, or
            // the 16 KB of text a slot keeps), named beside the number
            String what;
            const float use = doc.room (&what);
            readout (area, "ROOM", use, true, what);
        }
        // the output level and the file, small, at the end
        auto tail = s.withLeft (s.getRight() - 160);
        g.setColour (th.chromeDim); g.setFont (art::label (10));
        g.drawText ("out", tail.withHeight (12), Justification::left);
        auto ob = Rectangle<float> (tail.getX(), tail.getY() + 16, 150, 6);
        g.setColour (th.rail); g.fillRect (ob);
        g.setColour (th.checkA); g.fillRect (ob.withWidth (150 * jlimit (0.0f, 1.0f, host.level)));
        g.setColour (th.chromeDim);
        g.drawText (doc.file.existsAsFile() ? doc.file.getFileName() : String ("first night"), tail.withTrimmedTop (26), Justification::left, true);
        g.setColour (th.dark ? Colour (0xff44465a) : Colour (0xffc4c8d8));
        for (int k = 0; k < 3; ++k) g.drawLine ((float) getWidth() - 30 + (float) k * 8, (float) getHeight(), (float) getWidth(), (float) getHeight() - 30 + (float) k * 8, 1.0f);
    }
}

void MainComponent::mouseDown (const MouseEvent& e)
{
    if (! art::theme().pop || ! helpArea.expanded (0, 26).contains (e.getPosition())) return;
    int k = (e.x - helpArea.getX()) / 24;
    if (k == 0) menuItemSelected (22, 0);             // how to write a dream
    else if (k == 1) menuItemSelected (21, 0);        // the places
    else URL ("https://github.com/femifleming/sleepcousonique/blob/main/FLASHING.md").launchInDefaultBrowser();
}

void MainComponent::paint (Graphics& g)
{
    auto& th = art::theme();
    if (th.pop) { paintPop (g); return; }
    g.fillAll (th.window);
    g.setColour (th.bar);
    g.fillRect (sceneArea);
    g.setColour (th.line);
    g.drawHorizontalLine (sceneArea.getY(), 0, (float) getWidth());
    g.drawHorizontalLine (sceneArea.getBottom() - 1, 0, (float) getWidth());

    // is the SP-1 there: plugged in and talking (its hands arrive ~30 times a second)
    {
        auto ln = linkNow();
        const bool open = ln.open, live = ln.live;
        auto now = Time::getMillisecondCounter();
        const int dreamFor = shownInstrument == 1 ? 1 : 0;
        const String dev = deviceText (open ? ln.device : dreamFor);
        auto why = open ? String() : link.note();
        String text = ! open ? (! serial::Link::enabled ? dev + " link off (--no-link)"
                                : why.contains ("standby") ? String::fromUTF8 ("SP-1 is off (standby): hold \xe2\x80\xa2\xe2\x80\xa2 to turn it on")
                                : why.contains ("if it's a Wingie") ? String ("a USB serial device without Sleepcousonique on it")
                                : why.contains ("not as Sleepcousonique") ? String ("an SP-1 without Sleepcousonique on it")
                                : why.contains ("in use") ? dev + " port in use by another program"
                                : dev + " not connected")
                    : live ? dev + " connected  " + link.port().fromLastOccurrenceOf ("/", false, false)
                             + (ln.device != dreamFor ? "  (this dream is for the " + deviceText (dreamFor) + ")"
                                                      : "  slot " + String (ln.slot) + "  scene " + String (ln.scene))
                    : dev + " found, waiting for it to answer...";
        auto r = linkArea.toFloat();
        auto dot = Rectangle<float> (12, 12).withCentre ({ r.getRight() - 8, r.getCentreY() });
        auto green = Colour (0xff3f8f4a);
        if (live)
        {
            float pulse = 0.55f + 0.45f * std::sin ((float) now * 0.006f);   // breathing while the link is live
            g.setColour (green.withAlpha (0.25f * pulse));
            g.fillEllipse (dot.expanded (4));
            g.setColour (green);
            g.fillEllipse (dot);
        }
        else
        {
            g.setColour (open ? art::gold : th.chromeDim);
            g.drawEllipse (dot, 1.8f);
        }
        g.setColour (live ? th.chromeText : th.chromeDim);
        g.setFont (art::label (11.5f));
        g.drawText (text, r.withTrimmedRight (22), Justification::centredRight);
    }

    // meters: the SP-1's patience (load), its memory, and your ears
    auto m = meters.reduced (0, 8);
    auto bar1 = [&] (Rectangle<int> r, const String& label, float v, Colour c)
    {
        g.setColour (th.chromeText);
        g.setFont (art::label (11));
        g.drawText (label, r.removeFromTop (14), Justification::left);
        auto b = r.removeFromTop (8).toFloat();
        auto col = v > 1 ? Colour (0xffd22a26) : (v > 0.85f && label.startsWith ("load") ? Colour (0xffe8861a) : c);
        if (th.pop)
        {
            g.setColour (th.rail); g.fillRect (b);
            g.setColour (col); g.fillRect (b.withWidth (b.getWidth() * jlimit (0.0f, 1.0f, v)));
            return;
        }
        g.setColour (th.dark ? Colour (0x22ffffff) : Colour (0x22000000)); g.fillRoundedRectangle (b, 3);
        g.setColour (col);
        g.fillRoundedRectangle (b.withWidth (b.getWidth() * jlimit (0.0f, 1.0f, v)), 3);
    };
    int w = m.getWidth() / 3;
    {
        // the estimate, and when the SP-1 (or the Wingie) is plugged in, what it measures (avg, and its peaks)
        const int dreamFor = shownInstrument == 1 ? 1 : 0;
        auto meas = measuredFor (dreamFor);
        bool fresh = meas.avg >= 0 && Time::getMillisecondCounter() - meas.when < 12000;
        String label = "load " + String (roundToInt (doc.load * 100)) + "%";
        float v = doc.load;
        if (fresh)
        {
            label << "   " << deviceText (dreamFor) << " " << meas.avg << "%, peaks " << meas.peak << "%" << (meas.over ? "  TOO HEAVY" : "");
            v = jmax (v, (float) meas.peak / 100.0f);
        }
        bar1 (m.removeFromLeft (w - 8), label, v, th.pop ? th.checkA : art::blue);
    }
    m.removeFromLeft (8);
    {
        String what;                              // whichever runs out first: memory, places, cords or text
        const float use = doc.room (&what);
        bar1 (m.removeFromLeft (w - 8), what.upToFirstOccurrenceOf (" ", false, false) + " " + String (roundToInt (100.0f * use)) + "%",
              use, th.pop ? th.current : art::copper);
    }
    m.removeFromLeft (8);
    bar1 (m, "out", host.level, th.pop ? th.checkB : art::gold);

    g.setColour (th.bar);
    g.fillRect (statusArea);
    g.setColour (th.chromeText.withAlpha (0.85f));
    g.setFont (th.pop ? art::label (11.5f) : art::mono (12));
    g.drawText (statusText, statusArea.reduced (12, 0), Justification::left);
    g.setColour (th.chromeDim);
    {
        auto where = doc.file.existsAsFile() ? doc.file.getFullPathName() : String ("first night (an example - save to keep it)");
        if (roomShown.isNotEmpty()) where = roomShown + String::fromUTF8 ("   \xc2\xb7   ") + where;   // who is in the room, when a crowd listens
        g.drawText (where, statusArea.reduced (12, 0), Justification::right);
    }
}

// What the tapestry does when an attenuverter is dragged and let go: the engine hears it at
// once, the dream's text catches up (test modes; the tapestry's own code is in mouseDrag / mouseUp).
bool MainComponent::turnAtt (const String& place, const String& inlet, float amount)
{
    auto* n = doc.nodeNamed (place);
    auto* p = n ? scq::findPlace (n->place) : nullptr;
    int k = p ? p->ins.indexOf (inlet) : -1;
    if (k < 0) return false;
    host.setAtt (n->id, k, amount);
    doc.setOption (n->id, "att." + inlet, String (amount));
    textEdited (false);
    return true;
}

void MainComponent::status (const String& s)
{
    statusText = s;
    repaint (statusArea);
    repaint (pillArea);
}

void MainComponent::updateTitle()
{
    if (onTitle) onTitle ((doc.file.existsAsFile() ? doc.file.getFileNameWithoutExtension() : String ("first night")) + (doc.dirty ? String::fromUTF8 ("  \xe2\x80\xa2") : String())
                          + (doc.instrument() == SCQ_WINGIE ? String::fromUTF8 ("  \xe2\x80\x94  for the Wingie") : String())
                          + String::fromUTF8 ("  \xe2\x80\x94  Sleepcousonique"));
}

// ------------------------------------------------------------------------------------------ files

bool MainComponent::askToKeepChanges (std::function<void()> then)
{
    if (! doc.dirty) { then(); return true; }
    AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "this dream isn't saved",
                                  "Leave it without saving?", "leave it", "stay", this,
                                  ModalCallbackFunction::create ([then] (int r) { if (r == 1) then(); }));
    return false;
}

// ------------------------------------------------------------------------------------------ the instrument

void MainComponent::setInstrument (int i)
{
    if (pendingTyping) { pendingTyping = false; doc.text = journal.getText(); remember(); }
    else doc.text = journal.getText();
    if (doc.instrument() == i) { syncInstrument(); return; }
    doc.setInstrument (i);
    textEdited (true);
    // the other instrument's hands are mistakes now: say so, rather than leave them unexplained
    const int mistakes = host.check (doc.text, nights).errors;
    status ("this dream is now for the " + deviceText (i)
            + (i == SCQ_WINGIE ? String (": `instrument wingie` is its first line") : String (": its instrument line is gone"))
            + (mistakes > 0 ? "; " + String (mistakes) + (mistakes == 1 ? " line uses" : " lines use") + " what the " + deviceText (i)
                              + " hasn't (the " + deviceText (1 - i) + "'s hands): see the journal" : String())
            + "   (cmd-Z takes it back)");
}

void MainComponent::syncInstrument()
{
    const int i = doc.instrument();
    if (i == shownInstrument) return;
    shownInstrument = i;
    const bool w = i == SCQ_WINGIE;
    for (int k = 0; k < 2; ++k) instBtn[k].setToggleState (k == i, dontSendNotification);
    hands.setVisible (! w);
    wingiePanel.setVisible (w);
    wingieMeters.setVisible (w);
    browser.setInstrument (i);
    resized();                            // the Wingie's panel is taller (its face is nearly square)
    if (w) wingiePanel.reset();           // the faders are shared (fader1-3): each panel puts back its own
    else
    {
        hands.resend();
        if (host.listening()) host.setListening (false);   // only a dream for the Wingie listens
    }
    updateTitle();
    resized();
    repaint();
}

serial::Link::Measured MainComponent::measuredFor (int instrument) const
{
    return link.device() == instrument ? link.measured() : serial::Link::Measured {};
}

MainComponent::LinkNow MainComponent::linkNow()
{
    LinkNow n;
    n.open = link.connected();
    n.device = link.device();
    juce::uint32 count = 0;
    if (n.device == serial::wingie) { auto h = link.whands(); count = h.count; n.slot = h.slot; n.scene = h.scene; }
    else if (n.open) { auto h = link.hands(); count = h.count; n.slot = h.slot; n.scene = h.scene; }
    auto now = Time::getMillisecondCounter();
    if (count != handsSeen) { handsSeen = count; handsAt = now; }
    n.live = n.open && handsAt != 0 && now - handsAt < 1500;
    return n;
}

void MainComponent::newDream (int instrument)
{
    askToKeepChanges ([this, instrument]
    {
        auto today = Time::getCurrentTime().formatted ("%Y-%m-%d");
        doc.text = scq::starterDream (instrument, today);
        doc.file = File();
        nights = 0;
        journal.setText (doc.text);
        compile (true);
        doc.dirty = false;
        resetHistory();
        updateTitle();
        tapestry.fitAll();
    });
}

void MainComponent::openFile (const File& f)
{
    if (! f.existsAsFile()) return;
    doc.text = f.loadFileAsString();
    doc.file = f;
    auto* settings = props.getUserSettings();
    auto key = "nights:" + f.getFullPathName();
    nights = (uint32) settings->getIntValue (key, 0);
    settings->setValue (key, (int) nights + 1);    // one more night with this dream
    settings->setValue ("last", f.getFullPathName());
    settings->saveIfNeeded();
    journal.setText (doc.text);
    compile (true);
    doc.dirty = false;
    resetHistory();
    updateTitle();
    tapestry.fitAll();
}

void MainComponent::openLast()
{
    auto last = File (props.getUserSettings()->getValue ("last"));
    if (last.existsAsFile()) openFile (last);
}

void MainComponent::openDialog()
{
    askToKeepChanges ([this]
    {
        chooser = std::make_unique<FileChooser> ("open a dream", File::getSpecialLocation (File::userDocumentsDirectory), "*.dream;*.txt");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [this] (const FileChooser& fc) { openFile (fc.getResult()); });
    });
}

void MainComponent::save (bool as)
{
    doc.text = journal.getText();
    if (! as && doc.file != File())
    {
        doc.file.replaceWithText (doc.text);
        doc.dirty = false;
        updateTitle();
        status ("saved " + doc.file.getFileName());
        return;
    }
    chooser = std::make_unique<FileChooser> ("save the dream", File::getSpecialLocation (File::userDocumentsDirectory)
                                                 .getChildFile (Time::getCurrentTime().formatted ("%Y-%m-%d") + ".dream"), "*.dream");
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, [this] (const FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == File()) return;
        if (! f.hasFileExtension ("dream")) f = f.withFileExtension ("dream");
        doc.file = f;
        save (false);
    });
}

void MainComponent::sendToSP1 (int slot)
{
    doc.text = journal.getText();
    const int dreamFor = doc.instrument();            // the SP-1 or the Wingie: the dream says
    const String dev = deviceText (dreamFor);
    if (doc.errors > 0) { status ("fix the mistakes in the journal before sending"); return; }

    // the connected device is the other instrument: say so now, plainly, rather than send
    if (link.connected() && link.device() != dreamFor)
    {
        AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "send to " + dev,
                                          serial::mismatch (dreamFor, link.device(), link.port()));
        status ("not sent: the " + deviceText (link.device()) + " is connected, and this dream is for the " + dev);
        return;
    }

    // every scene must fit the processor, or it will stutter there
    auto chk = host.check (doc.text, nights);
    StringArray heavy;
    for (int k = 0; k < (int) chk.sceneLoads.size(); ++k)
        if (chk.sceneLoads[(size_t) k] > 0.85f)   // headroom: peaks run above the average
            heavy.add ((k < (int) doc.scenes.size() ? doc.scenes[(size_t) k].title : "scene " + String (k + 1))
                       + "  needs " + String (roundToInt (chk.sceneLoads[(size_t) k] * 100)) + "%");
    if (! heavy.isEmpty() && ! sendAnyway)
    {
        AlertWindow::showOkCancelBox (MessageBoxIconType::WarningIcon, "too heavy for the " + dev,
            "These scenes need more of the " + dev + "'s processor than it has, so they'll stutter there:\n\n"
            + heavy.joinIntoString ("\n") + "\n\nThe load meter shows each scene as you build it. "
            "Removing a heavy place (reverbs, choruses, the vocoder) or a few mixed cords brings it down"
            + (dreamFor == SCQ_WINGIE ? String (", and so does samplerate=half on a place that can take it.") : String (".")),
            "send anyway", "don't send", this,
            ModalCallbackFunction::create ([this, slot] (int r) { if (r == 1) { sendAnyway = true; sendToSP1 (slot); sendAnyway = false; } }));
        return;
    }
    auto sent = scales::withTunings (doc.text);       // the device has no scales folder: the tunings go with the dream
    if ((int) sent.getNumBytesAsUTF8() > doc.textMost)   // a slot keeps 16 KB: it would play it and lose it at power-off
    {
        AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "send to " + dev,
            "This dream is " + String ((int) sent.getNumBytesAsUTF8()) + " bytes of text; " + String (dreamFor == SCQ_WINGIE ? "a Wingie" : "an SP-1") + " slot keeps " + String (doc.textMost)
            + ". Notes after a ; and the positions at the end count too: a shorter journal, or fewer scenes, will fit.");
        return;
    }
    if (sendJob != nullptr && sendJob->isThreadRunning()) { status ("still sending the last one to the " + dev + "..."); return; }
    status (link.connected() ? "sending to the " + dev + (slot >= 0 ? ", slot " + String (slot + 1) : String()) + "..." : "looking for the " + dev + "...");
    sendBtn.setEnabled (false);
    sendingTo = dreamFor;
    sendJob = std::make_unique<SendJob>();
    auto* l = &link;
    sendJob->work = [l, sent, slot, dreamFor] (String& port) -> String
    {
        if (! serial::Link::enabled)                       // --no-link: nobody holds the port, open it just for this
            return serial::send (sent, slot, port, dreamFor);
        auto err = l->send (sent, slot, 8000, dreamFor);  // slot -1: the slot that is playing
        port = l->port();
        return err;
    };
    sendJob->done = [safe = Component::SafePointer<MainComponent> (this)] (const String& err, const String& port)
    {
        MessageManager::callAsync ([safe, err, port] { if (safe != nullptr) safe->sendFinished (err, port); });
    };
    sendJob->startThread();
}

void MainComponent::sendFinished (const String& err, const String& port)
{
    sendBtn.setEnabled (true);
    const String dev = deviceText (sendingTo);
    if (err.isEmpty()) status ("sent to the " + dev + " on " + port);
    else
    {
        status ("not sent to the " + dev);
        AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "send to " + dev, err);
    }
}

void MainComponent::addScene()
{
    doc.text = journal.getText();
    auto today = Time::getCurrentTime().formatted ("%Y-%m-%d");
    StringArray ls;
    ls.addLines (doc.text);
    // before the map, at the end of the dreams
    int at = ls.size();
    while (at > 0 && (ls[at - 1].startsWith (";@") || ls[at - 1].trim().isEmpty())) --at;
    StringArray add { "", today, String::repeatedString ("-", today.length()), "the voice is sine 4", "voice -> out" };
    if (doc.sceneSpans().front().header < 0)   // the first date: the old page becomes a scene too
    {
        ls.insert (0, "-----");
        ls.insert (0, "dream");
        at += 2;
    }
    for (int k = 0; k < add.size(); ++k) ls.insert (at + k, add[k]);
    doc.text = ls.joinIntoString ("\n");
    doc.dirty = true;
    remember();
    journal.setText (doc.text);
    compile (true);
    host.gotoScene ((int) doc.scenes.size() - 1);
    updateTitle();
}

void MainComponent::themeForShot (bool pop, bool dark, bool modular)
{
    art::setTheme (pop, dark, modular);
    hands.resized();
    resized();
    static_cast<Look*> (laf.get())->refresh();
    journal.applyTheme();
    tapestry.themeChanged();
    sendLookAndFeelChange();
    repaint();
}

// a snapshot: every value that can move while it plays, keyed by names (so it survives edits)
static String cordKey (const scq::Document& doc, const scq::CordInfo& c)
{
    auto* d = doc.node (c.dst);
    auto* s = c.src >= 0 ? doc.node (c.src) : nullptr;
    if (! d) return {};
    return c.src >= 0 ? (s ? "c:" + s->name + ":" + String (c.sport) + ">" + d->name + ":" + String (c.dport) : String())
                      : "b:" + d->name + ":" + String (c.dport);
}

Timeline::Snapshot MainComponent::capture()
{
    Timeline::Snapshot s;
    s.used = true;
    s.scene = doc.scene;
    for (auto& n : doc.nodes)
    {
        auto* p = scq::findPlace (n.place);
        if (! p || p->family >= scq::Hands) continue;
        if (p->tune) s.v["t:" + n.name] = scq_get_tune (n.id);
        if (p->att) for (int i = 0; i < p->ins.size(); ++i) s.v["a:" + n.name + ":" + String (i)] = scq_get_att (n.id, i);
        for (int i = 0; i < p->ins.size(); ++i)        // the panel knobs
        {
            float lo, hi;
            bool isOpt = false;
            for (auto& o : p->opts) if (o.name == p->ins[i]) isOpt = true;
            if (! isOpt && scq::knobRange (*p, i, lo, hi)) s.v["k:" + n.name + ":" + String (i)] = scq_get_knob (n.id, i);
        }
        for (int k = 0; k < (int) p->opts.size(); ++k)
        {
            auto& o = p->opts[(size_t) k];
            if (o.positions.isEmpty() && o.name != "tune") s.v["p:" + n.name + ":" + String (k)] = scq_get_param (n.id, k);
        }
    }
    std::map<String, int> seen;
    for (int k = 0; k < (int) doc.cords.size(); ++k)
    {
        auto& c = doc.cords[(size_t) k];
        auto base = cordKey (doc, c);
        if (base.isEmpty() || c.kind == scq::Door) continue;
        s.v[base + "#" + String (seen[base]++)] = scq_get_amount (k);
    }
    for (int i = 0; i < 4; ++i) s.v["f:" + String (i)] = scq_get_control (i);
    return s;
}

void MainComponent::applySnapshot (const Timeline::Values& values, int scene)
{
    if (scene != doc.scene && scene >= 0 && scene < (int) doc.scenes.size())
    {
        host.gotoScene (scene);                        // the SP-1s follow too
        auto r = host.describe();
        doc.readDescription (r.describe.toRawUTF8(), lastMessages.toRawUTF8());
        tapestry.refresh();
        refreshScenes();
    }
    std::map<String, int> nodes, cords, seen;
    for (auto& n : doc.nodes) nodes[n.name] = n.id;
    for (int k = 0; k < (int) doc.cords.size(); ++k)
    {
        auto base = cordKey (doc, doc.cords[(size_t) k]);
        if (base.isNotEmpty()) cords[base + "#" + String (seen[base]++)] = k;
    }
    // what's shown follows too: the knobs and the cord amounts move with a recall or a glide
    // (only in memory: the text keeps what you wrote)
    auto shown = [this] (int id, const String& key, float v)
    {
        for (auto& n : doc.nodes) if (n.id == id) n.opts[key] = scq::fmt (v);
    };
    for (auto& [key, v] : values)
    {
        auto kind = key.substring (0, 2), rest = key.substring (2);
        if (kind == "c:" || kind == "b:")
        {
            auto it = cords.find (key);
            if (it == cords.end()) continue;
            host.setAmount (it->second, v);
            doc.cords[(size_t) it->second].amt = v;
        }
        else if (kind == "f:") { int i = rest.getIntValue(); host.control (i, v); hands.showFader (i, v); wingiePanel.showFader (i, v); }
        else
        {
            auto name = rest.upToFirstOccurrenceOf (":", false, false);
            auto it = nodes.find (name);
            if (it == nodes.end()) continue;
            int idx = rest.fromFirstOccurrenceOf (":", false, false).getIntValue();
            auto* nd = doc.node (it->second);
            auto* p = nd ? scq::findPlace (nd->place) : nullptr;
            if (! p) continue;
            if (kind == "t:") { host.setTune (it->second, v); shown (it->second, "tune", v); }
            else if (kind == "a:") { host.setAtt (it->second, idx, v); if (idx < p->ins.size()) shown (it->second, "att." + p->ins[idx], v); }
            else if (kind == "p:") { host.setParam (it->second, idx, v); if (idx < (int) p->opts.size()) shown (it->second, p->opts[(size_t) idx].name, v); }
            else if (kind == "k:") { host.setKnob (it->second, idx, v); if (idx < p->ins.size()) shown (it->second, p->ins[idx], v); }
        }
    }
    static uint32 lastShown = 0;                       // redraw the boards at most ~15 times a second
    if (Time::getMillisecondCounter() - lastShown > 66) { lastShown = Time::getMillisecondCounter(); tapestry.refresh(); inspector.refresh(); }
}

void MainComponent::applyLook()
{
    auto* st = props.getUserSettings();
    // the look: openprocess (default) or the original journal; light, dark (default) or the computer's
    static bool listening = false;
    if (! listening) { Desktop::getInstance().addDarkModeSettingListener (this); listening = true; }
   #if SCQ_LEGACY
    const auto lookName = st->getValue ("look", "modular");      // legacy: the modular editor by default
   #else
    const auto lookName = st->getValue ("look", "journal");
   #endif
    bool pop = lookName == "openprocess" || lookName == "modular";
    auto appearance = st->getValue ("appearance", "light");
    bool dark = appearance == "system" ? Desktop::getInstance().isDarkModeActive() : appearance == "dark";
    art::setTheme (pop, dark, lookName == "modular");
    static_cast<Look*> (laf.get())->refresh();
    journal.applyTheme();
    tapestry.themeChanged();
    hands.repaint();
    sendLookAndFeelChange();
    repaint();
    const float slacks[] = { 0.0f, 0.5f, 1.0f, 1.8f, 3.0f };
    art::cableStyle().opacity = jlimit (0.1f, 1.0f, (float) st->getIntValue ("cableOpacity", 100) / 100.0f);
    art::cableStyle().slack = slacks[jlimit (0, 4, st->getIntValue ("cableSlack", 2))];
    tapestry.setLight ((float) st->getIntValue ("roomLight", 100) / 100.0f);
   #if SCQ_LEGACY
    tapestry.setRouting (false);     // legacy: hanging curves only (the router searches a grid)
   #else
    tapestry.setRouting (st->getBoolValue ("cableRouter", false));
   #endif
    showTimeline = st->getBoolValue ("showTimeline", true);
    tapestry.repaint();
    hands.resized();
    resized();                       // the two looks lay the window out differently
    st->saveIfNeeded();              // remembered at once, not just when the app quits
}

void MainComponent::textEdited (bool recompile)
{
    remember();
    journal.setText (doc.text);
    syncInstrument();
    if (recompile) compile (true);
    else { lastCode = codeOf (doc.text); doc.readPositions(); tapestry.refresh(); }
    inspector.refresh();
    updateTitle();
}

void MainComponent::remember()
{
    if (doc.text == committed) return;
    undoStack.push_back (committed);
    if (undoStack.size() > 300) undoStack.erase (undoStack.begin());
    redoStack.clear();
    committed = doc.text;
}

void MainComponent::resetHistory()
{
    undoStack.clear(); redoStack.clear();
    committed = doc.text;
}

void MainComponent::undoRedo (bool redo)
{
    if (pendingTyping) { pendingTyping = false; doc.text = journal.getText(); remember(); }
    auto& from = redo ? redoStack : undoStack;
    auto& to = redo ? undoStack : redoStack;
    if (from.empty()) { status (redo ? "nothing to redo" : "nothing to undo"); return; }
    to.push_back (doc.text);
    doc.text = from.back();
    from.pop_back();
    committed = doc.text;
    journal.setText (doc.text);
    compile (true);
    doc.dirty = true;
    updateTitle();
    status (redo ? "redone" : "undone");
}

bool MainComponent::keyPressed (const KeyPress& k)
{
    if (k == KeyPress ('z', ModifierKeys::commandModifier, 0)) { undoRedo (false); return true; }
    if (k == KeyPress ('z', ModifierKeys::commandModifier | ModifierKeys::shiftModifier, 0)
        || k == KeyPress ('Z', ModifierKeys::commandModifier | ModifierKeys::shiftModifier, 0)) { undoRedo (true); return true; }
    if (k == KeyPress ('s', ModifierKeys::commandModifier, 0)) { save (false); return true; }
    if (k == KeyPress ('o', ModifierKeys::commandModifier, 0)) { openDialog(); return true; }
    if (k == KeyPress ('n', ModifierKeys::commandModifier, 0)) { newDream (doc.instrument()); return true; }
    if (k == KeyPress ('u', ModifierKeys::commandModifier, 0)) { sendToSP1(); return true; }
    return false;
}

// ------------------------------------------------------------------------------------------ menus

StringArray MainComponent::getMenuBarNames() { return { "File", "Edit", "View", "Device", "Help" }; }

PopupMenu MainComponent::getMenuForIndex (int idx, const String&)
{
    PopupMenu m;
    if (idx == 0)
    {
        m.addItem (1, "New dream (SP-1)", true);
        m.addItem (8, "New dream for the Wingie", true);
        m.addItem (2, "Open...", true);
        m.addItem (3, "Save", true);
        m.addItem (4, "Save as...", true);
        m.addSeparator();
        m.addItem (5, "Send to " + deviceText (doc.instrument()), true);
        m.addItem (6, "Load the first night (example)", true);
        m.addItem (7, "Load a history of modular (example)", true);
    }
    else if (idx == 1)
    {
        PopupMenu::Item u ("Undo"); u.itemID = 31; u.isEnabled = ! undoStack.empty() || pendingTyping;
        u.shortcutKeyDescription = String::fromUTF8 ("\xe2\x8c\x98Z");
        m.addItem (u);
        PopupMenu::Item r ("Redo"); r.itemID = 32; r.isEnabled = ! redoStack.empty();
        r.shortcutKeyDescription = String::fromUTF8 ("\xe2\x87\xa7\xe2\x8c\x98Z");
        m.addItem (r);
    }
    else if (idx == 2)
    {
        m.addItem (11, "Journal only", true, view == 0);
        m.addItem (12, "Journal and tapestry", true, view == 1);
        m.addItem (13, "Tapestry only", true, view == 2);
        m.addSeparator();
        auto* st = props.getUserSettings();
        PopupMenu light, opac, slack, route;
        const int lights[] = { 100, 70, 45, 25, 10 };
        for (int i = 0; i < 5; ++i) light.addItem (40 + i, String (lights[i]) + "%", true, st->getIntValue ("roomLight", 100) == lights[i]);
        const int ops[] = { 100, 80, 60, 40, 20 };
        for (int i = 0; i < 5; ++i) opac.addItem (50 + i, String (ops[i]) + "%", true, st->getIntValue ("cableOpacity", 100) == ops[i]);
        const char* slacks[] = { "tight", "a little", "normal", "loose", "very loose" };
        for (int i = 0; i < 5; ++i) slack.addItem (60 + i, slacks[i], true, st->getIntValue ("cableSlack", 2) == i);
        route.addItem (70, "hanging curves", true, ! st->getBoolValue ("cableRouter", false));
        route.addItem (71, "around the boards (like Max)", true, st->getBoolValue ("cableRouter", false));
        PopupMenu look, mode;
       #if SCQ_LEGACY
        const auto lk = st->getValue ("look", "modular");
       #else
        const auto lk = st->getValue ("look", "journal");
       #endif
        auto ap = st->getValue ("appearance", "light");
        look.addItem (81, "journal (the original)", true, lk == "journal");
        look.addItem (80, "faltung (dark: partials field, light: mustard sleeve)", true, lk == "openprocess");
        look.addItem (82, "modular (the 2004 patch editor)", true, lk == "modular");
        mode.addItem (85, "dark", true, ap == "dark");
        mode.addItem (86, "light", true, ap == "light");
        mode.addItem (87, "follow the computer", true, ap == "system");
        m.addSubMenu ("Look", look);
        m.addSubMenu ("Light or dark", mode);
        m.addSeparator();
        m.addSubMenu ("Room light", light);
        m.addSubMenu ("Cable opacity", opac);
        m.addSubMenu ("Cable slack", slack);
        m.addSubMenu ("Cable routing", route);
        m.addSeparator();
        m.addItem (16, "Snapshots and timeline", true, showTimeline);
        m.addItem (14, "Find my patch (fit the tapestry in view)");
        m.addItem (15, "Sound settings...");
    }
    else if (idx == 3)
    {
        // which instrument this dream is for, sending to it, and the Wingie's microphones
        const int i = doc.instrument();
        m.addItem (90, "Patch for the SP-1", true, i == SCQ_SP1);
        m.addItem (91, "Patch for the Wingie", true, i == SCQ_WINGIE);
        m.addSeparator();
        m.addItem (5, "Send to " + deviceText (i) + " (the slot that is playing)", true);
        PopupMenu slots;
        const int n = i == SCQ_WINGIE ? 12 : 5;
        for (int k = 0; k < n; ++k) slots.addItem (100 + k, "slot " + String (k + 1));
        m.addSubMenu ("Send to " + deviceText (i) + " slot", slots);
        m.addSeparator();
        m.addItem (92, "Listen to the computer's microphone as the Wingie's", i == SCQ_WINGIE, host.listening());
        m.addItem (15, "Sound settings...");
        m.addSeparator();
        auto ln = linkNow();
        m.addItem (93, ln.open ? "connected: " + deviceText (ln.device) + " on " + link.port().fromLastOccurrenceOf ("/", false, false)
                                 + (ln.device != i ? "  (this dream is for the " + deviceText (i) + ")" : String())
                               : String ("no SP-1 or Wingie connected"), false);
    }
    else
    {
        m.addItem (21, "The places (reference)");
        m.addItem (22, "How to write a dream");
    }
    return m;
}

void MainComponent::menuItemSelected (int id, int)
{
    switch (id)
    {
        case 1: newDream (0); break;
        case 8: newDream (1); break;
        case 90: case 91: setInstrument (id - 90); break;
        case 92: if (wingiePanel.onListen) wingiePanel.onListen (! host.listening()); break;
        case 2: openDialog(); break;
        case 3: save (false); break;
        case 4: save (true); break;
        case 5: sendToSP1(); break;
        case 6: askToKeepChanges ([this]
                {
                    doc.text = String::fromUTF8 (ScqData::firstnight_dream, ScqData::firstnight_dreamSize);
                    doc.file = File(); nights = 0;
                    journal.setText (doc.text); compile (true); doc.dirty = false; resetHistory(); updateTitle(); tapestry.fitAll();
                }); break;
        case 7: askToKeepChanges ([this]
                {
                    doc.text = String::fromUTF8 (ScqData::ahistory_dream, ScqData::ahistory_dreamSize);
                    doc.file = File(); nights = 0;
                    journal.setText (doc.text); compile (true); doc.dirty = false; resetHistory(); updateTitle(); tapestry.fitAll();
                }); break;
        case 40: case 41: case 42: case 43: case 44:
        {
            const int lights[] = { 100, 70, 45, 25, 10 };
            props.getUserSettings()->setValue ("roomLight", lights[id - 40]); applyLook(); break;
        }
        case 50: case 51: case 52: case 53: case 54:
        {
            const int ops[] = { 100, 80, 60, 40, 20 };
            props.getUserSettings()->setValue ("cableOpacity", ops[id - 50]); applyLook(); break;
        }
        case 60: case 61: case 62: case 63: case 64: props.getUserSettings()->setValue ("cableSlack", id - 60); applyLook(); break;
        case 70: case 71: props.getUserSettings()->setValue ("cableRouter", id == 71); applyLook(); break;
        case 16: showTimeline = ! showTimeline; props.getUserSettings()->setValue ("showTimeline", showTimeline); resized(); break;
        case 80: case 81: case 82: props.getUserSettings()->setValue ("look", id == 80 ? "openprocess" : (id == 82 ? "modular" : "journal")); applyLook(); break;
        case 85: case 86: case 87: props.getUserSettings()->setValue ("appearance", id == 85 ? "dark" : (id == 86 ? "light" : "system")); applyLook(); break;
        case 31: undoRedo (false); break;
        case 32: undoRedo (true); break;
        case 11: case 12: case 13: setView (id - 11); break;
        case 14: tapestry.fitAll(); break;
        case 15: soundBtn.onClick(); break;
        case 21: case 22:
        {
            auto f = File::getSpecialLocation (File::tempDirectory).getChildFile ("sleepcousonique-places.md");
            f.replaceWithText (String::fromUTF8 (ScqData::places_md, ScqData::places_mdSize));
            f.startAsProcess();
            break;
        }
        default:
            if (id >= 100 && id < 112) sendToSP1 (id - 100);    // the protocol counts slots from 0
            break;
    }
}
