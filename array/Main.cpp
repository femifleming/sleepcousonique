// Sleepcousonique Array: several SP-1s in interface mode as one multichannel output.
//
// Every SP-1 plugged in is two outputs. The Array joins them (and a loopback input such as
// BlackHole 16ch) into one Core Audio device, then routes any source -- a loopback channel or
// a channel of a multichannel WAV -- to any SP-1 output, through one master volume. Units are
// told their number over USB serial, so each one shows it on its LEDs.
//
// The look is faltung's: a flat field, thin-line geometry, isometric folded blocks, slanted
// parallelogram controls, Oval-coloured lines. Each SP-1 is a block on the stage whose two
// towers rise with its outputs; a wireframe polyhedron behind them breathes with the whole.
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#if ! SCQ_LEGACY
 #include <juce_dsp/juce_dsp.h>
#endif
#include "Aggregate.h"
#include "../app/Serial.h"
#include "ArrayFonts.h"

using namespace juce;
static const String DOT = String::fromUTF8 ("  \xc2\xb7  ");

// ---------------------------------------------------------------------------- look (faltung dark)
namespace look
{
// two looks: faltung (Partials' field) and modular (the 2004 patch editor); apply() switches
inline Colour field, deep, panel, ink, dim, rule, fill, track, hot, warn;
inline Colour lines[8];
inline bool modular = false;
inline Image marble;
static void apply (bool mod)
{
    modular = mod;
    auto c = [] (uint32 v) { return Colour (v); };
    if (! mod)
    {
        field = c (0xff1b2731); deep = c (0xff141e26); panel = c (0xff22313d); ink = c (0xffe9eef0); dim = c (0xff8ea0ad);
        rule = c (0xff2c3b47); fill = c (0xff5aaad8); track = c (0xff3c4d5a); hot = c (0xfff3d34a); warn = c (0xffe2793a);
        const uint32 l[8] = { 0xffe2793a, 0xff5aaad8, 0xffa07ad0, 0xffe89aa8, 0xff9fd08a, 0xfff3d34a, 0xff7fc4c0, 0xffd8d0b8 };
        for (int i = 0; i < 8; ++i) lines[i] = c (l[i]);
    }
    else
    {
        field = c (0xffd4d0c8); deep = c (0xffb4b4b7); panel = c (0xffffffff); ink = c (0xff000000); dim = c (0xff404040);
        rule = c (0xff808080); fill = c (0xff2a3ee8); track = c (0xff808080); hot = c (0xffb08800); warn = c (0xffe01818);
        const uint32 l[8] = { 0xff70d8d0, 0xffe080d8, 0xff90d070, 0xffe8dc50, 0xffa8a0e8, 0xfff0a070, 0xffd8b8e8, 0xff7fb8e8 };
        for (int i = 0; i < 8; ++i) lines[i] = c (l[i]);
        if (! marble.isValid())                  // grey marble, streaked on a slant
        {
            marble = Image (Image::RGB, 800, 500, true);
            Graphics g (marble);
            Random r (11);
            g.fillAll (deep);
            for (int i = 0; i < 700; ++i)
            {
                g.setColour ((r.nextBool() ? Colours::white : Colours::black).withAlpha (0.02f + 0.04f * r.nextFloat()));
                g.fillEllipse (r.nextFloat() * 800, r.nextFloat() * 500, 20 + 70 * r.nextFloat(), 6 + 12 * r.nextFloat());
            }
            for (int i = 0; i < 120; ++i)
            {
                Path v; float x = r.nextFloat() * 1000 - 200, y = r.nextFloat() * 500;
                v.startNewSubPath (x, y);
                for (int k = 0; k < 10; ++k) { x += 20 + 40 * r.nextFloat(); y += 2 - 12 * r.nextFloat(); v.lineTo (x, y); }
                g.setColour ((r.nextFloat() < 0.7f ? Colours::white : Colour (0xff6a6a70)).withAlpha (0.1f + 0.25f * r.nextFloat()));
                g.strokePath (v, PathStrokeType (0.6f + r.nextFloat()));
            }
        }
    }
}
static void bevel (Graphics& g, Rectangle<float> r, bool raised)
{
    g.setColour (raised ? Colours::white : Colour (0xff808080));
    g.fillRect (r.getX(), r.getY(), r.getWidth(), 1.0f); g.fillRect (r.getX(), r.getY(), 1.0f, r.getHeight());
    g.setColour (raised ? Colour (0xff808080) : Colours::white);
    g.fillRect (r.getX(), r.getBottom() - 1, r.getWidth(), 1.0f); g.fillRect (r.getRight() - 1, r.getY(), 1.0f, r.getHeight());
}
static Typeface::Ptr sans, sansMed, mono;
static Font f (float h, bool m = false)
{
    if (modular) return FontOptions ("Tahoma", h * 0.98f, Font::plain);
    return FontOptions (m ? mono : sans).withHeight (h);
}
static Font fm (float h)
{
    if (modular) return FontOptions ("Tahoma", h * 0.92f, Font::bold);
    return FontOptions (sansMed).withHeight (h);
}
static Colour unitColour (int u) { return lines[u % 8]; }

static Path pg (Rectangle<float> r, float k = -1)        // the slanted parallelogram (modular: a plain box)
{
    if (k < 0) k = r.getHeight() * 0.45f;
    if (modular) k = 0;
    Path p;
    p.startNewSubPath (r.getX() + k, r.getY());
    p.lineTo (r.getRight(), r.getY());
    p.lineTo (r.getRight() - k, r.getBottom());
    p.lineTo (r.getX(), r.getBottom());
    p.closeSubPath();
    return p;
}
static Path quad (Point<float> a, Point<float> b, Point<float> c, Point<float> d)
{
    Path p; p.startNewSubPath (a); p.lineTo (b); p.lineTo (c); p.lineTo (d); p.closeSubPath(); return p;
}
static void cube (Graphics& g, Point<float> c, float r)
{
    g.setColour (Colour (0xffffe066));
    g.fillPath (quad ({ c.x, c.y - r }, { c.x + r * 0.87f, c.y - r / 2 }, c, { c.x - r * 0.87f, c.y - r / 2 }));
    g.setColour (Colour (0xfff2c200));
    g.fillPath (quad ({ c.x - r * 0.87f, c.y - r / 2 }, c, { c.x, c.y + r }, { c.x - r * 0.87f, c.y + r / 2 }));
    g.setColour (Colour (0xffd9a800));
    g.fillPath (quad ({ c.x + r * 0.87f, c.y - r / 2 }, c, { c.x, c.y + r }, { c.x + r * 0.87f, c.y + r / 2 }));
}
static float db (float lin) { return lin > 1.0e-6f ? 20.0f * std::log10 (lin) : -120.0f; }
static float meterPos (float lin)                         // -60..+6 dB onto 0..1
{
    return jlimit (0.0f, 1.0f, (db (lin) + 60.0f) / 66.0f);
}

struct Look : LookAndFeel_V4
{
    Look() { refresh(); }
    void refresh()
    {
        setColour (ResizableWindow::backgroundColourId, field);
        setColour (PopupMenu::backgroundColourId, panel);
        setColour (PopupMenu::textColourId, ink);
        setColour (PopupMenu::highlightedBackgroundColourId, fill);
        setColour (PopupMenu::highlightedTextColourId, field);
        setColour (ComboBox::backgroundColourId, field);
        setColour (ComboBox::textColourId, ink);
        setColour (ComboBox::outlineColourId, track);
        setColour (ComboBox::arrowColourId, dim);
        setColour (TooltipWindow::backgroundColourId, panel);
        setColour (TooltipWindow::textColourId, ink);
    }
    Typeface::Ptr getTypefaceForFont (const Font& fnt) override { return modular ? LookAndFeel_V4::getTypefaceForFont (fnt) : mono; }
    void drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        if (modular)                             // Windows: raised grey; on is cyan and sunken
        {
            const bool on = b.getToggleState() || down;
            g.setColour (on ? Colour (0xff00ffff) : (over ? Colour (0xffe0ddd6) : field)); g.fillRect (r);
            g.setColour (Colours::black); g.drawRect (r, 1.0f);
            bevel (g, r.reduced (1.0f), ! on);
            return;
        }
        auto p = pg (r);
        bool on = b.getToggleState();
        if (on || down) { g.setColour (fill); g.fillPath (p); }
        else if (over) { g.setColour (rule); g.fillPath (p); }
        g.setColour (on ? fill : ink.withAlpha (0.75f));
        g.strokePath (p, PathStrokeType (1.0f));
    }
    void drawButtonText (Graphics& g, TextButton& b, bool, bool) override
    {
        g.setFont (f (11.5f, true));
        g.setColour (modular ? ink : (b.getToggleState() ? field : ink));
        g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
    }
    void drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox&) override
    {
        if (modular)
        {
            Rectangle<float> r (0, 0, (float) w, (float) h);
            g.setColour (Colours::white); g.fillRect (r); bevel (g, r, false);
            auto bx = Rectangle<float> ((float) w - 18, 2, 16, (float) h - 4);
            g.setColour (field); g.fillRect (bx); bevel (g, bx, true);
            Path tri; tri.addTriangle (bx.getCentreX() - 4, bx.getCentreY() - 2, bx.getCentreX() + 4, bx.getCentreY() - 2, bx.getCentreX(), bx.getCentreY() + 3);
            g.setColour (Colours::black); g.fillPath (tri);
            return;
        }
        auto p = pg ({ 1.0f, 1.0f, (float) w - 2, (float) h - 2 });
        g.setColour (ink.withAlpha (0.75f)); g.strokePath (p, PathStrokeType (1.0f));
        g.setColour (dim); g.drawText ("v", w - 22, 0, 14, h, Justification::centred);
    }
    Font getComboBoxFont (ComboBox&) override { return f (11.5f, true); }
    Font getPopupMenuFont() override { return f (13.0f, true); }
    void positionComboBoxText (ComboBox& b, Label& l) override { l.setBounds (14, 1, b.getWidth() - 36, b.getHeight() - 2); l.setFont (f (11.5f, true)); }
};
}

