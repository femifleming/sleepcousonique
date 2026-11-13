#pragma once
#include <juce_graphics/juce_graphics.h>
#include <vector>

// The cable router: like Max's, every cable leaves its outlet to the right, travels in straight
// lines with right-angle turns around the boards, and comes into its inlet from the left.
// Cables avoid each other where they can, so a busy patch stays readable.
namespace router
{
struct Cable { juce::Point<float> from, to; int source = -1; };   // cables from one outlet share a source and bundle

// one path per cable, in the same order; obstacles are the boards
std::vector<juce::Path> route (const std::vector<juce::Rectangle<float>>& boards, const std::vector<Cable>& cables);
}
