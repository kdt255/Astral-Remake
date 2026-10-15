#pragma once
// Frame-rate independent animation toolkit for Dear ImGui - compile-verified against 1.91.9 and 1.92.9.
//
//   Ease::*         easing curves on t in [0,1]
//   Smooth()        exponential approach (retargetable at any time, no discontinuity, no duration)
//   SmoothDamp()    critically damped spring with velocity (natural motion, never overshoots)
//   Tween           bidirectional progress that follows a bool (hover in/out, open/close) - reversal is seamless
//   Presence        show/hide with fade+slide progress; Visible() tells when to stop drawing
//   StateMap<T>     per-ImGuiID state with stable references and frame-based garbage collection
//   Request()       "something is still moving" flag so an event-driven host loop can sleep when idle
//   AlphaScope      RAII fade of everything drawn inside (style.Alpha)
#include "imgui.h"
#include "imgui_internal.h"

#include <cfloat>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace Anim {

// Clamped so a breakpoint / alt-tab hitch (DeltaTime of seconds) does not teleport animations.
[[nodiscard]] inline float Dt() noexcept { return ImMin(ImGui::GetIO().DeltaTime, 1.0f / 15.0f); }

[[nodiscard]] constexpr float Clamp01(float t) noexcept { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); }

// ---------------------------------------------------------------------------------------------------------------
// Activity tracking for event-driven render loops
// ---------------------------------------------------------------------------------------------------------------
inline bool g_animating = false;

// Call from every animation that has not settled yet. The host loop reads it once per frame (see SKILL.md).
inline void Request() noexcept { g_animating = true; }

[[nodiscard]] inline bool ConsumeAnimating() noexcept
{
    const bool was = g_animating;
    g_animating = false;
    return was;
}

// ---------------------------------------------------------------------------------------------------------------
// Easing
// ---------------------------------------------------------------------------------------------------------------
namespace Ease {

[[nodiscard]] constexpr float Linear(float t) noexcept { return t; }
[[nodiscard]] constexpr float InQuad(float t) noexcept { return t * t; }
[[nodiscard]] constexpr float OutQuad(float t) noexcept { return 1.0f - (1.0f - t) * (1.0f - t); }
[[nodiscard]] constexpr float InOutQuad(float t) noexcept
{
    return t < 0.5f ? 2.0f * t * t : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
}
[[nodiscard]] constexpr float OutCubic(float t) noexcept
{
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}
[[nodiscard]] constexpr float InOutCubic(float t) noexcept
{
    const float u = -2.0f * t + 2.0f;
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - u * u * u * 0.5f;
}
[[nodiscard]] inline float OutExpo(float t) noexcept { return t >= 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t); }
[[nodiscard]] constexpr float OutBack(float t, float overshoot = 1.70158f) noexcept
{
    const float u = t - 1.0f;
    return 1.0f + u * u * ((overshoot + 1.0f) * u + overshoot);
}
[[nodiscard]] inline float OutElastic(float t) noexcept
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    constexpr float c4 = 2.0943951f; // 2*pi/3
    return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
}

} // namespace Ease

// ---------------------------------------------------------------------------------------------------------------
// Smoothing, springs
// ---------------------------------------------------------------------------------------------------------------

// current -> target with half-life behaviour; `rate` ~ 10 (slow) .. 30 (snappy) per second. Retargeting mid-flight is
// seamless, which is why this (not a timed tween) is the default for hover/selection feedback.
[[nodiscard]] inline float Smooth(float current, float target, float rate, float dt) noexcept
{
    const float v = target + (current - target) * std::exp(-rate * dt);
    return std::fabs(v - target) < 0.002f ? target : v; // settle exactly, so Request() can stop being called
}

// Critically damped spring (Game Programming Gems 4 / the "SmoothDamp" formulation). `smooth_time` ~ time to reach the
// target. Stable for any dt. `velocity` is state owned by the caller.
[[nodiscard]] inline float SmoothDamp(float current, float target, float& velocity, float smooth_time, float dt,
                                      float max_speed = FLT_MAX) noexcept
{
    if (dt <= 0.0f) return current;
    smooth_time = ImMax(0.0001f, smooth_time);
    const float omega = 2.0f / smooth_time;
    const float x = omega * dt;
    const float damp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

    const float goal = target;
    float change = current - target;
    if (max_speed < FLT_MAX) {
        const float max_change = max_speed * smooth_time;
        change = ImClamp(change, -max_change, max_change);
    }
    target = current - change;

    const float temp = (velocity + omega * change) * dt;
    velocity = (velocity - omega * temp) * damp;
    float out = target + (change + temp) * damp;

    if ((goal - current > 0.0f) == (out > goal)) { // overshoot guard
        out = goal;
        velocity = 0.0f;
    }
    return out;
}

struct Spring {
    float value = 0.0f;
    float velocity = 0.0f;

