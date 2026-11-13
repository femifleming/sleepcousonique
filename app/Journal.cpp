#include "Journal.h"
#include "Art.h"

using namespace juce;

static bool isPlaceWord (const String& w)
{
    if (scq::findPlace (w)) return true;
    auto base = w.trimCharactersAtEnd ("0123456789");
    return base != w && scq::findPlace (base) != nullptr;
}

int DreamTokeniser::readNextToken (CodeDocument::Iterator& it)
{
    auto c = it.peekNextChar();
    if (c == 0) { it.skip(); return Plain; }
    if (CharacterFunctions::isWhitespace (c)) { it.skipWhitespace(); return Plain; }
    if (c == ';')
    {
        it.skip();
        bool map = it.peekNextChar() == '@';
        while (! it.isEOF() && it.peekNextChar() != '\n' && it.peekNextChar() != '\r') it.skip();
        return map ? Map : Note;
    }
    if (c == '>' )
    {
        it.skip();
        if (it.peekNextChar() == '|') it.skip();
        return Passage;
    }
    if (c == '-' || c == '~' || c == '=')
    {
        // an arrow (-> ~40> =>), a line of dashes, or a negative number
        CodeDocument::Iterator look (it);
        look.skip();
        int dashes = c == '-' ? 1 : 0;
        while (look.peekNextChar() == '-') { look.skip(); ++dashes; }
        if (dashes >= 3) { while (it.peekNextChar() == '-') it.skip(); return Date; }
        CodeDocument::Iterator a (it);
        a.skip();
        while (CharacterFunctions::isDigit (a.peekNextChar()) || a.peekNextChar() == '.') a.skip();
        if (a.peekNextChar() == '>')
        {
            it.skip();
            while (CharacterFunctions::isDigit (it.peekNextChar()) || it.peekNextChar() == '.') it.skip();
            it.skip();
            return Passage;
        }
        it.skip();
        if (c == '-' && CharacterFunctions::isDigit (it.peekNextChar()))
        {
            while (CharacterFunctions::isDigit (it.peekNextChar()) || it.peekNextChar() == '.') it.skip();
            return Number;
        }
        return Plain;
    }
    if (CharacterFunctions::isDigit (c) || c == '.')
    {
        bool dash = false;
        while (CharacterFunctions::isDigit (it.peekNextChar()) || it.peekNextChar() == '.' || it.peekNextChar() == '-')
        {
            if (it.peekNextChar() == '-')
            {
                CodeDocument::Iterator a (it);
                a.skip();
                if (! CharacterFunctions::isDigit (a.peekNextChar())) break;
                dash = true;
            }
            it.skip();
        }
        return dash ? Date : Number;
    }
    if (CharacterFunctions::isLetter (c) || c == '_')
    {
        String w;
        while (CharacterFunctions::isLetterOrDigit (it.peekNextChar()) || it.peekNextChar() == '_') w << it.nextChar();
        if (it.peekNextChar() == '.')   // name.port
        {
            it.skip();
            while (CharacterFunctions::isLetterOrDigit (it.peekNextChar()) || it.peekNextChar() == '_') it.skip();
        }
        static const StringArray words { "the", "The", "is", "are", "lucid", "drift", "bottle", "sleepers" };
        if (words.contains (w)) return Word;
        if (auto* p = scq::findPlace (w.trimCharactersAtEnd ("0123456789")))
            return p->family >= scq::Hands ? Hand : Place;
        if (isPlaceWord (w)) return Place;
        return Plain;
    }
    it.skip();
    return Plain;
}

CodeEditorComponent::ColourScheme DreamTokeniser::getDefaultColourScheme()
{
    CodeEditorComponent::ColourScheme s;
    s.set ("plain",   art::ink);
    s.set ("note",    Colour (0xff8a8578));
    s.set ("date",    art::umber);
    s.set ("word",    Colour (0xff6b5f8a));
    s.set ("place",   art::blue);
    s.set ("passage", Colour (0xffa87a12));
    s.set ("number",  art::copper.darker (0.2f));
    s.set ("map",     Colour (0xffcfc9bb));
    s.set ("hand",    Colour (0xff3d7a4e));
    return s;
}

