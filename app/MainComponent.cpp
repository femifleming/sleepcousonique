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
        setColour (TextButton::textColourOnId, t.pop ? Colour (0xff1a1a1a) : t.window);
        setColour (PopupMenu::backgroundColourId, t.pop ? t.card : art::paper);
        setColour (PopupMenu::textColourId, art::ink);
        setColour (PopupMenu::highlightedBackgroundColourId, t.pop ? t.checkB : art::blue);
        setColour (PopupMenu::highlightedTextColourId, t.pop ? Colour (0xff1a1a1a) : art::paper);
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
        if (t.pop)                    // flat; on is checker
        {
            if (b.getToggleState()) art::fillChecker (g, r, 7);
            else
            {
                auto c = bg;
                if (over) c = c.brighter (t.dark ? 0.15f : 0.06f);
                if (down) c = c.darker (0.15f);
                g.setColour (c); g.fillRect (r);
            }
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
    art::setTheme (true, true);              // openprocess, dark, until the settings say otherwise
    laf = std::make_unique<Look>();
    setLookAndFeel (laf.get());
    PropertiesFile::Options o;
    o.applicationName = "Sleepcousonique";
    o.filenameSuffix = "settings";
    o.osxLibrarySubFolder = "Application Support";
    props.setStorageParameters (o);

    addAndMakeVisible (journal);
    addAndMakeVisible (tapestry);
    addAndMakeVisible (hands);
    bar = std::make_unique<StretchableLayoutResizerBar> (&split, 1, true);
    addAndMakeVisible (*bar);
    split.setItemLayout (0, 260, -0.8, -0.38);
    split.setItemLayout (1, 6, 6, 6);
    split.setItemLayout (2, 300, -1.0, -0.62);

    const char* kinds[] = { "->  hall", "~>  fog", "=>  water", ">|  door" };
    const char* tips[] = { "a plain wire", "a resistor: half as loud, softened", "a capacitor: slow to fill, slow to drain", "a diode: only the positive half" };
    for (int i = 0; i < 4; ++i)
    {
        kindBtn[i].setButtonText (kinds[i]);
        kindBtn[i].setTooltip (tips[i]);
        kindBtn[i].setClickingTogglesState (true);
        kindBtn[i].setRadioGroupId (1);
        kindBtn[i].onClick = [this, i] { tapestry.handKind = i; };
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
        auto* sel = new AudioDeviceSelectorComponent (host.devices, 0, 0, 2, 2, false, false, true, false);
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
    journal.onUndo = [this] (bool redo) { undoRedo (redo); };
    tapestry.onTextEdited = [this] (bool recompile)
    {
        remember();
        journal.setText (doc.text);
        if (recompile) compile (true);
        else { lastCode = codeOf (doc.text); doc.readPositions(); tapestry.refresh(); }
        updateTitle();
    };
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

    setSize (1440, 900);
    // start with the first night
    doc.text = String::fromUTF8 (ScqData::firstnight_dream, ScqData::firstnight_dreamSize);
    journal.setText (doc.text);
    compile (true);
    doc.dirty = false;
    resetHistory();
    applyLook();
    updateTitle();
    startTimerHz (20);
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
    auto r = getLocalBounds();
    auto top = r.removeFromTop (46).reduced (10, 8);
    sendBtn.setBounds (top.removeFromLeft (128));
    top.removeFromLeft (16);
    for (auto& k : kindBtn) k.setBounds (top.removeFromLeft (92));
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
    hands.setBounds (r.removeFromBottom (176));

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

void MainComponent::paint (Graphics& g)
{
    auto& th = art::theme();
    g.fillAll (th.window);
    g.setColour (th.bar);
    g.fillRect (sceneArea);
    g.setColour (th.line);
    g.drawHorizontalLine (sceneArea.getY(), 0, (float) getWidth());
    g.drawHorizontalLine (sceneArea.getBottom() - 1, 0, (float) getWidth());

    // is the SP-1 there: plugged in and talking (its hands arrive ~30 times a second)
    {
        bool open = link.connected();
        auto h = open ? link.hands() : serial::Link::Hands {};
        auto now = Time::getMillisecondCounter();
        if (h.count != handsSeen) { handsSeen = h.count; handsAt = now; }
        bool live = open && handsAt != 0 && now - handsAt < 1500;
        String text = ! open ? "SP-1 not connected"
                    : live ? "SP-1 connected  " + link.port().fromLastOccurrenceOf ("/", false, false)
                             + "  slot " + String (h.slot) + "  scene " + String (h.scene)
                    : "SP-1 found, waiting for it to answer...";
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
        // the estimate, and when an SP-1 is plugged in, what it measures (avg, and its peaks)
        auto meas = link.connected() ? link.measured() : serial::Link::Measured {};
        bool fresh = meas.avg >= 0 && Time::getMillisecondCounter() - meas.when < 12000;
        String label = "load " + String (roundToInt (doc.load * 100)) + "%";
        float v = doc.load;
        if (fresh)
        {
            label << "   SP-1 " << meas.avg << "%, peaks " << meas.peak << "%" << (meas.over ? "  TOO HEAVY" : "");
            v = jmax (v, (float) meas.peak / 100.0f);
        }
        bar1 (m.removeFromLeft (w - 8), label, v, th.pop ? th.checkA : art::blue);
    }
    m.removeFromLeft (8);
    bar1 (m.removeFromLeft (w - 8), "memory " + String (roundToInt (100.0f * (float) doc.poolUsed / (float) doc.poolTotal)) + "%",
          (float) doc.poolUsed / (float) doc.poolTotal, th.pop ? th.current : art::copper);
    m.removeFromLeft (8);
    bar1 (m, "out", host.level, th.pop ? th.checkB : art::gold);

    g.setColour (th.bar);
    g.fillRect (statusArea);
    g.setColour (th.chromeText.withAlpha (0.85f));
    g.setFont (th.pop ? art::label (11.5f) : art::mono (12));
    g.drawText (statusText, statusArea.reduced (12, 0), Justification::left);
    g.setColour (th.chromeDim);
    g.drawText (doc.file.existsAsFile() ? doc.file.getFullPathName() : String ("first night (an example - save to keep it)"),
                statusArea.reduced (12, 0), Justification::right);
}

void MainComponent::status (const String& s)
{
    statusText = s;
    repaint (statusArea);
}

void MainComponent::updateTitle()
{
    if (onTitle) onTitle ((doc.file.existsAsFile() ? doc.file.getFileNameWithoutExtension() : String ("first night")) + (doc.dirty ? String::fromUTF8 ("  \xe2\x80\xa2") : String()) + String::fromUTF8 ("  \xe2\x80\x94  Sleepcousonique"));
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

void MainComponent::newDream()
{
    askToKeepChanges ([this]
    {
        auto today = Time::getCurrentTime().formatted ("%Y-%m-%d");
        doc.text = "; a new dream\n\n" + today + "\n----------\nthe voice is sine 3\nvoice -> out\n";
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
    journal.setText (doc.text);
    compile (true);
    doc.dirty = false;
    resetHistory();
    updateTitle();
    tapestry.fitAll();
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

void MainComponent::sendToSP1()
{
    doc.text = journal.getText();
    if (doc.errors > 0) { status ("fix the mistakes in the journal before sending"); return; }

    // every scene must fit the SP-1's processor, or it will stutter there
    auto chk = host.check (doc.text, nights);
    StringArray heavy;
    for (int k = 0; k < (int) chk.sceneLoads.size(); ++k)
        if (chk.sceneLoads[(size_t) k] > 0.85f)   // headroom: peaks run above the average
            heavy.add ((k < (int) doc.scenes.size() ? doc.scenes[(size_t) k].title : "scene " + String (k + 1))
                       + "  needs " + String (roundToInt (chk.sceneLoads[(size_t) k] * 100)) + "%");
    if (! heavy.isEmpty() && ! sendAnyway)
    {
        AlertWindow::showOkCancelBox (MessageBoxIconType::WarningIcon, "too heavy for the SP-1",
            "These scenes need more of the SP-1's processor than it has, so they'll stutter there:\n\n"
            + heavy.joinIntoString ("\n") + "\n\nThe load meter shows each scene as you build it. "
            "Removing a heavy place (reverbs, choruses, the vocoder) or a few mixed cords brings it down.",
            "send anyway", "don't send", this,
            ModalCallbackFunction::create ([this] (int r) { if (r == 1) { sendAnyway = true; sendToSP1(); sendAnyway = false; } }));
        return;
    }
    status ("looking for the SP-1...");
    String port;
    String err;
    auto sent = scales::withTunings (doc.text);       // the SP-1 has no scales folder: the tunings go with the dream
    if (link.connected()) { err = link.send (sent, -1); port = link.port(); }   // -1: the slot that is playing
    else err = serial::send (sent, -1, port);
    if (err.isEmpty()) status ("sent to the SP-1 on " + port);
    else AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "send to SP-1", err);
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

void MainComponent::themeForShot (bool pop, bool dark)
{
    art::setTheme (pop, dark);
    static_cast<Look*> (laf.get())->refresh();
    journal.applyTheme();
    tapestry.themeChanged();
    sendLookAndFeelChange();
    repaint();
}

void MainComponent::applyLook()
{
    auto* st = props.getUserSettings();
    // the look: openprocess (default) or the original journal; light, dark (default) or the computer's
    static bool listening = false;
    if (! listening) { Desktop::getInstance().addDarkModeSettingListener (this); listening = true; }
    bool pop = st->getValue ("look", "openprocess") == "openprocess";
    auto appearance = st->getValue ("appearance", "dark");
    bool dark = appearance == "system" ? Desktop::getInstance().isDarkModeActive() : appearance == "dark";
    art::setTheme (pop, dark);
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
    tapestry.setRouting (st->getBoolValue ("cableRouter", false));
    tapestry.repaint();
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
    if (k == KeyPress ('n', ModifierKeys::commandModifier, 0)) { newDream(); return true; }
    if (k == KeyPress ('u', ModifierKeys::commandModifier, 0)) { sendToSP1(); return true; }
    return false;
}

// ------------------------------------------------------------------------------------------ menus

StringArray MainComponent::getMenuBarNames() { return { "File", "Edit", "View", "Help" }; }

PopupMenu MainComponent::getMenuForIndex (int idx, const String&)
{
    PopupMenu m;
    if (idx == 0)
    {
        m.addItem (1, "New dream", true);
        m.addItem (2, "Open...", true);
        m.addItem (3, "Save", true);
        m.addItem (4, "Save as...", true);
        m.addSeparator();
        m.addItem (5, "Send to SP-1", true);
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
        bool pop = st->getValue ("look", "openprocess") == "openprocess";
        auto ap = st->getValue ("appearance", "dark");
        look.addItem (80, "openprocess", true, pop);
        look.addItem (81, "journal (the original)", true, ! pop);
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
        m.addItem (14, "Find my patch (fit the tapestry in view)");
        m.addItem (15, "Sound settings...");
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
        case 1: newDream(); break;
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
        case 80: case 81: props.getUserSettings()->setValue ("look", id == 80 ? "openprocess" : "journal"); applyLook(); break;
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
        default: break;
    }
}
