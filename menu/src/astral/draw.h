// Astral-Remake - made by kdt255 (https://github.com/kdt255/Astral-Remake)
// Free software: MIT + Commons Clause, selling is not permitted. If you paid for this, you got scammed.
#pragma once
// ImDrawList primitives the stock API lacks: alpha-aware linear gradients on rounded shapes, feathered glows,
// stars, cap-height centred text and the small vector glyphs (chevron, plus, check) used across the menu.
#include "imgui.h"
#include "imgui_internal.h"

#include <cmath>

namespace astral::draw {

[[nodiscard]] inline float Snap(float v) noexcept { return std::floor(v + 0.5f); }
[[nodiscard]] inline ImVec2 Snap(const ImVec2& v) noexcept { return ImVec2(Snap(v.x), Snap(v.y)); }

[[nodiscard]] inline ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t) noexcept
{
    return ImVec4(ImLerp(a.x, b.x, t), ImLerp(a.y, b.y, t), ImLerp(a.z, b.z, t), ImLerp(a.w, b.w, t));
}

[[nodiscard]] inline ImVec4 Fade(const ImVec4& c, float alpha) noexcept { return ImVec4(c.x, c.y, c.z, c.w * alpha); }

// Packs through GetColorU32(ImVec4) so style.Alpha (window fades, BeginDisabled, search dimming) always applies.
[[nodiscard]] inline ImU32 Col(const ImVec4& c, float alpha = 1.0f) { return ImGui::GetColorU32(Fade(c, alpha)); }

// Recolors vertices [vtx_begin, end) along p0->p1, interpolating all four channels; the alpha already emitted per
// vertex (anti-aliasing fringe) is kept as a multiplier, so emit the source shape in opaque white.
void ShadeLinear(ImDrawList* dl, int vtx_begin, const ImVec2& p0, const ImVec2& p1, ImU32 c0, ImU32 c1);

void RectGradient(ImDrawList* dl, const ImVec2& min, const ImVec2& max, ImU32 c0, ImU32 c1, float rounding,
                  ImDrawFlags flags, bool horizontal);

// Feathered halo outside a rounded rect (rounding >= half the size gives a circle). `color` carries the peak alpha at
// the shape edge; it falls off as (1 - d/size)^1.55 to zero at `size`. Emit it before the shape it surrounds.
void Glow(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float rounding, ImU32 color, float size,
          const ImVec2& offset = ImVec2(0.0f, 0.0f));

void Star(ImDrawList* dl, const ImVec2& center, float radius, float rotation, ImU32 col);

struct CapMetrics {
    float top = 0.0f;       // offset of the cap line below the text origin
    float baseline = 0.0f;  // offset of the baseline below the text origin
};

[[nodiscard]] CapMetrics Cap(ImFont* font, float size);
[[nodiscard]] float TextWidth(ImFont* font, float size, const char* begin, const char* end = nullptr);
void Text(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 col, const char* begin,
          const char* end = nullptr);

// Places text so its cap height is centred on `center_y` (the way every label in the reference is aligned).
// Returns the snapped origin used.
ImVec2 TextCapCentered(ImDrawList* dl, ImFont* font, float size, float x, float center_y, ImU32 col,
                       const char* begin, const char* end = nullptr);

// Text whose cap top lands exactly on `cap_top` (captions above frames, slider labels).
ImVec2 TextCapTop(ImDrawList* dl, ImFont* font, float size, float x, float cap_top, ImU32 col, const char* begin,
                  const char* end = nullptr);

void Chevron(ImDrawList* dl, const ImVec2& center, float width, float height, bool up, ImU32 col, float thickness);
void Plus(ImDrawList* dl, const ImVec2& center, float half, float thickness, ImU32 col);
void Minus(ImDrawList* dl, const ImVec2& center, float half, float thickness, ImU32 col);

// Check mark inside a square box of `size`; `progress` 0..1 draws the stroke partially (draw-on animation).
void Check(ImDrawList* dl, const ImVec2& box_min, float size, ImU32 col, float thickness, float progress);

// Vertical/horizontal hue strip made of six gradient quads.
void HueStrip(ImDrawList* dl, const ImVec2& min, const ImVec2& max, bool vertical, float alpha);
void Checkerboard(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float cell, ImU32 a, ImU32 b);

} // namespace astral::draw
