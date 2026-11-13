// Atelier launcher - the splash: the logo's easel drawn large, the name, the version, the credit and what's loading.
// Solid colours only (Changeling greys + Cursed pink), drawn with ImGui's draw list so it's sharp at any size.
#include "splash.h"
#include "lang.h"

#include <imgui.h>

#include <cfloat>
#include <cstdio>

#ifndef ATELIER_VERSION
#define ATELIER_VERSION "1.0.0"
#endif

namespace Splash {

static ImFont *titleFont = nullptr, *smallFont = nullptr;

void setFonts(ImFont* title, ImFont* small) { titleFont = title; smallFont = small; }

static const ImU32 BG = IM_COL32(0x1a, 0x1a, 0x1a, 255), LINE = IM_COL32(0x33, 0x33, 0x33, 255),
                   TEXT = IM_COL32(0xd6, 0xd6, 0xd6, 255), DIM = IM_COL32(0x9a, 0x9a, 0x9a, 255),
                   PINK = IM_COL32(0x00, 0x7b, 0xb7, 255), DEEP = IM_COL32(0x00, 0x51, 0x84, 255),
                   GREY = IM_COL32(0x9e, 0x9e, 0x9e, 255), DARK = IM_COL32(0x3a, 0x3a, 0x3a, 255),
                   LIGHT = IM_COL32(0xe6, 0xe6, 0xe6, 255);

static void text(ImDrawList* dl, ImFont* f, float size, ImVec2 p, ImU32 col, const char* s) {
    if (!f) f = ImGui::GetFont();
    dl->AddText(f, size, p, col, s);
}

void draw(ImDrawList* dl, float x, float y, const char* status, float progress) {
    const ImVec2 o(x, y), end(x + W, y + H);
    dl->PushClipRect(o, end, true);
    dl->AddRectFilled(o, end, BG);

    // the art: the logo's easel, large, on the right (the logo's 512 x 512 drawing, scaled)
    const float s = 0.85f, ox = x + 490, oy = y - 20;
    auto P = [&](float px, float py) { return ImVec2(ox + (px - 256) * s, oy + py * s); };
    auto quad = [&](ImVec2 a, ImVec2 b, ImVec2 c, ImVec2 d, ImU32 col) { const ImVec2 q[4] = {a, b, c, d}; dl->AddConvexPolyFilled(q, 4, col); };
    quad(P(246, 70), P(266, 70), P(300, 470), P(280, 470), DARK);         // the back leg
    quad(P(244, 62), P(268, 62), P(140, 470), P(112, 470), GREY);         // the front legs, an A
    quad(P(244, 62), P(268, 62), P(400, 470), P(372, 470), GREY);
    dl->AddRectFilled(P(118, 108), P(394, 318), PINK);                    // the canvas
    quad(P(118, 250), P(394, 196), P(394, 244), P(118, 298), DEEP);      // a band of paint across it
    dl->AddRectFilled(P(96, 318), P(416, 342), LIGHT);                   // the ledge

    // the name, the suite, the version
    const float ts = 50, tx = x + 40, ty = y + 92;
    text(dl, titleFont, ts, ImVec2(tx, ty), TEXT, "Atelier");
    text(dl, nullptr, 16, ImVec2(tx + 2, ty + 62), PINK, T("Creative & Studio Suite"));
    char ver[96];
    snprintf(ver, sizeof ver, "%s " ATELIER_VERSION, T("Version"));
    text(dl, nullptr, 16, ImVec2(tx + 2, ty + 84), DIM, ver);

    // the credit, small
    const float cs = 13, cy = y + 306;
    text(dl, smallFont, cs, ImVec2(tx, cy), DIM, "Built with Dear ImGui, SDL2 and stb.");
    text(dl, smallFont, cs, ImVec2(tx, cy + 17), DIM, "\xc2\xa9 Cursed Entertainment");

    // what's loading, and how far along
    if (status) text(dl, nullptr, 16, ImVec2(tx, y + 362), TEXT, status);
    if (progress >= 0) {
        const float p = progress > 1 ? 1 : progress;
        dl->AddRectFilled(ImVec2(x, end.y - 3), end, LINE);
        dl->AddRectFilled(ImVec2(x, end.y - 3), ImVec2(x + W * p, end.y), PINK);
    }
    dl->AddRect(o, end, LINE);
    dl->PopClipRect();
}

}  // namespace Splash
