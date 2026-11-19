#include "Host.h"
#include "scq.h"
#include "Scales.h"

Host::Host()
{
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    devices.initialise (0, 2, nullptr, true);
    devices.getAudioDeviceSetup (setup);
    setup.sampleRate = 48000;
    devices.setAudioDeviceSetup (setup, true);
    devices.addAudioCallback (this);
    openMidiInputs();
    midiList = juce::MidiDeviceListConnection::make ([this] { openMidiInputs(); });
    startTimerHz (50);
}

// the room, 50 times a second: forget whoever has gone quiet, and pass on what was heard
void Host::timerCallback()
{
    radio.setWanted (roomAllowed && wantRoom);
    if (! radio.wanted() && ! radio.pretending())
    {
        roomLive = 0;
        return;
    }
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (radio.pretending()) radio.pretendTick (now);
    radio.room.tick (now);
    const auto w = radio.room.weather();
    roomCount = w.count;
    roomNear = w.near;
    roomStrength = w.strength;
    roomLive = (radio.pretending() || radio.state() == radio::State::listening) ? 1 : 0;
    for (auto& e : radio.room.takeEvents())
    {
        const auto wr = roomFifo.write (1);
        if (wr.blockSize1 + wr.blockSize2 < 1) break;             // a very busy room: the rest is dropped
        roomRing[(size_t) (wr.blockSize1 > 0 ? wr.startIndex1 : wr.startIndex2)] = e;
    }
}

Host::~Host()
{
    stopTimer();
    radio.setWanted (false);
    for (auto& d : juce::MidiInput::getAvailableDevices()) devices.removeMidiInputDeviceCallback (d.identifier, this);
    devices.removeAudioCallback (this);
}

void Host::openMidiInputs()
{
    for (auto& d : juce::MidiInput::getAvailableDevices())
    {
        if (devices.isMidiInputDeviceEnabled (d.identifier)) continue;
        devices.setMidiInputDeviceEnabled (d.identifier, true);
        devices.addMidiInputDeviceCallback (d.identifier, this);
    }
}

juce::StringArray Host::midiInputs() const
{
    juce::StringArray s;
    for (auto& d : juce::MidiInput::getAvailableDevices())
        if (devices.isMidiInputDeviceEnabled (d.identifier)) s.add (d.name);
    return s;
}

void Host::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    if (m.isSysEx()) return;
    auto n = m.getRawDataSize();
    if (n <= 0 || n > 3) return;
    const auto w = midiFifo.write (n);
    if (w.blockSize1 + w.blockSize2 < n) return;              // full: drop it
    auto* d = m.getRawData();
    for (int i = 0; i < w.blockSize1; ++i) midiRing[(size_t) (w.startIndex1 + i)] = d[i];
    for (int i = 0; i < w.blockSize2; ++i) midiRing[(size_t) (w.startIndex2 + i)] = d[w.blockSize1 + i];
}

void Host::audioDeviceAboutToStart (juce::AudioIODevice* d)
{
    const juce::ScopedLock sl (lock);
    rate = d->getCurrentSampleRate();
    for (auto& i : interp) i.reset();
    for (auto& f : fifo) f.clear();
}