// ---------------------------------------------------------------------------- the audio
struct Engine : AudioIODeviceCallback
{
    static constexpr int kMaxOut = 64, kIns = 16, kFile = 16, kScope = 256;   // sources: in 1-16, file 1-16
    std::atomic<uint32_t> route[kMaxOut] {};                    // per output: bit per source
    std::atomic<float> outLevel[kMaxOut] {}, outRms[kMaxOut] {}, srcLevel[kIns + kFile] {};
    std::atomic<int> sp1Outs { 0 }, devIns { 0 }, calls { 0 };
    std::atomic<float> master { 1.0f };                         // linear gain on every output
    std::atomic<bool> masterMute { false };
    float gainNow = 1.0f;

    // the last samples of every output, for the spectrograms (read by the drawing, unlocked:
    // a torn frame only blurs one column)
    static constexpr int kSpecRing = 2048;
    float specRing[kMaxOut][kSpecRing] {};
    std::atomic<int> specAt { 0 };

    // a short scope per unit (L/R pairs, decimated by 4) for the phase figures
    float scope[kMaxOut / 2][kScope][2] {};
    std::atomic<int> scopeAt[kMaxOut / 2] {};

    AudioFormatManager formats;
    TimeSliceThread reader { "array file" };
    std::unique_ptr<AudioFormatReaderSource> fileSrc;
    AudioTransportSource transport;
    std::atomic<int> fileChans { 0 };
    CriticalSection fileLock;
    AudioBuffer<float> fileBuf { kFile, 4096 };

    Engine() { formats.registerBasicFormats(); reader.startThread(); }
    ~Engine() override { transport.setSource (nullptr); reader.stopThread (500); }

    String openFile (const File& f)
    {
        auto* r = formats.createReaderFor (f);
        if (r == nullptr) return "can't read " + f.getFileName();
        auto chans = (int) r->numChannels;
        auto sr = r->sampleRate;
        auto src = std::make_unique<AudioFormatReaderSource> (r, true);
        const ScopedLock sl (fileLock);
        transport.setSource (nullptr);
        fileSrc = std::move (src);
        transport.setSource (fileSrc.get(), 48000, &reader, sr, jmin (chans, (int) kFile));
        fileChans = jmin (chans, (int) kFile);
        return {};
    }
    void setLoop (bool l) { const ScopedLock sl (fileLock); if (fileSrc) fileSrc->setLooping (l); }
    double filePos() const { return transport.getCurrentPosition(); }
    double fileLen() const { return transport.getLengthInSeconds(); }

    void audioDeviceAboutToStart (AudioIODevice* d) override
    {
        const ScopedLock sl (fileLock);
        transport.prepareToPlay (d->getCurrentBufferSizeSamples(), d->getCurrentSampleRate());
        fileBuf.setSize (kFile, jmax (4096, d->getCurrentBufferSizeSamples()));
    }
    void audioDeviceStopped() override { const ScopedLock sl (fileLock); transport.releaseResources(); }

    void audioDeviceIOCallbackWithContext (const float* const* in, int numIns, float* const* out, int numOuts, int n,
                                           const AudioIODeviceCallbackContext&) override
    {
        for (int c = 0; c < numOuts; ++c) if (out[c]) FloatVectorOperations::clear (out[c], n);
        devIns = numIns;
        calls = calls + 1;
        int fc = 0;
        {
            const ScopedTryLock sl (fileLock);
            if (sl.isLocked() && fileSrc != nullptr)
            {
                fc = fileChans.load();
                AudioBuffer<float> view (fileBuf.getArrayOfWritePointers(), jmax (1, fc), n);
                view.clear();
                AudioSourceChannelInfo info (&view, 0, n);
                transport.getNextAudioBlock (info);
            }
        }
        auto src = [&] (int s) -> const float*
        {
            if (s < kIns) return s < numIns ? in[s] : nullptr;
            int k = s - kIns;
            return k < fc ? fileBuf.getReadPointer (k) : nullptr;
        };
        for (int s = 0; s < kIns + kFile; ++s)
            if (auto* p = src (s)) srcLevel[s] = jmax (FloatVectorOperations::findMaximum (p, n), -FloatVectorOperations::findMinimum (p, n), srcLevel[s] * 0.85f);
            else srcLevel[s] = 0.0f;

        // the master: ramped across the block, so moving it never clicks
        const float target = masterMute.load() ? 0.0f : master.load();
        const float g0 = gainNow, g1 = target;
        gainNow = target;

        const int outs = jmin (numOuts, sp1Outs.load(), (int) kMaxOut);   // only the SP-1s: never the loopback's own outputs
        for (int o = 0; o < outs; ++o)
        {
            auto mask = route[o].load();
            if (! out[o]) continue;
            for (int s = 0; mask != 0 && s < kIns + kFile; ++s, mask >>= 1)
                if ((mask & 1u) != 0) if (auto* p = src (s)) FloatVectorOperations::add (out[o], p, n);
            if (g0 == g1) FloatVectorOperations::multiply (out[o], g1, n);
            else for (int i = 0; i < n; ++i) out[o][i] *= g0 + (g1 - g0) * (float) i / (float) n;
            auto pk = jmax (FloatVectorOperations::findMaximum (out[o], n), -FloatVectorOperations::findMinimum (out[o], n));
            outLevel[o] = jmax (pk, outLevel[o] * 0.86f);
            float ss = 0; for (int i = 0; i < n; ++i) ss += out[o][i] * out[o][i];
            outRms[o] = outRms[o] * 0.8f + 0.2f * std::sqrt (ss / (float) jmax (1, n));
        }
       #if ! SCQ_LEGACY                     // legacy: no spectrograms and no left/right figures
        {
            const int at = specAt.load();
            for (int o = 0; o < outs; ++o)
                if (out[o])
                    for (int i = 0; i < n; ++i) specRing[o][(at + i) & (kSpecRing - 1)] = out[o][i];
            specAt = (at + n) & (kSpecRing - 1);
        }
        for (int u = 0; u * 2 + 1 < outs; ++u)
        {
            int at = scopeAt[u].load();
            for (int i = 0; i < n; i += 4)
            {
                scope[u][at][0] = out[u * 2] ? out[u * 2][i] : 0.0f;
                scope[u][at][1] = out[u * 2 + 1] ? out[u * 2 + 1][i] : 0.0f;
                at = (at + 1) % kScope;
            }
            scopeAt[u] = at;
        }
       #endif
    }
};

// ---------------------------------------------------------------------------- the master fader
struct MasterFader : Component, SettableTooltipClient
{
    std::function<void (float)> onChange;        // dB
    float dB = 0.0f;
    static constexpr float kMin = -60.0f, kMax = 6.0f;
    float pos() const { return dB <= kMin ? 0.0f : (dB - kMin) / (kMax - kMin); }
    void set (float d, bool notify = true) { dB = jlimit (kMin, kMax, d); if (notify && onChange) onChange (dB); repaint(); }
    void mouseDown (const MouseEvent& e) override { drag (e); }
    void mouseDrag (const MouseEvent& e) override { drag (e); }
    void mouseDoubleClick (const MouseEvent&) override { set (0.0f); }
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override { set (dB + w.deltaY * 6.0f); }
    void drag (const MouseEvent& e)
    {
        auto r = getLocalBounds().toFloat().reduced (0, 12);
        float p = 1.0f - (e.position.y - r.getY()) / r.getHeight();
        set (kMin + jlimit (0.0f, 1.0f, p) * (kMax - kMin));
    }
    void paint (Graphics& g) override
    {
        using namespace look;
        auto r = getLocalBounds().toFloat().reduced (0, 12);
        const float k = 12.0f, w = r.getWidth() - k;
        // the track: a tall slanted column
        g.setColour (track);
        g.strokePath (quad ({ r.getX() + k, r.getY() }, { r.getX() + k + w, r.getY() }, { r.getX() + w, r.getBottom() }, { r.getX(), r.getBottom() }), PathStrokeType (1.0f));
        // dB ticks
        g.setFont (f (9.0f, true));
        for (float d : { 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -40.0f, -60.0f })
        {
            float y = r.getBottom() - (d - kMin) / (kMax - kMin) * r.getHeight();
            float sl = k * (1.0f - (y - r.getY()) / r.getHeight());
            g.setColour (d == 0.0f ? hot : dim.withAlpha (0.6f));
            g.drawHorizontalLine ((int) y, r.getX() + sl - 5, r.getX() + sl);
        }
        // the fill up to the value
        float fy = r.getBottom() - pos() * r.getHeight();
        float ks = k * pos();
        g.setColour (dB > 0.05f ? warn : fill);
        g.fillPath (quad ({ r.getX() + ks, fy }, { r.getX() + ks + w, fy }, { r.getX() + w, r.getBottom() }, { r.getX(), r.getBottom() }));
        g.setColour (ink);
        g.drawLine (r.getX() + ks - 6, fy, r.getX() + ks + w + 6, fy, 2.0f);
    }
};

// ---------------------------------------------------------------------------- one SP-1
struct Unit
{
    String serial, uid, port, mode;   // mode: "IFACE", "DREAM", or "" (no serial answer)
    int number = 0;                   // what it says it is
};

// ---------------------------------------------------------------------------- the window
struct ArrayComponent : Component, Timer
{
    look::Look lf;
    AudioDeviceManager devices;
    Engine engine;
    std::vector<Unit> units;
    std::vector<agg::Device> loops;
    PropertiesFile::Options popts;
    std::unique_ptr<PropertiesFile> props;
    String status = "looking for SP-1s...", fileName;
    bool shared = false, demo = false;
    int dbg = 0;
    double t = 0.0;                                   // seconds, for the drawing

    // what the drawing keeps between frames
    float peakHold[Engine::kMaxOut] {}, holdAge[Engine::kMaxOut] {}, shown[Engine::kMaxOut] {};
    static constexpr int kHist = 220;
    float hist[Engine::kMaxOut][kHist] {};
    int histAt = 0;
    float energy = 0.0f;

