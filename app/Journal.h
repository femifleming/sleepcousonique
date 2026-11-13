#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "Model.h"

// The journal: the patch as plain text. Type here, or paste here, or never look at it.
class DreamTokeniser : public juce::CodeTokeniser
{
public:
    enum { Plain, Note, Date, Word, Place, Passage, Number, Map, Hand };
    int readNextToken (juce::CodeDocument::Iterator&) override;
    juce::CodeEditorComponent::ColourScheme getDefaultColourScheme() override;
};

class Journal : public juce::Component, private juce::CodeDocument::Listener
{
public:
    Journal();
    ~Journal() override;

    std::function<void()> onTyped;
    std::function<void (int line)> onPickMessage;
    std::function<void (bool redo)> onUndo;       // cmd-Z / shift-cmd-Z: the app's one history

    void setText (const juce::String&);        // keeps the caret and scroll where they were
    juce::String getText() const { return code.getAllContent(); }
    void showLine (int line1);
    void setMessages (const std::vector<scq::Message>&, const juce::String& banner);
    bool editorHasFocus() const { return editor.hasKeyboardFocus (true); }
    juce::Range<int> selectedLines() const;
    void applyTheme();                           // colours from art::theme()      // 1-based, inclusive; empty when nothing is selected

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void codeDocumentTextInserted (const juce::String&, int) override { if (! settingText && onTyped) onTyped(); }
    void codeDocumentTextDeleted (int, int) override { if (! settingText && onTyped) onTyped(); }

    juce::CodeDocument code;
    DreamTokeniser tokeniser;
    // the editor, but cmd-Z goes to the app's history (tapestry edits and typing in one place)
    struct Editor : juce::CodeEditorComponent
    {
        using juce::CodeEditorComponent::CodeEditorComponent;
        std::function<void (bool)> undo;
        bool keyPressed (const juce::KeyPress& k) override
        {
            if (undo && k.getModifiers().isCommandDown() && (k.getKeyCode() == 'Z' || k.getKeyCode() == 'z'))
            { undo (k.getModifiers().isShiftDown()); return true; }
            return juce::CodeEditorComponent::keyPressed (k);
        }
    };
    Editor editor { code, &tokeniser };
    std::vector<scq::Message> messages;
    juce::String banner;
    bool settingText = false;
    juce::Rectangle<int> msgArea;
};