void Host::audioDeviceIOCallbackWithContext (const float* const*, int, float* const* out, int numOut, int frames,
                                             const juce::AudioIODeviceCallbackContext&)
{
    for (int c = 0; c < numOut; ++c) if (out[c]) juce::FloatVectorOperations::clear (out[c], frames);
    const juce::ScopedTryLock sl (lock);
    if (! sl.isLocked() || numOut < 1) return;

    {
        const auto rd = midiFifo.read (midiFifo.getNumReady());
        if (rd.blockSize1 > 0) scq_midi (midiRing.data() + rd.startIndex1, rd.blockSize1);
        if (rd.blockSize2 > 0) scq_midi (midiRing.data() + rd.startIndex2, rd.blockSize2);
    }
    const double ratio = 48000.0 / rate;
    const int need = (int) std::ceil (frames * ratio) + 8;
    float l[SCQ_BLOCK], r[SCQ_BLOCK];
    scq_room (roomLive.load(), roomCount.load(), roomNear.load(), roomStrength.load());
    while ((int) fifo[0].size() < need)
    {
        if (roomFifo.getNumReady() > 0)                           // one thing heard per block
        {
            const auto rd = roomFifo.read (1);
            const auto& e = roomRing[(size_t) (rd.blockSize1 > 0 ? rd.startIndex1 : rd.startIndex2)];
            scq_room_event (e.kind, e.voice);
        }
        scq_render (l, r, SCQ_BLOCK);
        fifo[0].insert (fifo[0].end(), l, l + SCQ_BLOCK);
        fifo[1].insert (fifo[1].end(), r, r + SCQ_BLOCK);
    }
    float* dst[2] = { out[0], numOut > 1 ? out[1] : nullptr };
    int used = 0;
    for (int c = 0; c < 2; ++c)
    {
        std::vector<float> tmp ((size_t) frames);
        if (std::abs (ratio - 1.0) < 1e-9) { std::copy (fifo[c].begin(), fifo[c].begin() + frames, tmp.begin()); used = frames; }
        else used = interp[c].process (ratio, fifo[c].data(), tmp.data(), frames);
        if (dst[c]) std::copy (tmp.begin(), tmp.end(), dst[c]);
        else if (c == 1) juce::FloatVectorOperations::addWithMultiply (out[0], tmp.data(), 0.0f, frames);
    }
    for (auto& f : fifo) f.erase (f.begin(), f.begin() + used);
    float pk = 0;
    for (int i = 0; i < frames; ++i) pk = juce::jmax (pk, std::abs (out[0][i]));
    level = juce::jmax (pk, level * 0.9f);
}

static Host::Result readBack (int errors)
{
    return { juce::String::fromUTF8 (scq_describe()), juce::String::fromUTF8 (scq_messages()), errors };
}

Host::Result Host::load (const juce::String& text, juce::uint32 nights)
{
    auto utf8 = scales::withTunings (text).toStdString();     // the scales folder's tunings it names
    const juce::ScopedLock sl (lock);
    int e = scq_load (utf8.data(), (int) utf8.size(), nights);
    auto r = readBack (e);
    noteRoom (r.describe);
    return r;
}

// Does the scene that plays listen to the room? (its N <id> <place> ... lines.) The timer
// starts and stops the radio from this, on the message thread and never under the audio lock;
// a crowd in another scene starts the listening when that scene comes.
void Host::noteRoom (const juce::String& described)
{
    bool wants = false;
    for (auto& line : juce::StringArray::fromLines (described))
        if (line.startsWith ("N "))
        {
            auto place = line.fromFirstOccurrenceOf (" ", false, false).fromFirstOccurrenceOf (" ", false, false).upToFirstOccurrenceOf (" ", false, false);
            if (place == "crowd" || place == "visitors") wants = true;
        }
    wantRoom = wants;
}

juce::uint32 Host::healed() const { return scq_healed(); }

Host::Result Host::describe()
{
    Result r;
    {
        const juce::ScopedLock sl (lock);
        r = readBack (0);
    }
    noteRoom (r.describe);
    return r;
}

Host::Result Host::check (const juce::String& text, juce::uint32 nights)
{
    auto utf8 = scales::withTunings (text).toStdString();
    std::vector<char> msgs (4096);
    const juce::ScopedLock sl (lock);
    int e = scq_check (utf8.data(), (int) utf8.size(), nights, msgs.data(), (int) msgs.size());
    Result r { {}, juce::String::fromUTF8 (msgs.data()), e, {} };
    for (int k = 0; k < scq_checked_scenes(); ++k) r.sceneLoads.push_back (scq_checked_load (k));
    return r;
}

void Host::setAmount (int cord, float amount) { const juce::ScopedLock sl (lock); scq_set_amount (cord, amount); }

void Host::setTune (int node, float o) { const juce::ScopedLock sl (lock); scq_set_tune (node, o); }
void Host::setAtt (int node, int in, float a) { const juce::ScopedLock sl (lock); scq_set_att (node, in, a); }

void Host::setParam (int node, int k, float v) { const juce::ScopedLock sl (lock); scq_set_param (node, k, v); }

void Host::setKnob (int node, int in, float v) { const juce::ScopedLock sl (lock); scq_set_knob (node, in, v); }

void Host::gotoScene (int s) { const juce::ScopedLock sl (lock); scq_goto_scene (s); }
int Host::scene() { return scq_scene(); }
void Host::control (int which, float v) { scq_control (which, v); }
float Host::lamp (int led) { return scq_lamp (led); }
float Host::probe (int node, int port) { return scq_probe (node, port); }