   #if ! SCQ_LEGACY
    // spectrograms: one scrolling picture per output, 40 Hz - 20 kHz on a log scale
    static constexpr int kFftOrder = 10, kFft = 1 << kFftOrder, kSpecW = 240, kSpecH = 64;
    dsp::FFT fft { kFftOrder };
    std::vector<float> fftBuf = std::vector<float> ((size_t) kFft * 2);
    std::vector<float> window = std::vector<float> ((size_t) kFft);
    Image specImg[Engine::kMaxOut];
    int specCol = 0;
    Rectangle<int> specArea;

    void spectra()
    {
        const int outs = engine.sp1Outs.load();
        const int at = engine.specAt.load();
        for (int o = 0; o < outs; ++o)
        {
            if (! specImg[o].isValid()) { specImg[o] = Image (Image::RGB, kSpecW, kSpecH, true); Graphics (specImg[o]).fillAll (look::modular ? Colour (0xff0c2e2a) : look::deep); }
            for (int i = 0; i < kFft; ++i)
                fftBuf[(size_t) i] = (demo ? demoSample (o, i) : engine.specRing[o][(at - kFft + i) & (Engine::kSpecRing - 1)]) * window[(size_t) i];
            std::fill (fftBuf.begin() + kFft, fftBuf.end(), 0.0f);
            fft.performFrequencyOnlyForwardTransform (fftBuf.data(), true);
            const Colour c = look::unitColour (o / 2);
            Image::BitmapData px (specImg[o], specCol, 0, 1, kSpecH, Image::BitmapData::writeOnly);
            for (int y = 0; y < kSpecH; ++y)
            {
                // row y: from 20 kHz at the top to 40 Hz at the bottom, log spaced
                const float f0 = 40.0f * std::pow (500.0f, (float) (kSpecH - 1 - y) / (float) kSpecH);
                const float f1 = 40.0f * std::pow (500.0f, (float) (kSpecH - y) / (float) kSpecH);
                int b0 = jlimit (1, kFft / 2 - 1, (int) (f0 * kFft / 48000.0f)), b1 = jlimit (b0 + 1, kFft / 2, (int) (f1 * kFft / 48000.0f) + 1);
                float m = 0; for (int b = b0; b < b1; ++b) m = jmax (m, fftBuf[(size_t) b]);
                const float level = jlimit (0.0f, 1.0f, (look::db (m * 2.0f / (float) kFft * 2.0f) + 90.0f) / 84.0f);
                // the colour map: field -> the unit's colour -> yellow -> white
                const Colour base = look::modular ? Colour (0xff0c2e2a) : look::deep;
                Colour col = level < 0.5f ? base.interpolatedWith (c, level * 2.0f)
                           : (level < 0.8f ? c.interpolatedWith (look::hot, (level - 0.5f) / 0.3f)
                                           : look::hot.interpolatedWith (Colours::white, (level - 0.8f) / 0.2f));
                px.setPixelColour (0, y, col);
            }
        }
        specCol = (specCol + 1) % kSpecW;
    }
    float demoSample (int o, int i) const
    {
        const float u = (float) (o / 2), tt = (float) t + (float) i / 48000.0f;
        const float f = 110.0f * (1.0f + u * 0.5f) * (1.0f + 0.5f * std::sin ((float) t * 0.4f + u));
        const float beat = std::pow (0.5f + 0.5f * std::sin ((float) t * 7.85f + u), 8.0f);
        Random r ((int64) (t * 1000) + i + o * 7777);
        return engine.outLevel[o].load() * (0.5f * std::sin (6.2832f * f * tt) + 0.25f * std::sin (6.2832f * f * 3.01f * tt)
                                            + 0.12f * std::sin (6.2832f * f * 7.3f * tt) + beat * 0.4f * (r.nextFloat() - 0.5f));
    }
   #endif

    TextButton rescanBtn { "rescan" }, ifaceAllBtn { "all to interface" }, sharedBtn { "share with other apps" },
               openBtn { "open wav..." }, playBtn { "play" }, loopBtn { "loop" }, diagBtn { "1 to 1" }, clearBtn { "clear" },
               muteBtn { "mute" }, lookBtn { "look" };
    ComboBox loopBox;
    MasterFader master;
    Rectangle<int> railArea, stageArea, histArea, unitsArea, srcArea, gridArea;
    struct UnitRow { TextButton show { "show" }, up { "up" }, down { "dn" }, mode { "mode" }; };
    OwnedArray<UnitRow> rows;
    std::unique_ptr<FileChooser> chooser;

    explicit ArrayComponent (bool demoMode = false) : demo (demoMode)
    {
        setLookAndFeel (&lf);
        LookAndFeel::setDefaultLookAndFeel (&lf);
       #if SCQ_LEGACY
        popts.applicationName = "Sleepcousonique Array Legacy";   // its own settings: the modular look by default
       #else
        popts.applicationName = "Sleepcousonique Array";
       #endif
        popts.filenameSuffix = ".settings";
        popts.osxLibrarySubFolder = "Application Support"; popts.folderName = "Sleepcousonique";
        props = std::make_unique<PropertiesFile> (popts);
       #if SCQ_LEGACY
        look::apply (props->getValue ("look", "modular") == "modular");    // legacy: the modular look by default
       #else
        look::apply (props->getValue ("look", "faltung") == "modular");
       #endif
        lf.refresh();
        shared = props->getBoolValue ("shared", false);
        sharedBtn.setToggleState (shared, dontSendNotification);
        loadRoutes();

        for (auto* b : { &rescanBtn, &ifaceAllBtn, &sharedBtn, &openBtn, &playBtn, &loopBtn, &diagBtn, &clearBtn, &muteBtn, &lookBtn }) addAndMakeVisible (b);
        lookBtn.setTooltip ("the look: faltung or modular (the 2004 patch editor)");
        lookBtn.onClick = [this]
        {
            look::apply (! look::modular);
            props->setValue ("look", look::modular ? "modular" : "faltung");
            lf.refresh(); sendLookAndFeelChange();
           #if ! SCQ_LEGACY
            for (auto& im : specImg) im = Image();
           #endif
            resized(); repaint();
        };
        addAndMakeVisible (loopBox);
        addAndMakeVisible (master);
        loopBtn.setClickingTogglesState (true);
        muteBtn.setClickingTogglesState (true);
        master.onChange = [this] (float d)
        {
            engine.master = d <= MasterFader::kMin ? 0.0f : Decibels::decibelsToGain (d);
            props->setValue ("master", d);
        };
        master.set ((float) props->getDoubleValue ("master", 0.0), true);
        master.setTooltip ("master volume: every output. double-click: 0 dB");
        muteBtn.onClick = [this] { engine.masterMute = muteBtn.getToggleState(); };
        rescanBtn.onClick = [this] { rescan(); };
        ifaceAllBtn.onClick = [this] { ifaceAll(); };
        sharedBtn.onClick = [this] { shared = ! shared; sharedBtn.setToggleState (shared, dontSendNotification); props->setValue ("shared", shared); rebuild(); };
        openBtn.onClick = [this] { chooseFile(); };
        playBtn.onClick = [this] { if (engine.transport.isPlaying()) engine.transport.stop(); else { if (engine.transport.hasStreamFinished()) engine.transport.setPosition (0); engine.transport.start(); } };
        loopBtn.onClick = [this] { engine.setLoop (loopBtn.getToggleState()); };
        diagBtn.onClick = [this] { for (int o = 0; o < Engine::kMaxOut; ++o) engine.route[o] = o < Engine::kIns ? (1u << o) : 0u; saveRoutes(); repaint(); };
        clearBtn.onClick = [this] { for (auto& r : engine.route) r = 0; saveRoutes(); repaint(); };
        loopBox.onChange = [this] { props->setValue ("loop", loopBox.getSelectedId() > 1 ? loops[(size_t) loopBox.getSelectedId() - 2].uid : String()); rebuild(); };
        loopBox.setTooltip ("a loopback input (BlackHole 16ch, Loopback...): what a DAW plays into it comes out of the SP-1s");

       #if ! SCQ_LEGACY
        dsp::WindowingFunction<float>::fillWindowingTables (window.data(), (size_t) kFft, dsp::WindowingFunction<float>::hann, false);
       #endif
        devices.initialise (0, 0, nullptr, false);
        devices.addAudioCallback (&engine);
        setSize (1360, 880);
        rescan();
       #if SCQ_LEGACY
        startTimerHz (12);                   // legacy: meters 12 times a second, only where they are
       #else
        startTimerHz (30);
       #endif
    }
    ~ArrayComponent() override
    {
        stopTimer();
        devices.removeAudioCallback (&engine);
        devices.closeAudioDevice();
        if (! demo) agg::destroy();
        LookAndFeel::setDefaultLookAndFeel (nullptr);
        setLookAndFeel (nullptr);
    }

    // ---- units ----
    void makeRows()
    {
        rows.clear();
        for (size_t i = 0; i < units.size(); ++i)
        {
            auto* r = rows.add (new UnitRow());
            for (auto* b : { &r->show, &r->up, &r->down, &r->mode }) addAndMakeVisible (b);
            const int k = (int) i;
            r->show.onClick = [this, k] { if (units[(size_t) k].port.isNotEmpty()) serial::ask (units[(size_t) k].port, "SCQ SHOW", { "OK" }); };
            r->up.onClick = [this, k] { move (k, -1); };
            r->down.onClick = [this, k] { move (k, +1); };
            r->mode.onClick = [this, k]
            {
                auto& u = units[(size_t) k];
                if (u.port.isEmpty()) return;
                serial::ask (u.port, u.mode == "IFACE" ? "SCQ MODE DREAM" : "SCQ MODE IFACE", { "OK" });
                rescan();
            };
        }
    }

