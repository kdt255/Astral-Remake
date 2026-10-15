// Astral-Remake - made by kdt255 (https://github.com/kdt255/Astral-Remake)
// Free software: MIT + Commons Clause, selling is not permitted. If you paid for this, you got scammed.
#include "astral/icons.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace astral {

namespace {

struct V {
    float x, y;
};

[[nodiscard]] inline V Sub(V a, V b) noexcept { return V{ a.x - b.x, a.y - b.y }; }
[[nodiscard]] inline float Len(V a) noexcept { return std::sqrt(a.x * a.x + a.y * a.y); }
[[nodiscard]] inline float Dot(V a, V b) noexcept { return a.x * b.x + a.y * b.y; }

[[nodiscard]] float Circle(V p, V c, float r) noexcept { return Len(Sub(p, c)) - r; }

[[nodiscard]] float Ring(V p, V c, float r, float half_thickness) noexcept
{
    return std::fabs(Len(Sub(p, c)) - r) - half_thickness;
}

[[nodiscard]] float Box(V p, V c, V half, float rounding) noexcept
{
    const float dx = std::fabs(p.x - c.x) - half.x + rounding;
    const float dy = std::fabs(p.y - c.y) - half.y + rounding;
    const float ox = std::max(dx, 0.0f);
    const float oy = std::max(dy, 0.0f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(dx, dy), 0.0f) - rounding;
}

[[nodiscard]] float Segment(V p, V a, V b, float radius) noexcept
{
    const V pa = Sub(p, a);
    const V ba = Sub(b, a);
    const float h = std::clamp(Dot(pa, ba) / Dot(ba, ba), 0.0f, 1.0f);
    return Len(V{ pa.x - ba.x * h, pa.y - ba.y * h }) - radius;
}

// Lens of two circles (radius r, centres +-d on the x axis): tips on the y axis. Inigo Quilez' exact SDF.
[[nodiscard]] float Vesica(V p, float r, float d) noexcept
{
    p = V{ std::fabs(p.x), std::fabs(p.y) };
    const float b = std::sqrt(r * r - d * d);
    return ((p.y - b) * d > p.x * b) ? Len(Sub(p, V{ 0.0f, b })) : Len(Sub(p, V{ -d, 0.0f })) - r;
}

[[nodiscard]] inline float Union(float a, float b) noexcept { return std::min(a, b); }
[[nodiscard]] inline float Cut(float a, float b) noexcept { return std::max(a, -b); }

struct Design {
    float w, h;
};

constexpr Design kDesign[static_cast<size_t>(Icon::Count)] = {
    { 12.0f, 17.0f }, // Mouse
    { 17.0f, 17.0f }, // Crosshair
    { 20.0f, 15.0f }, // Eye
    { 18.0f, 18.0f }, // Palette
    { 18.0f, 18.0f }, // Gear
    { 16.0f, 13.0f }, // List
    { 21.0f, 16.0f }, // Code
    { 16.0f, 16.0f }, // Search
    { 16.0f, 16.0f }, // GearSmall
};

[[nodiscard]] float GearSdf(V p, float cx, float body, float tooth_center, V tooth_half, float hole) noexcept
{
    const V q = Sub(p, V{ cx, cx });
    const float r = Len(q);
    constexpr float kSector = 6.28318530718f / 6.0f;
    const float a = std::atan2(q.y, q.x);
    const float local = a - kSector * std::round(a / kSector);
    const V t{ r * std::cos(local), r * std::sin(local) };
    const float tooth = Box(t, V{ tooth_center, 0.0f }, tooth_half, 0.7f);
    return Cut(Union(r - body, tooth), r - hole);
}

// Distances in design pixels, origin at the top-left of the icon's design box.
[[nodiscard]] float Sdf(Icon icon, V p) noexcept
{
    switch (icon) {
    case Icon::Mouse: {
        const float body = Box(p, V{ 6.0f, 8.5f }, V{ 6.0f, 8.5f }, 5.6f);
        const float split = Box(p, V{ 6.0f, 3.3f }, V{ 0.65f, 3.7f }, 0.0f);
        const float waist = Box(p, V{ 6.0f, 7.0f }, V{ 6.5f, 0.65f }, 0.0f);
        return Cut(body, Union(split, waist));
    }
    case Icon::Crosshair: {
        const V c{ 8.5f, 8.5f };
        float d = Ring(p, c, 5.75f, 1.15f);
        d = Union(d, Segment(p, V{ 8.5f, 0.8f }, V{ 8.5f, 5.0f }, 1.0f));
        d = Union(d, Segment(p, V{ 8.5f, 12.0f }, V{ 8.5f, 16.2f }, 1.0f));
        d = Union(d, Segment(p, V{ 0.8f, 8.5f }, V{ 5.0f, 8.5f }, 1.0f));
        d = Union(d, Segment(p, V{ 12.0f, 8.5f }, V{ 16.2f, 8.5f }, 1.0f));
        return Union(d, Circle(p, c, 1.2f));
    }
    case Icon::Eye: {
        const V c{ 10.0f, 7.5f };
        const float outer = Vesica(V{ p.y - c.y, p.x - c.x }, 9.254f, 3.154f) - 0.9f;
        const float iris = Cut(outer, Circle(p, c, 4.6f));
        const float pupil = Cut(Circle(p, c, 3.1f), Box(p, V{ c.x - 1.7f, c.y - 1.7f }, V{ 1.7f, 1.7f }, 0.0f));
        return Union(iris, pupil);
    }
    case Icon::Palette: {
        float holes = Circle(p, V{ 13.4f, 13.1f }, 2.85f);
        holes = Union(holes, Circle(p, V{ 4.8f, 9.6f }, 1.5f));
        holes = Union(holes, Circle(p, V{ 5.5f, 5.4f }, 1.5f));
        holes = Union(holes, Circle(p, V{ 9.4f, 3.6f }, 1.5f));
        holes = Union(holes, Circle(p, V{ 13.2f, 5.4f }, 1.5f));
        return Cut(Circle(p, V{ 9.0f, 9.0f }, 8.7f), holes);
    }
    case Icon::Gear:
        return GearSdf(p, 9.0f, 6.0f, 6.9f, V{ 2.05f, 2.45f }, 2.45f);
    case Icon::GearSmall:
        return GearSdf(p, 8.0f, 5.3f, 6.1f, V{ 1.85f, 2.15f }, 2.15f);
    case Icon::List: {
        float d = 1e9f;
        for (const float y : { 1.9f, 6.5f, 11.1f }) {
            d = Union(d, Box(p, V{ 1.5f, y }, V{ 1.45f, 1.45f }, 0.45f));
            d = Union(d, Box(p, V{ 10.3f, y }, V{ 5.7f, 1.05f }, 0.5f));
        }
        return d;
    }
    case Icon::Code: {
        float d = Segment(p, V{ 6.2f, 3.0f }, V{ 1.5f, 8.0f }, 1.0f);
        d = Union(d, Segment(p, V{ 1.5f, 8.0f }, V{ 6.2f, 13.0f }, 1.0f));
        d = Union(d, Segment(p, V{ 14.8f, 3.0f }, V{ 19.5f, 8.0f }, 1.0f));
        d = Union(d, Segment(p, V{ 19.5f, 8.0f }, V{ 14.8f, 13.0f }, 1.0f));
        return Union(d, Segment(p, V{ 12.3f, 1.0f }, V{ 8.7f, 15.0f }, 0.95f));
    }
    case Icon::Search: {
        const float ring = Ring(p, V{ 9.6f, 6.4f }, 4.85f, 1.05f);
        return Union(ring, Segment(p, V{ 6.2f, 9.8f }, V{ 1.7f, 14.3f }, 1.25f));
    }
    case Icon::Count:
    default:
        return 1e9f;
    }
}

void Rasterize(Icon icon, float scale, unsigned char* dst, int pitch, int w, int h)
{
    const Design& design = kDesign[static_cast<size_t>(icon)];
    const float ox = (static_cast<float>(w) - design.w * scale) * 0.5f;
    const float oy = (static_cast<float>(h) - design.h * scale) * 0.5f;
    for (int y = 0; y < h; ++y) {
        unsigned char* row = dst + y * pitch;
        for (int x = 0; x < w; ++x) {
            const V p{ (static_cast<float>(x) + 0.5f - ox) / scale, (static_cast<float>(y) + 0.5f - oy) / scale };
            const float coverage = std::clamp(0.5f - Sdf(icon, p) * scale, 0.0f, 1.0f);
            row[x * 4 + 0] = 255;
            row[x * 4 + 1] = 255;
            row[x * 4 + 2] = 255;
            row[x * 4 + 3] = static_cast<unsigned char>(coverage * 255.0f + 0.5f);
        }
    }
}

} // namespace

