#pragma once
#include <juce_core/juce_core.h>
#include <vector>

// The Mac's audio devices, and the one device the Array makes of them: every SP-1 in
// interface mode ("Sleepcousonique" sound cards) plus, optionally, a loopback input such as
// BlackHole, joined by Core Audio into one aggregate device. Core Audio keeps the members in
// time (drift correction), so the Array sees one device with 2 outputs per SP-1.
namespace agg
{
struct Device
{
    juce::uint32 id = 0;
    juce::String name, uid;
    int ins = 0, outs = 0;
    bool isSp1() const { return name.containsIgnoreCase ("Sleepcousonique"); }
    juce::String serial() const;      // the SP-1's USB serial number, from Core Audio's UID
};

std::vector<Device> list();

// (Re)makes the aggregate: SP-1s in `sp1Uids` order (outputs 1-2, 3-4, ...), then the loopback
// input (or none). shared = visible to other apps (a DAW can play straight into it).
// Returns its name, or "" (and why, in err).
juce::String make (const juce::StringArray& sp1Uids, const juce::String& loopUid, bool shared, juce::String& err);
void destroy();

// A loopback device (BlackHole) that is muted or at volume 0 passes pure silence: the volume
// keys turn it down whenever it is the Mac's output. Turns it back to full; true if it had to.
bool unmute (const juce::String& uid);

constexpr const char* kName = "SP-1 Array";
constexpr const char* kUid = "com.sleepcousonique.array";
}
