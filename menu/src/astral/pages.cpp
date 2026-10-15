// Astral-Remake - made by kdt255 (https://github.com/kdt255/Astral-Remake)
// Free software: MIT + Commons Clause, selling is not permitted. If you paid for this, you got scammed.
// Page layouts. Each page fills the category card and lays panels into two columns; everything else is widgets.
#include "astral/draw.h"
#include "astral/menu.h"
#include "astral/preview.h"
#include "astral/ui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace astral {

namespace {

using ui::PanelHeight;

struct ItemList {
    const char* const* items;
    int count;
};

#define ASTRAL_LIST(name, ...)                                    \
    constexpr const char* name##_items[] = { __VA_ARGS__ };       \
    constexpr ItemList name{ name##_items, IM_ARRAYSIZE(name##_items) }

ASTRAL_LIST(kGroups, "Pistol", "Heavy pistol", "SMG", "Rifle", "Shotgun", "Sniper", "Machine gun");
ASTRAL_LIST(kRageCategories, "General", "Pistol", "Heavy pistol", "SMG", "Rifle", "Shotgun", "Sniper", "Machine gun");
ASTRAL_LIST(kPistols, "Glock-18", "USP-S", "P2000", "P250", "Five-SeveN", "Tec-9", "CZ75-Auto", "Dual Berettas");
ASTRAL_LIST(kHeavyPistols, "Desert Eagle", "R8 Revolver");
ASTRAL_LIST(kSmgs, "MAC-10", "MP9", "MP7", "MP5-SD", "UMP-45", "P90", "PP-Bizon");
ASTRAL_LIST(kRifles, "AK-47", "M4A4", "M4A1-S", "Galil AR", "FAMAS", "AUG", "SG 553");
ASTRAL_LIST(kShotguns, "Nova", "XM1014", "Sawed-Off", "MAG-7");
ASTRAL_LIST(kSnipers, "SSG 08", "AWP", "G3SG1", "SCAR-20");
ASTRAL_LIST(kMachineGuns, "M249", "Negev");
constexpr ItemList kWeapons[kGroupCount] = { kPistols, kHeavyPistols, kSmgs, kRifles, kShotguns, kSnipers, kMachineGuns };

ASTRAL_LIST(kHitboxes, "Head", "Body", "Arms", "Legs", "Feet");
ASTRAL_LIST(kIgnoreIf, "Flashed", "In smoke", "High speed", "In air", "Jumping");
ASTRAL_LIST(kAssistModes, "Passive", "Aggressive");
ASTRAL_LIST(kFovStyles, "2D", "3D", "Dynamic");
ASTRAL_LIST(kSpreadModes, "Compensate", "Ignore");
ASTRAL_LIST(kSmoothCurves, "Linear", "Ease out", "Bezier");
ASTRAL_LIST(kPitch, "Default", "Up", "Down", "Fake up", "Fake fake up", "Custom");
ASTRAL_LIST(kYawBase, "Local view", "At targets");
ASTRAL_LIST(kRemoveSpread, "Full", "Partial");
ASTRAL_LIST(kDoubleTap, "Offensive", "Defensive");
ASTRAL_LIST(kQuickStop, "Early", "Between shots", "Force accuracy");
ASTRAL_LIST(kAutoScope, "Always", "On hit chance fail", "Before shot");

ASTRAL_LIST(kVisualTargets, "Enemy", "Team", "Local", "Other");
ASTRAL_LIST(kWeaponEsp, "Name", "Icon", "Ammo");
ASTRAL_LIST(kFlags, "Armor type", "C4 carrier", "Location", "Ping", "Reload", "Grenade pin", "Scope", "Flashed", "Defusing");
ASTRAL_LIST(kAnimations, "Smoothing", "Death fade");
ASTRAL_LIST(kMaterials, "Flat", "Shaded", "Glass", "Metallic");
ASTRAL_LIST(kSkyboxes, "Night", "Vertigo", "Cloudy", "Sunset", "Space");
ASTRAL_LIST(kWeather, "Rain", "Snow", "Ash");

ASTRAL_LIST(kPaintKits, "Default", "Asiimov", "Redline", "Vulcan", "Fire Serpent", "Hyper Beast", "Neo-Noir", "Printstream",
            "The Empress", "Bloodsport", "Slate");
ASTRAL_LIST(kKnives, "Default", "Karambit", "Butterfly", "M9 Bayonet", "Talon", "Skeleton", "Stiletto", "Ursus");
ASTRAL_LIST(kKnifeFinishes, "Vanilla", "Fade", "Doppler", "Tiger Tooth", "Marble Fade", "Slaughter", "Crimson Web", "Case Hardened");
ASTRAL_LIST(kGloves, "Default", "Sport", "Driver", "Specialist", "Moto", "Hand wraps");
ASTRAL_LIST(kGloveFinishes, "Vice", "Pandora's Box", "Crimson Kimono", "Emerald Web", "Fade");
ASTRAL_LIST(kAgentsT, "Default", "The Elite Mr. Muhlik", "Sir Bloody Darryl", "Number K", "Dragomir");
ASTRAL_LIST(kAgentsCt, "Default", "Special Agent Ava", "Lt. Commander Ricksaw", "Cmdr. Mae Jamison", "B Squadron Officer");

ASTRAL_LIST(kMiscPages, "Miscellaneous", "Automation", "Interface");
ASTRAL_LIST(kWalkSpeed, "Accurate", "Fast", "Silent");
ASTRAL_LIST(kRemove, "Post processing", "Flashbang overlay", "Smoke", "Scope overlay", "Shadows", "Fog");
ASTRAL_LIST(kReveal, "Sniper crosshair", "Enemy scoreboard", "Money", "Overwatch");
ASTRAL_LIST(kAntiCheat, "Valve MM", "Community", "Disabled");
ASTRAL_LIST(kRoundAlert, "Disabled", "Sound", "Flash taskbar");
ASTRAL_LIST(kPrimaries, "None", "AWP", "SSG 08", "AK-47 / M4", "Auto sniper");
ASTRAL_LIST(kSecondaries, "None", "Desert Eagle", "Dual Berettas", "P250");
ASTRAL_LIST(kUtility, "Armor", "Helmet", "Defuser", "HE grenade", "Smoke", "Molotov", "Flashbang");
ASTRAL_LIST(kAccents, "Salmon", "Crimson", "Sunset", "Custom");
ASTRAL_LIST(kScales, "Auto", "100%", "125%", "150%", "175%", "200%");
ASTRAL_LIST(kHitSounds, "None", "Bell", "Click", "Bubble");
ASTRAL_LIST(kConfigSort, "Recently modified", "Name");
ASTRAL_LIST(kScriptFilter, "All scripts", "Autoload only");

#undef ASTRAL_LIST

[[nodiscard]] float S(float v) { return draw::Snap(ui::Px(v)); }

bool Combo(const char* label, int* v, const ItemList& list) { return ui::Combo(label, v, list.items, list.count); }
bool Multi(const char* label, unsigned* v, const ItemList& list) { return ui::MultiCombo(label, v, list.items, list.count); }

ImRect CardFrame(const ImRect& card)
{
    const float inset = S(ui::Ctx().m.card_inset);
    return ImRect(ImVec2(card.Min.x + inset, card.Min.y + inset), ImVec2(card.Max.x - inset, card.Max.y - inset));
}

// Two combos in one card, split on the column boundary below.
void CardSplit(const ImRect& card, const ImRect& area, ImRect& left, ImRect& right)
{
    const ui::Context& c = ui::Ctx();
    const float inset = S(c.m.card_inset);
    const float gap = S(c.m.gap);
    const float col_w = draw::Snap((area.GetWidth() - gap) * 0.5f);
    const float mid = area.Min.x + col_w + gap * 0.5f;
    left = ImRect(ImVec2(card.Min.x + inset, card.Min.y + inset), ImVec2(draw::Snap(mid - S(6.0f)), card.Max.y - inset));
    right = ImRect(ImVec2(draw::Snap(mid + S(6.0f)), card.Min.y + inset), ImVec2(card.Max.x - inset, card.Max.y - inset));
}

int FmtAimTime(char* buf, int size, float v)
{
    return v >= 1999.5f ? ImFormatString(buf, static_cast<size_t>(size), "Unlimited")
                        : ImFormatString(buf, static_cast<size_t>(size), "%.0fms", static_cast<double>(v));
}

int FmtMinDamage(char* buf, int size, float v)
{
    const int d = static_cast<int>(std::lround(v));
    if (d <= 0) return ImFormatString(buf, static_cast<size_t>(size), "Auto");
    if (d > 100) return ImFormatString(buf, static_cast<size_t>(size), "hp + %d", d - 100);
    return ImFormatString(buf, static_cast<size_t>(size), "%dhp", d);
}

int FmtYaw(char* buf, int size, float v)
{
    const int d = static_cast<int>(std::lround(v));
    if (d == 180 || d == -180) return ImFormatString(buf, static_cast<size_t>(size), "Backward");
    if (d == 0) return ImFormatString(buf, static_cast<size_t>(size), "Forward");
    return ImFormatString(buf, static_cast<size_t>(size), "%d\xC2\xB0", d);
}

int FmtSeed(char* buf, int size, float v) { return ImFormatString(buf, static_cast<size_t>(size), "%d", static_cast<int>(std::lround(v))); }

void StatusLine(const char* text, bool error, float alpha)
{
    if (text == nullptr || text[0] == '\0' || alpha <= 0.01f) return;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const ui::Context& c = ui::Ctx();
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, ImVec2(window->WorkRect.Max.x, pos.y + S(c.m.row_h)));
    ImGui::ItemSize(bb, 0.0f);
    if (!ImGui::ItemAdd(bb, 0)) return;
    draw::TextCapCentered(window->DrawList, c.fonts.regular, ui::FontPx(c.m.font_body), pos.x, bb.GetCenter().y,
                          draw::Col(error ? c.pal.accent : c.pal.text_soft, alpha), text);
}

} // namespace