    void rescan()
    {
        if (demo)                                // --demo: six pretend units, no audio
        {
            units.clear();
            const char* sn[] = { "3F2A91C04E7711D2", "88B2004C1A9E3F07", "C41D7E2290AB5513", "0E9F33A1D6C2B848", "5B7700E1C3D29A6F", "A0C4E2F1937B0D5E" };
            for (int i = 0; i < 6; ++i)
            {
                Unit u; u.serial = sn[i]; u.uid = String ("demo") + String (i); u.mode = i == 5 ? "DREAM" : "IFACE"; u.number = i + 1; u.port = "-";
                units.push_back (u);
            }
            makeRows();
            engine.sp1Outs = 12;
            engine.fileChans = 8;
            fileName = "night-walk-8ch.wav" + DOT + "8 ch";
            loopBox.clear (dontSendNotification);
            loopBox.addItem ("BlackHole 16ch  (16 in)", 2);
            loopBox.setSelectedId (2, dontSendNotification);
            status = "6 SP-1s" + DOT + "12 outputs" + DOT + "shared as \"SP-1 Array\"";
            const int r[] = { 0, 1, 16, 17, 2, 3, 18, 19, 4, 5, 20, 21 };
            for (int o = 0; o < 12; ++o) engine.route[o] = 1u << r[o];
            engine.route[4] |= 1u << 22;
            resized(); repaint();
            return;
        }
        auto all = agg::list();
        loops.clear();
        std::vector<Unit> found;
        for (auto& d : all)
        {
            if (d.isSp1() && d.outs >= 2) { Unit u; u.serial = d.serial(); u.uid = d.uid; found.push_back (u); }
            else if (d.ins > 0 && ! d.isSp1()) loops.push_back (d);
        }
        for (auto& port : serial::candidates())
        {
            auto me = serial::ask (port, "SCQ WHO", { "SCQ ME" });          // "SCQ ME <serial> <unit> <mode>"
            auto tk = StringArray::fromTokens (me, " ", "");
            if (tk.size() < 5) continue;
            bool matched = false;
            for (auto& u : found)
                if (u.serial == tk[2] || u.uid.contains (tk[2])) { u.port = port; u.number = tk[3].getIntValue(); u.mode = tk[4]; matched = true; }
            if (! matched) { Unit u; u.serial = tk[2]; u.port = port; u.number = tk[3].getIntValue(); u.mode = tk[4]; found.push_back (u); }
        }
        auto order = StringArray::fromTokens (props->getValue ("order"), ",", "");
        std::stable_sort (found.begin(), found.end(), [&] (const Unit& a, const Unit& b)
        {
            int ia = order.indexOf (a.serial), ib = order.indexOf (b.serial);
            if (ia < 0) ia = 1000; if (ib < 0) ib = 1000;
            return ia < ib;
        });
        units = found;

        loopBox.clear (dontSendNotification);
        loopBox.addItem ("no loopback input", 1);
        auto want = props->getValue ("loop");
        int sel = 1;
        for (size_t i = 0; i < loops.size(); ++i)
        {
            loopBox.addItem (loops[i].name + "  (" + String (loops[i].ins) + " in)", (int) i + 2);
            if (loops[i].uid == want) sel = (int) i + 2;
        }
        if (want.isEmpty())
            for (size_t i = 0; i < loops.size(); ++i) if (loops[i].name.containsIgnoreCase ("BlackHole")) { sel = (int) i + 2; break; }
        loopBox.setSelectedId (sel, dontSendNotification);
        makeRows();
        rebuild();
    }

    void move (int k, int d)
    {
        int j = k + d;
        if (j < 0 || j >= (int) units.size()) return;
        std::swap (units[(size_t) k], units[(size_t) j]);
        StringArray order;
        for (auto& u : units) order.add (u.serial);
        props->setValue ("order", order.joinIntoString (","));
        rebuild(); resized(); repaint();
    }

    // ---- the aggregate ----
    void rebuild()
    {
        if (demo) return;
        devices.closeAudioDevice();
        for (size_t i = 0; i < units.size(); ++i)
            if (units[i].port.isNotEmpty() && units[i].number != (int) i + 1)
            {
                serial::ask (units[i].port, "SCQ UNIT " + String ((int) i + 1), { "OK" });
                units[i].number = (int) i + 1;
            }
        StringArray uids;
        for (auto& u : units) if (u.uid.isNotEmpty()) uids.add (u.uid);
        engine.sp1Outs = uids.size() * 2;
        String loopUid = loopBox.getSelectedId() > 1 ? loops[(size_t) loopBox.getSelectedId() - 2].uid : String();
        if (uids.isEmpty())
        {
            status = units.empty() ? "no SP-1 found: plug one in (Sleepcousonique firmware) and press rescan"
                                   : "the SP-1s answer, but macOS shows no Sleepcousonique sound card yet";
            agg::destroy();
            resized(); repaint();
            return;
        }
        // a loopback driver that has stopped answering would freeze the whole Array as it opens:
        // ask first (a second, short-lived copy of this program does the asking), and go on without it
        String stuck;
        if (loopUid.isNotEmpty() && ! agg::answers (loopUid, 3000))
        {
            stuck = loops[(size_t) loopBox.getSelectedId() - 2].name;
            loopUid = {};
        }
        const bool unmuted = loopUid.isNotEmpty() && agg::unmute (loopUid);
        String err;
        auto name = agg::make (uids, loopUid, shared, err);
        if (name.isEmpty()) { status = err; resized(); repaint(); return; }

        auto* type = devices.getCurrentDeviceTypeObject();
        if (type == nullptr) { devices.setCurrentAudioDeviceType ("CoreAudio", true); type = devices.getCurrentDeviceTypeObject(); }
        if (type) type->scanForDevices();
        AudioDeviceManager::AudioDeviceSetup s;
        devices.getAudioDeviceSetup (s);
        s.outputDeviceName = name;
        s.inputDeviceName = loopUid.isNotEmpty() ? name : String();
        s.sampleRate = 48000;
        s.bufferSize = 256;
        s.useDefaultInputChannels = s.useDefaultOutputChannels = false;
        s.outputChannels.setRange (0, 128, true);
        s.inputChannels.setRange (0, loopUid.isNotEmpty() ? 64 : 0, true);
        auto e = devices.setAudioDeviceSetup (s, true);
        int n = uids.size();
        status = e.isNotEmpty() ? "couldn't open the array: " + e
               : String (n) + (n == 1 ? " SP-1" : " SP-1s") + DOT + String (n * 2) + " outputs" + (shared ? DOT + "shared as \"" + name + "\"" : String())
                 + (unmuted ? DOT + "the loopback input was muted: turned it back up" : String())
                 + (stuck.isNotEmpty() ? DOT + stuck + " isn't answering (left out): restart Core Audio or the Mac to get it back" : String());
        resized(); repaint();
    }

    void ifaceAll()
    {
        for (auto& u : units) if (u.port.isNotEmpty()) serial::ask (u.port, "SCQ MODE IFACE", { "OK" });
        rescan();
    }

    void playFile (const File& f)
    {
        if (! f.existsAsFile()) { fileName = "no such file: " + f.getFileName(); repaint(); return; }
        auto err = engine.openFile (f);
        fileName = err.isEmpty() ? f.getFileName() + DOT + String (engine.fileChans.load()) + " ch" : err;
        loopBtn.setToggleState (true, dontSendNotification);
        engine.setLoop (true);
        if (err.isEmpty()) { engine.transport.setPosition (0); engine.transport.start(); }
        repaint();
    }

    void chooseFile()
    {
        chooser = std::make_unique<FileChooser> ("a multichannel wav", File(), "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& c)
        {
            auto f = c.getResult();
            if (! f.existsAsFile()) return;
            auto err = engine.openFile (f);
            fileName = err.isEmpty() ? f.getFileName() + DOT + String (engine.fileChans.load()) + " ch" : err;
            engine.setLoop (loopBtn.getToggleState());
            repaint();
        });
    }

    // ---- routes ----
    void loadRoutes()
    {
        auto tk = StringArray::fromTokens (props->getValue ("routes"), ",", "");
        for (int o = 0; o < Engine::kMaxOut; ++o)
            engine.route[o] = o < tk.size() ? (uint32_t) tk[o].getLargeIntValue() : (o < Engine::kIns ? (1u << o) : 0u);
    }
    void saveRoutes()
    {
        StringArray tk;
        for (auto& r : engine.route) tk.add (String ((int64) r.load()));
        props->setValue ("routes", tk.joinIntoString (","));
    }

    // ---- layout ----
    //  rail (left): cube, title, status, master fader + its meter, global buttons
    //  stage (top): one isometric block per SP-1, its towers rising with its outputs
    //  history: the last seconds of every output as Oval lines
    //  units (bottom left) | sources + routes (bottom right)
    void resized() override
    {
        auto r = getLocalBounds();
        railArea = r.removeFromLeft (220);
        auto rail = railArea.reduced (20, 18);
        rail.removeFromTop (150);
        auto fader = rail.removeFromTop (330);
        master.setBounds (fader.removeFromLeft (64));
        rail.removeFromTop (14);
        { auto mr = rail.removeFromTop (26); muteBtn.setBounds (mr.removeFromLeft (84)); mr.removeFromLeft (8); lookBtn.setBounds (mr.removeFromLeft (84)); }
        rail.removeFromTop (22);
        for (auto* b : { &rescanBtn, &ifaceAllBtn, &sharedBtn }) { b->setBounds (rail.removeFromTop (28)); rail.removeFromTop (8); }

        r.reduce (20, 18);
       #if SCQ_LEGACY
        stageArea = r.removeFromTop (96);    // legacy: a flat strip of bars, no history
        histArea = {};
        r.removeFromTop (16);
       #else
        stageArea = r.removeFromTop (250);
        r.removeFromTop (12);
        {
            auto mid = r.removeFromTop (150);
            histArea = mid.removeFromLeft (jmax (300, mid.getWidth() / 3)).withTrimmedTop (18);
            mid.removeFromLeft (20);
            specArea = mid;
        }
        r.removeFromTop (16);
       #endif
        unitsArea = r.removeFromLeft (420);
        auto ua = unitsArea.withTrimmedTop (30);
        for (int i = 0; i < rows.size(); ++i)
        {
            auto row = ua.removeFromTop (34);
            auto b = row.removeFromRight (190).withSizeKeepingCentre (190, 20);
            rows[i]->show.setBounds (b.removeFromLeft (50)); b.removeFromLeft (4);
            rows[i]->mode.setBounds (b.removeFromLeft (50)); b.removeFromLeft (4);
            rows[i]->up.setBounds (b.removeFromLeft (38)); b.removeFromLeft (4);
            rows[i]->down.setBounds (b.removeFromLeft (38));
        }
        r.removeFromLeft (24);
        srcArea = r.removeFromTop (70);
        auto s1 = srcArea.withTrimmedTop (2).removeFromTop (28);
        s1.removeFromLeft (52);
        loopBox.setBounds (s1.removeFromLeft (290));
        auto s2 = srcArea.withTrimmedTop (36).removeFromTop (28);
        s2.removeFromLeft (52);
        openBtn.setBounds (s2.removeFromLeft (110)); s2.removeFromLeft (6);
        playBtn.setBounds (s2.removeFromLeft (64)); s2.removeFromLeft (6);
        loopBtn.setBounds (s2.removeFromLeft (64));
        gridArea = r.withTrimmedTop (4);
        auto gb = gridArea.withHeight (22);
        clearBtn.setBounds (gb.removeFromRight (64)); gb.removeFromRight (6);
        diagBtn.setBounds (gb.removeFromRight (72));
    }

