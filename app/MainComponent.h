#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "Model.h"
#include "Host.h"
#include "Journal.h"
#include "Tapestry.h"
#include "Hands.h"
#include "Serial.h"
#include "Pop.h"

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
    void themeForShot (bool pop, bool dark);   // screenshots only: nothing is saved
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
    void textEdited (bool recompile);   // the tapestry or the settings column changed the text
    void mouseDown (const juce::MouseEvent&) override;
    void resizedPop();
    void paintPop (juce::Graphics&);
    void darkModeSettingChanged() override { applyLook(); }   // the computer switched light / dark                // room light, cable opacity / slack / routing from the settings

    void timerCallback() override;
    void compile (bool force = false);
    void afterLoad (const Host::Result&);
    void refreshScenes();
    void setView (int);
    void newDream();
    void openDialog();
    void save (bool as);
    void sendToSP1();
    void addScene();
    void status (const juce::String&);
    void updateTitle();
    juce::String codeOf (const juce::String& text) const;

    scq::Document doc;
    Host host;
    serial::Link link;
    Journal journal;
    Tapestry tapestry { doc, host };
    Hands hands { host };
    juce::StretchableLayoutManager split;
    std::unique_ptr<juce::StretchableLayoutResizerBar> bar;

    juce::TextButton sendBtn { "send to SP-1" }, soundBtn { "sound" }, undoBtn { "undo" }, redoBtn { "redo" };
    PlaceBrowser browser;
    Inspector inspector { doc, host };
    juce::Rectangle<int> pillArea, marginArea, stripArea, contentArea, helpArea;
    juce::TextButton kindBtn[4], viewBtn[3], addSceneBtn { "+ scene" };
    juce::OwnedArray<juce::TextButton> sceneBtns;
    juce::Rectangle<int> meters, statusArea, sceneArea, linkArea;   // linkArea: is the SP-1 there
    juce::uint32 handsSeen = 0, handsAt = 0;                         // the last hands report, and when

    int view = 1;            // 0 journal, 1 both, 2 tapestry
    juce::String statusText, lastCode, lastMessages;
    juce::uint32 typedAt = 0;
    bool pendingTyping = false;
    bool sendAnyway = false;
    juce::uint32 nights = 0;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::ApplicationProperties props;
    std::unique_ptr<juce::LookAndFeel_V4> laf;
};
