#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "Model.h"
#include "Host.h"
#include "Journal.h"
#include "Tapestry.h"
#include "Hands.h"

class MainComponent : public juce::Component,
                      public juce::DragAndDropContainer,
                      public juce::MenuBarModel,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int, const juce::String&) override;
    void menuItemSelected (int, int) override;

    void openFile (const juce::File&);
    void showView (int v) { setView (v); }
    bool askToKeepChanges (std::function<void()> then);
    std::function<void (const juce::String&)> onTitle;

private:
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
    Journal journal;
    Tapestry tapestry { doc, host };
    Hands hands { host };
    juce::StretchableLayoutManager split;
    std::unique_ptr<juce::StretchableLayoutResizerBar> bar;

    juce::TextButton sendBtn { "send to SP-1" }, soundBtn { "sound" };
    juce::TextButton kindBtn[4], viewBtn[3], addSceneBtn { "+ scene" };
    juce::OwnedArray<juce::TextButton> sceneBtns;
    juce::Rectangle<int> meters, statusArea, sceneArea;

    int view = 1;            // 0 journal, 1 both, 2 tapestry
    juce::String statusText, lastCode, lastMessages;
    juce::uint32 typedAt = 0;
    bool pendingTyping = false;
    juce::uint32 nights = 0;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::ApplicationProperties props;
    std::unique_ptr<juce::LookAndFeel_V4> laf;
};
