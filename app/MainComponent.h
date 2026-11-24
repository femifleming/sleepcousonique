#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "Model.h"
#include "Host.h"
#include "Journal.h"
#include "Tapestry.h"
#include "Hands.h"
#include "Serial.h"
#include "Pop.h"
#include "Timeline.h"
#include "Wingie.h"

class MainComponent : public juce::Component,
                      public juce::DarkModeSettingListener,
                      public juce::DragAndDropContainer,
                      public juce::MenuBarModel,
                      private juce::Timer
{
public:
    void lookForShot (float light, bool router, int hlFirst, int hlLast)   // screenshots only: nothing is saved
    { tapestry.setLight (light); tapestry.setRouting (router); tapestry.setHighlight (hlFirst, hlLast); hlShot = hlLast >= hlFirst; }
    bool hlShot = false;
    void themeForShot (bool pop, bool dark, bool modular = false);   // screenshots only: nothing is saved
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int, const juce::String&) override;
    void menuItemSelected (int, int) override;

    void openFile (const juce::File&);
    void openLast();                 // the dream you had open last time, if it's still there
    void showView (int v) { setView (v); }
    // test modes only (see Main.cpp): the same calls the hands panel and a knob on a board make
    Host& engine() { return host; }
    bool turnAtt (const juce::String& place, const juce::String& inlet, float amount);   // drag an attenuverter, let go
    void reload() { compile (true); }                                                    // what any edit does
    bool askToKeepChanges (std::function<void()> then);
    std::function<void (const juce::String&)> onTitle;

private:
    // one history for everything: each entry is the whole dream text (positions included)
    std::vector<juce::String> undoStack, redoStack;
    juce::String committed;
    void remember();                 // the text changed: keep what it was
    void resetHistory();             // a new or opened dream
    void undoRedo (bool redo);
    void applyLook();
    void textEdited (bool recompile);
    Timeline::Snapshot capture();        // what's sounding now, by names
    void applySnapshot (const Timeline::Values&, int scene);
    bool showTimeline = true;   // the tapestry or the settings column changed the text
    void mouseDown (const juce::MouseEvent&) override;
    void resizedPop();
    void paintPop (juce::Graphics&);
    void darkModeSettingChanged() override { applyLook(); }   // the computer switched light / dark                // room light, cable opacity / slack / routing from the settings

    void timerCallback() override;
    void compile (bool force = false);
    void afterLoad (const Host::Result&);
    void refreshScenes();
    void setView (int);
    void newDream (int instrument = 0);
    void setInstrument (int instrument);   // the SP-1 / Wingie switch: an undoable text edit
    void syncInstrument();                 // the text says which: show its hands, browser and sends
    int shownInstrument = -1;
    int sendingTo = 0;
    serial::Link::Measured measuredFor (int instrument) const;   // the device's report, if that device is the one connected
    juce::String deviceText (int instrument) const { return instrument == 1 ? "Wingie" : "SP-1"; }
    void openDialog();
    void save (bool as);
    void sendToSP1 (int slot = -1);        // -1: the slot that is playing
    void sendFinished (const juce::String& err, const juce::String& port);
    void addScene();
    void status (const juce::String&);
    void updateTitle();
    juce::String codeOf (const juce::String& text) const;

    scq::Document doc;
    Host host;
    serial::Link link { true };      // an SP-1 or a Wingie, whichever is plugged in
    juce::uint32 eventShown = 0;     // the last device event shown on the status line
    // a send runs on its own thread (it can take seconds): the window stays live meanwhile.
    // Declared after the link, so it is finished (and gone) before the link is.
    struct SendJob : juce::Thread
    {
        SendJob() : juce::Thread ("send to SP-1") {}
        ~SendJob() override { stopThread (40000); }
        void run() override { err = work (port); if (done) done (err, port); }
        std::function<juce::String (juce::String&)> work;
        std::function<void (const juce::String&, const juce::String&)> done;
        juce::String err, port;
    };
    std::unique_ptr<SendJob> sendJob;
    Journal journal;
    Tapestry tapestry { doc, host };
    Hands hands { host };
    WingiePanel wingiePanel { host };      // in place of the SP-1's hands for a dream for the Wingie
    WingieMeters wingieMeters { host };
    juce::StretchableLayoutManager split;
    std::unique_ptr<juce::StretchableLayoutResizerBar> bar;

    juce::TextButton sendBtn { "send to SP-1" }, soundBtn { "sound" }, undoBtn { "undo" }, redoBtn { "redo" };
    juce::TextButton instBtn[2];             // SP-1 / Wingie
    PlaceBrowser browser;
    Inspector inspector { doc, host };
    Timeline timeline;
    juce::Rectangle<int> pillArea, marginArea, stripArea, contentArea, helpArea;
    juce::TextButton kindBtn[5], viewBtn[3], addSceneBtn { "+ scene" };
    juce::OwnedArray<juce::TextButton> sceneBtns;
    juce::Rectangle<int> meters, statusArea, sceneArea, linkArea;   // linkArea: is the SP-1 there
    juce::uint32 handsSeen = 0, handsAt = 0;                         // the last hands report, and when
    struct LinkNow { bool open = false, live = false; int device = -1, slot = 1, scene = 1; };
    LinkNow linkNow();                                               // what is connected, and is it talking (its hands keep coming)

    int view = 1;            // 0 journal, 1 both, 2 tapestry
    juce::String statusText, lastCode, lastMessages;
    juce::String roomShown;      // the room's line in the status bar ("the room: 7 devices nearby"), empty without a crowd
    juce::uint32 typedAt = 0;
    bool pendingTyping = false;
    bool sendAnyway = false;
    juce::uint32 nights = 0;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::ApplicationProperties props;
    std::unique_ptr<juce::LookAndFeel_V4> laf;
};
