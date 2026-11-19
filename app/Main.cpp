#include "Scales.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include "MainComponent.h"
#include "Art.h"
#include "Serial.h"
#include "scq.h"

class SleepcousoniqueApp : public juce::JUCEApplication
{
public:
   #if SCQ_LEGACY
    const juce::String getApplicationName() override { return "Sleepcousonique Legacy"; }
   #else
    const juce::String getApplicationName() override { return "Sleepcousonique"; }
   #endif
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override
    {
        // the test modes run beside an open window; otherwise one Sleepcousonique at a time
        auto args = getCommandLineParameters();
        return args.contains ("--send") || args.contains ("--shot") || args.contains ("--storm");
    }

    // --storm report.txt file.dream PLACE INLET AMOUNT: a test of the whole program, sound card
    // and all. Opens the dream, then does what a hand does: turns PLACE's INLET attenuverter to
    // AMOUNT (as a drag on the board), throws fader1 between its ends for three seconds (as the
    // hands panel), turns the attenuverter back to 1, makes an edit's reload, and writes what
    // it saw: the output level and PLACE's first outlet before, during and after, and how many
    // places the engine had to start again.
    void storm (const juce::StringArray& args, int at)
    {
        auto out = juce::File::getCurrentWorkingDirectory().getChildFile (args[at + 1].unquoted());
        window->main->openFile (juce::File::getCurrentWorkingDirectory().getChildFile (args[at + 2].unquoted()));
        auto place = args[at + 3], inlet = args[at + 4];
        float amount = args[at + 5].getFloatValue();
        struct Watch : juce::Timer
        {
            MainComponent* main = nullptr;
            juce::File out;
            juce::String place, inlet, text;
            float amount = 0;
            int tick = 0;
            float level[3] {}, outlet[3] {};
            void timerCallback() override
            {
                auto& host = main->engine();
                int phase = tick < 100 ? 0 : (tick < 250 ? 1 : 2);          // 2 s before, 3 s storm, then after
                if (tick == 0) host.control (SCQ_CTL_LEAN1, 0.3f);
                if (tick == 100) text << "attenuverter turned: " << (main->turnAtt (place, inlet, amount) ? "yes" : "NO (no such place or inlet)") << "\n";
                if (phase == 1 && tick % 6 == 0) host.control (SCQ_CTL_LEAN1, (tick / 6) % 2 ? 0.0f : 1.0f);
                if (tick == 250) { host.control (SCQ_CTL_LEAN1, 0.3f); main->turnAtt (place, inlet, 1.0f); main->reload(); }
                if (phase < 2 || tick >= 350)                                // (after: once it has had 2 s to settle)
                {
                    level[phase] = juce::jmax (level[phase], host.level);
                    auto d = host.describe().describe;
                    for (auto& line : juce::StringArray::fromLines (d))
                        if (line.startsWith ("N ") && line.contains (" " + place + " "))
                        {
                            float v = std::abs (host.probe (line.substring (2).getIntValue(), 0));
                            if (! std::isfinite (v)) v = 1.0e30f;
                            outlet[phase] = juce::jmax (outlet[phase], v);
                            break;
                        }
                }
                if (++tick >= 500)
                {
                    stopTimer();
                    static const char* const names[3] = { "before", "during", "after " };
                    for (int k = 0; k < 3; ++k)
                        text << names[k] << ": output level " << juce::String (level[k], 3) << ", " << place << " outlet up to " << juce::String (outlet[k], 3) << "\n";
                    text << "places started again by the engine: " << (int) host.healed() << "\n";
                    text << "room: " << (host.roomNote().isEmpty() ? juce::String ("(no crowd in this dream)") : host.roomNote()) << "\n";
                    out.replaceWithText (text);
                    juce::JUCEApplication::quit();
                }
            }
        };
        auto* w = new Watch();                    // lives until the program quits
        w->main = window->main; w->out = out; w->place = place; w->inlet = inlet; w->amount = amount;
        w->startTimerHz (50);
    }

