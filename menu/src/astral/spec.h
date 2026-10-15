// Astral-Remake - made by kdt255 (https://github.com/kdt255/Astral-Remake)
// Free software: MIT + Commons Clause, selling is not permitted. If you paid for this, you got scammed.
#pragma once
// Visual specification of the Astral menu, measured from the 1:1 reference captures.
// Every metric is in design pixels at 100% scale; widgets multiply through ui::Px().
#include "imgui.h"

#include <cstdint>

namespace astral {

[[nodiscard]] constexpr ImVec4 Rgb(std::uint32_t rgb, float a = 1.0f) noexcept
{
    return ImVec4(static_cast<float>((rgb >> 16) & 0xFF) / 255.0f, static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
                  static_cast<float>(rgb & 0xFF) / 255.0f, a);
}

enum class AccentPreset : int { Salmon, Crimson, Sunset, Custom, Count };

struct Palette {
    ImVec4 window_bg = Rgb(0x2A2E3B);
    ImVec4 card = Rgb(0x2F3341);        // panel headers, top card, footer
    ImVec4 panel = Rgb(0x252935);       // panel bodies, color picker
    ImVec4 frame = Rgb(0x30343F);       // combos, unchecked boxes, slider tracks, buttons
    ImVec4 frame_hover = Rgb(0x353946);
    ImVec4 frame_active = Rgb(0x3A3E4B);
    ImVec4 dropdown = Rgb(0x3B3F4D);    // opened combo list
    ImVec4 popup = Rgb(0x2A2E3B);       // settings / bind popups
    ImVec4 popup_border = Rgb(0x3A3E4C);
    ImVec4 tooltip = Rgb(0x323645);
    ImVec4 text = Rgb(0xFFFFFF);
    ImVec4 text_soft = Rgb(0xC9CCD4);   // combo values, footer, chevrons
    ImVec4 text_hover = Rgb(0xBBBEC6);  // unchecked label under the mouse
    ImVec4 text_dim = Rgb(0x8E93A1);    // unchecked labels, section captions
    ImVec4 grip = Rgb(0x727684);

    ImVec4 accent;                      // flat accent: plus buttons, selected items, scroll thumb
    ImVec4 accent_top;                  // checkbox gradient, top
    ImVec4 accent_bottom;               // checkbox gradient, bottom
    ImVec4 accent_end;                  // slider gradient tail
    ImVec4 title_left;
    ImVec4 title_right;
};

[[nodiscard]] inline Palette MakePalette(AccentPreset preset) noexcept
{
    Palette p;
    switch (preset) {
    case AccentPreset::Crimson:
        p.accent = Rgb(0xE5101B);
        p.accent_top = Rgb(0xF0222B);
        p.accent_bottom = Rgb(0xD80B15);
        p.accent_end = Rgb(0xFF8A1C);
        p.title_left = Rgb(0xE4070F);
        p.title_right = Rgb(0xFF900F);
        break;
    case AccentPreset::Sunset:
        p.accent = Rgb(0xF2581C);
        p.accent_top = Rgb(0xF96A28);
        p.accent_bottom = Rgb(0xEA4C14);
        p.accent_end = Rgb(0xFFAA22);
        p.title_left = Rgb(0xEE3A12);
        p.title_right = Rgb(0xFF9C1A);
        break;
    case AccentPreset::Salmon:
    case AccentPreset::Custom:
    case AccentPreset::Count:
    default:
        p.accent = Rgb(0xFF8A84);
        p.accent_top = Rgb(0xFFA094);
        p.accent_bottom = Rgb(0xFF7E7E);
        p.accent_end = Rgb(0xFFB27C);
        p.title_left = Rgb(0xE5696E);
        p.title_right = Rgb(0xEBA453);
        break;
    }
    return p;
}

// User accent: `start` drives checkboxes, plus buttons and the left side of the title bar, `end` the gradient tails.
[[nodiscard]] inline Palette MakeCustomPalette(const ImVec4& start, const ImVec4& end) noexcept
{
    auto mix = [](const ImVec4& a, const ImVec4& b, float t) {
        return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, 1.0f);
    };
    const ImVec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    const ImVec4 black(0.0f, 0.0f, 0.0f, 1.0f);
    Palette p;
    p.accent = ImVec4(start.x, start.y, start.z, 1.0f);
    p.accent_top = mix(start, white, 0.12f);
    p.accent_bottom = mix(start, black, 0.04f);
    p.accent_end = ImVec4(end.x, end.y, end.z, 1.0f);
    p.title_left = mix(start, black, 0.10f);
    p.title_right = mix(end, black, 0.07f);
    return p;
}

struct Metrics {
    // Window
    float window_w = 600.0f;
    float window_h = 620.0f;
    float window_min_w = 540.0f;
    float window_min_h = 470.0f;
    float window_rounding = 6.0f;
    float window_shadow = 18.0f;

    // Title bar
    float title_h = 44.0f;
    float title_text_x = 13.0f;
    float tab_spacing = 36.0f;
    float tab_last_from_right = 29.0f;  // center of the right-most tab icon, from the window's right edge
    float tab_hit = 30.0f;

    // Page frame
    float pad_x = 14.0f;
    float pad_y = 12.0f;
    float gap = 12.0f;
    float card_h = 42.0f;
    float card_inset = 7.0f;
    float card_rounding = 6.0f;
    float card_text_x = 10.0f;
    float card_chevron_right = 14.0f;
    float footer_h = 26.0f;
    float footer_text_x = 13.0f;

    // Panels
    float panel_header_h = 32.0f;
    float panel_rounding = 6.0f;
    float panel_pad_x = 14.0f;
    float panel_pad_y = 11.0f;
    float panel_extra_bottom = 4.0f;
    float item_spacing = 10.0f;
    float scroll_w = 3.0f;
    float scroll_right = 5.0f;
    float scroll_step = 64.0f;

    // Items
    float checkbox = 16.0f;
    float checkbox_rounding = 3.5f;
    float row_h = 18.0f;
    float label_gap = 12.0f;
    float glow = 9.0f;
    float glow_alpha = 0.255f;
    float dot = 14.0f;
    float accessory_inset = 1.0f;
    float accessory_gap = 6.0f;
    float frame_h = 20.0f;
    float frame_rounding = 4.0f;
    float frame_text_x = 8.0f;
    float caption_to_frame = 16.0f;     // caption cap-top to frame top
    float slider_h = 21.0f;
    float slider_track_y = 16.0f;
    float slider_track_h = 3.0f;
    float button_h = 24.0f;
    float list_row_h = 30.0f;

    // Popups
    float popup_rounding = 6.0f;
    float popup_pad_x = 14.0f;
    float popup_pad_y = 11.0f;
    float popup_w = 256.0f;
    float popup_offset = 6.0f;
    float dropdown_item_h = 20.0f;
    float dropdown_pad_bottom = 4.0f;
    float bind_w = 226.0f;
    float picker_sv = 170.0f;
    float picker_bar = 12.0f;
    float picker_gap = 8.0f;
    float picker_side_w = 118.0f;
    float tooltip_wrap = 300.0f;
    float tooltip_pad_x = 10.0f;
    float tooltip_pad_y = 8.0f;

    // Typography (ImGui pixel sizes: ascent - descent)
    float font_body = 15.0f;
    float font_header = 18.0f;
    float font_title = 17.5f;
    float font_small = 11.0f;
    float font_tiny = 9.0f;
};

} // namespace astral
