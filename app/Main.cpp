#include "Scales.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include "MainComponent.h"
#include "Art.h"
#include "Serial.h"

class SleepcousoniqueApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Sleepcousonique"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override
    {
        // the test modes run beside an open window; otherwise one Sleepcousonique at a time
        auto args = getCommandLineParameters();
        return args.contains ("--send") || args.contains ("--shot");
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
        window = std::make_unique<Window>();
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
                juce::Timer::callAfterDelay (700, [this, t] { window->main->themeForShot (t[0] != "journal", t[1] != "light"); });
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
        auto f = juce::File::getCurrentWorkingDirectory().getChildFile (commandLine.unquoted().trim());
        if (commandLine.isNotEmpty() && f.existsAsFile()) window->main->openFile (f);
    }

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

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        auto f = juce::File (commandLine.unquoted().trim());
        if (window && f.existsAsFile()) window->main->openFile (f);
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
