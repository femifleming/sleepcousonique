#pragma once
#include <juce_core/juce_core.h>

// Sends a dream to an SP-1 running the Sleepcousonique firmware, over its USB serial port.
//
// Protocol (text, then bytes):
//   host -> SP-1   "SCQ LOAD <bytes> <slot>\n" followed by exactly <bytes> of UTF-8 patch text
//   SP-1 -> host   "OK <errors>\n"  or  "ERR <reason>\n"
//   host -> SP-1   "SCQ HELLO\n"     SP-1 -> host  "SLEEPCOUSONIQUE <version>\n"
namespace serial
{
juce::StringArray candidates();                      // ports that might be an SP-1
juce::String send (const juce::String& text, int slot, juce::String& portUsed);   // "" on success, else why not

// The live link: holds the SP-1's port open while it's plugged in, follows its hands
// ("SCQ HANDS ..." lines, about 30 a second) and sends dreams over the same connection.
class Link : private juce::Thread
{
public:
    Link();
    ~Link() override;

    struct Hands
    {
        float fader[4] {};
        juce::uint32 buttons = 0;     // bits: doors 0-3, PLAY 4, dots 5, FFWD 6, RWD 7, VOL+ 8, VOL- 9
        int scene = 1, slot = 1;
        float lamp[8] {};
        juce::uint32 count = 0;       // goes up with every report
    };
    bool connected() const { return isConnected.load(); }
    juce::String port() const;
    Hands hands() const;

    // "" on success, else why not. Blocks the caller for up to ~12 s.
    juce::String send (const juce::String& text, int slot);

private:
    void run() override;
    bool connect();
    void handle (const juce::String& line);
    void drop();

    int fd = -1;
    std::atomic<bool> isConnected { false };
    juce::String portName;
    mutable juce::CriticalSection lock;
    Hands latest;
    juce::String partial;

    // a send handed to the link thread
    juce::CriticalSection sendLock;
    juce::String pendingText, sendResult;
    int pendingSlot = -1;
    bool sendRequested = false, awaitingReply = false;
    juce::uint32 replyDeadline = 0;
    juce::WaitableEvent sendDone;
};
}