// ---- Legit -----------------------------------------------------------------------------------------------------

void Menu::PageLegit(const ImRect& card, const ImRect& area)
{
    Legit& L = cfg_.legit;
    L.group = ImClamp(L.group, 0, kGroupCount - 1);
    ImRect left, right;
    CardSplit(card, area, left, right);
    ui::CardCombo("##group", &L.group, kGroups.items, kGroups.count, left);
    const ItemList& weapons = kWeapons[L.group];
    ui::CardCombo("##weapon", &L.weapon[L.group], weapons.items, weapons.count, right);
    ui::Ctx().path_page = kGroups.items[L.group];

    LegitWeapon& w = L.w[L.group];
    ImGui::PushID(L.group);
    ui::BeginColumns("##legit", area, 2);

    ui::BeginPanel("Main");
    ui::Checkbox("Override weapon", &w.override_weapon);
    ui::Tooltip("Use these settings instead of the group defaults for the selected weapon.");
    Multi("Ignore if", &w.ignore_if, kIgnoreIf);
    ui::Checkbox("Wall penetration", &w.wall_penetration);
    if (ui::BeginSettings("##wall")) {
        ui::Slider("Minimum damage", &w.wall_min_damage, 1.0f, 100.0f, "%.0fhp");
        ui::EndSettings();
    }
    ui::Checkbox("Backtrack", &w.backtrack);
    if (ui::BeginSettings("##backtrack")) {
        ui::Slider("Maximum time", &w.backtrack_time, 0.0f, 200.0f, "%.0fms");
        ui::EndSettings();
    }
    ui::Slider("Max aim time", &w.max_aim_time, 0.0f, 2000.0f, "%.0fms", FmtAimTime);
    ui::Slider("Target swap delay", &w.target_swap_delay, 0.0f, 1000.0f, "%.0fms");
    ui::EndPanel();

    ui::BeginPanel("Trigger bot", PanelHeight::Fill);
    ui::Checkbox("Enable", &w.trigger_enable);
    Multi("Hitboxes", &w.trigger_hitboxes, kHitboxes);
    ui::Checkbox("Overshoot", &w.overshoot);
    if (ui::BeginSettings("##overshoot")) {
        ui::Slider("Amount", &w.overshoot_amount, 0.0f, 100.0f, "%.0f%%");
        ui::EndSettings();
    }
    ui::Checkbox("Spread adjustment", &w.spread_adjustment);
    if (ui::BeginSettings("##spread")) {
        Combo("Mode", &w.spread_mode, kSpreadModes);
        ui::Checkbox("Limited", &w.spread_limited);
        ui::EndSettings();
    }
    ui::Checkbox("Wall penetration", &w.trigger_wall_penetration);
    ui::Slider("Minimum damage", &w.trigger_min_damage, 1.0f, 100.0f, "%.0fhp");
    ui::Slider("Hit chance", &w.trigger_hit_chance, 0.0f, 100.0f, "%.0f%%");
    ui::Slider("Reaction time", &w.reaction_time, 0.0f, 500.0f, "%.0fms");
    ui::Slider("Hitbox extension", &w.hitbox_extension, 0.0f, 100.0f, "%.0f%%");
    ui::EndPanel();

    ui::NextColumn();
    ui::BeginPanel("Aim assist", PanelHeight::Fill);
    Combo("##assist_mode", &w.assist_mode, kAssistModes);
    ui::Checkbox("Enable", &w.assist_enable);
    Multi("Hitboxes", &w.assist_hitboxes, kHitboxes);
    Combo("FOV style", &w.fov_style, kFovStyles);
    ui::Slider("Maximum distance", &w.max_distance, 0.0f, 100.0f, "%.1fin");
    ui::Slider("Maximum FOV", &w.max_fov, 0.0f, 30.0f, "%.1f\xC2\xB0");
    ui::Checkbox("Mouse redirection", &w.mouse_redirection);
    if (ui::BeginSettings("##redirect")) {
        ui::Slider("Strength", &w.redirection_strength, 0.0f, 100.0f, "%.0f%%");
        ui::EndSettings();
    }
    ui::Checkbox("Smoothing", &w.smoothing);
    if (ui::BeginSettings("##smooth")) {
        ui::Slider("Amount", &w.smooth_amount, 0.0f, 100.0f, "%.0f%%");
        Combo("Curve", &w.smooth_curve, kSmoothCurves);
        ui::EndSettings();
    }
    ui::Checkbox("Recoil control", &w.recoil_control);
    if (ui::BeginSettings("##rcs")) {
        ui::Slider("Vertical", &w.rcs_vertical, 0.0f, 100.0f, "%.0f%%");
        ui::Slider("Horizontal", &w.rcs_horizontal, 0.0f, 100.0f, "%.0f%%");
        ui::EndSettings();
    }
    ui::Checkbox("Quick stop", &w.quick_stop);
    ui::EndPanel();

    ui::EndColumns();
    ImGui::PopID();
}

