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
        static const StringArray words { "the", "The", "is", "are", "lucid", "drift", "value", "bottle", "sleepers" };
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
    auto& t = art::theme();
    CodeEditorComponent::ColourScheme s;
    s.set ("plain",   art::ink);
    s.set ("note",    t.note);
    s.set ("date",    t.date);
    s.set ("word",    t.word);
    s.set ("place",   t.place);
    s.set ("passage", t.passage);
    s.set ("number",  t.number);
    s.set ("map",     t.map);
    s.set ("hand",    t.hand);
    return s;
}

Journal::Journal()
{
    code.addListener (this);
    editor.setFont (art::mono (14.5f));
    applyTheme();
    editor.setLineNumbersShown (true);
    editor.undo = [this] (bool redo) { if (onUndo) onUndo (redo); };
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

void Journal::applyTheme()
{
    auto& t = art::theme();
    editor.setColour (CodeEditorComponent::backgroundColourId, art::paper);
    editor.setColour (CodeEditorComponent::defaultTextColourId, art::ink);
    editor.setColour (CodeEditorComponent::lineNumberBackgroundId, t.gutter);
    editor.setColour (CodeEditorComponent::lineNumberTextId, t.gutterText);
    editor.setColour (CodeEditorComponent::highlightColourId, art::gold.withAlpha (0.3f));
    editor.setColour (CaretComponent::caretColourId, art::ink);
    editor.setColour (ScrollBar::thumbColourId, t.dark ? Colour (0x40ffffff) : Colour (0x40000000));
    editor.setColourScheme (tokeniser.getDefaultColourScheme());
    repaint();
}

Range<int> Journal::selectedLines() const
{
    auto sel = editor.getHighlightedRegion();
    if (sel.isEmpty()) return {};
    CodeDocument::Position a (code, sel.getStart()), b (code, jmax (sel.getStart(), sel.getEnd() - 1));
    return { a.getLineNumber() + 1, b.getLineNumber() + 2 };      // end exclusive
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
    g.setColour (art::theme().gutter);
    g.fillRect (msgArea);
    g.setColour (art::theme().line);
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
        g.setColour (m.error ? art::theme().sound : art::theme().date);
        g.drawText ((m.error ? "line " : "line ") + String (m.line) + "   " + m.text, r.removeFromTop (18), Justification::left);
    }
}

void Journal::mouseDown (const MouseEvent& e)
{
    if (! msgArea.contains (e.getPosition())) return;
    int row = (e.y - msgArea.getY() - 5) / 18 - (banner.isNotEmpty() ? 1 : 0);
    if (row >= 0 && row < (int) messages.size() && onPickMessage) onPickMessage (messages[(size_t) row].line);
}
