# Atelier design rules

Every Atelier app looks and behaves as one suite. Diorama is the reference: when in doubt, match it
(`D:\cc\Diorama\src\gui.cpp`, `applyStyle()`; screenshot `D:\cc\Diorama\images\window.jpg`).
These rules are for every window: the apps, their dialogs, the launcher, and engines with their own UI (Demiurge's
editor theme uses the same values). Web pages follow the colours too.

## Craft ports

Chimera (PhotoCraft), Polyhymnia (SoundCraft) and Morpheus (EffectCraft) **keep the original app's UI exactly**:
layout, colours, icons, fonts, spacing and behaviour, as the original has them. Sections 1-5 and 8-9 do not apply to
them. What they do take from Atelier: the splash screen, the 12 languages, opening empty, Inoculant on image output,
and everything in section 7 (installer, shortcut names, our painted logo as the app icon, README, site).

## 1. Colours

| Token | Hex | Used for |
| --- | --- | --- |
| `BG` | `#1a1a1a` | menu bar, status bar, behind everything |
| `PANEL` | `#242424` | panels, windows, child areas, inactive tabs |
| `POPUP` | `#212121` | menus, popups, tooltips |
| `WELL` | `#141414` | input fields, slider tracks, viewports' frames, code editors |
| `RAISED` | `#2e2e2e` | a field or item under the mouse |
| `BUTTON` | `#333333` | buttons at rest, hovered list rows |
| `LINE` | `#3a3a3a` | borders, separators, dividers |
| `TEXT` | `#e6e6e6` | text |
| `DIM` | `#9a9a9a` | secondary text, hints, disabled |
| `BLUE` | `#007bb7` | **the Atelier blue**: slider grabs, check marks, the pressed button, the active/selected item, hovered tab, drag-and-drop target, links |
| `DEEP` | `#005184` | active tab, panel and section headers, selected rows, hovered buttons, the grab while dragging |
| `WARN` | `#f0b060` | warnings |
| `ERROR` | `#f07070` | errors |
| `OK` | `#5ab04a` | success, "running" |

- **Solid colours only:** no gradients, no shadows, no glows, no transparency effects (a modal dims the window
  behind it to 50 % black; that's the one exception).
- **Blue is for controls and selection.** Content keeps its own colours: track and clip colours, layer label colours,
  the user's pictures. Pink (Cursed's colour) is not used in Atelier apps' controls; it may appear in a splash or
  logo where the app's art uses it.
- Text on `BLUE` or `DEEP` is `TEXT` (white-ish), never black.

## 2. Shape and spacing

| Setting | Value |
| --- | --- |
| Window / panel corners | **0** (square) |
| Frames, buttons, tabs, popups, scrollbars, grabs | **2 px** rounding at most |
| Window border | 0 (panels are separated by `LINE` dividers) |
| Popup border | 1 px `LINE` |
| Window padding | 10 x 10 |
| Frame padding | 6 x 4 |
| Item spacing | 6 x 6 |
| Indent | 16 |
| Scrollbar | thin, `PANEL` track, `BUTTON` grab, `BLUE` while dragged |

## 3. Type

- **UI font:** Segoe UI (`C:\Windows\Fonts\segoeui.ttf`), 16 px; bold Segoe UI for titles and headers; symbols
  merged from Segoe UI Symbol. Linux/macOS: the system sans at the same size.
- **Code and numbers that line up:** Consolas 16 px (counters, timecode, script editors).
- Sentence case for labels and menu items ("Import audio..."), Title Case only for proper names and Photoshop-style
  command names the original uses ("Make Work Path").
- Commands that open a dialog end with "...".

## 4. Layout