    int sources() const { return Engine::kIns + engine.fileChans.load(); }
    Rectangle<float> cell (int o, int s) const
    {
        const float x0 = (float) gridArea.getX() + 46, y0 = (float) gridArea.getY() + 50;
        const float cw = jmin (22.0f, ((float) gridArea.getWidth() - 140) / (float) jmax (1, sources()));
        const float ch = jmin (17.0f, ((float) gridArea.getHeight() - 60) / (float) jmax (1, engine.sp1Outs.load()));
        return { x0 + (float) s * cw, y0 + (float) o * ch, cw - 3, ch - 4 };
    }
    void mouseDown (const MouseEvent& e) override
    {
        for (int o = 0; o < engine.sp1Outs.load(); ++o)
            for (int s = 0; s < sources(); ++s)
                if (cell (o, s).expanded (1).contains (e.position))
                {
                    engine.route[o] = engine.route[o].load() ^ (1u << s);
                    saveRoutes(); repaint();
                    return;
                }
    }

    void demoSignal()
    {
        for (int o = 0; o < 12; ++o)
        {
            float u = (float) (o / 2);
            float env = 0.5f + 0.5f * std::sin ((float) t * (0.7f + 0.23f * u) + u * 1.7f + (o % 2) * 0.6f);
            float beat = std::pow (0.5f + 0.5f * std::sin ((float) t * 7.85f + u), 6.0f);
            float v = o >= 10 ? 0.0f : jlimit (0.0f, 1.2f, 0.08f + 0.55f * env * env + 0.35f * beat);
            engine.outLevel[o] = v; engine.outRms[o] = v * 0.62f;
        }
        for (int s = 0; s < 24; ++s) engine.srcLevel[(size_t) s] = (s < 6 || (s >= 16 && s < 23)) ? 0.25f + 0.5f * (0.5f + 0.5f * std::sin ((float) t * 1.3f + (float) s)) : 0.0f;
        for (int u = 0; u < 5; ++u)
            for (int i = 0; i < Engine::kScope; ++i)
            {
                float ph = (float) i * 0.11f + (float) t * 3.0f;
                engine.scope[u][i][0] = 0.6f * engine.outLevel[u * 2] * std::sin (ph * (1.0f + 0.25f * (float) u));
                engine.scope[u][i][1] = 0.6f * engine.outLevel[u * 2 + 1] * std::sin (ph * (1.5f + 0.1f * (float) u) + 0.7f);
            }
    }

    void timerCallback() override
    {
        t += 1.0 / 30.0;
        if (demo) demoSignal();
        if (getenv ("ARRAY_DEBUG") != nullptr && ++dbg % 20 == 0)
        {
            auto* d = devices.getCurrentAudioDevice();
            fprintf (stderr, "dev=%s ins=%d outs=%d sr=%.0f in1=%.3f out1=%.3f master=%.2f calls=%d\n",
                     d ? d->getName().toRawUTF8() : "-", engine.devIns.load(),
                     d ? d->getActiveOutputChannels().countNumberOfSetBits() : -1,
                     d ? d->getCurrentSampleRate() : 0.0, engine.srcLevel[0].load(), engine.outLevel[0].load(),
                     engine.master.load(), engine.calls.load());
        }
        const int outs = engine.sp1Outs.load();
        float sum = 0.0f;
        for (int o = 0; o < outs; ++o)
        {
            float v = engine.outLevel[o].load();
            shown[o] = v > shown[o] ? v : shown[o] * 0.9f + v * 0.1f;     // fast up, slow down
            if (v >= peakHold[o]) { peakHold[o] = v; holdAge[o] = 0; }
            else if ((holdAge[o] += 1.0f / 30.0f) > 1.2f) peakHold[o] = jmax (v, peakHold[o] * 0.93f);
            hist[o][histAt] = look::meterPos (v);
            sum += v;
        }
        histAt = (histAt + 1) % kHist;
       #if ! SCQ_LEGACY
        spectra();
       #endif
        energy = energy * 0.85f + 0.15f * (outs > 0 ? sum / (float) outs : 0.0f);
        playBtn.setButtonText (engine.transport.isPlaying() ? "stop" : "play");
       #if SCQ_LEGACY
        repaint (stageArea);
        auto fb = master.getBounds();
        repaint (fb.getRight(), fb.getY(), 150, fb.getHeight());
        repaint (unitsArea.getX() + 160, unitsArea.getY() + 30, 70, (int) units.size() * 34);
        repaint (gridArea.getX(), gridArea.getY() + 24, gridArea.getWidth(), 22);
        if (outs > 0) { auto e = cell (0, sources()); repaint ((int) e.getX(), (int) e.getY(), 110, (int) (cell (outs, 0).getY() - e.getY())); }
        if (engine.fileLen() > 0) repaint (srcArea);
       #else
        repaint();
       #endif
    }

    // ---- drawing ----
    void head (Graphics& g, Rectangle<int> a, const String& name, const String& right)
    {
        using namespace look;
        g.setColour (ink); g.setFont (fm (13)); g.drawText (name, a.getX(), a.getY(), 240, 18, Justification::left);
        g.setColour (dim); g.setFont (f (10.5f, true)); g.drawText (right, a.getX(), a.getY(), a.getWidth(), 18, Justification::right);
        g.setColour (fill.withAlpha (0.7f)); g.drawHorizontalLine (a.getY() + 22, (float) a.getX(), (float) a.getRight());
    }

    // a segmented slanted meter: rms as the body, peak as the edge, hold as a tick
    void segMeter (Graphics& g, Rectangle<float> r, float peak, float rms, float hold, Colour c, bool vertical)
    {
        using namespace look;
       #if SCQ_LEGACY
        {
            const float pp = meterPos (peak), ph = meterPos (hold);
            g.setColour (modular ? Colours::black : track); g.fillRect (r);
            g.setColour (pp > 60.0f / 66.0f ? warn : (modular ? Colour (0xff30e030) : c));
            if (vertical) g.fillRect (r.withTop (r.getBottom() - pp * r.getHeight()));
            else g.fillRect (r.withWidth (pp * r.getWidth()));
            g.setColour (ink);
            if (hold > 0.001f) { if (vertical) g.fillRect (r.getX(), r.getBottom() - ph * r.getHeight(), r.getWidth(), 1.5f); else g.fillRect (r.getX() + ph * r.getWidth(), r.getY(), 1.5f, r.getHeight()); }
            ignoreUnused (rms);
            return;
        }
       #endif
        const int segs = vertical ? 22 : 30;
        const float pp = meterPos (peak), pr = meterPos (rms), ph = meterPos (hold);
        if (modular)                                 // LEDs: green, yellow, red on black
        {
            g.setColour (Colours::black); g.fillRect (r.expanded (1.0f));
            for (int i = 0; i < segs; ++i)
            {
                const float a = (float) i / (float) segs, b2 = (float) (i + 1) / (float) segs, db0 = a * 66.0f - 60.0f;
                Rectangle<float> sg = vertical ? Rectangle<float> (r.getX() + 1, r.getBottom() - b2 * r.getHeight() + 1, r.getWidth() - 2, r.getHeight() / (float) segs - 1.5f)
                                               : Rectangle<float> (r.getX() + a * r.getWidth() + 1, r.getY() + 1, r.getWidth() / (float) segs - 1.5f, r.getHeight() - 2);
                const Colour lit = db0 > 0.0f ? Colour (0xffff3030) : (db0 > -9.0f ? Colour (0xffffe030) : Colour (0xff30e030));
                const bool isHold = ph > a && ph <= b2 && hold > 0.001f;
                g.setColour (a < pp || isHold ? lit : Colour (0xff0a3a0a));
                g.fillRect (sg);
            }
            ignoreUnused (c, pr);
            return;
        }
        for (int i = 0; i < segs; ++i)
        {
            const float a = (float) i / (float) segs, b = (float) (i + 1) / (float) segs;
            Rectangle<float> s = vertical ? Rectangle<float> (r.getX(), r.getBottom() - b * r.getHeight() + 1, r.getWidth(), r.getHeight() / (float) segs - 2)
                                          : Rectangle<float> (r.getX() + a * r.getWidth(), r.getY(), r.getWidth() / (float) segs - 2, r.getHeight());
            const float db0 = a * 66.0f - 60.0f;
            Colour seg = db0 > 0.0f ? warn : (db0 > -9.0f ? hot : c);
            auto p = vertical ? pg (s, 3.0f) : pg (s, s.getHeight() * 0.5f);
            if (a < pr) { g.setColour (seg); g.fillPath (p); }
            else if (a < pp) { g.setColour (seg.withAlpha (0.45f)); g.fillPath (p); }
            else { g.setColour (track.withAlpha (0.55f)); g.strokePath (p, PathStrokeType (0.7f)); }
            if (ph > a && ph <= b && hold > 0.001f) { g.setColour (ink); g.fillPath (p); }
        }
    }