    float Update(float target, float smooth_time, float dt) noexcept
    {
        value = SmoothDamp(value, target, velocity, smooth_time, dt);
        if (std::fabs(value - target) < 0.001f && std::fabs(velocity) < 0.001f) {
            value = target;
            velocity = 0.0f;
        }
        return value;
    }
    [[nodiscard]] bool Settled(float target) const noexcept { return value == target && velocity == 0.0f; }
};

// ---------------------------------------------------------------------------------------------------------------
// Tween: linear progress t in [0,1] that follows a boolean. Reversing mid-way continues from the current progress, so
// there is never a jump. Apply the easing when *reading* (Eased()), not when advancing.
// ---------------------------------------------------------------------------------------------------------------
struct Tween {
    float t = 0.0f;
    float duration = 0.20f;

    float Update(bool forward, float dt) noexcept
    {
        const float step = dt / ImMax(duration, 0.0001f);
        t = Clamp01(forward ? t + step : t - step);
        if (t > 0.0f && t < 1.0f) Request();
        return t;
    }
    [[nodiscard]] float Eased(float (*ease)(float) = &Ease::OutCubic) const noexcept { return ease(t); }
    [[nodiscard]] bool Moving() const noexcept { return t > 0.0f && t < 1.0f; }
};

// Show/hide helper: draw while Visible(); use Progress() for alpha and a few pixels of slide.
class Presence {
public:
    explicit Presence(float duration = 0.18f) noexcept { tween_.duration = duration; }

    float Update(bool open) noexcept { tween_.Update(open, Dt()); return Progress(); }
    [[nodiscard]] float Progress() const noexcept { return tween_.Eased(); }
    [[nodiscard]] bool Visible() const noexcept { return tween_.t > 0.0f; }
    [[nodiscard]] bool FullyOpen() const noexcept { return tween_.t >= 1.0f; }

private:
    Tween tween_;
};

// ---------------------------------------------------------------------------------------------------------------
// Colors
// ---------------------------------------------------------------------------------------------------------------
[[nodiscard]] inline ImU32 LerpColor(ImU32 a, ImU32 b, float t) noexcept
{
    const ImVec4 ca = ImGui::ColorConvertU32ToFloat4(a);
    const ImVec4 cb = ImGui::ColorConvertU32ToFloat4(b);
    return ImGui::ColorConvertFloat4ToU32(
        ImVec4(ImLerp(ca.x, cb.x, t), ImLerp(ca.y, cb.y, t), ImLerp(ca.z, cb.z, t), ImLerp(ca.w, cb.w, t)));
}

[[nodiscard]] inline ImU32 ScaleAlpha(ImU32 col, float a) noexcept
{
    const float src = static_cast<float>((col >> IM_COL32_A_SHIFT) & 0xFF);
    const ImU32 alpha = static_cast<ImU32>(src * ImSaturate(a) + 0.5f);
    return (col & ~static_cast<ImU32>(IM_COL32_A_MASK)) | (alpha << IM_COL32_A_SHIFT);
}

// Multiplies style.Alpha for everything emitted while alive (window chrome, widgets, text). Put it around Begin()/End().
class AlphaScope {
public:
    explicit AlphaScope(float alpha) { ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * ImSaturate(alpha)); }
    ~AlphaScope() { ImGui::PopStyleVar(); }
    AlphaScope(const AlphaScope&) = delete;
    AlphaScope& operator=(const AlphaScope&) = delete;
};

// ---------------------------------------------------------------------------------------------------------------
// StateMap: per-ID animation state. Unlike ImGuiStorage::GetFloatRef(), references stay valid across insertions
// (node-based map), so several channels can be held at once. Entries untouched for `max_age` frames are dropped,
// which keeps widgets inside collapsed headers / closed tabs from leaking state forever.
// UI thread only.
// ---------------------------------------------------------------------------------------------------------------
template <class T>
class StateMap {
public:
    [[nodiscard]] T& Get(ImGuiID id, const T& initial = T{})
    {
        Entry& e = map_[id];
        if (e.last_frame < 0) e.value = initial;
        e.last_frame = ImGui::GetFrameCount();
        return e.value;
    }

    // Call once per frame; the sweep itself only runs every `interval` frames.
    void Collect(int max_age = 240, int interval = 60)
    {
        const int frame = ImGui::GetFrameCount();
        if (frame - last_sweep_ < interval) return;
        last_sweep_ = frame;
        for (auto it = map_.begin(); it != map_.end();) {
            if (frame - it->second.last_frame > max_age) it = map_.erase(it);
            else ++it;
        }
    }

    [[nodiscard]] std::size_t Size() const noexcept { return map_.size(); }
    void Clear() noexcept { map_.clear(); }

private:
    struct Entry {
        T value{};
        int last_frame = -1;
    };
    std::unordered_map<ImGuiID, Entry> map_;
    int last_sweep_ = 0;
};

} // namespace Anim
