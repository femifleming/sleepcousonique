#include "Host.h"
#include "scq.h"
#include "Scales.h"
#include "Fleet.h"

Host::Host()
{
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    devices.initialise (0, 2, nullptr, true);
    devices.getAudioDeviceSetup (setup);
    setup.sampleRate = 48000;
    devices.setAudioDeviceSetup (setup, true);
    devices.addAudioCallback (this);
}

Host::~Host() { devices.removeAudioCallback (this); }

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

    const double ratio = 48000.0 / rate;
    const int need = (int) std::ceil (frames * ratio) + 8;
    float l[SCQ_BLOCK], r[SCQ_BLOCK];
    while ((int) fifo[0].size() < need)
    {
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
    return readBack (e);
}

Host::Result Host::describe()
{
    const juce::ScopedLock sl (lock);
    return readBack (0);
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

void Host::setAmount (int cord, float amount) { { const juce::ScopedLock sl (lock); scq_set_amount (cord, amount); } if (fleet) fleet->setAmount (cord, amount); }

void Host::setTune (int node, float o) { { const juce::ScopedLock sl (lock); scq_set_tune (node, o); } if (fleet) fleet->setTune (node, o); }
void Host::setAtt (int node, int in, float a) { { const juce::ScopedLock sl (lock); scq_set_att (node, in, a); } if (fleet) fleet->setAtt (node, in, a); }

void Host::setParam (int node, int k, float v) { { const juce::ScopedLock sl (lock); scq_set_param (node, k, v); } if (fleet) fleet->setParam (node, k, v); }

void Host::gotoScene (int s) { { const juce::ScopedLock sl (lock); scq_goto_scene (s); } if (fleet) fleet->gotoScene (s); }
int Host::scene() { return scq_scene(); }
void Host::control (int which, float v) { scq_control (which, v); if (fleet) fleet->control (which, v); }
float Host::lamp (int led) { return scq_lamp (led); }
float Host::probe (int node, int port) { return scq_probe (node, port); }