Journal::Journal()
{
    code.addListener (this);
    editor.setFont (art::mono (14.5f));
    editor.setColour (CodeEditorComponent::backgroundColourId, art::paper);
    editor.setColour (CodeEditorComponent::defaultTextColourId, art::ink);
    editor.setColour (CodeEditorComponent::lineNumberBackgroundId, Colour (0xfff2ede0));
    editor.setColour (CodeEditorComponent::lineNumberTextId, Colour (0xffb0a890));
    editor.setColour (CodeEditorComponent::highlightColourId, art::gold.withAlpha (0.3f));
    editor.setColour (CaretComponent::caretColourId, art::ink);
    editor.setColour (ScrollBar::thumbColourId, Colour (0x40000000));
    editor.setLineNumbersShown (true);
    editor.setScrollbarThickness (10);
    editor.setTabSize (4, true);
    addAndMakeVisible (editor);
}

Journal::~Journal() { code.removeListener (this); }

void Journal::setText (const String& t)
{
    if (t == code.getAllContent()) return;
    auto caret = editor.getCaretPos().getPosition();
    auto top = editor.getFirstLineOnScreen();
    settingText = true;
    code.replaceAllContent (t);
    settingText = false;
    editor.scrollToLine (top);
    editor.moveCaretTo (CodeDocument::Position (code, jmin (caret, code.getNumCharacters())), false);
}

void Journal::showLine (int line1)
{
    if (line1 < 1) return;
    CodeDocument::Position p (code, line1 - 1, 0);
    editor.scrollToKeepLinesOnScreen ({ jmax (0, line1 - 4), line1 + 4 });
    editor.moveCaretTo (p, false);
    editor.moveCaretTo (CodeDocument::Position (code, line1 - 1, code.getLine (line1 - 1).length()), true);
    editor.grabKeyboardFocus();
}

void Journal::setMessages (const std::vector<scq::Message>& m, const String& b)
{
    messages = m;
    banner = b;
    resized();
    repaint();
}

void Journal::resized()
{
    auto r = getLocalBounds();
    int n = jmin (6, (int) messages.size()) + (banner.isNotEmpty() ? 1 : 0);
    msgArea = r.removeFromBottom (n > 0 ? 10 + n * 18 : 0);
    editor.setBounds (r);
}

void Journal::paint (Graphics& g)
{
    if (msgArea.isEmpty()) return;
    g.setColour (Colour (0xfff2ede0));
    g.fillRect (msgArea);
    g.setColour (Colour (0x22000000));
    g.drawHorizontalLine (msgArea.getY(), 0, (float) getWidth());
    auto r = msgArea.reduced (10, 5);
    g.setFont (art::mono (12));
    if (banner.isNotEmpty())
    {
        g.setColour (art::umber);
        g.drawText (banner, r.removeFromTop (18), Justification::left);
    }
    for (int i = 0; i < jmin (6, (int) messages.size()); ++i)
    {
        auto& m = messages[(size_t) i];
        g.setColour (m.error ? Colour (0xffa3301f) : Colour (0xff8a6a12));
        g.drawText ((m.error ? "line " : "line ") + String (m.line) + "   " + m.text, r.removeFromTop (18), Justification::left);
    }
}

void Journal::mouseDown (const MouseEvent& e)
{
    if (! msgArea.contains (e.getPosition())) return;
    int row = (e.y - msgArea.getY() - 5) / 18 - (banner.isNotEmpty() ? 1 : 0);
    if (row >= 0 && row < (int) messages.size() && onPickMessage) onPickMessage (messages[(size_t) row].line);
}