    void initialise (const juce::String& commandLine) override
    {
        auto args = juce::StringArray::fromTokens (commandLine, true);
        int send = args.indexOf ("--send");
        if (send >= 0)
        {
            // --send file.dream: the editor's own "send to SP-1", from the command line
            auto f = juce::File::getCurrentWorkingDirectory().getChildFile (args[send + 1].unquoted());
            // through the live link, exactly as the window's "send to SP-1" does
            serial::Link link;
            for (int i = 0; i < 100 && ! link.connected(); ++i) juce::Thread::sleep (100);
            juce::String err = link.connected() ? link.send (scales::withTunings (f.loadFileAsString()), -1) : juce::String ("no SP-1 found");
            std::printf ("%s\n", err.isEmpty() ? ("sent to " + link.port()).toRawUTF8() : err.toRawUTF8());
            std::fflush (stdout);
            setApplicationReturnValue (err.isEmpty() ? 0 : 1);
            quit();
            return;
        }
        if (args.contains ("--no-link")) serial::Link::enabled = false;   // leave the SP-1's port to whoever has it
        window = std::make_unique<Window>();
        if (args.contains ("--no-radio")) window->main->engine().roomAllowed = false;        // never touch Bluetooth
        if (int pr = args.indexOf ("--pretend"); pr >= 0)                // --pretend 7: a made-up room of seven devices
            window->main->engine().radio.pretend (args[pr + 1].getIntValue());
        if (int st = args.indexOf ("--storm"); st >= 0) { storm (args, st); return; }
        int shot = args.indexOf ("--shot");
        if (shot >= 0)
        {
            // --shot out.png [file.dream]: open, settle, photograph the window, quit
            auto out = juce::File::getCurrentWorkingDirectory().getChildFile (args[shot + 1].unquoted());
            int v = args.indexOf ("--view");
            if (v >= 0) window->main->showView (args[v + 1].getIntValue());
            if (args.size() > shot + 2 && ! args[shot + 2].startsWith ("--")) window->main->openFile (juce::File::getCurrentWorkingDirectory().getChildFile (args[shot + 2].unquoted()));
            if (int lk = args.indexOf ("--look"); lk >= 0)   // --look light,router,hlFirst,hlLast
            {
                auto t = juce::StringArray::fromTokens (args[lk + 1], ",", "");
                juce::Timer::callAfterDelay (600, [this, t] { window->main->lookForShot (t[0].getFloatValue(), t[1] == "1", t[2].getIntValue(), t[3].getIntValue()); });
            }
            if (int th = args.indexOf ("--theme"); th >= 0)   // --theme openprocess|journal,dark|light
            {
                auto t = juce::StringArray::fromTokens (args[th + 1], ",", "");
                juce::Timer::callAfterDelay (700, [this, t] { window->main->themeForShot (t[0] != "journal", t[1] != "light", t[0] == "modular"); });
            }
            int wait = args.contains ("--wait") ? args[args.indexOf ("--wait") + 1].getIntValue() : 2800;
            juce::Timer::callAfterDelay (wait - 800, [this] { window->main->resized(); window->main->repaint(); });
            juce::Timer::callAfterDelay (wait, [this, out]
            {
                auto img = window->main->createComponentSnapshot (window->main->getLocalBounds(), true, 1.0f);
                juce::PNGImageFormat png;
                juce::FileOutputStream os (out);
                if (os.openedOk()) { os.setPosition (0); os.truncate(); png.writeImageToStream (img, os); }
                quit();
            });
            return;
        }
        // a dream to open: one the system handed us before the window was up, the command line's,
        // or the last one you had open
        auto f = firstDream (commandLine);
        if (pending.existsAsFile()) f = pending;
        pending = juce::File();
        if (f.existsAsFile()) window->main->openFile (f);
        else window->main->openLast();
    }

    // "path", "a path with spaces" or several of them, quoted or not
    static juce::File firstDream (const juce::String& commandLine)
    {
        auto words = juce::StringArray::fromTokens (commandLine, true);
        for (auto w : words)
        {
            w = w.unquoted().trim();
            if (w.isEmpty() || w.startsWith ("-")) continue;
            auto f = juce::File::isAbsolutePath (w) ? juce::File (w) : juce::File::getCurrentWorkingDirectory().getChildFile (w);
            if (f.existsAsFile()) return f;
        }
        auto whole = juce::File::getCurrentWorkingDirectory().getChildFile (commandLine.unquoted().trim());
        return commandLine.isNotEmpty() && whole.existsAsFile() ? whole : juce::File();
    }
    juce::File pending;      // a file opened from the Finder before the window existed

    void shutdown() override
    {
       #if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu (nullptr);
       #endif
        window = nullptr;
    }

    void systemRequestedQuit() override
    {
        if (window) window->main->askToKeepChanges ([] { quit(); });
        else quit();
    }

    // the Finder (double-click, Open With, drop on the Dock icon) and a second launch land here,
    // sometimes before initialise() has made the window: keep it until there is one
    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        auto f = firstDream (commandLine);
        if (! f.existsAsFile()) return;
        if (window) window->main->askToKeepChanges ([this, f] { window->main->openFile (f); });
        else pending = f;
    }

    struct Window : juce::DocumentWindow
    {
        Window() : DocumentWindow ("Sleepcousonique", art::paper, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            main = new MainComponent();
            main->onTitle = [this] (const juce::String& t) { setName (t); };
            setContentOwned (main, true);
           #if JUCE_MAC
            juce::MenuBarModel::setMacMainMenu (main);
           #else
            setMenuBar (main);
           #endif
            setResizable (true, true);
            setResizeLimits (900, 600, 4000, 3000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
        MainComponent* main = nullptr;
    };

private:
    std::unique_ptr<Window> window;
};

START_JUCE_APPLICATION (SleepcousoniqueApp)
