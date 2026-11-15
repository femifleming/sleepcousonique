#include "Aggregate.h"

#if JUCE_MAC
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>

namespace agg
{
static juce::String cfString (CFStringRef s)
{
    if (s == nullptr) return {};
    char buf[512];
    if (CFStringGetCString (s, buf, sizeof buf, kCFStringEncodingUTF8)) return juce::String::fromUTF8 (buf);
    return {};
}

static juce::String stringProp (AudioObjectID id, AudioObjectPropertySelector sel)
{
    AudioObjectPropertyAddress a { sel, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    CFStringRef s = nullptr;
    UInt32 size = sizeof s;
    if (AudioObjectGetPropertyData (id, &a, 0, nullptr, &size, &s) != noErr || s == nullptr) return {};
    auto out = cfString (s);
    CFRelease (s);
    return out;
}

static int channels (AudioObjectID id, AudioObjectPropertyScope scope)
{
    AudioObjectPropertyAddress a { kAudioDevicePropertyStreamConfiguration, scope, kAudioObjectPropertyElementMain };
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize (id, &a, 0, nullptr, &size) != noErr || size == 0) return 0;
    juce::HeapBlock<char> mem (size);
    auto* list = reinterpret_cast<AudioBufferList*> (mem.getData());
    if (AudioObjectGetPropertyData (id, &a, 0, nullptr, &size, list) != noErr) return 0;
    int n = 0;
    for (UInt32 i = 0; i < list->mNumberBuffers; ++i) n += (int) list->mBuffers[i].mNumberChannels;
    return n;
}

std::vector<Device> list()
{
    std::vector<Device> out;
    AudioObjectPropertyAddress a { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize (kAudioObjectSystemObject, &a, 0, nullptr, &size) != noErr) return out;
    std::vector<AudioObjectID> ids (size / sizeof (AudioObjectID));
    if (AudioObjectGetPropertyData (kAudioObjectSystemObject, &a, 0, nullptr, &size, ids.data()) != noErr) return out;
    for (auto id : ids)
    {
        Device d;
        d.id = id;
        d.name = stringProp (id, kAudioObjectPropertyName);
        d.uid = stringProp (id, kAudioDevicePropertyDeviceUID);
        d.ins = channels (id, kAudioDevicePropertyScopeInput);
        d.outs = channels (id, kAudioDevicePropertyScopeOutput);
        if (d.uid == kUid) continue;                     // our own aggregate
        out.push_back (d);
    }
    return out;
}

// Apple's USB audio UIDs read "AppleUSBAudioEngine:<maker>:<product>:<serial or location>:<n>".
juce::String Device::serial() const
{
    auto parts = juce::StringArray::fromTokens (uid, ":", "");
    if (parts.size() >= 4 && parts[0].startsWith ("AppleUSBAudioEngine")) return parts[3];
    return uid;
}

static AudioObjectID findByUid (const juce::String& uid)
{
    for (auto& d : list()) if (d.uid == uid) return d.id;
    AudioObjectPropertyAddress a { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    UInt32 size = 0;
    AudioObjectGetPropertyDataSize (kAudioObjectSystemObject, &a, 0, nullptr, &size);
    std::vector<AudioObjectID> ids (size / sizeof (AudioObjectID));
    AudioObjectGetPropertyData (kAudioObjectSystemObject, &a, 0, nullptr, &size, ids.data());
    for (auto id : ids) if (stringProp (id, kAudioDevicePropertyDeviceUID) == uid) return id;
    return 0;
}

void destroy()
{
    if (auto id = findByUid (kUid)) AudioHardwareDestroyAggregateDevice (id);
}

bool unmute (const juce::String& uid)
{
    auto id = findByUid (uid);
    if (id == 0) return false;
    bool changed = false;
    for (auto scope : { kAudioDevicePropertyScopeOutput, kAudioDevicePropertyScopeInput })
    {
        AudioObjectPropertyAddress pv { kAudioDevicePropertyVolumeScalar, scope, kAudioObjectPropertyElementMain };
        AudioObjectPropertyAddress pm { kAudioDevicePropertyMute, scope, kAudioObjectPropertyElementMain };
        Float32 v = 1.0f; UInt32 m = 0, size = sizeof v;
        if (AudioObjectHasProperty (id, &pv) && AudioObjectGetPropertyData (id, &pv, 0, nullptr, &size, &v) == noErr && v < 0.99f)
        {
            Float32 one = 1.0f;
            changed |= AudioObjectSetPropertyData (id, &pv, 0, nullptr, sizeof one, &one) == noErr;
        }
        size = sizeof m;
        if (AudioObjectHasProperty (id, &pm) && AudioObjectGetPropertyData (id, &pm, 0, nullptr, &size, &m) == noErr && m != 0)
        {
            UInt32 zero = 0;
            changed |= AudioObjectSetPropertyData (id, &pm, 0, nullptr, sizeof zero, &zero) == noErr;
        }
    }
    return changed;
}

static CFStringRef cf (const juce::String& s) { return CFStringCreateWithCString (nullptr, s.toRawUTF8(), kCFStringEncodingUTF8); }

juce::String make (const juce::StringArray& sp1Uids, const juce::String& loopUid, bool shared, juce::String& err)
{
    destroy();
    if (sp1Uids.isEmpty()) { err = "no SP-1 in interface mode"; return {}; }

    CFMutableArrayRef subs = CFArrayCreateMutable (nullptr, 0, &kCFTypeArrayCallBacks);
    auto addSub = [&] (const juce::String& uid, bool drift)
    {
        CFMutableDictionaryRef d = CFDictionaryCreateMutable (nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFStringRef u = cf (uid);
        CFDictionarySetValue (d, CFSTR (kAudioSubDeviceUIDKey), u);
        int one = drift ? 1 : 0;
        CFNumberRef n = CFNumberCreate (nullptr, kCFNumberIntType, &one);
        CFDictionarySetValue (d, CFSTR (kAudioSubDeviceDriftCompensationKey), n);
        CFArrayAppendValue (subs, d);
        CFRelease (n); CFRelease (u); CFRelease (d);
    };
    // the first SP-1 is the clock; every other member is drift-corrected to it
    for (int i = 0; i < sp1Uids.size(); ++i) addSub (sp1Uids[i], i > 0);
    if (loopUid.isNotEmpty()) addSub (loopUid, true);

    CFMutableDictionaryRef desc = CFDictionaryCreateMutable (nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFStringRef name = cf (kName), uid = cf (kUid), clock = cf (sp1Uids[0]);
    int priv = shared ? 0 : 1, stacked = 0;
    CFNumberRef privN = CFNumberCreate (nullptr, kCFNumberIntType, &priv);
    CFNumberRef stackN = CFNumberCreate (nullptr, kCFNumberIntType, &stacked);
    CFDictionarySetValue (desc, CFSTR (kAudioAggregateDeviceNameKey), name);
    CFDictionarySetValue (desc, CFSTR (kAudioAggregateDeviceUIDKey), uid);
    CFDictionarySetValue (desc, CFSTR (kAudioAggregateDeviceSubDeviceListKey), subs);
    CFDictionarySetValue (desc, CFSTR (kAudioAggregateDeviceMainSubDeviceKey), clock);
    CFDictionarySetValue (desc, CFSTR (kAudioAggregateDeviceIsPrivateKey), privN);
    CFDictionarySetValue (desc, CFSTR (kAudioAggregateDeviceIsStackedKey), stackN);

    AudioObjectID dev = 0;
    OSStatus st = AudioHardwareCreateAggregateDevice (desc, &dev);
    CFRelease (privN); CFRelease (stackN); CFRelease (name); CFRelease (uid); CFRelease (clock);
    CFRelease (desc); CFRelease (subs);
    if (st != noErr || dev == 0) { err = "Core Audio would not make the aggregate (" + juce::String ((int) st) + ")"; return {}; }

    // 48 kHz: the SP-1's only rate
    AudioObjectPropertyAddress a { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    Float64 sr = 48000.0;
    AudioObjectSetPropertyData (dev, &a, 0, nullptr, sizeof sr, &sr);
    juce::Thread::sleep (300);           // let Core Audio publish it before JUCE looks
    return kName;
}
}
#else
namespace agg
{
std::vector<Device> list() { return {}; }
juce::String Device::serial() const { return uid; }
juce::String make (const juce::StringArray&, const juce::String&, bool, juce::String& err) { err = "the Array needs macOS for now"; return {}; }
void destroy() {}
bool unmute (const juce::String&) { return false; }
}
#endif
