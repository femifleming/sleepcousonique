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

    void setText (const juce::String&);        // keeps the caret and scroll where they were
    juce::String getText() const { return code.getAllContent(); }
    void showLine (int line1);
    void setMessages (const std::vector<scq::Message>&, const juce::String& banner);
    bool editorHasFocus() const { return editor.hasKeyboardFocus (true); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void codeDocumentTextInserted (const juce::String&, int) override { if (! settingText && onTyped) onTyped(); }
    void codeDocumentTextDeleted (int, int) override { if (! settingText && onTyped) onTyped(); }

    juce::CodeDocument code;
    DreamTokeniser tokeniser;
    juce::CodeEditorComponent editor { code, &tokeniser };
    std::vector<scq::Message> messages;
    juce::String banner;
    bool settingText = false;
    juce::Rectangle<int> msgArea;
};