// ---- Rage ------------------------------------------------------------------------------------------------------

void Menu::PageRage(const ImRect& card, const ImRect& area)
{
    Rage& R = cfg_.rage;
    R.category = ImClamp(R.category, 0, kRageCategories.count - 1);

    if (R.category == 0) {
        ui::CardCombo("##category", &R.category, kRageCategories.items, kRageCategories.count, CardFrame(card));
        ui::Ctx().path_page = "General";
        ui::BeginColumns("##rage_general", area, 2);

        ui::BeginPanel("Rage aim");
        ui::Checkbox("Enable", &R.enable);
        ui::Checkbox("Visualize angle", &R.visualize_angle);
        ui::Tooltip("Snaps crosshair to the aim point.");
        ui::Checkbox("Force silent", &R.force_silent);
        ui::Checkbox("Auto shoot", &R.auto_shoot);
        ui::Checkbox("Head shot only", &R.head_shot_only);
        ui::Checkbox("Remove spread", &R.remove_spread);
        if (ui::BeginSettings("##remove_spread")) {
            Combo("Mode", &R.remove_spread_mode, kRemoveSpread);
            ui::EndSettings();
        }
        ui::EndPanel();

        ui::BeginPanel("Helpers");
        ui::Checkbox("Double tap", &R.double_tap);
        if (ui::BeginSettings("##double_tap")) {
            Combo("Mode", &R.double_tap_mode, kDoubleTap);
            ui::EndSettings();
        }
        ui::Checkbox("Auto retreat", &R.auto_retreat);
        ui::Tooltip("Walks back to cover after the shot.");
        if (ui::BeginSettings("##retreat")) {
            ui::Slider("Distance", &R.retreat_distance, 10.0f, 120.0f, "%.0fu");
            ui::EndSettings();
        }
        ui::Checkbox("Auto duck peek", &R.auto_duck_peek);
        ui::Checkbox("Visualize shot matrix", &R.visualize_shot_matrix);
        if (ui::BeginSettings("##matrix")) {
            ui::ColorRow("Matrix color", &R.shot_matrix_color);
            ui::EndSettings();
        }
        ui::EndPanel();

        ui::NextColumn();
        ui::BeginPanel("Anti aim");
        ui::Checkbox("Enable", &R.aa_enable);
        Combo("Pitch", &R.pitch, kPitch);
        Combo("Yaw base", &R.yaw_base, kYawBase);
        ui::Slider("Yaw offset", &R.yaw_offset, -180.0f, 180.0f, "%.0f", FmtYaw);
        ui::Checkbox("Manual direction", &R.manual_direction);
        if (ui::BeginSettings("##manual")) {
            ui::Checkbox("Static yaw", &R.manual_static);
            ui::Slider("Offset", &R.manual_offset, 0.0f, 180.0f, "%.0f\xC2\xB0");
            ui::EndSettings();
        }
        ui::Checkbox("Jitter", &R.jitter);
        if (ui::BeginSettings("##jitter")) {
            ui::Slider("Range", &R.jitter_range, 0.0f, 180.0f, "%.0f\xC2\xB0");
            ui::EndSettings();
        }
        ui::Checkbox("Quick update", &R.quick_update);
        ui::EndPanel();
        ui::EndColumns();
        return;
    }

    const int group = R.category - 1;
    ImRect left, right;
    CardSplit(card, area, left, right);
    ui::CardCombo("##category", &R.category, kRageCategories.items, kRageCategories.count, left);
    const ItemList& weapons = kWeapons[group];
    ui::CardCombo("##weapon", &R.weapon[group], weapons.items, weapons.count, right);
    ui::Ctx().path_page = kGroups.items[group];

    RageWeapon& w = R.w[group];
    ImGui::PushID(group);
    ui::BeginColumns("##rage_weapon", area, 2);

    ui::BeginPanel("Main");
    ui::Checkbox("Override weapon", &w.override_weapon);
    ui::Slider("Maximum FOV", &w.max_fov, 1.0f, 180.0f, "%.0f\xC2\xB0");
    ui::Checkbox("Wall penetration", &w.wall_penetration);
    ui::EndPanel();

    ui::BeginPanel("Hitboxes");
    Multi("Hitboxes", &w.hitboxes, kHitboxes);
    Multi("Additional points", &w.additional_points, kHitboxes);
    ui::Slider("Point spread limit", &w.point_spread, 0.0f, 100.0f, "%.0f%%");
    ui::EndPanel();

    ui::NextColumn();
    ui::BeginPanel("Targeting");
    ui::Slider("Minimum damage", &w.min_damage, 0.0f, 110.0f, "%.0f", FmtMinDamage);
    ui::Slider("Bias", &w.bias, 0.0f, 100.0f, "%.0f");
    ui::Checkbox("Prefer last target", &w.prefer_last_target);
    ui::Checkbox("Force lethal in air", &w.force_lethal_in_air);
    ui::EndPanel();

    ui::BeginPanel("Accuracy");
    ui::Checkbox("Hit chance", &w.hit_chance);
    if (ui::BeginSettings("##hit_chance")) {
        ui::Slider("Hit chance", &w.hit_chance_value, 0.0f, 100.0f, "%.0f%%");
        ui::Checkbox("Dynamic", &w.hit_chance_dynamic);
        ui::Tooltip("Increases the chance of firing at a target that is hiding their hitboxes (e.g., by crouching).");
        ui::Checkbox("Skip if accurate", &w.hit_chance_skip);
        ui::EndSettings();
    }
    ui::Checkbox("Quick stop", &w.quick_stop);
    if (ui::BeginSettings("##quick_stop")) {
        Multi("Options", &w.quick_stop_flags, kQuickStop);
        ui::EndSettings();
    }
    ui::Checkbox("Jump scout", &w.jump_scout);
    Multi("Auto scope", &w.auto_scope, kAutoScope);
    ui::EndPanel();

    ui::EndColumns();
    ImGui::PopID();
}

