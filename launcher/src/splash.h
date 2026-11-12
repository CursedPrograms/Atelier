// Atelier launcher - the splash: shown in its own small window while the launcher starts, and again in Help > About.
// (After LayerTack's.)
#pragma once

struct ImDrawList;
struct ImFont;

namespace Splash {
constexpr float W = 640, H = 400;
void setFonts(ImFont* title, ImFont* small);    // loaded with the interface font (nullptr: ImGui's own)
// The splash, W x H, its top-left corner at x, y. status: the line at the bottom left (nullptr: none);
// progress 0..1: the pink line along the bottom edge (below 0: none).
void draw(ImDrawList* dl, float x, float y, const char* status, float progress);
}
