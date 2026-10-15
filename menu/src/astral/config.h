// Astral-Remake - made by kdt255 (https://github.com/kdt255/Astral-Remake)
// Free software: MIT + Commons Clause, selling is not permitted. If you paid for this, you got scammed.
#pragma once
// Menu state. Kept trivially copyable on purpose: configs are persisted as a versioned, checksummed blob.
#include "astral/spec.h"
#include "imgui.h"

#include <cstdint>
#include <type_traits>

namespace astral {

struct ColorValue {
    ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    float rainbow = 0.0f; // hue cycles per 10 s (0..100)
    float pulse = 0.0f;   // alpha breathing strength (0..100)

    [[nodiscard]] ImVec4 Resolve(double time) const;
};

[[nodiscard]] constexpr ColorValue MakeColor(std::uint32_t rgb, float a = 1.0f) noexcept
{
    return ColorValue{ Rgb(rgb, a), 0.0f, 0.0f };
}

enum WeaponGroup : int { kPistol, kHeavyPistol, kSmg, kRifle, kShotgun, kSniper, kMachineGun, kGroupCount };

struct LegitWeapon {
    bool override_weapon = true;
    unsigned ignore_if = (1u << 2) | (1u << 3);
    bool wall_penetration = true;
    float wall_min_damage = 25.0f;
    bool backtrack = true;
    float backtrack_time = 200.0f;
    float max_aim_time = 2000.0f;
    float target_swap_delay = 0.0f;

    bool trigger_enable = false;
    unsigned trigger_hitboxes = 0x1Fu;
    bool overshoot = true;
    float overshoot_amount = 35.0f;
    bool spread_adjustment = true;
    int spread_mode = 0;
    bool spread_limited = false;
    bool trigger_wall_penetration = false;
    float trigger_min_damage = 90.0f;
    float trigger_hit_chance = 100.0f;
    float reaction_time = 0.0f;
    float hitbox_extension = 80.0f;

    int assist_mode = 0;
    bool assist_enable = false;
    unsigned assist_hitboxes = 0x3u;
    int fov_style = 1;
    float max_distance = 30.0f;
    float max_fov = 5.0f;
    bool mouse_redirection = false;
    float redirection_strength = 50.0f;
    bool smoothing = true;
    float smooth_amount = 40.0f;
    int smooth_curve = 0;
    bool recoil_control = true;
    float rcs_vertical = 100.0f;
    float rcs_horizontal = 100.0f;
    bool quick_stop = false;
};

struct RageWeapon {
    bool override_weapon = true;
    float max_fov = 180.0f;
    bool wall_penetration = true;
    unsigned hitboxes = 0x1Fu;
    unsigned additional_points = 0x1Fu;
    float point_spread = 86.0f;
    float min_damage = 101.0f;
    float bias = 50.0f;
    bool prefer_last_target = true;
    bool force_lethal_in_air = false;
    bool hit_chance = true;
    float hit_chance_value = 65.0f;
    bool hit_chance_dynamic = false;
    bool hit_chance_skip = false;
    bool quick_stop = true;
    unsigned quick_stop_flags = 0x1u;
    bool jump_scout = true;
    unsigned auto_scope = 0u;
};

struct Rage {
    int category = 0; // 0 = General, 1.. = weapon group + 1
    int weapon[kGroupCount] = {};

    bool enable = true;
    bool visualize_angle = true;
    bool force_silent = true;
    bool auto_shoot = true;
    bool head_shot_only = true;
    bool remove_spread = true;
    int remove_spread_mode = 0;

    bool double_tap = true;
    int double_tap_mode = 0;
    bool auto_retreat = true;
    float retreat_distance = 40.0f;
    bool auto_duck_peek = false;
    bool visualize_shot_matrix = true;
    ColorValue shot_matrix_color = MakeColor(0x4E8BFF, 0.7f);

    bool aa_enable = false;
    int pitch = 0;
    int yaw_base = 0;
    float yaw_offset = 180.0f;
    bool manual_direction = false;
    bool manual_static = true;
    float manual_offset = 90.0f;
    bool jitter = false;
    float jitter_range = 30.0f;
    bool quick_update = false;

    RageWeapon w[kGroupCount];
};

struct Legit {
    int group = kRifle;
    int weapon[kGroupCount] = {};
    LegitWeapon w[kGroupCount];
};

struct PlayerVisuals {
    bool enable = true;
    bool sound_only = false;
    float sound_duration = 2.0f;
    bool box = false;
    ColorValue box_color = MakeColor(0xFFFFFF);
    bool name = true;
    ColorValue name_color = MakeColor(0xFFFFFF);
    bool health = true;
    unsigned weapon = (1u << 1) | (1u << 2);
    unsigned flags = (1u << 1) | (1u << 3) | (1u << 5) | (1u << 6);
    bool skeleton = false;
    ColorValue skeleton_color = MakeColor(0xFFFFFF);
    bool backtrack_skeleton = false;
    ColorValue backtrack_color = MakeColor(0xE8A07E, 0.25f);
    bool reveal_on_radar = true;
    bool off_screen = false;
    float off_screen_radius = 60.0f;
    bool bullet_tracer = false;
    float tracer_duration = 2.0f;
    bool grenade_trajectory = false;
    ColorValue trajectory_color = MakeColor(0x7EA6FF);
    bool visualize_aimbot = false;
    ColorValue aimbot_color = MakeColor(0xFF5A5A);
    unsigned animations = 0x3u;

