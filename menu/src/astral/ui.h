// Astral-Remake - made by kdt255 (https://github.com/kdt255/Astral-Remake)
// Free software: MIT + Commons Clause, selling is not permitted. If you paid for this, you got scammed.
#pragma once
// Astral widget kit. Every interactive widget follows the stock ItemSize/ItemAdd/ButtonBehavior skeleton so
// clipping, hover tests, IsItemEdited(), BeginDisabled() and nav keep working; all drawing goes through the spec.
#include "astral/config.h"
#include "astral/icons.h"
#include "astral/spec.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <cmath>
#include <unordered_map>

namespace astral::ui {

struct Fonts {
    ImFont* regular = nullptr; // Inter SemiBold
    ImFont* bold = nullptr;    // Inter Bold
};

enum class BindMode : int { Hold, Toggle, Always };
enum class BindKind : int { Bool, Float };

struct Bind {
    ImGuiKey key = ImGuiKey_None;
    BindMode mode = BindMode::Hold;
    BindKind kind = BindKind::Bool;
    bool show_in_list = true;
    void* target = nullptr; // bool* or float* inside the Config; Config has static lifetime
    float value = 0.0f;     // override applied while active (BindKind::Float)
    float min = 0.0f, max = 1.0f;
    float saved = 0.0f;
    bool active = false;
    bool latched = false;   // Toggle state
    char label[48] = {};
};

struct ColumnLayout {
    ImRect area;
    int count = 0;
    int index = 0;
    float width = 0.0f;
    float cursor = 0.0f;
    float reserve = 0.0f;   // measured height of panels below the fill panel (this frame)
    bool after_fill = false;
    ImGuiID id = 0;
};

struct PanelFrame {
    ImGuiID id = 0;
    float x = 0.0f, y = 0.0f, w = 0.0f;
    float header = 0.0f;
    bool fill = false;
    const char* title = nullptr;
};

struct AccessoryLine {
    ImGuiID last_accessory = 0;
    int frame = -1;
    float center_y = 0.0f;
    float right = 0.0f;
};

struct Context {
    AccentPreset accent = AccentPreset::Salmon;
    ImVec4 custom_start = Rgb(0xFF8A84);
    ImVec4 custom_end = Rgb(0xFFB27C);
    Palette pal = MakePalette(AccentPreset::Salmon);
    Metrics m;
    float scale = 1.0f;
    Fonts fonts;
    IconAtlas icons;
    ImGuiStorage state;     // animation values keyed by widget id + salt (GetFloat/SetFloat only, never refs)
    double time = 0.0;
    float dt = 0.0f;

    char search[64] = {};
    bool show_paths = false;
    const char* path_tab = "";
    const char* path_page = "";
    const char* path_panel = "";

    std::unordered_map<ImGuiID, Bind> binds;
    Bind draft;
    ImGuiID draft_owner = 0;
    bool listening = false;
    int listen_frame = 0;

    ColumnLayout layout;
    ImVector<PanelFrame> panels;
    AccessoryLine accessory;
    ImVec4 picker_backup = ImVec4(0, 0, 0, 0);
    float right_override = 0.0f;   // > 0: content right edge for items (side columns inside popups)
    bool animations = true;
    bool last_dimmed = false;      // search state of the last labeled row, inherited by its accessories
    int bind_popup_depth = 0;
    ImVec2 bind_pos = ImVec2(0.0f, 0.0f);
};

[[nodiscard]] Context& Ctx();
[[nodiscard]] inline float Px(float v) { return v * Ctx().scale; }
// Font sizes are rounded to whole pixels: fractional sizes (15px * 125% = 18.75px) defeat hinting and blur stems.
[[nodiscard]] inline float FontPx(float v) { return std::floor(v * Ctx().scale + 0.5f); }

void SetScale(float scale);   // rebuilds icons; call outside a frame
void SetAccent(AccentPreset preset, const ImVec4& custom_start, const ImVec4& custom_end);
void BeginFrame();            // time + bind processing; call after ImGui::NewFrame()
void Shutdown();              // after the renderer backend shut down, before ImGui::DestroyContext()

[[nodiscard]] float Animate(ImGuiID id, const char* salt, float target, float rate, float initial);
[[nodiscard]] float Ease(float t);

// ---- Layout ----------------------------------------------------------------------------------------------------
enum class PanelHeight { Fit, Fill };

void BeginColumns(const char* id, const ImRect& area, int count);
void NextColumn();
void EndColumns();
bool BeginPanel(const char* title, PanelHeight height = PanelHeight::Fit); // title == nullptr: headerless pane
void EndPanel();
[[nodiscard]] ImRect PanelInnerRect();

// ---- Items (panels and popups) --------------------------------------------------------------------------------
using Formatter = int (*)(char* buf, int size, float value);

bool Checkbox(const char* label, bool* v);
bool Slider(const char* label, float* v, float min, float max, const char* fmt = "%.0f", Formatter formatter = nullptr);
bool SliderInt(const char* label, int* v, int min, int max, const char* fmt = "%d");
bool Combo(const char* label, int* current, const char* const items[], int count);
bool MultiCombo(const char* label, unsigned* mask, const char* const items[], int count);
void Caption(const char* text);
void ColorRow(const char* label, ColorValue* color, bool alpha = true);
bool Button(const char* label, float width = 0.0f, bool accent = false);
bool InputText(const char* id, const char* hint, char* buf, size_t size);
bool ListRow(const char* label, const char* detail, bool selected);
void Separator();
void Tooltip(const char* text);

// Accessories sit right-aligned on the line of the previous item (checkbox, caption row).
enum class SettingsIcon { Plus, Gear };
bool ColorDot(const char* id, ColorValue* color, bool alpha = true);
bool BeginSettings(const char* id, SettingsIcon icon = SettingsIcon::Plus, float width = 0.0f);
void EndSettings();

// ---- Frame chrome ---------------------------------------------------------------------------------------------
bool CardCombo(const char* id, int* current, const char* const items[], int count, const ImRect& frame);

// Draws the "Bind" list overlay (call outside the menu window).
void DrawBindsOverlay(float alpha);

[[nodiscard]] const char* KeyName(ImGuiKey key);

} // namespace astral::ui
