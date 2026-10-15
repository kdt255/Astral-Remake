// Astral-Remake - made by kdt255 (https://github.com/kdt255/Astral-Remake)
// Free software: MIT + Commons Clause, selling is not permitted. If you paid for this, you got scammed.
#pragma once
// Vector icons rasterized from signed distance functions into a user texture that the ImGui renderer backend owns
// (ImGui 1.92 RegisterUserTexture path). Filled shapes with holes stay crisp at any DPI without an icon font.
#include "imgui.h"

#include <array>
#include <vector>

#if IMGUI_VERSION_NUM < 19200
#error "IconAtlas relies on the ImTextureData / RegisterUserTexture API introduced in Dear ImGui 1.92"
#endif

namespace astral {

enum class Icon : int { Mouse, Crosshair, Eye, Palette, Gear, List, Code, Search, GearSmall, Count };

class IconAtlas {
public:
    IconAtlas() = default;
    ~IconAtlas();
    IconAtlas(const IconAtlas&) = delete;
    IconAtlas& operator=(const IconAtlas&) = delete;

    // Rasterizes every icon for `scale` (1.0 = design pixels). Call outside NewFrame()/Render().
    void Build(float scale);

    // Unregisters textures the backend finished destroying. Call once per frame before NewFrame().
    void Collect();

    // Warning: call after the renderer backend shutdown (it releases the GPU copy) and before ImGui::DestroyContext().
    void Release();

    void Draw(ImDrawList* dl, Icon icon, const ImVec2& center, ImU32 col) const;
    [[nodiscard]] ImVec2 Size(Icon icon) const;
    [[nodiscard]] bool Ready() const noexcept { return tex_ != nullptr; }

private:
    struct Entry {
        int x = 0, y = 0, w = 0, h = 0;
    };

    std::array<Entry, static_cast<size_t>(Icon::Count)> entries_{};
    ImTextureData* tex_ = nullptr;
    std::vector<ImTextureData*> retired_;
    float scale_ = 0.0f;
};

} // namespace astral