- **Menu bar** at the top on `BG`, then the tool/options bar, then the work area, then a **status bar** on `BG` at
  the bottom (file name, state, size, what's happening).
- **Controls in a panel on the left**, the work (canvas, timeline, viewport) in the middle, lists and properties on
  the right (an Inspector, the Layers panel, "your songs"). Apps ported from a Craft app follow
  the original instead (see **Craft ports** above).
- **Every panel is a dock with a header tab** as Diorama's: a dropdown arrow at the left, the title, a close button
  at the right; the active tab `DEEP`, others `PANEL`. Panels can be resized (drag handles, sizes remembered),
  moved, tabbed together and closed, and Window > Reset Layout brings the default back.
- **Inspector sections** have a full-width `DEEP` header bar with a fold arrow, an enable checkbox where it applies,
  and a "..." menu at the right (Diorama's Transform / Picture / Animate).
- Engines use the Unity-like default: Hierarchy left, Scene/viewport middle, Inspector right, Project and Console
  along the bottom.

## 5. Controls

- **Every creative parameter is a slider** (`BLUE` grab on a `WELL` track, the value written on it, double-click or
  Ctrl+click to type a number, right-click to reset). Prompts and presets set the sliders; they never hide them.
- Buttons: `BUTTON` at rest, `DEEP` hovered, `BLUE` pressed; one primary button per dialog may sit in `BLUE`.
- Check boxes with a `BLUE` tick; drop-downs on `WELL` with a down arrow at the right.
- Icons: the original Craft app's exact icons in ports; otherwise one consistent icon set, white, tinted per state.
  Never letters as icons.
- Tooltips on every icon button, with the shortcut.

## 6. Behaviour

- **Opens empty.** "Open the last project at start" is a Preferences option, off by default.
- **A splash screen** while loading, in the LayerTack style used across Atelier: the app's logo drawn large, its name in
  two tones, the version, credits and what's loading; click or Esc skips it.
- **Keyboard shortcuts take priority** over whatever widget has focus (one global handler before any window), except
  while typing in a text field. Space never presses a focused button. Menus show the same shortcut text the handler
  uses.
- **Undo for everything** that changes the document; the History lists it.
- **Before / after:** apps that change a picture (not Chimera) show Mimic's wipe on the picture, keys 1 / 2 / 3.
- **Image apps** offer "Save with Inoculant poison" on output and "Cure poison" on import.
- **12 languages:** English plus Japanese, Chinese, Spanish, Korean, French, German, Russian, Serbian, Afrikaans,
  Yucatec Maya and Pennsylvania Dutch, from `lang/` packs.
- **Feature-rich:** port everything the original has and add the obvious extras.

## 7. The suite around the app

- Installed to `%LOCALAPPDATA%\Programs\<App>`; shortcuts named **"Atelier - <App>"**; install.bat, .ps1 and .sh.
- Painted app logos (no SVG redraws); the app icon is the logo.
- README and GitHub Pages site (`index.html`, `site/style.css`, `site/site.js`, never inline), the Cursed footer,
  `FUNDING.yml`, `link.bf`, topics.
- No mention of an app's previous names, and no branding of the engine or library it's built on beyond the licence
  credit the licence requires.

## 8. Dear ImGui: the style in code

```cpp
// Atelier style (from Diorama). Call once after ImGui::CreateContext().
static ImVec4 col4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }
void applyAtelierStyle() {
    const ImU32 BG = IM_COL32(0x1a,0x1a,0x1a,255), PANEL = IM_COL32(0x24,0x24,0x24,255), POPUP = IM_COL32(0x21,0x21,0x21,255),
                WELL = IM_COL32(0x14,0x14,0x14,255), RAISED = IM_COL32(0x2e,0x2e,0x2e,255), BUTTON = IM_COL32(0x33,0x33,0x33,255),
                LINE = IM_COL32(0x3a,0x3a,0x3a,255), TEXT = IM_COL32(0xe6,0xe6,0xe6,255), DIM = IM_COL32(0x9a,0x9a,0x9a,255),
                BLUE = IM_COL32(0x00,0x7b,0xb7,255), DEEP = IM_COL32(0x00,0x51,0x84,255);
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0; s.FrameRounding = 2; s.GrabRounding = 2; s.PopupRounding = 2; s.ScrollbarRounding = 2; s.TabRounding = 2;
    s.WindowBorderSize = 0; s.FrameBorderSize = 0; s.PopupBorderSize = 1; s.IndentSpacing = 16;
    s.WindowPadding = ImVec2(10, 10); s.FramePadding = ImVec2(6, 4); s.ItemSpacing = ImVec2(6, 6);
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = col4(TEXT); c[ImGuiCol_TextDisabled] = col4(DIM);
    c[ImGuiCol_WindowBg] = col4(PANEL); c[ImGuiCol_ChildBg] = col4(PANEL); c[ImGuiCol_PopupBg] = col4(POPUP);
    c[ImGuiCol_Border] = col4(LINE); c[ImGuiCol_Separator] = col4(LINE);
    c[ImGuiCol_FrameBg] = col4(WELL); c[ImGuiCol_FrameBgHovered] = col4(RAISED); c[ImGuiCol_FrameBgActive] = col4(RAISED);
    c[ImGuiCol_MenuBarBg] = col4(BG); c[ImGuiCol_TitleBg] = col4(BG); c[ImGuiCol_TitleBgActive] = col4(BG);
    c[ImGuiCol_CheckMark] = col4(BLUE); c[ImGuiCol_SliderGrab] = col4(BLUE); c[ImGuiCol_SliderGrabActive] = col4(DEEP);
    c[ImGuiCol_Button] = col4(BUTTON); c[ImGuiCol_ButtonHovered] = col4(DEEP); c[ImGuiCol_ButtonActive] = col4(BLUE);
    c[ImGuiCol_Header] = col4(DEEP); c[ImGuiCol_HeaderHovered] = col4(BUTTON); c[ImGuiCol_HeaderActive] = col4(BLUE);
    c[ImGuiCol_Tab] = col4(PANEL); c[ImGuiCol_TabActive] = col4(DEEP); c[ImGuiCol_TabHovered] = col4(BLUE);
    c[ImGuiCol_ResizeGrip] = col4(PANEL); c[ImGuiCol_ResizeGripHovered] = col4(DEEP); c[ImGuiCol_ResizeGripActive] = col4(BLUE);
    c[ImGuiCol_ScrollbarBg] = col4(PANEL); c[ImGuiCol_ScrollbarGrab] = col4(BUTTON); c[ImGuiCol_ScrollbarGrabActive] = col4(BLUE);
    c[ImGuiCol_DragDropTarget] = col4(BLUE); c[ImGuiCol_NavHighlight] = col4(BLUE);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.5f);
}
```

## 9. Checking a screen

Before committing a UI change, put its screenshot beside Diorama's and check: square panels, no gradient or shadow,
only the tokens above, blue only on controls and selection, every panel with its header tab, controls left, sliders
for parameters, the status bar, nothing that says another product's name.
