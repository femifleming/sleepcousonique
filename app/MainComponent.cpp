#include "MainComponent.h"
#include "Art.h"
#include "Serial.h"
#include "ScqData.h"
#include "scq.h"

using namespace juce;

namespace
{
struct Look : LookAndFeel_V4
{
    Look()
    {
        setColour (TextButton::buttonColourId, Colour (0xfff4f0e4));
        setColour (TextButton::buttonOnColourId, art::ink);
        setColour (TextButton::textColourOffId, art::ink);
        setColour (TextButton::textColourOnId, art::paper);
        setColour (PopupMenu::backgroundColourId, art::paper);
        setColour (PopupMenu::textColourId, art::ink);
        setColour (PopupMenu::highlightedBackgroundColourId, art::blue);
        setColour (PopupMenu::highlightedTextColourId, art::paper);
        setColour (PopupMenu::headerTextColourId, art::umber);
        setColour (AlertWindow::backgroundColourId, art::paper);
        setColour (AlertWindow::textColourId, art::ink);
        setColour (TextEditor::backgroundColourId, Colours::white);
        setColour (TextEditor::textColourId, art::ink);
        setColour (ResizableWindow::backgroundColourId, art::paper);
    }
    void drawButtonBackground (Graphics& g, Button& b, const Colour& bg, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.5f);
        auto c = b.getToggleState() ? art::ink : bg;
        if (over && ! b.getToggleState()) c = c.darker (0.06f);
        if (down) c = c.darker (0.15f);
        g.setColour (c);
        g.fillRoundedRectangle (r, 4);
        g.setColour (Colour (0x33000000));
        g.drawRoundedRectangle (r, 4, 1);
    }
    Font getTextButtonFont (TextButton&, int h) override { return art::mono (jmin (13.0f, (float) h * 0.5f)); }
    Font getPopupMenuFont() override { return art::mono (13.5f); }
};
}

MainComponent::MainComponent()
{
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
    tapestry.onTextEdited = [this] (bool recompile)
    {
        journal.setText (doc.text);
        if (recompile) compile (true);
        else { lastCode = codeOf (doc.text); doc.readPositions(); tapestry.refresh(); }
        updateTitle();
    };
    tapestry.onStatus = [this] (const String& s) { status (s); };
    tapestry.onShowLine = [this] (int line) { if (view == 2) setView (1); journal.showLine (line); };
    hands.typing = [this] { return journal.editorHasFocus(); };
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
    updateTitle();
    startTimerHz (20);
    MessageManager::callAsync ([this] { tapestry.fitAll(); });
}

MainComponent::~MainComponent()
{
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
        compile();
    }
    if (host.scene() != doc.scene)   // a journey happened (or a tab was pressed)
    {
        auto r = host.describe();
        doc.readDescription (r.describe.toRawUTF8(), lastMessages.toRawUTF8());
        tapestry.refresh();
        refreshScenes();
    }
    repaint (meters);
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
    g.fillAll (Colour (0xffe8e3d6));
    g.setColour (Colour (0xfff2ede0));
    g.fillRect (sceneArea);
    g.setColour (Colour (0x22000000));
    g.drawHorizontalLine (sceneArea.getY(), 0, (float) getWidth());
    g.drawHorizontalLine (sceneArea.getBottom() - 1, 0, (float) getWidth());

    // meters: the SP-1's patience (load), its memory, and your ears
    auto m = meters.reduced (0, 8);
    auto bar1 = [&] (Rectangle<int> r, const String& label, float v, Colour c)
    {
        g.setColour (art::ink);
        g.setFont (art::mono (11));
        g.drawText (label, r.removeFromTop (14), Justification::left);
        auto b = r.removeFromTop (8).toFloat();
        g.setColour (Colour (0x22000000)); g.fillRoundedRectangle (b, 3);
        g.setColour (v > 1 ? Colour (0xffb0301f) : c);
        g.fillRoundedRectangle (b.withWidth (b.getWidth() * jlimit (0.0f, 1.0f, v)), 3);
    };
    int w = m.getWidth() / 3;
    bar1 (m.removeFromLeft (w - 8), "load " + String (roundToInt (doc.load * 100)) + "%", doc.load, art::blue);
    m.removeFromLeft (8);
    bar1 (m.removeFromLeft (w - 8), "memory " + String (roundToInt (100.0f * (float) doc.poolUsed / (float) doc.poolTotal)) + "%",
          (float) doc.poolUsed / (float) doc.poolTotal, art::copper);
    m.removeFromLeft (8);
    bar1 (m, "ears", host.level, art::gold);

    g.setColour (Colour (0xfff2ede0));
    g.fillRect (statusArea);
    g.setColour (art::ink.withAlpha (0.8f));
    g.setFont (art::mono (12));
    g.drawText (statusText, statusArea.reduced (12, 0), Justification::left);
    g.setColour (art::ink.withAlpha (0.45f));
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
        doc.text = "; a new dream\n\n" + today + "\n----------\nthe voice is aira 3\nvoice -> ears\n";
        doc.file = File();
        nights = 0;
        journal.setText (doc.text);
        compile (true);
        doc.dirty = false;
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
    status ("looking for the SP-1...");
    String port;
    auto err = serial::send (doc.text, -1, port);   // -1: the slot that is playing
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
    StringArray add { "", today, String::repeatedString ("-", today.length()), "the voice is aira 4", "voice -> ears" };
    if (doc.sceneSpans().front().header < 0)   // the first date: the old page becomes a scene too
    {
        ls.insert (0, "-----");
        ls.insert (0, "dream");
        at += 2;
    }
    for (int k = 0; k < add.size(); ++k) ls.insert (at + k, add[k]);
    doc.text = ls.joinIntoString ("\n");
    doc.dirty = true;
    journal.setText (doc.text);
    compile (true);
    host.gotoScene ((int) doc.scenes.size() - 1);
    updateTitle();
}

bool MainComponent::keyPressed (const KeyPress& k)
{
    if (k == KeyPress ('s', ModifierKeys::commandModifier, 0)) { save (false); return true; }
    if (k == KeyPress ('o', ModifierKeys::commandModifier, 0)) { openDialog(); return true; }
    if (k == KeyPress ('n', ModifierKeys::commandModifier, 0)) { newDream(); return true; }
    if (k == KeyPress ('u', ModifierKeys::commandModifier, 0)) { sendToSP1(); return true; }
    return false;
}

// ------------------------------------------------------------------------------------------ menus

StringArray MainComponent::getMenuBarNames() { return { "File", "View", "Help" }; }

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
        m.addItem (11, "Journal only", true, view == 0);
        m.addItem (12, "Journal and tapestry", true, view == 1);
        m.addItem (13, "Tapestry only", true, view == 2);
        m.addSeparator();
        m.addItem (14, "Fit the tapestry in view");
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
                    journal.setText (doc.text); compile (true); doc.dirty = false; updateTitle(); tapestry.fitAll();
                }); break;
        case 7: askToKeepChanges ([this]
                {
                    doc.text = String::fromUTF8 (ScqData::ahistory_dream, ScqData::ahistory_dreamSize);
                    doc.file = File(); nights = 0;
                    journal.setText (doc.text); compile (true); doc.dirty = false; updateTitle(); tapestry.fitAll();
                }); break;
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