    bool glow = true;
    ColorValue glow_color = MakeColor(0xFF707B);
    bool chams = true;
    int chams_material = 0;
    ColorValue chams_visible = MakeColor(0xFF8A84);
    ColorValue chams_hidden = MakeColor(0x7E8CFF, 0.6f);
};

struct WorldVisuals {
    bool dropped_name = true;
    ColorValue dropped_name_color = MakeColor(0xE6E6EA);
    bool dropped_icon = true;
    ColorValue dropped_icon_color = MakeColor(0xE6E6EA);
    bool dropped_ammo = true;
    ColorValue dropped_ammo_color = MakeColor(0x1C8EF0);

    bool grenade_name = true;
    ColorValue grenade_name_color = MakeColor(0xE6E6EA);
    bool grenade_icon = true;
    ColorValue grenade_icon_color = MakeColor(0xE6E6EA);
    bool grenade_warning = true;
    float warning_distance = 15.0f;
    bool inferno_range = true;
    ColorValue inferno_color = MakeColor(0xFF6A2A, 0.6f);
    bool colored_smoke = true;
    ColorValue smoke_color = MakeColor(0x8EA2FF, 0.8f);

    bool enable = true;
    ColorValue world_color = MakeColor(0xFFFFFF);
    ColorValue cloud_color = MakeColor(0xF00000);
    ColorValue sun_color = MakeColor(0x5A5A5A);
    ColorValue sky_color = MakeColor(0x8A1414);
    bool override_skybox = true;
    int skybox = 0;
    bool override_fog = false;
    float fog_start = 0.0f;
    float fog_end = 3000.0f;
    ColorValue fog_color = MakeColor(0xB7C4D8, 0.5f);
    bool override_lighting = false;
    float lighting = 100.0f;
    bool override_exposure = false;
    float exposure = 50.0f;
    bool override_particles = false;
    ColorValue particle_color = MakeColor(0xFFFFFF);
    bool weather = true;
    int weather_type = 0;
    float wind_direction = 0.0f;
    float wind_strength = 20.0f;
    float wind_turbulence = 20.0f;
    float density = 100.0f;
    float wetness = 20.0f;
};

struct Visuals {
    int target = 0; // Enemy, Team, Local, Other
    PlayerVisuals players[3];
};

struct SkinEntry {
    bool enable = false;
    int paint_kit = 0;
    float wear = 0.01f;
    float seed = 0.0f;
    bool stattrak = false;
    float stattrak_kills = 1337.0f;
    char name_tag[32] = {};
};

struct Skins {
    int group = kRifle;
    int weapon[kGroupCount] = {};
    SkinEntry entries[kGroupCount][8];
    bool knife_enable = true;
    int knife_model = 2;
    int knife_skin = 4;
    float knife_wear = 0.02f;
    bool gloves_enable = false;
    int glove_model = 0;
    int glove_skin = 0;
    float glove_wear = 0.05f;
    int agent_t = 0;
    int agent_ct = 0;
};

struct Misc {
    int page = 0; // Miscellaneous, Automation, Interface

    bool bunny_hop = true;
    bool easy_strafe = true;
    bool air_strafe = true;
    float strafe_smoothing = 50.0f;
    bool strafe_wasd = true;
    int walk_speed = 0;
    bool quick_stop = false;
    bool edge_jump = false;
    bool edge_jump_duck = true;
    bool jump_bug = false;
    bool edge_bug = false;
    bool increase_jump_height = false;

    bool third_person = false;
    float third_person_distance = 120.0f;
    bool third_person_dead = true;
    bool free_camera = false;
    float free_camera_speed = 5.0f;
    bool free_look = false;
    bool fov_override = true;
    float fov = 100.0f;
    float viewmodel_fov = 68.0f;
    bool show_viewmodel = false;
    bool preserve_death_notices = true;
    unsigned remove = (1u << 0) | (1u << 1);
    unsigned reveal = (1u << 0) | (1u << 1);
    bool dev_menu_paths = false;

    bool quick_switch = false;
    bool straight_throw = false;
    bool auto_nade_release = false;
    float nade_release_damage = 50.0f;
    bool auto_buy = false;
    float quick_switch_delay = 0.0f;
    int auto_buy_primary = 0;
    int auto_buy_secondary = 0;
    unsigned auto_buy_utility = 0x0u;
    bool auto_accept = true;
    bool auto_pistol = false;
    int new_round_alert = 0;
    int anti_cheat_safety = 0;

    int accent = static_cast<int>(AccentPreset::Salmon);
    ColorValue accent_start = MakeColor(0xFF8A84);
    ColorValue accent_end = MakeColor(0xFFB27C);
    int dpi_mode = 0;
    bool menu_animations = true;
    bool watermark = true;
    bool binds_list = true;
    ColorValue binds_list_accent = MakeColor(0xFF8A84);
    bool spectator_list = false;
    bool hit_marker = true;
    ColorValue hit_marker_color = MakeColor(0xFFFFFF);
    int hit_sound = 0;
};

struct Scripts {
    bool allow_unsafe = false;
    bool autoload = true;
    bool reload_on_change = true;
};

struct Config {
    Legit legit;
    Rage rage;
    Visuals visuals;
    WorldVisuals world;
    Skins skins;
    Misc misc;
    Scripts scripts;
};

static_assert(std::is_trivially_copyable_v<Config>, "Config is persisted as a raw blob; keep it trivially copyable");

} // namespace astral