IconAtlas::~IconAtlas()
{
    if (ImGui::GetCurrentContext() != nullptr) {
        Release();
        return;
    }
    // Context already gone: nobody references the textures any more, only the CPU copies are left.
    if (tex_ != nullptr) IM_DELETE(tex_);
    for (ImTextureData* t : retired_) IM_DELETE(t);
    tex_ = nullptr;
    retired_.clear();
}

void IconAtlas::Build(float scale)
{
    scale = std::max(scale, 0.5f);
    if (tex_ != nullptr && std::fabs(scale - scale_) < 0.001f) return;
    scale_ = scale;

    constexpr int kPad = 1;
    const int atlas_w = scale <= 2.0f ? 256 : 512;
    int x = 0, y = 0, row_h = 0;
    for (size_t i = 0; i < entries_.size(); ++i) {
        const Design& d = kDesign[i];
        Entry& e = entries_[i];
        e.w = static_cast<int>(std::ceil(d.w * scale)) + 2;
        e.h = static_cast<int>(std::ceil(d.h * scale)) + 2;
        if (x + e.w + kPad > atlas_w) {
            x = 0;
            y += row_h + kPad;
            row_h = 0;
        }
        e.x = x + kPad;
        e.y = y + kPad;
        x += e.w + kPad;
        row_h = std::max(row_h, e.h);
    }
    int atlas_h = 32;
    while (atlas_h < y + row_h + kPad * 2) atlas_h *= 2;

    if (tex_ != nullptr && (tex_->Width < atlas_w || tex_->Height < atlas_h)) {
        tex_->WantDestroyNextFrame = true;
        retired_.push_back(tex_);
        tex_ = nullptr;
    }

    const bool fresh = tex_ == nullptr;
    if (fresh) {
        tex_ = IM_NEW(ImTextureData)();
        tex_->Create(ImTextureFormat_RGBA32, atlas_w, atlas_h);
    } else {
        std::memset(tex_->GetPixels(), 0, static_cast<size_t>(tex_->GetSizeInBytes()));
    }

    for (size_t i = 0; i < entries_.size(); ++i) {
        const Entry& e = entries_[i];
        Rasterize(static_cast<Icon>(i), scale, static_cast<unsigned char*>(tex_->GetPixelsAt(e.x, e.y)), tex_->GetPitch(), e.w, e.h);
    }

    const ImTextureRect all{ 0, 0, static_cast<unsigned short>(tex_->Width), static_cast<unsigned short>(tex_->Height) };
    tex_->UsedRect = all;
    if (fresh) {
        ImGui::RegisterUserTexture(tex_);
    } else if (tex_->Status == ImTextureStatus_OK || tex_->Status == ImTextureStatus_WantUpdates) {
        tex_->Updates.push_back(all);
        tex_->UpdateRect = all;
        tex_->SetStatus(ImTextureStatus_WantUpdates);
    }
}

