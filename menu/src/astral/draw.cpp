// Astral-Remake - made by kdt255 (https://github.com/kdt255/Astral-Remake)
// Free software: MIT + Commons Clause, selling is not permitted. If you paid for this, you got scammed.
#include "astral/draw.h"

#include <cfloat>

namespace astral::draw {

namespace {

constexpr float kPi = 3.14159265358979323846f;

[[nodiscard]] inline int Channel(ImU32 c, int shift) noexcept { return static_cast<int>((c >> shift) & 0xFF); }

[[nodiscard]] inline ImU32 Pack(float r, float g, float b, float a) noexcept
{
    auto q = [](float v) { return static_cast<ImU32>(ImClamp(v, 0.0f, 255.0f) + 0.5f); };
    return (q(r) << IM_COL32_R_SHIFT) | (q(g) << IM_COL32_G_SHIFT) | (q(b) << IM_COL32_B_SHIFT) |
           (q(a) << IM_COL32_A_SHIFT);
}

} // namespace

void ShadeLinear(ImDrawList* dl, int vtx_begin, const ImVec2& p0, const ImVec2& p1, ImU32 c0, ImU32 c1)
{
    const ImVec2 d(p1.x - p0.x, p1.y - p0.y);
    const float len2 = d.x * d.x + d.y * d.y;
    const float inv = len2 > 0.0f ? 1.0f / len2 : 0.0f;

    const float r0 = static_cast<float>(Channel(c0, IM_COL32_R_SHIFT));
    const float g0 = static_cast<float>(Channel(c0, IM_COL32_G_SHIFT));
    const float b0 = static_cast<float>(Channel(c0, IM_COL32_B_SHIFT));
    const float a0 = static_cast<float>(Channel(c0, IM_COL32_A_SHIFT));
    const float dr = static_cast<float>(Channel(c1, IM_COL32_R_SHIFT)) - r0;
    const float dg = static_cast<float>(Channel(c1, IM_COL32_G_SHIFT)) - g0;
    const float db = static_cast<float>(Channel(c1, IM_COL32_B_SHIFT)) - b0;
    const float da = static_cast<float>(Channel(c1, IM_COL32_A_SHIFT)) - a0;

    ImDrawVert* const end = dl->VtxBuffer.Data + dl->VtxBuffer.Size;
    for (ImDrawVert* v = dl->VtxBuffer.Data + vtx_begin; v < end; ++v) {
        const float t = ImSaturate(((v->pos.x - p0.x) * d.x + (v->pos.y - p0.y) * d.y) * inv);
        const float coverage = static_cast<float>(Channel(v->col, IM_COL32_A_SHIFT)) / 255.0f;
        v->col = Pack(r0 + dr * t, g0 + dg * t, b0 + db * t, (a0 + da * t) * coverage);
    }
}

void RectGradient(ImDrawList* dl, const ImVec2& min, const ImVec2& max, ImU32 c0, ImU32 c1, float rounding,
                  ImDrawFlags flags, bool horizontal)
{
    if (max.x <= min.x || max.y <= min.y) return;
    const int first = dl->VtxBuffer.Size;
    dl->AddRectFilled(min, max, IM_COL32_WHITE, rounding, flags);
    ShadeLinear(dl, first, min, horizontal ? ImVec2(max.x, min.y) : ImVec2(min.x, max.y), c0, c1);
}

void Glow(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float rounding, ImU32 color, float size,
          const ImVec2& offset)
{
    const int peak = Channel(color, IM_COL32_A_SHIFT);
    if (peak == 0 || size <= 0.0f) return;

    constexpr int kRings = 6;
    const ImU32 rgb = color & ~IM_COL32_A_MASK;
    const ImVec2 mn(min.x + offset.x, min.y + offset.y);
    const ImVec2 mx(max.x + offset.x, max.y + offset.y);
    const float r0 = ImClamp(rounding, 0.0f, ImMin(mx.x - mn.x, mx.y - mn.y) * 0.5f);
    const int seg = ImClamp(static_cast<int>((r0 + size) * 0.5f), 3, 10);
    const int n = 4 * (seg + 1);
    const ImVec2 centers[4] = { ImVec2(mx.x - r0, mn.y + r0), ImVec2(mx.x - r0, mx.y - r0), ImVec2(mn.x + r0, mx.y - r0),
                                ImVec2(mn.x + r0, mn.y + r0) };
    const ImVec2 uv = dl->_Data->TexUvWhitePixel;

    dl->PrimReserve(kRings * n * 6, (kRings + 1) * n);
    const unsigned int base = dl->_VtxCurrentIdx; // read after PrimReserve: it may start a new VtxOffset block

    for (int ring = 0; ring <= kRings; ++ring) {
        const float t = static_cast<float>(ring) / static_cast<float>(kRings);
        const float falloff = std::pow(1.0f - t, 1.55f); // measured from the reference glows
        const ImU32 col = rgb | (static_cast<ImU32>(static_cast<float>(peak) * falloff + 0.5f) << IM_COL32_A_SHIFT);
        const float r = r0 + size * t;
        for (int corner = 0; corner < 4; ++corner) {
            for (int i = 0; i <= seg; ++i) {
                const float a = (-0.5f + 0.5f * static_cast<float>(corner) + 0.5f * static_cast<float>(i) / static_cast<float>(seg)) * kPi;
                dl->PrimWriteVtx(ImVec2(centers[corner].x + std::cos(a) * r, centers[corner].y + std::sin(a) * r), uv, col);
            }
        }
    }
    for (int ring = 0; ring < kRings; ++ring) {
        const unsigned int inner = base + static_cast<unsigned int>(ring * n);
        const unsigned int outer = inner + static_cast<unsigned int>(n);
        for (int i = 0; i < n; ++i) {
            const unsigned int a = static_cast<unsigned int>(i);
            const unsigned int b = static_cast<unsigned int>((i + 1) % n);
            dl->PrimWriteIdx(static_cast<ImDrawIdx>(inner + a));
            dl->PrimWriteIdx(static_cast<ImDrawIdx>(inner + b));
            dl->PrimWriteIdx(static_cast<ImDrawIdx>(outer + b));
            dl->PrimWriteIdx(static_cast<ImDrawIdx>(inner + a));
            dl->PrimWriteIdx(static_cast<ImDrawIdx>(outer + b));
            dl->PrimWriteIdx(static_cast<ImDrawIdx>(outer + a));
        }
    }
}

void Star(ImDrawList* dl, const ImVec2& center, float radius, float rotation, ImU32 col)
{
    ImVec2 pts[10];
    for (int i = 0; i < 10; ++i) {
        const float r = (i & 1) ? radius * 0.47f : radius;
        const float a = rotation - kPi * 0.5f + static_cast<float>(i) * kPi / 5.0f;
        pts[i] = ImVec2(center.x + std::cos(a) * r, center.y + std::sin(a) * r);
    }
    dl->AddConcavePolyFilled(pts, 10, col);
}

CapMetrics Cap(ImFont* font, float size)
{
    ImFontBaked* baked = font->GetFontBaked(size);
    const ImFontGlyph* glyph = baked != nullptr ? baked->FindGlyph(static_cast<ImWchar>('H')) : nullptr;
    if (glyph == nullptr) return CapMetrics{ size * 0.2f, size * 0.8f };
    return CapMetrics{ glyph->Y0, glyph->Y1 };
}

float TextWidth(ImFont* font, float size, const char* begin, const char* end)
{
    return font->CalcTextSizeA(size, FLT_MAX, 0.0f, begin, end).x;
}

void Text(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 col, const char* begin, const char* end)
{
    if (begin == nullptr || begin == end || *begin == '\0') return;
    dl->AddText(font, size, Snap(pos), col, begin, end);
}

ImVec2 TextCapCentered(ImDrawList* dl, ImFont* font, float size, float x, float center_y, ImU32 col,
                       const char* begin, const char* end)
{
    const CapMetrics cap = Cap(font, size);
    const ImVec2 pos(Snap(x), std::floor(center_y - (cap.top + cap.baseline) * 0.5f));
    Text(dl, font, size, pos, col, begin, end);
    return pos;
}

ImVec2 TextCapTop(ImDrawList* dl, ImFont* font, float size, float x, float cap_top, ImU32 col, const char* begin,
                  const char* end)
{
    const CapMetrics cap = Cap(font, size);
    const ImVec2 pos(Snap(x), Snap(cap_top - cap.top));
    Text(dl, font, size, pos, col, begin, end);
    return pos;
}

void Chevron(ImDrawList* dl, const ImVec2& center, float width, float height, bool up, ImU32 col, float thickness)
{
    const float hw = width * 0.5f;
    const float hh = height * 0.5f;
    const float s = up ? -1.0f : 1.0f;
    const ImVec2 pts[3] = { ImVec2(center.x - hw, center.y - hh * s), ImVec2(center.x, center.y + hh * s),
                            ImVec2(center.x + hw, center.y - hh * s) };
    dl->AddPolyline(pts, 3, col, ImDrawFlags_None, thickness);
}

void Plus(ImDrawList* dl, const ImVec2& center, float half, float thickness, ImU32 col)
{
    const float t = thickness * 0.5f;
    dl->AddRectFilled(ImVec2(center.x - half, center.y - t), ImVec2(center.x + half, center.y + t), col);
    dl->AddRectFilled(ImVec2(center.x - t, center.y - half), ImVec2(center.x + t, center.y - t), col);
    dl->AddRectFilled(ImVec2(center.x - t, center.y + t), ImVec2(center.x + t, center.y + half), col);
}

void Minus(ImDrawList* dl, const ImVec2& center, float half, float thickness, ImU32 col)
{
    const float t = thickness * 0.5f;
    dl->AddRectFilled(ImVec2(center.x - half, center.y - t), ImVec2(center.x + half, center.y + t), col);
}

void Check(ImDrawList* dl, const ImVec2& box_min, float size, ImU32 col, float thickness, float progress)
{
    if (progress <= 0.0f) return;
    const float u = size / 16.0f;
    const ImVec2 a(box_min.x + 3.8f * u, box_min.y + 7.9f * u);
    const ImVec2 b(box_min.x + 6.8f * u, box_min.y + 10.7f * u);
    const ImVec2 c(box_min.x + 11.7f * u, box_min.y + 4.9f * u);
    const float l1 = ImSqrt(ImLengthSqr(ImVec2(b.x - a.x, b.y - a.y)));
    const float l2 = ImSqrt(ImLengthSqr(ImVec2(c.x - b.x, c.y - b.y)));
    const float drawn = (l1 + l2) * ImSaturate(progress);

    ImVec2 pts[3] = { a, b, c };
    int count = 3;
    if (drawn <= l1) {
        const float t = l1 > 0.0f ? drawn / l1 : 1.0f;
        pts[1] = ImVec2(ImLerp(a.x, b.x, t), ImLerp(a.y, b.y, t));
        count = 2;
    } else if (drawn < l1 + l2) {
        const float t = (drawn - l1) / l2;
        pts[2] = ImVec2(ImLerp(b.x, c.x, t), ImLerp(b.y, c.y, t));
    }
    dl->AddPolyline(pts, count, col, ImDrawFlags_None, thickness);
}

void HueStrip(ImDrawList* dl, const ImVec2& min, const ImVec2& max, bool vertical, float alpha)
{
    const ImU32 a = static_cast<ImU32>(ImSaturate(alpha) * 255.0f + 0.5f);
    const ImU32 hues[7] = { IM_COL32(255, 0, 0, a),   IM_COL32(255, 255, 0, a), IM_COL32(0, 255, 0, a),
                            IM_COL32(0, 255, 255, a), IM_COL32(0, 0, 255, a),   IM_COL32(255, 0, 255, a),
                            IM_COL32(255, 0, 0, a) };
    for (int i = 0; i < 6; ++i) {
        if (vertical) {
            const float y0 = ImLerp(min.y, max.y, static_cast<float>(i) / 6.0f);
            const float y1 = ImLerp(min.y, max.y, static_cast<float>(i + 1) / 6.0f);
            dl->AddRectFilledMultiColor(ImVec2(min.x, y0), ImVec2(max.x, y1), hues[i], hues[i], hues[i + 1], hues[i + 1]);
        } else {
            const float x0 = ImLerp(min.x, max.x, static_cast<float>(i) / 6.0f);
            const float x1 = ImLerp(min.x, max.x, static_cast<float>(i + 1) / 6.0f);
            dl->AddRectFilledMultiColor(ImVec2(x0, min.y), ImVec2(x1, max.y), hues[i], hues[i + 1], hues[i + 1], hues[i]);
        }
    }
}

void Checkerboard(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float cell, ImU32 a, ImU32 b)
{
    dl->AddRectFilled(min, max, a);
    if (cell <= 0.0f) return;
    int row = 0;
    for (float y = min.y; y < max.y; y += cell, ++row) {
        int col = row & 1;
        for (float x = min.x; x < max.x; x += cell, ++col) {
            if ((col & 1) == 0) continue;
            dl->AddRectFilled(ImVec2(x, y), ImVec2(ImMin(x + cell, max.x), ImMin(y + cell, max.y)), b);
        }
    }
}

} // namespace astral::draw
