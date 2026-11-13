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
}