// ---- Visuals ---------------------------------------------------------------------------------------------------

void Menu::PageVisuals(const ImRect& card, const ImRect& area)
{
    Visuals& V = cfg_.visuals;
    ui::CardCombo("##target", &V.target, kVisualTargets.items, kVisualTargets.count, CardFrame(card));
    ui::Ctx().path_page = kVisualTargets.items[V.target];

    if (V.target == 3) {
        WorldVisuals& W = cfg_.world;
        ui::BeginColumns("##visuals_other", area, 2);

        ui::BeginPanel("Dropped items");
        ui::Checkbox("Name", &W.dropped_name);
        ui::ColorDot("##name_col", &W.dropped_name_color);
        ui::Checkbox("Icon", &W.dropped_icon);
        ui::ColorDot("##icon_col", &W.dropped_icon_color);
        ui::Checkbox("Ammo", &W.dropped_ammo);
        ui::ColorDot("##ammo_col", &W.dropped_ammo_color);
        ui::EndPanel();

        ui::BeginPanel("Grenades");
        ui::Checkbox("Name", &W.grenade_name);
        ui::ColorDot("##name_col", &W.grenade_name_color);
        ui::Checkbox("Icon", &W.grenade_icon);
        ui::ColorDot("##icon_col", &W.grenade_icon_color);
        ui::Checkbox("Warning", &W.grenade_warning);
        if (ui::BeginSettings("##warning")) {
            ui::Slider("Distance", &W.warning_distance, 1.0f, 40.0f, "%.0fm");
            ui::EndSettings();
        }
        ui::Checkbox("Inferno range", &W.inferno_range);
        if (ui::BeginSettings("##inferno")) {
            ui::ColorRow("Range color", &W.inferno_color);
            ui::EndSettings();
        }
        ui::Checkbox("Colored smoke", &W.colored_smoke);
        if (ui::BeginSettings("##smoke")) {
            ui::ColorRow("Smoke color", &W.smoke_color);
            ui::EndSettings();
        }
        ui::EndPanel();

        ui::NextColumn();
        ui::BeginPanel("World");
        ui::Checkbox("Enable", &W.enable);
        ui::ColorRow("World color", &W.world_color);
        ui::ColorRow("Cloud color", &W.cloud_color);
        ui::ColorRow("Sun color", &W.sun_color);
        ui::ColorRow("Sky color", &W.sky_color);
        ui::Checkbox("Override skybox", &W.override_skybox);
        if (ui::BeginSettings("##skybox")) {
            Combo("Skybox", &W.skybox, kSkyboxes);
            ui::EndSettings();
        }
        ui::Checkbox("Override fog", &W.override_fog);
        if (ui::BeginSettings("##fog")) {
            ui::Slider("Start", &W.fog_start, 0.0f, 5000.0f, "%.0fu");
            ui::Slider("End", &W.fog_end, 0.0f, 5000.0f, "%.0fu");
            ui::ColorRow("Fog color", &W.fog_color);
            ui::EndSettings();
        }
        ui::Checkbox("Override lighting", &W.override_lighting);
        if (ui::BeginSettings("##lighting")) {
            ui::Slider("Brightness", &W.lighting, 0.0f, 200.0f, "%.0f%%");
            ui::EndSettings();
        }
        ui::Checkbox("Override exposure", &W.override_exposure);
        if (ui::BeginSettings("##exposure")) {
            ui::Slider("Exposure", &W.exposure, 0.0f, 100.0f, "%.0f%%");
            ui::EndSettings();
        }
        ui::Checkbox("Override particles", &W.override_particles);
        if (ui::BeginSettings("##particles")) {
            ui::ColorRow("Particle color", &W.particle_color);
            ui::EndSettings();
        }
        ui::Checkbox("Weather", &W.weather);
        if (ui::BeginSettings("##weather")) {
            Combo("Type", &W.weather_type, kWeather);
            ui::Slider("Wind direction", &W.wind_direction, -180.0f, 180.0f, "%.0f\xC2\xB0");
            ui::Slider("Wind strength", &W.wind_strength, 0.0f, 100.0f, "%.0f%%");
            ui::Slider("Wind turbulence", &W.wind_turbulence, 0.0f, 100.0f, "%.0f%%");
            ui::Slider("Density", &W.density, 0.0f, 100.0f, "%.0f%%");
            ui::Slider("Wetness", &W.wetness, 0.0f, 100.0f, "%.0f%%");
            ui::EndSettings();
        }
        ui::EndPanel();
        ui::EndColumns();
        return;
    }

    PlayerVisuals& P = V.players[ImClamp(V.target, 0, 2)];
    ImGui::PushID(V.target);
    ui::BeginColumns("##visuals_player", area, 2);

    ui::BeginPanel("Overlay", PanelHeight::Fill);
    ui::Checkbox("Enable", &P.enable);
    ui::Checkbox("Sound only", &P.sound_only);
    if (ui::BeginSettings("##sound")) {
        ui::Slider("Duration", &P.sound_duration, 0.5f, 5.0f, "%.1fs");
        ui::EndSettings();
    }
    ui::Checkbox("Box", &P.box);
    ui::ColorDot("##box_col", &P.box_color);
    ui::Checkbox("Name", &P.name);
    ui::Checkbox("Health", &P.health);
    Multi("Weapon", &P.weapon, kWeaponEsp);
    Multi("Flags", &P.flags, kFlags);
    ui::Checkbox("Skeleton", &P.skeleton);
    ui::ColorDot("##skeleton_col", &P.skeleton_color);
    ui::Checkbox("Backtrack skeleton", &P.backtrack_skeleton);
    ui::ColorDot("##backtrack_col", &P.backtrack_color);
    ui::Checkbox("Reveal on radar", &P.reveal_on_radar);
    ui::Checkbox("Off screen", &P.off_screen);
    if (ui::BeginSettings("##offscreen")) {
        ui::Slider("Radius", &P.off_screen_radius, 20.0f, 100.0f, "%.0f%%");
        ui::EndSettings();
    }
    ui::Checkbox("Bullet tracer", &P.bullet_tracer);
    if (ui::BeginSettings("##tracer")) {
        ui::Slider("Duration", &P.tracer_duration, 0.5f, 10.0f, "%.1fs");
        ui::EndSettings();
    }
    ui::Checkbox("Grenade trajectory", &P.grenade_trajectory);
    ui::ColorDot("##trajectory_col", &P.trajectory_color);
    ui::Checkbox("Visualize aim bot", &P.visualize_aimbot);
    ui::ColorDot("##aimbot_col", &P.aimbot_color);
    Multi("Animations", &P.animations, kAnimations);
    ui::EndPanel();

    ui::BeginPanel("Model");
    ui::Checkbox("Glow", &P.glow);
    ui::ColorDot("##glow_col", &P.glow_color);
    ui::Checkbox("Chams", &P.chams);
    if (ui::BeginSettings("##chams")) {
        Combo("Material", &P.chams_material, kMaterials);
        ui::ColorRow("Visible", &P.chams_visible);
        ui::ColorRow("Hidden", &P.chams_hidden);
        ui::EndSettings();
    }
    ui::EndPanel();

    ui::NextColumn();
    ui::BeginPanel(nullptr, PanelHeight::Fill);
    {
        ImGuiWindow* pane = ImGui::GetCurrentWindow();
        const ImRect r(pane->Pos, ImVec2(pane->Pos.x + pane->Size.x, pane->Pos.y + pane->Size.y));
        DrawEspPreview(pane->DrawList, r, P, ui::Ctx().time);
    }
    ui::EndPanel();

    ui::EndColumns();
    ImGui::PopID();
}

