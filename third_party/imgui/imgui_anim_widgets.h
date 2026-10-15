#pragma once
// [CUSTOM] Widget hover/durum animasyonu - tek yerden yonetilen, frame-rate bagimsiz gecisler.
// imgui_widgets.cpp icindeki stok widget'lar ve kendi custom widget'larin ayni kanallari kullanir.
//
//   ImAnimWidgets::Update(id, hovered, held, value)  -> per-ID yumusatilmis {Hot, Act, Val}
//   ImAnimWidgets::FrameColor(base, hovered, active, st)
//   ImAnimWidgets::NewFrame()                        -> her frame bir kez (eski ID'leri temizler)
//
// Warning: UI thread'e ozel. Worker thread'den cagrilamaz (StateMap thread-safe degil).
#include "anim.h"

namespace ImAnimWidgets
{

struct ItemState
{
    float Hot = 0.0f;       // hover 0..1
    float Act = 0.0f;       // basili/aktif 0..1
    float Val = 0.0f;       // acik-kapali durum (checkbox vb.) 0..1
    bool  Seeded = false;   // ilk frame'de hedeften basla, sifirdan animasyon yapma
};

inline Anim::StateMap<ItemState>& States()
{
    static Anim::StateMap<ItemState> s;
    return s;
}

// value < 0 -> bu widget'ta durum kanali kullanilmiyor.
inline const ItemState& Update(ImGuiID id, bool hovered, bool held, float value = -1.0f)
{
    ItemState& st = States().Get(id);
    const float dt = Anim::Dt();
    const float hot_t = hovered ? 1.0f : 0.0f;
    const float act_t = held ? 1.0f : 0.0f;

    if (!st.Seeded)
    {
        st.Hot = hot_t;
        st.Act = act_t;
        st.Val = (value >= 0.0f) ? value : 0.0f;
        st.Seeded = true;
        return st;
    }

    st.Hot = Anim::Smooth(st.Hot, hot_t, 18.0f, dt);
    st.Act = Anim::Smooth(st.Act, act_t, 30.0f, dt);
    if (value >= 0.0f)
        st.Val = Anim::Smooth(st.Val, value, 20.0f, dt);

    if (st.Hot != hot_t || st.Act != act_t || (value >= 0.0f && st.Val != value))
        Anim::Request();                                    // idle dongu uyumasin
    return st;
}

// base -> hovered -> active karisimi (ImGuiCol_ indeksleri).
inline ImU32 FrameColor(ImGuiCol base, ImGuiCol hovered, ImGuiCol active, const ItemState& st)
{
    ImU32 col = ImGui::GetColorU32(base);
    if (st.Hot > 0.0f)
        col = Anim::LerpColor(col, ImGui::GetColorU32(hovered), st.Hot);
    if (st.Act > 0.0f)
        col = Anim::LerpColor(col, ImGui::GetColorU32(active), st.Act);
    return col;
}

inline void NewFrame()
{
    States().Collect();
}

} // namespace ImAnimWidgets