    void paintRail (Graphics& g)
    {
        using namespace look;
        g.setColour (modular ? field : deep); g.fillRect (railArea);
        g.setColour (rule); g.drawVerticalLine (railArea.getRight() - 1, 0.0f, (float) getHeight());
        auto r = railArea.reduced (20, 18);
        static const Image logo = ImageCache::getFromMemory (ArrayFonts::logo_png, ArrayFonts::logo_pngSize);
        g.drawImage (logo, Rectangle<float> ((float) r.getX() - 4, (float) r.getY() + 2, 56, 56), RectanglePlacement::centred);
        g.setColour (ink); g.setFont (fm (21)); g.drawText ("sp-1 array", r.getX() + 56, r.getY() + 12, 160, 26, Justification::left);
        g.setColour (dim); g.setFont (f (10.5f, true)); g.drawText ("interface mode", r.getX() + 56, r.getY() + 38, 160, 16, Justification::left);
        g.setColour (ink); g.setFont (f (10.5f, true));
        g.drawFittedText (status.replace (DOT, "\n"), r.getX(), r.getY() + 70, r.getWidth(), 64, Justification::topLeft, 4);

        // the master: fader, then a tall meter of the loudest output, then the number
        auto fb = master.getBounds();
        g.setColour (ink); g.setFont (fm (12)); g.drawText ("master", fb.getX(), fb.getY() - 14, 80, 14, Justification::left);
        const int outs = engine.sp1Outs.load();
        float pk = 0, rm = 0, hd = 0;
        for (int o = 0; o < outs; ++o) { pk = jmax (pk, shown[o]); rm = jmax (rm, engine.outRms[o].load()); hd = jmax (hd, peakHold[o]); }
        segMeter (g, { (float) fb.getRight() + 14, (float) fb.getY() + 12, 26, (float) fb.getHeight() - 24 }, pk, rm, hd, fill, true);
        const bool over = master.dB > 0.05f;
        g.setColour (over ? warn : hot); g.setFont (fm (22));
        auto num = master.dB <= MasterFader::kMin ? String ("-inf") : (over ? "+" : "") + String (std::abs (master.dB) < 0.05f ? 0.0f : master.dB, 1);
        g.drawText (num, fb.getRight() + 50, fb.getY() + 4, 90, 26, Justification::left);
        g.setColour (dim); g.setFont (f (10, true));
        g.drawText ("db", fb.getRight() + 50, fb.getY() + 30, 40, 14, Justification::left);
        g.drawText ("peak " + (hd > 1e-5f ? String (db (hd), 1) : String ("-inf")), fb.getRight() + 50, fb.getY() + 52, 110, 14, Justification::left);
        if (engine.masterMute.load()) { g.setColour (warn); g.setFont (fm (12)); g.drawText ("muted", fb.getRight() + 50, fb.getY() + 72, 80, 14, Justification::left); }
        // the dB scale beside the meter
        g.setFont (f (8.5f, true));
        for (float d : { 6.0f, 0.0f, -12.0f, -24.0f, -40.0f, -60.0f })
        {
            float y = (float) fb.getBottom() - 12 - (d + 60.0f) / 66.0f * (float) (fb.getHeight() - 24);
            g.setColour (dim.withAlpha (0.7f));
            g.drawText (String ((int) d), fb.getRight() + 44, (int) y - 6, 30, 12, Justification::left);
        }
    }