// ---- Skins -----------------------------------------------------------------------------------------------------

void Menu::PageSkins(const ImRect& card, const ImRect& area)
{
    Skins& K = cfg_.skins;
    K.group = ImClamp(K.group, 0, kGroupCount - 1);
    ImRect left, right;
    CardSplit(card, area, left, right);
    ui::CardCombo("##group", &K.group, kGroups.items, kGroups.count, left);
    const ItemList& weapons = kWeapons[K.group];
    ui::CardCombo("##weapon", &K.weapon[K.group], weapons.items, weapons.count, right);
    const int weapon = ImClamp(K.weapon[K.group], 0, weapons.count - 1);
    ui::Ctx().path_page = weapons.items[weapon];
    SkinEntry& e = K.entries[K.group][weapon];

    ImGui::PushID(K.group * 16 + weapon);
    ui::BeginColumns("##skins", area, 2);

    ui::BeginPanel("Weapon", PanelHeight::Fill);
    ui::Checkbox("Enable", &e.enable);
    Combo("Finish", &e.paint_kit, kPaintKits);
    ui::Slider("Wear", &e.wear, 0.0f, 1.0f, "%.3f");
    ui::Slider("Seed", &e.seed, 0.0f, 1000.0f, "%.0f", FmtSeed);
    ui::Checkbox("StatTrak", &e.stattrak);
    if (ui::BeginSettings("##stattrak")) {
        ui::Slider("Kills", &e.stattrak_kills, 0.0f, 9999.0f, "%.0f", FmtSeed);
        ui::EndSettings();
    }
    ui::Caption("Name tag");
    ui::InputText("##name_tag", "No name tag", e.name_tag, sizeof(e.name_tag));
    ui::EndPanel();

    ui::BeginPanel("Agents");
    Combo("Terrorist", &K.agent_t, kAgentsT);
    Combo("Counter-Terrorist", &K.agent_ct, kAgentsCt);
    ui::EndPanel();

    ui::NextColumn();
    ui::BeginPanel("Knife");
    ui::Checkbox("Enable", &K.knife_enable);
    Combo("Model", &K.knife_model, kKnives);
    Combo("Finish", &K.knife_skin, kKnifeFinishes);
    ui::Slider("Wear", &K.knife_wear, 0.0f, 1.0f, "%.3f");
    ui::EndPanel();

    ui::BeginPanel("Gloves", PanelHeight::Fill);
    ui::Checkbox("Enable", &K.gloves_enable);
    Combo("Model", &K.glove_model, kGloves);
    Combo("Finish", &K.glove_skin, kGloveFinishes);
    ui::Slider("Wear", &K.glove_wear, 0.0f, 1.0f, "%.3f");
    ui::EndPanel();

    ui::EndColumns();
    ImGui::PopID();
}

