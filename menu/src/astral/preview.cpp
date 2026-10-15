// Astral-Remake - made by kdt255 (https://github.com/kdt255/Astral-Remake)
// Free software: MIT + Commons Clause, selling is not permitted. If you paid for this, you got scammed.
#include "astral/preview.h"

#include "astral/draw.h"
#include "astral/ui.h"

#include <cmath>

namespace astral {

namespace {

// Invisible reference skeleton: only used to place the box, the skeleton ESP and the text around them.
struct Rig {
    ImVec2 head, neck, pelvis;
    ImVec2 shoulder[2], elbow[2], wrist[2], hip[2], knee[2], ankle[2];
    ImRect bounds;
};

[[nodiscard]] Rig Pose(const ImVec2& feet, float h, double time)
{
    const float breathe = static_cast<float>(std::sin(time * 1.9)) * 0.003f;
    const float sway = static_cast<float>(std::sin(time * 0.85)) * 0.005f;
    auto p = [&](float x, float y) { return ImVec2(feet.x + x * h, feet.y - h + y * h); };

    Rig r;
    r.head = p(sway * 0.5f, 0.07f + breathe);
    r.neck = p(sway * 0.4f, 0.14f + breathe);
    r.pelvis = p(sway * 0.05f, 0.50f);
    for (int s = 0; s < 2; ++s) {
        const float k = s == 0 ? -1.0f : 1.0f;
        const float arm = static_cast<float>(std::sin(time * 1.25 + s * 1.9)) * 0.003f;
        r.shoulder[s] = p(k * 0.122f + sway * 0.3f, 0.19f + breathe);
        r.elbow[s] = p(k * 0.152f + arm, 0.335f + breathe);
        r.wrist[s] = p(k * 0.158f + arm * 1.5f, 0.455f);
        r.hip[s] = p(k * 0.056f, 0.525f);
        r.knee[s] = p(k * 0.061f, 0.735f);
        r.ankle[s] = p(k * 0.06f, 0.935f);
    }
    r.bounds = ImRect(p(-0.215f, -0.012f), p(0.215f, 1.0f));
    return r;
}

void Skeleton(ImDrawList* dl, const Rig& r, ImU32 col, float t, const ImVec2& offset)
{
    auto o = [&](const ImVec2& v) { return ImVec2(v.x + offset.x, v.y + offset.y); };
    dl->AddLine(o(r.head), o(r.neck), col, t);
    dl->AddLine(o(r.neck), o(r.pelvis), col, t);
    for (int s = 0; s < 2; ++s) {
        dl->AddLine(o(r.neck), o(r.shoulder[s]), col, t);
        dl->AddLine(o(r.shoulder[s]), o(r.elbow[s]), col, t);
        dl->AddLine(o(r.elbow[s]), o(r.wrist[s]), col, t);
        dl->AddLine(o(r.pelvis), o(r.hip[s]), col, t);
        dl->AddLine(o(r.hip[s]), o(r.knee[s]), col, t);
        dl->AddLine(o(r.knee[s]), o(r.ankle[s]), col, t);
    }
}

void ShadowText(ImDrawList* dl, ImFont* font, float size, const ImVec2& pos, ImU32 col, const char* text)
{
    const ImVec2 p = draw::Snap(pos);
    draw::Text(dl, font, size, ImVec2(p.x + 1.0f, p.y + 1.0f), draw::Col(ImVec4(0, 0, 0, 0.85f)), text);
    draw::Text(dl, font, size, p, col, text);
}

void RifleIcon(ImDrawList* dl, const ImVec2& center, float s, ImU32 col)
{
    auto p = [&](float x, float y) { return ImVec2(center.x + x * s, center.y + y * s); };
    dl->AddRectFilled(p(-1.0f, -0.16f), p(0.62f, 0.06f), col);  // receiver + barrel
    dl->AddRectFilled(p(0.62f, -0.10f), p(1.0f, -0.02f), col);  // muzzle
    const ImVec2 stock[4] = { p(-1.0f, -0.16f), p(-0.62f, -0.12f), p(-0.6f, 0.2f), p(-1.0f, 0.28f) };
    dl->AddConvexPolyFilled(stock, 4, col);
    const ImVec2 grip[4] = { p(-0.42f, 0.06f), p(-0.28f, 0.06f), p(-0.34f, 0.36f), p(-0.48f, 0.36f) };
    dl->AddConvexPolyFilled(grip, 4, col);
    const ImVec2 mag[4] = { p(-0.12f, 0.06f), p(0.04f, 0.06f), p(0.12f, 0.42f), p(-0.02f, 0.46f) };
    dl->AddConvexPolyFilled(mag, 4, col);
    dl->AddRectFilled(p(-0.25f, -0.26f), p(0.1f, -0.16f), col); // rail
}

} // namespace

void DrawEspPreview(ImDrawList* dl, const ImRect& area, const PlayerVisuals& v, double time)
{
    if (!v.enable) return;

    ui::Context& c = ui::Ctx();
    const float h = draw::Snap(ImMin(area.GetHeight() * 0.70f, area.GetWidth() * 1.55f));
    const ImVec2 feet(draw::Snap(area.GetCenter().x - h * 0.02f), draw::Snap(area.GetCenter().y + h * 0.5f + ui::Px(4.0f)));
    const Rig rig = Pose(feet, h, time);

    if (v.sound_only) {
        for (int i = 0; i < 3; ++i) {
            const float t = static_cast<float>(std::fmod(time * 0.6 + i / 3.0, 1.0));
            const float r = ImLerp(h * 0.05f, h * 0.24f, t);
            dl->AddEllipse(ImVec2(feet.x, feet.y - h * 0.012f), ImVec2(r, r * 0.28f), draw::Col(c.pal.accent, (1.0f - t) * 0.7f), 0.0f,
                           0, ui::Px(1.5f));
        }
    }

    const float line = ImMax(1.0f, ui::Px(1.0f));
    if (v.backtrack_skeleton) {
        for (int i = 3; i >= 1; --i)
            Skeleton(dl, rig, draw::Col(v.backtrack_color.Resolve(time), 0.55f / static_cast<float>(i)), line,
                     ImVec2(-h * 0.035f * static_cast<float>(i), 0.0f));
    }
    if (v.skeleton) Skeleton(dl, rig, draw::Col(v.skeleton_color.Resolve(time)), ui::Px(1.4f), ImVec2(0, 0));

    const ImRect box(draw::Snap(rig.bounds.Min), draw::Snap(rig.bounds.Max));
    if (v.box) {
        const ImU32 black = draw::Col(ImVec4(0, 0, 0, 0.85f));
        dl->AddRect(ImVec2(box.Min.x - 1.0f, box.Min.y - 1.0f), ImVec2(box.Max.x + 1.0f, box.Max.y + 1.0f), black, 0.0f, 0, 1.0f);
        dl->AddRect(ImVec2(box.Min.x + 1.0f, box.Min.y + 1.0f), ImVec2(box.Max.x - 1.0f, box.Max.y - 1.0f), black, 0.0f, 0, 1.0f);
        dl->AddRect(box.Min, box.Max, draw::Col(v.box_color.Resolve(time)), 0.0f, 0, 1.0f);
    }

    ImFont* small = c.fonts.regular;
    ImFont* tiny = c.fonts.bold;
    const float fs_small = ui::FontPx(c.m.font_small + 1.0f);
    const float fs_tiny = ui::FontPx(c.m.font_tiny);
    const ImU32 flag_col = draw::Col(Rgb(0xFF9F43));

    float top = box.Min.y - ui::Px(3.0f);
    if (v.name) {
        const float tw = draw::TextWidth(small, fs_small, "Name");
        top -= fs_small;
        ShadowText(dl, small, fs_small, ImVec2(box.GetCenter().x - tw * 0.5f, top), draw::Col(v.name_color.Resolve(time)), "Name");
    }
    if ((v.flags & (1u << 3)) != 0) {
        const float tw = draw::TextWidth(tiny, fs_tiny, "50MS");
        top -= fs_tiny + ui::Px(1.0f);
        ShadowText(dl, tiny, fs_tiny, ImVec2(box.GetCenter().x - tw * 0.5f, top), flag_col, "50MS");
    }

    if (v.health) {
        constexpr float kHealth = 0.7f;
        const float w = ImMax(2.0f, ui::Px(2.0f));
        const float x1 = box.Min.x - ui::Px(5.0f);
        const ImVec2 b0(x1 - w, box.Min.y), b1(x1, box.Max.y);
        dl->AddRectFilled(ImVec2(b0.x - 1.0f, b0.y - 1.0f), ImVec2(b1.x + 1.0f, b1.y + 1.0f), draw::Col(ImVec4(0, 0, 0, 0.7f)));
        const float fill_top = ImLerp(b1.y, b0.y, kHealth);
        const ImVec4 hc = draw::Mix(Rgb(0xE8503A), Rgb(0x7DDB4F), kHealth);
        dl->AddRectFilled(ImVec2(b0.x, fill_top), b1, draw::Col(hc));
        const float tw = draw::TextWidth(tiny, fs_tiny, "70");
        ShadowText(dl, tiny, fs_tiny, ImVec2(b0.x - tw * 0.5f + w * 0.5f, fill_top - fs_tiny * 0.5f), draw::Col(c.pal.text), "70");
    }

    float bottom = box.Max.y + ui::Px(4.0f);
    if ((v.weapon & (1u << 2)) != 0) {
        constexpr float kAmmo = 14.0f / 30.0f;
        const float bh = ImMax(2.0f, ui::Px(2.0f));
        const ImVec2 a0(box.Min.x, bottom), a1(box.Max.x, bottom + bh);
        dl->AddRectFilled(ImVec2(a0.x - 1.0f, a0.y - 1.0f), ImVec2(a1.x + 1.0f, a1.y + 1.0f), draw::Col(ImVec4(0, 0, 0, 0.7f)));
        dl->AddRectFilled(a0, ImVec2(ImLerp(a0.x, a1.x, kAmmo), a1.y), draw::Col(Rgb(0x6FA8FF)));
        ShadowText(dl, tiny, fs_tiny, ImVec2(ImLerp(a0.x, a1.x, kAmmo) - ui::Px(3.0f), a0.y - fs_tiny * 0.35f), draw::Col(c.pal.text), "14");
        bottom = a1.y + ui::Px(4.0f);
    }
    if ((v.weapon & (1u << 0)) != 0) {
        const float tw = draw::TextWidth(tiny, fs_tiny, "AK-47");
        ShadowText(dl, tiny, fs_tiny, ImVec2(box.GetCenter().x - tw * 0.5f, bottom), draw::Col(c.pal.text), "AK-47");
        bottom += fs_tiny + ui::Px(2.0f);
    }
    if ((v.weapon & (1u << 1)) != 0) {
        const float s = ui::Px(13.0f);
        RifleIcon(dl, ImVec2(box.GetCenter().x + 1.0f, bottom + s * 0.45f + 1.0f), s, draw::Col(ImVec4(0, 0, 0, 0.8f)));
        RifleIcon(dl, ImVec2(box.GetCenter().x, bottom + s * 0.45f), s, draw::Col(ImVec4(1, 1, 1, 0.92f)));
    }

    static const char* const kFlagText[] = { "HK", "C4", "LONG A", nullptr, "RELOAD", "PIN", "SCOPE", "FLASHED", "DEFUSE" };
    float fy = box.Min.y;
    for (int i = 0; i < IM_ARRAYSIZE(kFlagText); ++i) {
        if (kFlagText[i] == nullptr || (v.flags & (1u << i)) == 0) continue;
        ShadowText(dl, tiny, fs_tiny, ImVec2(box.Max.x + ui::Px(5.0f), fy), i == 0 ? draw::Col(c.pal.text) : flag_col, kFlagText[i]);
        fy += fs_tiny + ui::Px(1.0f);
    }
}

} // namespace astral