    void paintStage (Graphics& g)
    {
        using namespace look;
        if (SCQ_LEGACY || modular)
        {   // legacy and the modular look: one flat module strip per SP-1 with its meters
            auto a = stageArea.toFloat();
            if (modular && marble.isValid())
            {
                Graphics::ScopedSaveState clip (g);
                g.reduceClipRegion (stageArea);
                for (float y = a.getY(); y < a.getBottom(); y += (float) marble.getHeight())
                    for (float x = a.getX(); x < a.getRight(); x += (float) marble.getWidth())
                        g.drawImageAt (marble, (int) x, (int) y);
                bevel (g, a, false);
            }
            else { g.setColour (deep); g.fillRect (a); }
            const int n = (int) units.size();
            g.setColour (dim); g.setFont (f (10, true));
            if (n == 0) { g.drawText ("no SP-1s yet: plug them in, hold dots + PLAY for a second on each, then rescan", stageArea, Justification::centred); return; }
            const float slot = jmin (170.0f, (a.getWidth() - 20) / (float) n);
            for (int u = 0; u < n; ++u)
            {
                const float x = a.getX() + 10 + (float) u * slot, w = slot - 12;
                const Colour c = unitColour (u);
                const bool live = units[(size_t) u].mode == "IFACE" && units[(size_t) u].uid.isNotEmpty();
                if (modular)                         // a module strip: the triangle, the bold name, two LED columns
                {
                    Rectangle<float> m (x, a.getY() + 8, w, a.getHeight() - 16);
                    g.setColour (live ? c : c.withMultipliedSaturation (0.3f)); g.fillRect (m); bevel (g, m, true);
                    Path tri; tri.addTriangle (m.getX() + 4, m.getY() + 5, m.getX() + 14, m.getY() + 5, m.getX() + 9, m.getY() + 14);
                    g.setColour (Colour (0xffffa000)); g.fillPath (tri); g.setColour (Colours::black); g.strokePath (tri, PathStrokeType (1.0f));
                    g.setColour (Colours::black); g.setFont (fm (12.5f));
                    g.drawText ("SP-1 #" + String (u + 1), (int) m.getX() + 18, (int) m.getY() + 2, (int) w - 20, 16, Justification::centredLeft);
                    g.setFont (f (10.0f));
                    const bool tallStrip = m.getHeight() > 120;
                    g.drawText (live ? "out " + String (u * 2 + 1) + "-" + String (u * 2 + 2) : String ("dreaming"),
                                (int) m.getX() + 6, tallStrip ? (int) m.getBottom() - 16 : (int) m.getY() + 24, (int) w - 12, 14,
                                tallStrip ? Justification::centredRight : Justification::centredLeft);
                    const bool tall = m.getHeight() > 120;   // the legacy stage is a short strip: bars across instead
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        const int o = jmin (u * 2 + ch, Engine::kMaxOut - 1);
                        if (tall)
                        {
                            Rectangle<float> col (m.getX() + 14 + (float) ch * 26, m.getY() + 24, 14, m.getHeight() - 46);
                            segMeter (g, col, shown[o], 0, peakHold[o], c, true);
                            g.drawText (ch ? "R" : "L", (int) col.getX() - 2, (int) col.getBottom() + 3, 18, 12, Justification::centred);
                        }
                        else
                        {
                            Rectangle<float> row (m.getX() + 18, m.getY() + 46 + (float) ch * 14, w - 30, 9);
                            segMeter (g, row, shown[o], 0, peakHold[o], c, false);
                            g.drawText (ch ? "R" : "L", (int) m.getX() + 4, (int) row.getY() - 2, 12, 12, Justification::centred);
                        }
                    }
                    // the peak, in a dark value box
                    const float pk = jmax (peakHold[u * 2], peakHold[jmin (u * 2 + 1, Engine::kMaxOut - 1)]);
                    Rectangle<float> vb = tall ? Rectangle<float> (m.getX() + 72, m.getY() + 26, jmin (60.0f, w - 80), 14)
                                               : Rectangle<float> (m.getRight() - 64, m.getY() + 24, 58, 14);
                    g.setColour (Colour (0xff5a5a5a)); g.fillRect (vb); bevel (g, vb, false);
                    g.setColour (Colour (0xfff0f0f0)); g.setFont (f (10.0f));
                    g.drawText (pk > 1e-5f ? String (db (pk), 1) + "dB" : String ("-inf"), vb, Justification::centred);
                    continue;
                }
                g.setColour (live ? c : c.withMultipliedBrightness (0.4f));
                g.fillRect (x, a.getY() + 10, w, 20.0f);
                g.setColour (deep); g.setFont (fm (12));
                g.drawText ("#" + String (u + 1), (int) x + 6, (int) a.getY() + 10, 40, 20, Justification::centredLeft);
                g.setFont (f (9.5f, true));
                g.drawText (live ? "out " + String (u * 2 + 1) + "-" + String (u * 2 + 2) : String ("dreaming"), (int) x, (int) a.getY() + 10, (int) w - 6, 20, Justification::centredRight);
                for (int ch = 0; ch < 2; ++ch)
                {
                    const int o = jmin (u * 2 + ch, Engine::kMaxOut - 1);
                    segMeter (g, { x, a.getY() + 38 + (float) ch * 22, w, 14 }, shown[o], 0, peakHold[o], c, false);
                }
            }
            return;
        }
        auto a = stageArea.toFloat();
        g.setColour (deep.interpolatedWith (field, 0.5f)); g.fillRect (a);
        g.saveState();
        g.reduceClipRegion (stageArea);
        // stdio chevrons drifting slowly behind
        for (int i = 0; i < 6; ++i)
        {
            float x0 = a.getX() + std::fmod ((float) t * 10.0f + (float) i * 170.0f, a.getWidth() + 300.0f) - 200.0f, y0 = a.getY() - 30;
            Path ch;
            ch.startNewSubPath (x0, y0); ch.lineTo (x0 + 60, y0); ch.lineTo (x0 + 240, y0 + 180); ch.lineTo (x0 + 60, y0 + 360);
            ch.lineTo (x0, y0 + 360); ch.lineTo (x0 + 180, y0 + 180); ch.closeSubPath();
            g.setColour (fill.withAlpha (0.035f)); g.fillPath (ch);
        }
        // a Fell wireframe polyhedron: its points lean outward with the energy of the whole array
        {
            const float e = jlimit (0.0f, 1.0f, energy * 1.6f);
            const Point<float> P[] = { { 0.06f, 0.86f }, { 0.22f, 0.40f }, { 0.46f, 0.58f }, { 0.74f, 0.24f }, { 0.95f, 0.66f },
                                       { 0.70f, 0.94f }, { 0.36f, 0.95f }, { 0.14f, 0.62f }, { 0.52f, 0.12f }, { 0.84f, 0.48f } };
            const int E[][2] = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 5 }, { 5, 6 }, { 6, 0 }, { 1, 7 }, { 7, 6 }, { 2, 8 }, { 8, 3 },
                                 { 8, 1 }, { 2, 9 }, { 9, 4 }, { 9, 5 }, { 2, 6 }, { 9, 3 } };
            Point<float> q[10];
            for (int i = 0; i < 10; ++i)
            {
                auto c = Point<float> (0.5f, 0.55f);
                auto d = P[i] - c;
                float wob = 0.03f * std::sin ((float) t * 0.9f + (float) i * 1.3f);
                auto p = c + d * (0.92f + 0.18f * e + wob);
                q[i] = { a.getX() + p.x * a.getWidth(), a.getY() + p.y * a.getHeight() };
            }
            g.setColour (dim.withAlpha (0.18f + 0.3f * e));
            for (auto& ed : E) g.drawLine ({ q[ed[0]], q[ed[1]] }, 0.8f);
            // its one white face, as on the sleeves
            g.setColour (ink.withAlpha (0.05f + 0.22f * e));
            g.fillPath (quad (q[2], q[8], q[3], q[9]));
        }

        // the SP-1s as isometric blocks, each with two towers (left, right) that rise with level
        const int n = (int) units.size();
        if (n == 0)
        {
            g.setColour (dim); g.setFont (f (12, true));
            g.drawText ("no SP-1s yet: plug them in, hold dots + PLAY for a second on each, then rescan",
                        stageArea.reduced (24), Justification::centred);
            g.restoreState();
            return;
        }
        const float slot = jmin (190.0f, (a.getWidth() - 40) / (float) n);
        const float bw = slot * 0.62f, depth = bw * 0.32f, baseY = a.getBottom() - 46;
        const float x0 = a.getCentreX() - slot * (float) n / 2.0f + (slot - bw) / 2.0f;
        for (int u = 0; u < n; ++u)
        {
            const float x = x0 + (float) u * slot;
            const Colour c = unitColour (u);
            const bool live = units[(size_t) u].mode == "IFACE" && units[(size_t) u].uid.isNotEmpty();
            // the base block
            const float bh = 26;
            auto top = quad ({ x, baseY - bh }, { x + depth, baseY - bh - depth * 0.6f }, { x + bw + depth, baseY - bh - depth * 0.6f }, { x + bw, baseY - bh });
            auto side = quad ({ x + bw, baseY - bh }, { x + bw + depth, baseY - bh - depth * 0.6f }, { x + bw + depth, baseY - depth * 0.6f }, { x + bw, baseY });
            g.setColour (c.withMultipliedBrightness (live ? 0.75f : 0.35f)); g.fillPath (side);
            g.setColour (c.withMultipliedBrightness (live ? 1.15f : 0.45f)); g.fillPath (top);
            g.setColour (live ? c : c.withMultipliedBrightness (0.4f)); g.fillRect (x, baseY - bh, bw, bh);
            g.setColour (deep); g.setFont (fm (13));
            g.drawText ("#" + String (u + 1), (int) x + 6, (int) (baseY - bh), 40, (int) bh, Justification::centredLeft);
            g.setFont (f (9.5f, true));
            g.drawText (live ? "out " + String (u * 2 + 1) + "-" + String (u * 2 + 2) : String ("dreaming"),
                        (int) x, (int) (baseY - bh), (int) bw - 6, (int) bh, Justification::centredRight);
            // two towers on the top face, rising with the level (rms solid, peak as glass, hold as a line)
            for (int ch = 0; ch < 2; ++ch)
            {
                const int o = u * 2 + ch;
                if (o >= Engine::kMaxOut) break;
                const float tw = bw * 0.30f, tx = x + bw * (ch == 0 ? 0.12f : 0.56f) + depth * 0.35f, ty = baseY - bh - depth * 0.3f;
                const float maxH = a.getHeight() - 110;
                const float hp = meterPos (shown[o]) * maxH, hr = meterPos (engine.outRms[o].load()) * maxH, hh = meterPos (peakHold[o]) * maxH;
                const float d2 = tw * 0.5f;
                auto tower = [&] (float h, float alpha)
                {
                    if (h < 1) return;
                    g.setColour (c.withAlpha (alpha)); g.fillRect (tx, ty - h, tw, h);
                    g.setColour (c.withMultipliedBrightness (0.7f).withAlpha (alpha));
                    g.fillPath (quad ({ tx + tw, ty - h }, { tx + tw + d2, ty - h - d2 * 0.6f }, { tx + tw + d2, ty - d2 * 0.6f }, { tx + tw, ty }));
                    g.setColour (c.withMultipliedBrightness (1.3f).withAlpha (alpha));
                    g.fillPath (quad ({ tx, ty - h }, { tx + d2, ty - h - d2 * 0.6f }, { tx + tw + d2, ty - h - d2 * 0.6f }, { tx + tw, ty - h }));
                };
                tower (hp, 0.28f);                                  // the peak: glass
                tower (hr, 0.95f);                                  // the body: rms
                if (hh > 2)                                         // the hold: a floating line
                {
                    g.setColour (meterPos (peakHold[o]) > 60.0f / 66.0f ? warn : ink);
                    g.drawLine (tx, ty - hh, tx + tw, ty - hh, 1.6f);
                    g.drawLine (tx + tw, ty - hh, tx + tw + d2, ty - hh - d2 * 0.6f, 1.6f);
                }
                g.setColour (dim); g.setFont (f (8.5f, true));
                g.drawText (ch == 0 ? "l" : "r", (int) tx, (int) ty + 2, (int) tw, 10, Justification::centred);
            }
            // the phase figure (L against R) as a small wireframe beside the block
            if (live && u < Engine::kMaxOut / 2)
            {
                const float cx = x + bw * 0.5f, cy = baseY + 24, rr = 13;
                g.setColour (track); g.drawEllipse (cx - rr, cy - rr * 0.55f, rr * 2, rr * 1.1f, 0.6f);
                Path ph;
                const int at = engine.scopeAt[u].load();
                for (int i = 0; i < 96; ++i)
                {
                    int k = (at - 96 + i + Engine::kScope) % Engine::kScope;
                    float l = engine.scope[u][k][0], r2 = engine.scope[u][k][1];
                    float px = cx + (r2 - l) * rr * 0.9f, py = cy - (l + r2) * rr * 0.45f;
                    if (i == 0) ph.startNewSubPath (px, py); else ph.lineTo (px, py);
                }
                g.setColour (c.withAlpha (0.85f)); g.strokePath (ph, PathStrokeType (0.8f));
            }
        }
        g.setColour (dim); g.setFont (f (10, true));
        g.drawText ("the array", stageArea.getX() + 12, stageArea.getY() + 10, 200, 14, Justification::left);
        g.drawText (String (n * 2) + " outputs" + DOT + "towers: level" + DOT + "line: peak hold" + DOT + "below: left/right figure",
                    stageArea.getX() + 12, stageArea.getY() + 10, stageArea.getWidth() - 24, 14, Justification::right);
        g.restoreState();
    }

    void paintHistory (Graphics& g)
    {
        using namespace look;
        if (histArea.isEmpty()) return;
        auto a = histArea.toFloat();
        g.setColour (rule); g.drawHorizontalLine ((int) a.getBottom(), a.getX(), a.getRight());
        for (float d : { -6.0f, -24.0f, -48.0f })
        {
            float y = a.getBottom() - (d + 60.0f) / 66.0f * a.getHeight();
            g.setColour (rule.withAlpha (0.8f)); g.drawHorizontalLine ((int) y, a.getX() + 30, a.getRight());
            g.setColour (dim.withAlpha (0.6f)); g.setFont (f (8.5f, true)); g.drawText (String ((int) d), (int) a.getX(), (int) y - 6, 28, 12, Justification::left);
        }
        const int outs = engine.sp1Outs.load();
        for (int o = 0; o < outs; ++o)
        {
            Path p;
            for (int i = 0; i < kHist; ++i)
            {
                int k = (histAt + i) % kHist;
                float x = a.getX() + 30 + (float) i / (float) (kHist - 1) * (a.getWidth() - 30);
                float y = a.getBottom() - hist[o][k] * a.getHeight();
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (unitColour (o / 2).withAlpha (o % 2 ? 0.55f : 0.9f));
            g.strokePath (p, PathStrokeType (o % 2 ? 0.8f : 1.2f));
        }
        g.setColour (dim); g.setFont (f (10, true));
        g.drawText ("the last 7 seconds, every output", (int) a.getRight() - 300, (int) a.getY() - 2, 300, 14, Justification::right);
    }

   #if ! SCQ_LEGACY
    void paintSpectra (Graphics& g)
    {
        using namespace look;
        const int outs = engine.sp1Outs.load(), n = outs / 2;
        g.setColour (dim); g.setFont (f (10, true));
        g.drawText ("spectrograms" + DOT + "40 hz to 20 khz" + DOT + "newest on the right", specArea.getX(), specArea.getY(), specArea.getWidth(), 14, Justification::left);
        if (n == 0) return;
        auto a = specArea.withTrimmedTop (18).toFloat();
        const float cw = a.getWidth() / (float) n, gap = 6.0f;
        for (int u = 0; u < n; ++u)
            for (int ch = 0; ch < 2; ++ch)
            {
                const int o = u * 2 + ch;
                if (! specImg[o].isValid()) continue;
                Rectangle<float> r (a.getX() + (float) u * cw, a.getY() + (float) ch * (a.getHeight() / 2.0f), cw - gap, a.getHeight() / 2.0f - 3.0f);
                // the picture is a ring: draw its older part, then its newer part
                const float split = (float) specCol / (float) kSpecW;
                g.setImageResamplingQuality (Graphics::lowResamplingQuality);
                g.drawImage (specImg[o], (int) r.getX(), (int) r.getY(), (int) (r.getWidth() * (1.0f - split)), (int) r.getHeight(),
                             specCol, 0, kSpecW - specCol, kSpecH);
                if (specCol > 0)
                    g.drawImage (specImg[o], (int) (r.getX() + r.getWidth() * (1.0f - split)), (int) r.getY(), (int) (r.getWidth() * split) + 1, (int) r.getHeight(),
                                 0, 0, specCol, kSpecH);
                g.setColour (unitColour (u).withAlpha (0.8f));
                g.drawRect (r, 0.8f);
                g.setFont (f (9.0f, true));
                g.drawText ("#" + String (u + 1) + (ch ? " r" : " l"), (int) r.getX() + 4, (int) r.getY() + 2, 40, 11, Justification::left);
            }
    }
   #endif

    void paintUnits (Graphics& g)
    {
        using namespace look;
        head (g, unitsArea, "units", "each one is 2 outputs");
        auto ua = unitsArea.withTrimmedTop (30);
        for (size_t i = 0; i < units.size(); ++i)
        {
            auto row = ua.removeFromTop (34);
            auto& u = units[i];
            const Colour c = unitColour ((int) i);
            g.setColour (c); g.fillPath (pg ({ (float) row.getX(), (float) row.getY() + 7, 22, 12 }));
            g.setColour (ink); g.setFont (fm (12.5f));
            g.drawText ("#" + String ((int) i + 1), row.getX() + 28, row.getY() + 2, 34, 22, Justification::left);
            g.setFont (f (10, true));
            String m = u.uid.isEmpty() ? "no sound card" : (u.mode == "IFACE" ? "interface" : (u.mode == "DREAM" ? "dreaming" : "?"));
            g.setColour (u.mode == "DREAM" ? warn : dim);
            g.drawText (u.serial.substring (0, 6).toLowerCase() + "  " + m, row.getX() + 60, row.getY() + 2, 130, 22, Justification::left);
            // two small segmented meters
            for (int ch = 0; ch < 2; ++ch)
            {
                int o = jmin ((int) i * 2 + ch, Engine::kMaxOut - 1);
                segMeter (g, { (float) row.getX() + 166, (float) row.getY() + 6 + (float) ch * 8, 56, 5 }, shown[o], engine.outRms[o].load(), peakHold[o], c, false);
            }
        }
    }

    void paintSources (Graphics& g)
    {
        using namespace look;
        g.setColour (ink); g.setFont (f (11.5f, true));
        g.drawText ("input", srcArea.getX(), srcArea.getY() + 2, 50, 28, Justification::left);
        g.drawText ("file", srcArea.getX(), srcArea.getY() + 36, 50, 28, Justification::left);
        g.setColour (dim); g.setFont (f (10.5f, true));
        g.drawText (fileName.isEmpty() ? "no file" : fileName, srcArea.getX() + 52 + 110 + 6 + 64 + 6 + 64 + 12, srcArea.getY() + 36, 300, 28, Justification::left);
        // the file's place, as a slanted strip
        if (engine.fileLen() > 0)
        {
            Rectangle<float> st ((float) srcArea.getX() + 360, (float) srcArea.getY() + 8, (float) srcArea.getWidth() - 370, 16);
            if (st.getWidth() > 40)
            {
                g.setColour (track); g.strokePath (pg (st, 8), PathStrokeType (0.8f));
                float p = (float) (engine.filePos() / engine.fileLen());
                g.setColour (hot.withAlpha (0.85f)); g.fillPath (pg (st.withWidth (jmax (10.0f, st.getWidth() * p)), 8));
            }
        }
    }

    void paintGrid (Graphics& g)
    {
        using namespace look;
        head (g, gridArea, "routes", "");
        const int outs = engine.sp1Outs.load(), srcs = sources();
        g.setFont (f (9.5f, true));
        for (int s = 0; s < srcs; ++s)
        {
            auto c = cell (0, s);
            g.setColour (s < Engine::kIns ? dim : hot);
            g.drawText (s < Engine::kIns ? String (s + 1) : "f" + String (s - Engine::kIns + 1), (int) c.getX() - 4, gridArea.getY() + 26, (int) c.getWidth() + 8, 12, Justification::centred);
            float v = meterPos (engine.srcLevel[(size_t) s].load());
            g.setColour (track); g.fillRect (c.getX(), (float) gridArea.getY() + 40, c.getWidth(), 3.0f);
            g.setColour (s < Engine::kIns ? fill : hot); g.fillRect (c.getX(), (float) gridArea.getY() + 40, c.getWidth() * v, 3.0f);
        }
        for (int o = 0; o < outs; ++o)
        {
            auto c0 = cell (o, 0);
            const Colour uc = unitColour (o / 2);
            g.setColour (modular ? ink : uc);
            g.drawText ("#" + String (o / 2 + 1) + (o % 2 ? " r" : " l"), gridArea.getX(), (int) c0.getY() - 1, 44, (int) c0.getHeight() + 2, Justification::left);
            auto mask = engine.route[o].load();
            for (int s = 0; s < srcs; ++s)
            {
                auto c = cell (o, s);
               #if SCQ_LEGACY
                Path p; p.addRectangle (c);
               #else
                auto p = pg (c, c.getHeight() * 0.5f);
               #endif
                bool on = (mask >> s) & 1u;
                if (on)
                {
                    g.setColour (uc.withAlpha (0.35f + 0.65f * jlimit (0.0f, 1.0f, meterPos (engine.srcLevel[(size_t) s].load()) * 1.2f)));
                    g.fillPath (p);
                }
                else { g.setColour (track.withAlpha (0.8f)); g.strokePath (p, PathStrokeType (0.7f)); }
            }
            auto e = cell (o, srcs);
            segMeter (g, { e.getX() + 8, e.getY() + 2, 90, e.getHeight() - 4 }, shown[o], engine.outRms[o].load(), peakHold[o], uc, false);
        }
        g.setColour (dim); g.setFont (f (10, true));
        if (outs > 0)
            g.drawText ("click a square: that source plays out of that output. a source can go to many outputs.",
                        gridArea.getX(), (int) cell (outs, 0).getY() + 6, gridArea.getWidth(), 14, Justification::left);
        else
            g.drawText ("no outputs yet", gridArea.getX() + 46, gridArea.getY() + 56, 300, 16, Justification::left);
    }

    void paint (Graphics& g) override
    {
        g.fillAll (look::field);
        paintRail (g);
        paintStage (g);
        paintHistory (g);
       #if ! SCQ_LEGACY
        paintSpectra (g);
       #endif
        paintUnits (g);
        paintSources (g);
        paintGrid (g);
    }
};