// ---- Misc ------------------------------------------------------------------------------------------------------

void Menu::PageMisc(const ImRect& card, const ImRect& area)
{
    Misc& M = cfg_.misc;
    ui::CardCombo("##page", &M.page, kMiscPages.items, kMiscPages.count, CardFrame(card));
    ui::Ctx().path_page = kMiscPages.items[M.page];
    ImGui::PushID(M.page);

    if (M.page == 0) {
        ui::BeginColumns("##misc", area, 2);
        ui::BeginPanel("Movement", PanelHeight::Fill);
        ui::Checkbox("Bunny hop", &M.bunny_hop);
        ui::Checkbox("Easy strafe", &M.easy_strafe);
        ui::Checkbox("Air strafe", &M.air_strafe);
        if (ui::BeginSettings("##air_strafe")) {
            ui::Slider("Smoothing", &M.strafe_smoothing, 0.0f, 100.0f, "%.0f%%");
            ui::Checkbox("WASD movement", &M.strafe_wasd);
            ui::EndSettings();
        }
        Combo("Override walk speed", &M.walk_speed, kWalkSpeed);
        ui::Checkbox("Quick stop", &M.quick_stop);
        ui::Checkbox("Edge jump", &M.edge_jump);
        if (ui::BeginSettings("##edge_jump")) {
            ui::Checkbox("Duck on edge jump", &M.edge_jump_duck);
            ui::EndSettings();
        }
        ui::Checkbox("Jump bug", &M.jump_bug);
        ui::Checkbox("Edge bug", &M.edge_bug);
        ui::Checkbox("Increase jump height", &M.increase_jump_height);
        ui::EndPanel();

        ui::NextColumn();
        ui::BeginPanel("Extras", PanelHeight::Fill);
        ui::Checkbox("Third person", &M.third_person);
        if (ui::BeginSettings("##third_person")) {
            ui::Slider("Distance", &M.third_person_distance, 30.0f, 300.0f, "%.0fu");
            ui::Checkbox("While dead", &M.third_person_dead);
            ui::EndSettings();
        }
        ui::Checkbox("Free camera", &M.free_camera);
        if (ui::BeginSettings("##free_camera")) {
            ui::Slider("Speed", &M.free_camera_speed, 1.0f, 20.0f, "%.0f");
            ui::EndSettings();
        }
        ui::Checkbox("Free look", &M.free_look);
        ui::Checkbox("FOV override", &M.fov_override);
        if (ui::BeginSettings("##fov", ui::SettingsIcon::Gear)) {
            ui::Slider("Field of view", &M.fov, 60.0f, 140.0f, "%.0f\xC2\xB0");
            ui::Slider("Viewmodel FOV", &M.viewmodel_fov, 54.0f, 90.0f, "%.0f\xC2\xB0");
            ui::EndSettings();
        }
        ui::Checkbox("Show viewmodel", &M.show_viewmodel);
        ui::Checkbox("Preserve death notices", &M.preserve_death_notices);
        Multi("Remove", &M.remove, kRemove);
        Multi("Reveal", &M.reveal, kReveal);
        ui::Checkbox("[DEV] Display menu paths", &M.dev_menu_paths);
        ui::EndPanel();
        ui::EndColumns();
    } else if (M.page == 1) {
        ui::BeginColumns("##automation", area, 2);
        ui::BeginPanel("Automation", PanelHeight::Fill);
        ui::Checkbox("Quick switch", &M.quick_switch);
        if (ui::BeginSettings("##quick_switch")) {
            ui::Slider("Delay", &M.quick_switch_delay, 0.0f, 500.0f, "%.0fms");
            ui::EndSettings();
        }
        ui::Checkbox("Straight throw", &M.straight_throw);
        ui::Checkbox("Auto nade release", &M.auto_nade_release);
        if (ui::BeginSettings("##nade_release")) {
            ui::Slider("Minimum damage", &M.nade_release_damage, 1.0f, 100.0f, "%.0fhp");
            ui::EndSettings();
        }
        ui::Checkbox("Auto buy", &M.auto_buy);
        if (ui::BeginSettings("##auto_buy")) {
            Combo("Primary", &M.auto_buy_primary, kPrimaries);
            Combo("Secondary", &M.auto_buy_secondary, kSecondaries);
            Multi("Utility", &M.auto_buy_utility, kUtility);
            ui::EndSettings();
        }
        ui::Checkbox("Auto accept", &M.auto_accept);
        ui::Checkbox("Auto pistol", &M.auto_pistol);
        ui::EndPanel();

        ui::NextColumn();
        ui::BeginPanel("Safety");
        Combo("Anti cheat safety", &M.anti_cheat_safety, kAntiCheat);
        Combo("New round alert", &M.new_round_alert, kRoundAlert);
        ui::EndPanel();
        ui::EndColumns();
    } else {
        ui::BeginColumns("##interface", area, 2);
        ui::BeginPanel("Menu");
        Combo("Accent", &M.accent, kAccents);
        if (M.accent == static_cast<int>(AccentPreset::Custom)) {
            ui::ColorRow("Accent start", &M.accent_start, false);
            ui::ColorRow("Accent end", &M.accent_end, false);
        }
        Combo("Scale", &M.dpi_mode, kScales);
        ui::Checkbox("Animations", &M.menu_animations);
        ui::EndPanel();

        ui::BeginPanel("Help", PanelHeight::Fill);
        ui::Caption("Insert  -  show / hide the menu");
        ui::Caption("Right click  -  bind an option to a key");
        ui::Caption("Ctrl + F  -  search options");
        ui::Separator();
        ui::Caption("Astral-Remake by kdt255");
        ui::Caption("Free - if you paid for this, you got scammed");
        ui::EndPanel();

        ui::NextColumn();
        ui::BeginPanel("Indicators", PanelHeight::Fill);
        ui::Checkbox("Watermark", &M.watermark);
        ui::Checkbox("Binds list", &M.binds_list);
        ui::Checkbox("Spectator list", &M.spectator_list);
        ui::Checkbox("Hit marker", &M.hit_marker);
        ui::ColorDot("##hit_marker_col", &M.hit_marker_color);
        Combo("Hit sound", &M.hit_sound, kHitSounds);
        ui::EndPanel();
        ui::EndColumns();
    }
    ImGui::PopID();
}