void IconAtlas::Collect()
{
    for (size_t i = 0; i < retired_.size();) {
        ImTextureData* t = retired_[i];
        if (t->Status == ImTextureStatus_Destroyed) {
            ImGui::UnregisterUserTexture(t);
            IM_DELETE(t);
            retired_.erase(retired_.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            ++i;
        }
    }
}

void IconAtlas::Release()
{
    if (tex_ != nullptr) {
        ImGui::UnregisterUserTexture(tex_);
        IM_DELETE(tex_);
        tex_ = nullptr;
    }
    for (ImTextureData* t : retired_) {
        ImGui::UnregisterUserTexture(t);
        IM_DELETE(t);
    }
    retired_.clear();
    scale_ = 0.0f;
}

void IconAtlas::Draw(ImDrawList* dl, Icon icon, const ImVec2& center, ImU32 col) const
{
    if (tex_ == nullptr || icon == Icon::Count) return;
    const Entry& e = entries_[static_cast<size_t>(icon)];
    const ImVec2 p0(std::floor(center.x - static_cast<float>(e.w) * 0.5f + 0.5f),
                    std::floor(center.y - static_cast<float>(e.h) * 0.5f + 0.5f));
    const ImVec2 p1(p0.x + static_cast<float>(e.w), p0.y + static_cast<float>(e.h));
    const float iw = 1.0f / static_cast<float>(tex_->Width);
    const float ih = 1.0f / static_cast<float>(tex_->Height);
    dl->AddImage(tex_->GetTexRef(), p0, p1, ImVec2(static_cast<float>(e.x) * iw, static_cast<float>(e.y) * ih),
                 ImVec2(static_cast<float>(e.x + e.w) * iw, static_cast<float>(e.y + e.h) * ih), col);
}

ImVec2 IconAtlas::Size(Icon icon) const
{
    if (icon == Icon::Count) return ImVec2(0.0f, 0.0f);
    const Entry& e = entries_[static_cast<size_t>(icon)];
    return ImVec2(static_cast<float>(e.w), static_cast<float>(e.h));
}

} // namespace astral