// ---------------------------------------------------------------------------- app
struct ArrayApp : JUCEApplication
{
    struct Window : DocumentWindow
    {
        explicit Window (bool demo) : DocumentWindow ("Sleepcousonique Array", look::field, allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new ArrayComponent (demo), true);
            setResizable (true, true);
            setResizeLimits (1100, 760, 2600, 1700);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    const String getApplicationName() override { return "Sleepcousonique Array"; }
    const String getApplicationVersion() override { return "1.2.0"; }
    void initialise (const String&) override
    {
        look::sans = Typeface::createSystemTypefaceFor (ArrayFonts::InterRegular_ttf, ArrayFonts::InterRegular_ttfSize);
        look::sansMed = Typeface::createSystemTypefaceFor (ArrayFonts::InterMedium_ttf, ArrayFonts::InterMedium_ttfSize);
        look::mono = Typeface::createSystemTypefaceFor (ArrayFonts::IBMPlexMonoRegular_ttf, ArrayFonts::IBMPlexMonoRegular_ttfSize);
        auto args = StringArray::fromTokens (getCommandLineParameters(), true);
        if (int pr = args.indexOf ("--probe"); pr >= 0 && pr + 1 < args.size())     // see agg::answers
        {
            setApplicationReturnValue (agg::probe (args[pr + 1].unquoted()));
            quit();
            return;
        }
        const bool demo = args.contains ("--demo");
        window = std::make_unique<Window> (demo);
        // for tests: --iface puts every SP-1 into interface mode, --play FILE opens a wav and plays
        // it looped, --for SECONDS quits after that long (leaving the SP-1s as they are)
        if (auto* ac = dynamic_cast<ArrayComponent*> (window->getContentComponent()))
        {
            if (args.contains ("--iface")) ac->ifaceAll();
            if (int pl = args.indexOf ("--play"); pl >= 0 && pl + 1 < args.size())
                ac->playFile (File::getCurrentWorkingDirectory().getChildFile (args[pl + 1].unquoted()));
        }
        if (int fo = args.indexOf ("--for"); fo >= 0 && fo + 1 < args.size())
            Timer::callAfterDelay (jmax (1, args[fo + 1].getIntValue()) * 1000, [] { JUCEApplication::getInstance()->systemRequestedQuit(); });
        int shot = args.indexOf ("--shot");
        if (shot >= 0 && shot + 1 < args.size())
        {
            auto out = File::getCurrentWorkingDirectory().getChildFile (args[shot + 1].unquoted());
            Timer::callAfterDelay (2500, [this, out]
            {
                auto* c = window->getContentComponent();
                auto img = c->createComponentSnapshot (c->getLocalBounds(), true, 2.0f);
                FileOutputStream os (out);
                if (os.openedOk()) { os.setPosition (0); os.truncate(); PNGImageFormat().writeImageToStream (img, os); }
                quit();
            });
        }
    }
    void shutdown() override { window = nullptr; look::sans = nullptr; look::sansMed = nullptr; look::mono = nullptr; }
    std::unique_ptr<Window> window;
};

START_JUCE_APPLICATION (ArrayApp)