// ---- Configs ---------------------------------------------------------------------------------------------------

void Menu::RefreshConfigs()
{
    configs_ = persist::ListConfigs(config_sort_ == 0 ? persist::SortOrder::Modified : persist::SortOrder::Name);
    if (config_selected_ >= static_cast<int>(configs_.size())) config_selected_ = static_cast<int>(configs_.size()) - 1;
    configs_loaded_ = true;
}

void Menu::PageConfigs(const ImRect& card, const ImRect& area)
{
    const ui::Context& c = ui::Ctx();
    if (!configs_loaded_) RefreshConfigs();
    if (ui::CardCombo("##sort", &config_sort_, kConfigSort.items, kConfigSort.count, CardFrame(card))) RefreshConfigs();

    auto report = [this](const std::string& text, bool error) {
        config_status_ = text;
        config_status_error_ = error;
        config_status_time_ = ui::Ctx().time;
    };
    auto selected_name = [this]() -> std::string {
        return config_selected_ >= 0 && config_selected_ < static_cast<int>(configs_.size()) ? configs_[config_selected_].name : std::string();
    };

    ui::BeginColumns("##configs", area, 2);
    ui::BeginPanel("Configs", PanelHeight::Fill);
    if (configs_.empty()) ui::Caption("No configs yet");
    for (int i = 0; i < static_cast<int>(configs_.size()); ++i) {
        ImGui::PushID(i);
        if (ui::ListRow(configs_[i].name.c_str(), configs_[i].modified.c_str(), i == config_selected_)) {
            config_selected_ = i;
            delete_armed_ = false;
            ImFormatString(config_name_, sizeof(config_name_), "%s", configs_[i].name.c_str());
        }
        ImGui::PopID();
    }
    ui::EndPanel();

    ui::NextColumn();
    ui::BeginPanel("Actions", PanelHeight::Fill);
    ui::Caption("Name");
    ui::InputText("##config_name", "Config name", config_name_, sizeof(config_name_));

    std::string error;
    if (ui::Button("Create", 0.0f, true)) {
        const std::string name = config_name_;
        if (persist::SaveConfig(cfg_, name, error)) {
            report("Created " + name, false);
            RefreshConfigs();
            for (int i = 0; i < static_cast<int>(configs_.size()); ++i)
                if (configs_[i].name == name) config_selected_ = i;
        } else {
            report(error, true);
        }
    }
    if (ui::Button("Save")) {
        const std::string name = selected_name();
        if (name.empty()) report("Select a config first", true);
        else if (persist::SaveConfig(cfg_, name, error)) report("Saved " + name, false), RefreshConfigs();
        else report(error, true);
    }
    if (ui::Button("Load")) {
        const std::string name = selected_name();
        if (name.empty()) report("Select a config first", true);
        else if (persist::LoadConfig(cfg_, name, error)) report("Loaded " + name, false);
        else report(error, true);
    }
    if (ui::Button(delete_armed_ ? "Click again to delete##delete" : "Delete##delete")) {
        const std::string name = selected_name();
        if (name.empty()) {
            report("Select a config first", true);
        } else if (!delete_armed_) {
            delete_armed_ = true;
        } else {
            delete_armed_ = false;
            if (persist::DeleteConfig(name, error)) report("Deleted " + name, false), RefreshConfigs();
            else report(error, true);
        }
    }
    ui::Separator();
    if (ui::Button("Export to clipboard")) {
        ImGui::SetClipboardText(persist::ExportConfig(cfg_).c_str());
        report("Config copied to clipboard", false);
    }
    if (ui::Button("Import from clipboard")) {
        if (persist::ImportConfig(cfg_, ImGui::GetClipboardText(), error)) report("Imported from clipboard", false);
        else report(error, true);
    }
    if (ui::Button("Open folder")) persist::OpenFolder(false);

    const float age = static_cast<float>(c.time - config_status_time_);
    StatusLine(config_status_.c_str(), config_status_error_, ImSaturate((5.0f - age) / 0.6f));
    ui::EndPanel();
    ui::EndColumns();
}

// ---- Scripts ---------------------------------------------------------------------------------------------------

void Menu::RefreshScripts()
{
    scripts_ = persist::ListScripts();
    autoload_ = persist::LoadAutoload();
    if (script_selected_ >= static_cast<int>(scripts_.size())) script_selected_ = -1;
    preview_.clear();
    if (script_selected_ >= 0) preview_ = persist::ReadScriptPreview(scripts_[script_selected_].name, 40);
    scripts_loaded_ = true;
}

void Menu::PageScripts(const ImRect& card, const ImRect& area)
{
    const ui::Context& c = ui::Ctx();
    if (!scripts_loaded_) RefreshScripts();
    ui::CardCombo("##filter", &script_filter_, kScriptFilter.items, kScriptFilter.count, CardFrame(card));

    auto is_autoload = [this](const std::string& name) { return std::find(autoload_.begin(), autoload_.end(), name) != autoload_.end(); };

    ui::BeginColumns("##scripts", area, 2);
    ui::BeginPanel("Scripts", PanelHeight::Fill);
    int shown = 0;
    for (int i = 0; i < static_cast<int>(scripts_.size()); ++i) {
        const bool autoload = is_autoload(scripts_[i].name);
        if (script_filter_ == 1 && !autoload) continue;
        ++shown;
        char detail[48];
        if (autoload) ImFormatString(detail, sizeof(detail), "autoload");
        else ImFormatString(detail, sizeof(detail), "%.1f kb", static_cast<double>(scripts_[i].size) / 1024.0);
        ImGui::PushID(i);
        if (ui::ListRow(scripts_[i].name.c_str(), detail, i == script_selected_)) {
            script_selected_ = i;
            preview_ = persist::ReadScriptPreview(scripts_[i].name, 40);
        }
        ImGui::PopID();
    }
    if (shown == 0) ui::Caption(scripts_.empty() ? "Put .lua files into the scripts folder" : "No autoload scripts");
    ui::EndPanel();

    ui::NextColumn();
    ui::BeginPanel("Script", PanelHeight::Fill);
    if (script_selected_ >= 0 && script_selected_ < static_cast<int>(scripts_.size())) {
        const persist::FileEntry& s = scripts_[script_selected_];
        bool autoload = is_autoload(s.name);
        if (ui::Checkbox("Load on startup", &autoload)) {
            if (autoload) autoload_.push_back(s.name);
            else autoload_.erase(std::remove(autoload_.begin(), autoload_.end(), s.name), autoload_.end());
            std::string error;
            persist::SaveAutoload(autoload_, error);
        }
        ui::Checkbox("Reload on change", &cfg_.scripts.reload_on_change);

        // Read-only source preview
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        const float fs = ui::FontPx(c.m.font_small + 1.0f);
        const float line_h = draw::Snap(fs * 1.2f);
        const int lines = static_cast<int>(preview_.size());
        const ImVec2 pos = window->DC.CursorPos;
        const float pad = S(8.0f);
        const ImRect block(pos, ImVec2(window->WorkRect.Max.x, pos.y + pad * 2.0f + line_h * static_cast<float>(ImMax(lines, 1))));
        ImGui::ItemSize(block, 0.0f);
        if (ImGui::ItemAdd(block, 0)) {
            ImDrawList* dl = window->DrawList;
            dl->AddRectFilled(block.Min, block.Max, draw::Col(c.pal.frame), ui::Px(c.m.frame_rounding));
            dl->PushClipRect(block.Min, block.Max, true);
            if (lines == 0)
                draw::Text(dl, c.fonts.regular, fs, ImVec2(block.Min.x + pad, block.Min.y + pad), draw::Col(c.pal.text_dim), "(empty file)");
            for (int i = 0; i < lines; ++i) {
                const float y = block.Min.y + pad + line_h * static_cast<float>(i);
                char number[8];
                ImFormatString(number, sizeof(number), "%2d", i + 1);
                draw::Text(dl, c.fonts.regular, fs, ImVec2(block.Min.x + pad, y), draw::Col(c.pal.grip), number);
                draw::Text(dl, c.fonts.regular, fs, ImVec2(block.Min.x + pad + S(22.0f), y), draw::Col(c.pal.text_soft), preview_[i].c_str());
            }
            dl->PopClipRect();
        }
    } else {
        ui::Caption("Select a script to inspect it");
    }
    if (ui::Button("Refresh")) RefreshScripts();
    if (ui::Button("Open folder")) persist::OpenFolder(true);
    ui::EndPanel();
    ui::EndColumns();
}

} // namespace astral
