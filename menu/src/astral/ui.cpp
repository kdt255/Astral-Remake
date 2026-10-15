// Astral-Remake - made by kdt255 (https://github.com/kdt255/Astral-Remake)
// Free software: MIT + Commons Clause, selling is not permitted. If you paid for this, you got scammed.
#include "astral/ui.h"

#include "astral/draw.h"

#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace astral::ui {

namespace {

Context g_ctx;

constexpr ImGuiWindowFlags kPopupFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                                         ImGuiWindowFlags_NoScrollWithMouse;

[[nodiscard]] float S(float v) { return draw::Snap(Px(v)); }

[[nodiscard]] float ContentRight(ImGuiWindow* window)
{
    return g_ctx.right_override > 0.0f ? g_ctx.right_override : window->WorkRect.Max.x;
}

[[nodiscard]] float BodySize() { return FontPx(g_ctx.m.font_body); }

[[nodiscard]] bool ContainsNoCase(const char* begin, const char* end, const char* needle)
{
    const size_t n = std::strlen(needle);
    if (n == 0) return true;
    for (const char* p = begin; p + n <= end; ++p) {
        size_t i = 0;
        while (i < n && std::tolower(static_cast<unsigned char>(p[i])) == std::tolower(static_cast<unsigned char>(needle[i]))) ++i;
        if (i == n) return true;
    }
    return false;
}

// Dims rows that do not match the footer search. Accessories reuse the verdict of the row they belong to.
class SearchScope {
public:
    explicit SearchScope(const char* label, bool accessory = false)
    {
        bool dim = false;
        if (GImGui->BeginPopupStack.Size > 0) {
            dim = false;
        } else if (accessory) {
            dim = g_ctx.last_dimmed;
        } else if (g_ctx.search[0] != '\0' && label != nullptr) {
            dim = !ContainsNoCase(label, ImGui::FindRenderedTextEnd(label), g_ctx.search);
            g_ctx.last_dimmed = dim;
        } else {
            g_ctx.last_dimmed = false;
        }
        if (dim) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.26f);
            pushed_ = true;
        }
    }
    ~SearchScope()
    {
        if (pushed_) ImGui::PopStyleVar();
    }
    SearchScope(const SearchScope&) = delete;
    SearchScope& operator=(const SearchScope&) = delete;

private:
    bool pushed_ = false;
};

void ShowTooltip(const char* text)
{
    Context& c = g_ctx;
    const ImGuiStyle& style = ImGui::GetStyle();
    const float fade = ImSaturate((GImGui->HoverItemDelayTimer - style.HoverDelayNormal) / 0.12f + 0.15f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.Alpha * (c.animations ? fade : 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Px(c.m.tooltip_pad_x), Px(c.m.tooltip_pad_y)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Px(c.m.popup_rounding));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, c.pal.tooltip);
    ImGui::PushStyleColor(ImGuiCol_Text, c.pal.text);
    if (ImGui::BeginTooltipEx(ImGuiTooltipFlags_OverridePrevious, ImGuiWindowFlags_None)) {
        ImGui::PushFont(c.fonts.regular, BodySize());
        ImGui::PushTextWrapPos(Px(c.m.tooltip_wrap));
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::PopFont();
        ImGui::EndTooltip();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
}

void AutoTooltip(const char* label)
{
    Context& c = g_ctx;
    if (!c.show_paths || label == nullptr) return;
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_Stationary)) return;
    const char* end = ImGui::FindRenderedTextEnd(label);
    char path[192];
    ImFormatString(path, IM_ARRAYSIZE(path), "%s > %s%s%s > %.*s", c.path_tab, c.path_page, c.path_page[0] ? " > " : "",
                   c.path_panel, static_cast<int>(end - label), label);
    ShowTooltip(path);
}

// Popup chrome shared by settings / bind popups: shadow + rounded body drawn into the popup's own draw list.
void PopupBackground(ImGuiWindow* window)
{
    Context& c = g_ctx;
    ImDrawList* dl = window->DrawList;
    const ImVec2 min = window->Pos;
    const ImVec2 max(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y);
    const float r = Px(c.m.popup_rounding);
    dl->PushClipRectFullScreen();
    draw::Glow(dl, min, max, r, draw::Col(Rgb(0x000000), 0.32f), Px(16.0f), ImVec2(0.0f, Px(4.0f)));
    dl->PopClipRect();
    dl->AddRectFilled(min, max, draw::Col(c.pal.popup), r);
    dl->AddRect(min, max, draw::Col(c.pal.popup_border, 0.55f), r, 0, 1.0f);
}

// Tracks open->closed transitions per popup so every opening restarts its reveal animation.
float PopupReveal(ImGuiID popup_id, bool open)
{
    Context& c = g_ctx;
    const ImGuiID was_key = ImHashStr("was_open", 0, popup_id);
    const ImGuiID t_key = ImHashStr("reveal", 0, popup_id);
    const bool was = c.state.GetBool(was_key, false);
    c.state.SetBool(was_key, open);
    if (!open) return 0.0f;
    if (!was) c.state.SetFloat(t_key, 0.0f);
    if (!c.animations) return 1.0f;
    float t = c.state.GetFloat(t_key, 0.0f);
    t = ImMin(1.0f, t + ImMin(ImGui::GetIO().DeltaTime, 1.0f / 15.0f) / 0.16f);
    c.state.SetFloat(t_key, t);
    return t;
}

// ---- Binds ---------------------------------------------------------------------------------------------------

[[nodiscard]] bool IsBindableKey(ImGuiKey key)
{
    if (key == ImGuiKey_MouseLeft || key == ImGuiKey_MouseWheelX || key == ImGuiKey_MouseWheelY) return false;
    if (key >= ImGuiKey_ReservedForModCtrl && key <= ImGuiKey_ReservedForModSuper) return false;
    return key >= ImGuiKey_NamedKey_BEGIN && key < ImGuiKey_NamedKey_END;
}

void ApplyBind(Bind& b, bool active)
{
    if (b.target == nullptr || active == b.active) return;
    if (b.kind == BindKind::Bool) {
        bool* v = static_cast<bool*>(b.target);
        if (active) {
            b.saved = *v ? 1.0f : 0.0f;
            *v = true;
        } else {
            *v = b.saved != 0.0f;
        }
    } else {
        float* v = static_cast<float*>(b.target);
        if (active) {
            b.saved = *v;
            *v = ImClamp(b.value, ImMin(b.min, b.max), ImMax(b.min, b.max));
        } else {
            *v = b.saved;
        }
    }
    b.active = active;
}

void ProcessBinds()
{
    Context& c = g_ctx;
    const ImGuiIO& io = ImGui::GetIO();
    for (auto& entry : c.binds) {
        Bind& b = entry.second;
        bool active = b.active;
        switch (b.mode) {
        case BindMode::Hold:
            active = b.key != ImGuiKey_None && ImGui::IsKeyDown(b.key);
            break;
        case BindMode::Toggle:
            if (b.key != ImGuiKey_None && !io.WantTextInput && !c.listening && ImGui::IsKeyPressed(b.key, false)) b.latched = !b.latched;
            active = b.latched;
            break;
        case BindMode::Always:
            active = true;
            break;
        }
        ApplyBind(b, active);
    }
}

bool KeyButton(const char* id_str, ImGuiKey* key)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    Context& c = g_ctx;
    const ImGuiID id = window->GetID(id_str);
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, ImVec2(ContentRight(window), pos.y + S(c.m.button_h)));
    ImGui::ItemSize(bb, 0.0f);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered = false, held = false;
    if (ImGui::ButtonBehavior(bb, id, &hovered, &held) && !c.listening) {
        c.listening = true;
        c.listen_frame = ImGui::GetFrameCount();
    }

    bool changed = false;
    if (c.listening && ImGui::GetFrameCount() > c.listen_frame) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            c.listening = false;
        } else if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false) || ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
            *key = ImGuiKey_None;
            c.listening = false;
            changed = true;
        } else {
            for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
                const ImGuiKey candidate = static_cast<ImGuiKey>(k);
                if (!IsBindableKey(candidate) || !ImGui::IsKeyPressed(candidate, false)) continue;
                *key = candidate;
                c.listening = false;
                changed = true;
                break;
            }
        }
    }

    const float listen = Animate(id, "listen", c.listening ? 1.0f : 0.0f, 16.0f, 0.0f);
    const float hot = Animate(id, "hot", hovered ? 1.0f : 0.0f, 18.0f, 0.0f);
    ImDrawList* dl = window->DrawList;
    const float r = Px(c.m.frame_rounding);
    dl->AddRectFilled(bb.Min, bb.Max, draw::Col(draw::Mix(c.pal.frame, c.pal.frame_hover, hot)), r);
    if (listen > 0.01f) {
        draw::Glow(dl, bb.Min, bb.Max, r, draw::Col(c.pal.accent, 0.3f * listen), Px(8.0f));
        draw::RectGradient(dl, bb.Min, bb.Max, draw::Col(c.pal.accent_top, listen), draw::Col(c.pal.accent_bottom, listen), r, 0, false);
    }
    const char* text = c.listening ? "Press a key..." : KeyName(*key);
    ImFont* bold = c.fonts.bold;
    const float fs = BodySize();
    const float tw = draw::TextWidth(bold, fs, text);
    draw::TextCapCentered(dl, bold, fs, bb.GetCenter().x - tw * 0.5f, bb.GetCenter().y,
                          draw::Col(draw::Mix(c.pal.text_soft, c.pal.text, ImMax(hot, listen))), text);
    ImGui::RenderNavCursor(bb, id);
    return changed;
}

void BindPopup(ImGuiID popup_id, ImGuiID owner, Formatter formatter, const char* fmt)
{
    Context& c = g_ctx;
    const bool open = ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_None);
    const float t = PopupReveal(popup_id, open);
    if (!open) return;

    const float w = S(c.m.bind_w);
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 at = c.bind_pos;
    at.x = ImClamp(at.x, vp->WorkPos.x, vp->WorkPos.x + vp->WorkSize.x - w);
    ImGui::SetNextWindowPos(draw::Snap(at), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(w, 0.0f), ImVec2(w, FLT_MAX));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * t);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(c.m.popup_pad_x), S(c.m.popup_pad_y)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(S(8.0f), S(c.m.item_spacing)));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0, 0, 0, 0));
    if (!ImGui::BeginPopupEx(popup_id, kPopupFlags | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(4);
        return;
    }
    ++c.bind_popup_depth;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    PopupBackground(window);
    if (ImGui::IsWindowAppearing()) c.listening = false;

    // Title
    {
        const ImVec2 pos = window->DC.CursorPos;
        const float h = S(c.m.row_h);
        ImGui::ItemSize(ImRect(pos, ImVec2(pos.x + 1.0f, pos.y + h)), 0.0f);
        const char* title = "Bind";
        const float fs = FontPx(c.m.font_header);
        const float tw = draw::TextWidth(c.fonts.bold, fs, title);
        draw::TextCapCentered(window->DrawList, c.fonts.bold, fs, window->Pos.x + (window->Size.x - tw) * 0.5f, pos.y + h * 0.5f,
                              draw::Col(c.pal.text), title);
    }

    Bind& d = c.draft;
    if (d.kind == BindKind::Float) Slider("New value", &d.value, d.min, d.max, fmt, formatter);
    KeyButton("##bind_key", &d.key);
    static const char* const kModes[] = { "Hold", "Toggle", "Always on" };
    int mode = static_cast<int>(d.mode);
    if (Combo("##bind_mode", &mode, kModes, IM_ARRAYSIZE(kModes))) d.mode = static_cast<BindMode>(mode);
    Checkbox("Display in binds list", &d.show_in_list);

    const auto existing = c.binds.find(owner);
    const bool exists = existing != c.binds.end();
    const float half = draw::Snap((ContentRight(window) - window->DC.CursorPos.x - S(8.0f)) * 0.5f);
    const bool confirm = Button(exists ? "Update" : "Create", half);
    ImGui::SameLine(0.0f, S(8.0f));
    const bool cancel = Button(exists ? "Remove" : "Cancel", half);

    if (confirm) {
        if (exists) ApplyBind(existing->second, false);
        Bind b = d;
        b.active = false;
        b.latched = false;
        c.binds[owner] = b;
        c.listening = false;
        ImGui::CloseCurrentPopup();
    } else if (cancel) {
        if (exists) {
            ApplyBind(existing->second, false);
            c.binds.erase(existing);
        }
        c.listening = false;
        ImGui::CloseCurrentPopup();
    }

    --c.bind_popup_depth;
    ImGui::EndPopup();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
}

// Right-click on a bindable item opens its bind editor; the popup lives at the item's popup-stack level.
void BindContext(ImGuiID id, const char* label, BindKind kind, void* target, float min, float max, bool hovered,
                 Formatter formatter, const char* fmt)
{
    Context& c = g_ctx;
    if (target == nullptr || c.bind_popup_depth > 0) return;
    const ImGuiID popup_id = ImHashStr("##bind", 0, id);
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        const auto it = c.binds.find(id);
        if (it != c.binds.end()) {
            c.draft = it->second;
        } else {
            c.draft = Bind{};
            c.draft.kind = kind;
            c.draft.target = target;
            c.draft.min = min;
            c.draft.max = max;
            c.draft.value = kind == BindKind::Float ? *static_cast<float*>(target) : 1.0f;
            const char* end = ImGui::FindRenderedTextEnd(label);
            ImFormatString(c.draft.label, IM_ARRAYSIZE(c.draft.label), "%.*s", static_cast<int>(end - label), label);
        }
        c.draft_owner = id;
        c.listening = false;
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        c.bind_pos = ImVec2(mouse.x + Px(6.0f), mouse.y + Px(6.0f));
        ImGui::OpenPopupEx(popup_id);
    }
    if (c.draft_owner == id) BindPopup(popup_id, id, formatter, fmt);
}

// ---- Dropdown --------------------------------------------------------------------------------------------------

void JoinSelected(char* out, int size, const char* const items[], int count, unsigned mask)
{
    int n = 0;
    out[0] = '\0';
    for (int i = 0; i < count && n < size - 1; ++i) {
        if ((mask & (1u << i)) == 0) continue;
        n += ImFormatString(out + n, static_cast<size_t>(size - n), n > 0 ? ", %s" : "%s", items[i]);
    }
    if (n == 0) ImFormatString(out, static_cast<size_t>(size), "None");
}

bool Dropdown(ImGuiID popup_id, const ImRect& frame, const char* const items[], int count, int* current, unsigned* mask,
              const char* preview, float text_x)
{
    Context& c = g_ctx;
    const bool open = ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_None);
    const float reveal = PopupReveal(popup_id, open);
    if (!open) return false;

    const float head_h = frame.GetHeight();
    const float item_h = S(c.m.dropdown_item_h);
    const float full_h = head_h + item_h * static_cast<float>(count) + S(c.m.dropdown_pad_bottom);
    ImVec2 pos = frame.Min;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float bottom = vp->WorkPos.y + vp->WorkSize.y;
    if (pos.y + full_h > bottom) pos.y = ImMax(vp->WorkPos.y, bottom - full_h);

    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(ImVec2(frame.GetWidth(), full_h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, Px(c.m.frame_rounding));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0, 0, 0, 0));
    const bool visible = ImGui::BeginPopupEx(popup_id, kPopupFlags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
    if (!visible) return false;

    bool changed = false;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;
    const float e = 1.0f - (1.0f - reveal) * (1.0f - reveal) * (1.0f - reveal);
    const float vis_h = draw::Snap(ImLerp(head_h, full_h, e));
    const ImRect body(window->Pos, ImVec2(window->Pos.x + window->Size.x, window->Pos.y + vis_h));
    const float r = Px(c.m.frame_rounding);

    dl->PushClipRectFullScreen();
    draw::Glow(dl, body.Min, body.Max, r, draw::Col(Rgb(0x000000), 0.30f * e), Px(14.0f), ImVec2(0.0f, Px(4.0f)));
    dl->PopClipRect();
    dl->AddRectFilled(body.Min, body.Max, draw::Col(c.pal.dropdown), r);
    dl->PushClipRect(body.Min, body.Max, true);

    ImFont* font = c.fonts.regular;
    const float fs = BodySize();
    const float x = body.Min.x + text_x;

    // Header row repeats the value and closes the list.
    {
        const ImRect head(body.Min, ImVec2(body.Max.x, body.Min.y + head_h));
        const ImGuiID head_id = window->GetID("##head");
        ImGui::ItemAdd(head, head_id);
        bool hh = false, hd = false;
        if (ImGui::ButtonBehavior(head, head_id, &hh, &hd)) ImGui::CloseCurrentPopup();
        dl->PushClipRect(head.Min, ImVec2(head.Max.x - S(24.0f), head.Max.y), true);
        draw::TextCapCentered(dl, font, fs, x, head.GetCenter().y, draw::Col(c.pal.text), preview);
        dl->PopClipRect();
        const ImVec2 icon_c(draw::Snap(head.Max.x - S(text_x > S(9.0f) ? 14.0f : 11.0f)), draw::Snap(head.GetCenter().y));
        if (mask != nullptr) draw::Minus(dl, icon_c, Px(4.5f), S(2.0f), draw::Col(c.pal.text));
        else draw::Chevron(dl, ImVec2(icon_c.x, icon_c.y + 0.5f), Px(8.0f), Px(4.0f), true, draw::Col(c.pal.text), Px(1.5f));
    }

    for (int i = 0; i < count; ++i) {
        ImGui::PushID(i);
        const float y = body.Min.y + head_h + item_h * static_cast<float>(i);
        const ImRect row(ImVec2(body.Min.x, y), ImVec2(body.Max.x, y + item_h));
        const ImGuiID id = window->GetID("##item");
        ImGui::ItemAdd(row, id);
        bool hovered = false, held = false;
        const bool pressed = ImGui::ButtonBehavior(row, id, &hovered, &held);
        if (pressed) {
            if (mask != nullptr) {
                *mask ^= (1u << i);
            } else {
                *current = i;
                ImGui::CloseCurrentPopup();
            }
            changed = true;
        }
        const bool selected = mask != nullptr ? (*mask & (1u << i)) != 0 : *current == i;
        const float slide = Animate(id, "slide", hovered ? 1.0f : 0.0f, 20.0f, 0.0f);
        const float sel = Animate(id, "sel", selected ? 1.0f : 0.0f, 16.0f, selected ? 1.0f : 0.0f);
        const ImVec4 idle = draw::Mix(c.pal.text_dim, c.pal.text, slide);
        const ImVec4 picked = draw::Mix(c.pal.accent, c.pal.accent_end, slide * 0.45f);
        draw::TextCapCentered(dl, font, fs, x + Px(6.0f) * slide, row.GetCenter().y, draw::Col(draw::Mix(idle, picked, sel)), items[i]);
        ImGui::PopID();
    }

    dl->PopClipRect();
    ImGui::EndPopup();
    return changed;
}

// ---- Color picker ----------------------------------------------------------------------------------------------

bool ParseHex(const char* text, ImVec4* out)
{
    while (*text == ' ' || *text == '#') ++text;
    unsigned int v = 0;
    const size_t len = std::strlen(text);
    if (len < 6) return false;
    for (size_t i = 0; i < (len >= 8 ? 8u : 6u); ++i) {
        const char ch = text[i];
        unsigned int nib = 0;
        if (ch >= '0' && ch <= '9') nib = static_cast<unsigned int>(ch - '0');
        else if (ch >= 'a' && ch <= 'f') nib = static_cast<unsigned int>(ch - 'a' + 10);
        else if (ch >= 'A' && ch <= 'F') nib = static_cast<unsigned int>(ch - 'A' + 10);
        else return false;
        v = (v << 4) | nib;
    }
    if (len < 8) v = (v << 8) | 0xFFu;
    *out = ImVec4(static_cast<float>((v >> 24) & 0xFF) / 255.0f, static_cast<float>((v >> 16) & 0xFF) / 255.0f,
                  static_cast<float>((v >> 8) & 0xFF) / 255.0f, static_cast<float>(v & 0xFF) / 255.0f);
    return true;
}

bool DragArea(const char* id_str, const ImRect& bb, ImVec2* t)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const ImGuiID id = window->GetID(id_str);
    ImGui::ItemAdd(bb, id);
    bool hovered = false, held = false;
    ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick);
    if (!held) return false;
    const ImVec2 m = ImGui::GetIO().MousePos;
    *t = ImVec2(ImSaturate((m.x - bb.Min.x) / bb.GetWidth()), ImSaturate((m.y - bb.Min.y) / bb.GetHeight()));
    return true;
}

void PickerPopup(ImGuiID popup_id, ColorValue* cv, bool alpha, const ImVec2& anchor)
{
    Context& c = g_ctx;
    const bool open = ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_None);
    const float reveal = PopupReveal(popup_id, open);
    if (!open) return;

    const float pad = S(10.0f);
    const float sv = S(c.m.picker_sv);
    const float bar = S(c.m.picker_bar);
    const float gap = S(c.m.picker_gap);
    const float side = S(c.m.picker_side_w);
    const float w = pad + sv + gap + bar + S(12.0f) + side + pad;
    const float h = pad + sv + gap + bar + pad;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 pos(anchor.x + S(14.0f), anchor.y - S(18.0f));
    pos.x = ImClamp(pos.x, vp->WorkPos.x, vp->WorkPos.x + vp->WorkSize.x - w);
    pos.y = ImClamp(pos.y, vp->WorkPos.y, vp->WorkPos.y + vp->WorkSize.y - h);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * reveal);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(S(8.0f), S(c.m.item_spacing)));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0, 0, 0, 0));
    if (!ImGui::BeginPopupEx(popup_id, kPopupFlags)) {
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(4);
        return;
    }

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;
    const ImVec2 o = window->Pos;
    const ImVec2 o_max(o.x + w, o.y + h);
    const float r = Px(c.m.popup_rounding);
    if (ImGui::IsWindowAppearing()) c.picker_backup = cv->color;

    ImVec4& col = cv->color;
    const ImGuiID hue_key = ImHashStr("hue", 0, popup_id);
    float hue = 0.0f, sat = 0.0f, val = 0.0f;
    ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, hue, sat, val);
    if (sat <= 0.0001f || val <= 0.0001f) hue = c.state.GetFloat(hue_key, hue);
    bool hsv_changed = false;

    dl->PushClipRectFullScreen();
    draw::Glow(dl, o, o_max, r, draw::Col(ImVec4(col.x, col.y, col.z, 1.0f), 0.30f), Px(12.0f));
    draw::Glow(dl, o, o_max, r, draw::Col(Rgb(0x000000), 0.25f), Px(16.0f), ImVec2(0.0f, Px(4.0f)));
    dl->PopClipRect();
    dl->AddRectFilled(o, o_max, draw::Col(c.pal.panel), r);

    // Saturation / value square
    const ImRect sv_rect(ImVec2(o.x + pad, o.y + pad), ImVec2(o.x + pad + sv, o.y + pad + sv));
    ImVec2 t;
    if (DragArea("##sv", sv_rect, &t)) {
        sat = t.x;
        val = 1.0f - t.y;
        hsv_changed = true;
    }
    float hr = 0.0f, hg = 0.0f, hb = 0.0f;
    ImGui::ColorConvertHSVtoRGB(hue, 1.0f, 1.0f, hr, hg, hb);
    const ImU32 hue_col = draw::Col(ImVec4(hr, hg, hb, 1.0f));
    const ImU32 white = draw::Col(ImVec4(1, 1, 1, 1));
    const ImU32 black = draw::Col(ImVec4(0, 0, 0, 1));
    const ImU32 clear = draw::Col(ImVec4(0, 0, 0, 0));
    dl->AddRectFilledMultiColor(sv_rect.Min, sv_rect.Max, white, hue_col, hue_col, white);
    dl->AddRectFilledMultiColor(sv_rect.Min, sv_rect.Max, clear, clear, black, black);

    // Hue strip
    const ImRect hue_rect(ImVec2(sv_rect.Max.x + gap, sv_rect.Min.y), ImVec2(sv_rect.Max.x + gap + bar, sv_rect.Max.y));
    if (DragArea("##hue", hue_rect, &t)) {
        hue = ImMin(t.y, 0.9999f);
        hsv_changed = true;
    }
    draw::HueStrip(dl, hue_rect.Min, hue_rect.Max, true, ImGui::GetStyle().Alpha);

    // Alpha strip + swatch
    const ImRect alpha_rect(ImVec2(sv_rect.Min.x, sv_rect.Max.y + gap), ImVec2(sv_rect.Max.x, sv_rect.Max.y + gap + bar));
    const ImRect swatch(ImVec2(hue_rect.Min.x, alpha_rect.Min.y), ImVec2(hue_rect.Max.x, alpha_rect.Max.y));
    if (alpha && DragArea("##alpha", alpha_rect, &t)) col.w = t.x;
    const float cell = draw::Snap(bar * 0.5f);
    if (alpha) {
        draw::Checkerboard(dl, alpha_rect.Min, alpha_rect.Max, cell, draw::Col(Rgb(0xC8C8C8)), draw::Col(Rgb(0x8C8C8C)));
        dl->AddRectFilledMultiColor(alpha_rect.Min, alpha_rect.Max, draw::Col(ImVec4(col.x, col.y, col.z, 0.0f)),
                                    draw::Col(ImVec4(col.x, col.y, col.z, 1.0f)), draw::Col(ImVec4(col.x, col.y, col.z, 1.0f)),
                                    draw::Col(ImVec4(col.x, col.y, col.z, 0.0f)));
    } else {
        dl->AddRectFilled(alpha_rect.Min, alpha_rect.Max, draw::Col(ImVec4(col.x, col.y, col.z, 1.0f))); // opaque-only colors
    }
    draw::Checkerboard(dl, swatch.Min, swatch.Max, cell, draw::Col(Rgb(0xC8C8C8)), draw::Col(Rgb(0x8C8C8C)));
    dl->AddRectFilled(swatch.Min, swatch.Max, draw::Col(col));

    if (hsv_changed) {
        ImGui::ColorConvertHSVtoRGB(hue, sat, val, col.x, col.y, col.z);
        c.state.SetFloat(hue_key, hue);
    }

    // Handles
    const ImVec2 sv_handle(sv_rect.Min.x + sat * sv, sv_rect.Min.y + (1.0f - val) * sv);
    dl->AddCircleFilled(sv_handle, Px(4.5f), draw::Col(ImVec4(col.x, col.y, col.z, 1.0f)));
    dl->AddCircle(sv_handle, Px(4.5f), white, 0, Px(1.5f));
    const float hy = draw::Snap(hue_rect.Min.y + hue * sv);
    dl->AddRectFilled(ImVec2(hue_rect.Min.x - 1.0f, hy - S(2.0f)), ImVec2(hue_rect.Max.x + 1.0f, hy + S(2.0f)), black, 1.0f);
    dl->AddRectFilled(ImVec2(hue_rect.Min.x, hy - 1.0f), ImVec2(hue_rect.Max.x, hy + 1.0f), white);
    if (alpha) {
        const float ax = draw::Snap(alpha_rect.Min.x + col.w * sv);
        dl->AddRectFilled(ImVec2(ax - S(2.0f), alpha_rect.Min.y - 1.0f), ImVec2(ax + S(2.0f), alpha_rect.Max.y + 1.0f), black, 1.0f);
        dl->AddRectFilled(ImVec2(ax - 1.0f, alpha_rect.Min.y), ImVec2(ax + 1.0f, alpha_rect.Max.y), white);
    }

    // Side column
    const float side_x = hue_rect.Max.x + S(12.0f);
    const float saved_right = c.right_override;
    c.right_override = side_x + side;
    ImGui::SetCursorScreenPos(ImVec2(side_x, sv_rect.Min.y));
    ImGui::BeginGroup(); // new lines restart at side_x, not at the popup's left edge
    Slider("Rainbow", &cv->rainbow, 0.0f, 100.0f, "%.0f");
    Slider("Pulse", &cv->pulse, 0.0f, 100.0f, "%.0f");
    if (Button("Copy", side)) {
        char hex[16];
        ImFormatString(hex, IM_ARRAYSIZE(hex), "#%02X%02X%02X%02X", static_cast<int>(col.x * 255.0f + 0.5f),
                       static_cast<int>(col.y * 255.0f + 0.5f), static_cast<int>(col.z * 255.0f + 0.5f),
                       static_cast<int>(col.w * 255.0f + 0.5f));
        ImGui::SetClipboardText(hex);
    }
    if (Button("Paste", side)) {
        ImVec4 parsed;
        const char* clip = ImGui::GetClipboardText();
        if (clip != nullptr && ParseHex(clip, &parsed)) col = alpha ? parsed : ImVec4(parsed.x, parsed.y, parsed.z, col.w);
    }
    if (Button("Undo", side)) col = c.picker_backup;
    ImGui::EndGroup();
    c.right_override = saved_right;

    ImGui::EndPopup();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
}

ImVec2 AccessoryCenter(ImGuiID id, float width)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    AccessoryLine& line = g_ctx.accessory;
    float right = 0.0f, cy = 0.0f;
    if (line.frame == g.FrameCount && line.last_accessory == g.LastItemData.ID) {
        right = line.right;
        cy = line.center_y;
    } else {
        right = ContentRight(window) - Px(g_ctx.m.accessory_inset);
        cy = g.LastItemData.Rect.GetCenter().y;
    }
    const ImVec2 center(draw::Snap(right - width * 0.5f), draw::Snap(cy));
    line.last_accessory = id;
    line.frame = g.FrameCount;
    line.center_y = cy;
    line.right = right - width - Px(g_ctx.m.accessory_gap);
    return center;
}

bool SliderImpl(const char* label, float* v, float min, float max, const char* fmt, Formatter formatter, bool integer,
                bool bindable)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    Context& c = g_ctx;
    const ImGuiID id = window->GetID(label);
    const ImVec2 pos = window->DC.CursorPos;
    const float w = ContentRight(window) - pos.x;
    const ImRect bb(pos, ImVec2(pos.x + w, pos.y + S(c.m.slider_h)));
    ImGui::ItemSize(bb, 0.0f);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered = false, held = false;
    ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick);
    const ImRect track(ImVec2(pos.x, pos.y + S(c.m.slider_track_y)), ImVec2(pos.x + w, pos.y + S(c.m.slider_track_y) + S(c.m.slider_track_h)));

    bool changed = false;
    if (held && max != min) {
        const float t = ImSaturate((ImGui::GetIO().MousePos.x - track.Min.x) / ImMax(track.GetWidth(), 1.0f));
        float nv = ImLerp(min, max, t);
        if (integer) nv = std::round(nv);
        if (nv != *v) {
            *v = nv;
            changed = true;
            ImGui::MarkItemEdited(id);
        }
    }
    if (bindable) BindContext(id, label, BindKind::Float, v, min, max, hovered, formatter, fmt);

    const float frac = max != min ? ImSaturate((*v - min) / (max - min)) : 0.0f;
    const float shown = Animate(id, "fill", frac, held ? 40.0f : 16.0f, frac);
    const float hot = Animate(id, "hot", (hovered || held) ? 1.0f : 0.0f, 18.0f, 0.0f);
    const float act = Animate(id, "act", frac > 0.0001f ? 1.0f : 0.0f, 14.0f, frac > 0.0001f ? 1.0f : 0.0f);

    {
        SearchScope scope(label);
        ImDrawList* dl = window->DrawList;
        ImFont* font = c.fonts.regular;
        const float fs = BodySize();
        const ImVec4 tc = draw::Mix(draw::Mix(c.pal.text_dim, c.pal.text_hover, hot), c.pal.text, ImMax(act, hot * 0.6f));
        const char* label_end = ImGui::FindRenderedTextEnd(label);

        char value[64];
        int n = 0;
        if (formatter != nullptr) n = formatter(value, IM_ARRAYSIZE(value), *v);
        else if (integer) n = ImFormatString(value, IM_ARRAYSIZE(value), fmt, static_cast<int>(std::lround(*v)));
        else n = ImFormatString(value, IM_ARRAYSIZE(value), fmt, static_cast<double>(*v));
        const float vw = draw::TextWidth(font, fs, value, value + n);

        const float cap_top = pos.y;
        dl->PushClipRect(bb.Min, ImVec2(bb.Max.x - vw - S(8.0f), bb.Max.y), true);
        draw::TextCapTop(dl, font, fs, pos.x, cap_top, draw::Col(tc), label, label_end);
        dl->PopClipRect();
        draw::TextCapTop(dl, font, fs, bb.Max.x - vw, cap_top, draw::Col(tc), value, value + n);

        const float tr = track.GetHeight() * 0.5f;
        dl->AddRectFilled(track.Min, track.Max, draw::Col(c.pal.frame), tr);
        const float fill_x = track.Min.x + track.GetWidth() * shown;
        if (fill_x > track.Min.x + 0.5f) {
            draw::Glow(dl, track.Min, ImVec2(fill_x, track.Max.y), tr, draw::Col(c.pal.accent, 0.22f + 0.1f * hot), Px(6.0f));
            dl->PushClipRect(track.Min, ImVec2(fill_x, track.Max.y), true);
            draw::RectGradient(dl, track.Min, track.Max, draw::Col(c.pal.accent), draw::Col(c.pal.accent_end), tr, 0, true);
            dl->PopClipRect();
        }
    }
    ImGui::RenderNavCursor(bb, id);
    AutoTooltip(label);
    return changed;
}

bool ComboImpl(const char* label, int* current, unsigned* mask, const char* const items[], int count)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems || count <= 0) return false;
    Context& c = g_ctx;
    const ImGuiID id = window->GetID(label);
    const char* label_end = ImGui::FindRenderedTextEnd(label);
    const bool has_label = label_end != label;

    if (current != nullptr) *current = ImClamp(*current, 0, count - 1);

    const ImVec2 pos = window->DC.CursorPos;
    const float w = ContentRight(window) - pos.x;
    const float top = has_label ? S(1.0f + c.m.caption_to_frame) : 0.0f;
    const ImRect total(pos, ImVec2(pos.x + w, pos.y + top + S(c.m.frame_h)));
    const ImRect frame(ImVec2(pos.x, pos.y + top), total.Max);
    ImGui::ItemSize(ImRect(total.Min, ImVec2(total.Max.x, total.Max.y - 1.0f)), 0.0f); // frames overhang their row by 1px
    if (!ImGui::ItemAdd(total, id)) return false;

    const ImGuiID popup_id = ImHashStr("##dropdown", 0, id);
    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior(frame, id, &hovered, &held);
    const bool open = ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_None);
    if (pressed && !open) ImGui::OpenPopupEx(popup_id);

    char preview[256];
    if (mask != nullptr) JoinSelected(preview, IM_ARRAYSIZE(preview), items, count, *mask);
    else ImFormatString(preview, IM_ARRAYSIZE(preview), "%s", items[*current]);

    const float open_t = Animate(id, "open", open ? 1.0f : 0.0f, 20.0f, 0.0f);
    const float hot = Animate(id, "hot", hovered ? 1.0f : 0.0f, 18.0f, 0.0f);
    {
        SearchScope scope(has_label ? label : preview);
        ImDrawList* dl = window->DrawList;
        ImFont* font = c.fonts.regular;
        const float fs = BodySize();
        if (has_label)
            draw::TextCapTop(dl, font, fs, pos.x, pos.y,
                             draw::Col(draw::Mix(c.pal.text_dim, c.pal.text, ImMax(open_t, hot * 0.45f))), label, label_end);
        const float r = Px(c.m.frame_rounding);
        dl->AddRectFilled(frame.Min, frame.Max, draw::Col(draw::Mix(c.pal.frame, c.pal.frame_hover, hot)), r);
        const float tx = S(c.m.frame_text_x);
        dl->PushClipRect(frame.Min, ImVec2(frame.Max.x - S(21.0f), frame.Max.y), true);
        draw::TextCapCentered(dl, font, fs, frame.Min.x + tx, frame.GetCenter().y,
                              draw::Col(draw::Mix(c.pal.text_soft, c.pal.text, hot * 0.5f)), preview);
        dl->PopClipRect();
        const ImU32 icon = draw::Col(draw::Mix(c.pal.text_soft, c.pal.text, hot * 0.5f));
        const ImVec2 icon_c(draw::Snap(frame.Max.x - S(11.0f)), draw::Snap(frame.GetCenter().y));
        if (mask != nullptr) draw::Plus(dl, ImVec2(icon_c.x - 0.5f, icon_c.y), Px(5.0f), Px(1.5f), icon);
        else draw::Chevron(dl, ImVec2(icon_c.x, icon_c.y + 0.5f), Px(8.0f), Px(4.0f), false, icon, Px(1.5f));
    }
    ImGui::RenderNavCursor(frame, id);
    AutoTooltip(has_label ? label : nullptr);

    return Dropdown(popup_id, frame, items, count, current, mask, preview, S(c.m.frame_text_x));
}

} // namespace

// ---- Context ---------------------------------------------------------------------------------------------------

Context& Ctx() { return g_ctx; }

float Ease(float t)
{
    t = ImSaturate(t);
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}

float Animate(ImGuiID id, const char* salt, float target, float rate, float initial)
{
    Context& c = g_ctx;
    const ImGuiID key = ImHashStr(salt, 0, id);
    if (!c.animations) {
        c.state.SetFloat(key, target);
        return target;
    }
    float v = c.state.GetFloat(key, initial);
    const float dt = ImMin(ImGui::GetIO().DeltaTime, 1.0f / 15.0f);
    v = target + (v - target) * std::exp(-rate * dt);
    if (std::fabs(v - target) < 0.0015f) v = target; // settle exactly so an idle menu stops changing
    c.state.SetFloat(key, v);
    return v;
}

void SetAccent(AccentPreset preset, const ImVec4& custom_start, const ImVec4& custom_end)
{
    g_ctx.accent = preset;
    g_ctx.custom_start = custom_start;
    g_ctx.custom_end = custom_end;
    g_ctx.pal = preset == AccentPreset::Custom ? MakeCustomPalette(custom_start, custom_end) : MakePalette(preset);
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_TextSelectedBg] = draw::Fade(g_ctx.pal.accent, 0.35f);
    style.Colors[ImGuiCol_NavCursor] = g_ctx.pal.accent;
    style.Colors[ImGuiCol_CheckMark] = g_ctx.pal.accent;
    style.Colors[ImGuiCol_SliderGrab] = g_ctx.pal.accent;
    style.Colors[ImGuiCol_InputTextCursor] = g_ctx.pal.accent;
}

void SetScale(float scale)
{
    Context& c = g_ctx;
    c.scale = ImClamp(scale, 0.5f, 4.0f);
    c.icons.Build(c.scale);

    ImGuiStyle s;
    ImGui::StyleColorsDark(&s);
    s.WindowPadding = ImVec2(0.0f, 0.0f);
    s.FramePadding = ImVec2(c.m.frame_text_x, 4.5f);
    s.ItemSpacing = ImVec2(8.0f, c.m.item_spacing);
    s.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
    s.WindowRounding = c.m.window_rounding;
    s.ChildRounding = 0.0f;
    s.FrameRounding = c.m.frame_rounding;
    s.PopupRounding = c.m.popup_rounding;
    s.ScrollbarSize = 6.0f;
    s.ScrollbarRounding = 3.0f;
    s.GrabRounding = 3.0f;
    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize = 0.0f;
    s.PopupBorderSize = 0.0f;
    s.FrameBorderSize = 0.0f;
    s.WindowMinSize = ImVec2(32.0f, 32.0f);
    s.HoverDelayNormal = 0.45f;
    s.HoverStationaryDelay = 0.12f;
    s.ScaleAllSizes(c.scale);
    s.FontSizeBase = FontPx(c.m.font_body);
    s.FontScaleDpi = 1.0f;

    ImVec4* col = s.Colors;
    col[ImGuiCol_Text] = c.pal.text;
    col[ImGuiCol_TextDisabled] = c.pal.text_dim;
    col[ImGuiCol_WindowBg] = c.pal.window_bg;
    col[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    col[ImGuiCol_PopupBg] = c.pal.popup;
    col[ImGuiCol_Border] = c.pal.popup_border;
    col[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    col[ImGuiCol_FrameBg] = c.pal.frame;
    col[ImGuiCol_FrameBgHovered] = c.pal.frame_hover;
    col[ImGuiCol_FrameBgActive] = c.pal.frame_active;
    col[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    col[ImGuiCol_ScrollbarGrab] = c.pal.frame_hover;
    col[ImGuiCol_ScrollbarGrabHovered] = c.pal.grip;
    col[ImGuiCol_ScrollbarGrabActive] = c.pal.text_dim;
    col[ImGuiCol_Button] = c.pal.frame;
    col[ImGuiCol_ButtonHovered] = c.pal.frame_hover;
    col[ImGuiCol_ButtonActive] = c.pal.frame_active;
    col[ImGuiCol_Header] = c.pal.frame;
    col[ImGuiCol_HeaderHovered] = c.pal.frame_hover;
    col[ImGuiCol_HeaderActive] = c.pal.frame_active;
    col[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
    col[ImGuiCol_ResizeGripHovered] = ImVec4(0, 0, 0, 0);
    col[ImGuiCol_ResizeGripActive] = ImVec4(0, 0, 0, 0);
    col[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.45f);
    ImGui::GetStyle() = s;
    SetAccent(c.accent, c.custom_start, c.custom_end);
}

void BeginFrame()
{
    Context& c = g_ctx;
    const ImGuiIO& io = ImGui::GetIO();
    c.dt = io.DeltaTime;
    c.time += static_cast<double>(io.DeltaTime);
    ProcessBinds();
}

void Shutdown()
{
    g_ctx.icons.Release();
    g_ctx.state.Clear();
    g_ctx.binds.clear();
    g_ctx.panels.clear();
}

const char* KeyName(ImGuiKey key)
{
    switch (key) {
    case ImGuiKey_None: return "None";
    case ImGuiKey_MouseRight: return "Mouse 2";
    case ImGuiKey_MouseMiddle: return "Mouse 3";
    case ImGuiKey_MouseX1: return "Mouse 4";
    case ImGuiKey_MouseX2: return "Mouse 5";
    case ImGuiKey_LeftAlt: return "Left Alt";
    case ImGuiKey_RightAlt: return "Right Alt";
    case ImGuiKey_LeftShift: return "Left Shift";
    case ImGuiKey_RightShift: return "Right Shift";
    case ImGuiKey_LeftCtrl: return "Left Ctrl";
    case ImGuiKey_RightCtrl: return "Right Ctrl";
    default: return ImGui::GetKeyName(key);
    }
}

// ---- Layout ----------------------------------------------------------------------------------------------------

void BeginColumns(const char* id, const ImRect& area, int count)
{
    Context& c = g_ctx;
    ColumnLayout& L = c.layout;
    L.area = area;
    L.count = ImMax(1, count);
    L.index = 0;
    L.width = draw::Snap((area.GetWidth() - S(c.m.gap) * static_cast<float>(L.count - 1)) / static_cast<float>(L.count));
    L.cursor = area.Min.y;
    L.reserve = 0.0f;
    L.after_fill = false;
    L.id = ImGui::GetID(id);
}

static void StoreColumnReserve()
{
    Context& c = g_ctx;
    ColumnLayout& L = c.layout;
    c.state.SetFloat(ImHashStr("reserve", 0, L.id + static_cast<ImGuiID>(L.index)), L.after_fill ? L.reserve : 0.0f);
}

void NextColumn()
{
    ColumnLayout& L = g_ctx.layout;
    StoreColumnReserve();
    L.index = ImMin(L.index + 1, L.count - 1);
    L.cursor = L.area.Min.y;
    L.reserve = 0.0f;
    L.after_fill = false;
}

void EndColumns() { StoreColumnReserve(); }

bool BeginPanel(const char* title, PanelHeight height)
{
    Context& c = g_ctx;
    ColumnLayout& L = c.layout;
    ImGuiWindow* parent = ImGui::GetCurrentWindow();

    PanelFrame p;
    p.id = parent->GetID(title != nullptr ? title : "##pane");
    p.x = L.area.Min.x + static_cast<float>(L.index) * (L.width + S(c.m.gap));
    p.w = L.index == L.count - 1 ? L.area.Max.x - p.x : L.width;
    p.y = L.cursor;
    p.header = title != nullptr ? S(c.m.panel_header_h) : 0.0f;
    p.fill = height == PanelHeight::Fill;
    p.title = title;
    c.panels.push_back(p);
    c.path_panel = title != nullptr ? title : "";

    ImDrawList* dl = parent->DrawList;
    const float r = Px(c.m.panel_rounding);
    if (title != nullptr) {
        const ImVec2 h0(p.x, p.y), h1(p.x + p.w, p.y + p.header);
        dl->AddRectFilled(h0, h1, draw::Col(c.pal.card), r, ImDrawFlags_RoundCornersTop);
        const float fs = FontPx(c.m.font_header);
        const float tw = draw::TextWidth(c.fonts.bold, fs, title, ImGui::FindRenderedTextEnd(title));
        draw::TextCapCentered(dl, c.fonts.bold, fs, p.x + (p.w - tw) * 0.5f, p.y + p.header * 0.5f, draw::Col(c.pal.text), title,
                              ImGui::FindRenderedTextEnd(title));
    }

    float body_h = 0.0f;
    ImGuiChildFlags child_flags = ImGuiChildFlags_None;
    if (p.fill) {
        const float reserve = c.state.GetFloat(ImHashStr("reserve", 0, L.id + static_cast<ImGuiID>(L.index)), 0.0f);
        body_h = ImMax(S(40.0f), L.area.Max.y - p.y - p.header - reserve);
    } else {
        child_flags |= ImGuiChildFlags_AutoResizeY;
    }

    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + p.header));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(c.m.panel_pad_x), S(c.m.panel_pad_y)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(S(8.0f), S(c.m.item_spacing)));
    ImGui::BeginChild(p.id, ImVec2(p.w, body_h), child_flags | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar(1); // WindowPadding is consumed by Begin; ItemSpacing stays until EndPanel

    if (p.fill) {
        ImGuiWindow* child = ImGui::GetCurrentWindow();
        const ImGuiID target_key = ImHashStr("scroll_target", 0, child->ID);
        float target = c.state.GetFloat(target_key, child->Scroll.y);
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
            !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup))
            target -= wheel * Px(c.m.scroll_step);
        target = ImClamp(target, 0.0f, child->ScrollMax.y);
        c.state.SetFloat(target_key, target);
        const float smooth = Animate(child->ID, "scroll", target, 16.0f, child->Scroll.y);
        if (std::fabs(smooth - child->Scroll.y) > 0.01f) ImGui::SetScrollY(smooth);
    }
    return true;
}

void EndPanel()
{
    Context& c = g_ctx;
    ColumnLayout& L = c.layout;
    IM_ASSERT(!c.panels.empty() && "EndPanel() without BeginPanel()");
    const PanelFrame p = c.panels.back();
    c.panels.pop_back();

    ImGuiWindow* child = ImGui::GetCurrentWindow();
    child->DC.CursorMaxPos.y += S(c.m.panel_extra_bottom);

    if (child->ScrollMax.y > 0.5f) {
        const float visible = child->Size.y;
        const float content = visible + child->ScrollMax.y;
        const float inset = S(6.0f);
        const float track = visible - inset * 2.0f;
        const float thumb = ImMax(S(24.0f), track * visible / content);
        const float y = child->Pos.y + inset + (track - thumb) * ImSaturate(child->Scroll.y / child->ScrollMax.y);
        const float x1 = child->Pos.x + child->Size.x - S(c.m.scroll_right);
        const float x0 = x1 - S(c.m.scroll_w);
        ImDrawList* dl = child->DrawList;
        dl->PushClipRectFullScreen();
        draw::Glow(dl, ImVec2(x0, y), ImVec2(x1, y + thumb), (x1 - x0) * 0.5f, draw::Col(c.pal.accent, 0.22f), Px(5.0f));
        draw::RectGradient(dl, ImVec2(x0, y), ImVec2(x1, y + thumb), draw::Col(c.pal.accent), draw::Col(c.pal.accent_end, 0.95f),
                           (x1 - x0) * 0.5f, 0, false);
        dl->PopClipRect();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(1);

    const ImRect body(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    ImGuiWindow* parent = ImGui::GetCurrentWindow();
    const float r = Px(c.m.panel_rounding);
    parent->DrawList->AddRectFilled(body.Min, body.Max, draw::Col(c.pal.panel), r,
                                    p.header > 0.0f ? ImDrawFlags_RoundCornersBottom : ImDrawFlags_RoundCornersAll);

    const float bottom = body.Max.y;
    if (L.after_fill) L.reserve += (bottom - p.y) + S(c.m.gap);
    if (p.fill) {
        L.after_fill = true;
        L.reserve = 0.0f;
    }
    L.cursor = bottom + S(c.m.gap);
    c.path_panel = "";
}

ImRect PanelInnerRect()
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    return window->InnerRect;
}

// ---- Items -----------------------------------------------------------------------------------------------------

bool Checkbox(const char* label, bool* v)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    Context& c = g_ctx;
    const ImGuiID id = window->GetID(label);
    ImFont* font = c.fonts.regular;
    const float fs = BodySize();
    const char* label_end = ImGui::FindRenderedTextEnd(label);
    const float box = S(c.m.checkbox);
    const float row_h = S(c.m.row_h);
    const float gap = S(c.m.label_gap);
    const float text_w = label_end != label ? draw::TextWidth(font, fs, label, label_end) : 0.0f;

    const ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, ImVec2(pos.x + box + (text_w > 0.0f ? gap + text_w : 0.0f), pos.y + row_h));
    ImGui::ItemSize(bb, 0.0f);
    ImGui::SetNextItemAllowOverlap();
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (pressed) {
        *v = !*v;
        ImGui::MarkItemEdited(id);
    }
    BindContext(id, label, BindKind::Bool, v, 0.0f, 1.0f, hovered, nullptr, nullptr);

    const float on = Animate(id, "on", *v ? 1.0f : 0.0f, 15.0f, *v ? 1.0f : 0.0f);
    const float hot = Animate(id, "hot", (hovered || held) ? 1.0f : 0.0f, 18.0f, 0.0f);
    {
        SearchScope scope(label);
        ImDrawList* dl = window->DrawList;
        const ImVec2 b0(pos.x, pos.y + draw::Snap((row_h - box) * 0.5f));
        const ImVec2 b1(b0.x + box, b0.y + box);
        const float r = Px(c.m.checkbox_rounding);
        if (on > 0.01f) draw::Glow(dl, b0, b1, r, draw::Col(c.pal.accent, c.m.glow_alpha * on), Px(c.m.glow));
        dl->AddRectFilled(b0, b1, draw::Col(draw::Mix(c.pal.frame, c.pal.frame_hover, hot)), r);
        if (on > 0.01f) draw::RectGradient(dl, b0, b1, draw::Col(c.pal.accent_top, on), draw::Col(c.pal.accent_bottom, on), r, 0, false);
        draw::Check(dl, b0, box, draw::Col(c.pal.text, ImSaturate(on * 1.4f)), Px(2.0f), on);
        if (text_w > 0.0f) {
            const ImVec4 tc = draw::Mix(draw::Mix(c.pal.text_dim, c.pal.text_hover, hot), c.pal.text, on);
            draw::TextCapCentered(dl, font, fs, b1.x + gap, (b0.y + b1.y) * 0.5f, draw::Col(tc), label, label_end);
        }
    }
    ImGui::RenderNavCursor(bb, id);
    AutoTooltip(label);
    return pressed;
}

bool Slider(const char* label, float* v, float min, float max, const char* fmt, Formatter formatter)
{
    return SliderImpl(label, v, min, max, fmt, formatter, false, true);
}

bool SliderInt(const char* label, int* v, int min, int max, const char* fmt)
{
    float f = static_cast<float>(*v);
    const bool changed = SliderImpl(label, &f, static_cast<float>(min), static_cast<float>(max), fmt, nullptr, true, false);
    *v = static_cast<int>(std::lround(f));
    return changed;
}

bool Combo(const char* label, int* current, const char* const items[], int count)
{
    return ComboImpl(label, current, nullptr, items, count);
}

bool MultiCombo(const char* label, unsigned* mask, const char* const items[], int count)
{
    return ComboImpl(label, nullptr, mask, items, count);
}

void Caption(const char* text)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;
    Context& c = g_ctx;
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, ImVec2(ContentRight(window), pos.y + S(c.m.row_h)));
    ImGui::ItemSize(bb, 0.0f);
    if (!ImGui::ItemAdd(bb, 0)) return;
    SearchScope scope(text);
    draw::TextCapCentered(window->DrawList, c.fonts.regular, BodySize(), pos.x, bb.GetCenter().y, draw::Col(c.pal.text_dim), text,
                          ImGui::FindRenderedTextEnd(text));
}

void ColorRow(const char* label, ColorValue* color, bool alpha)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;
    Context& c = g_ctx;
    const ImGuiID id = window->GetID(label);
    const ImVec2 pos = window->DC.CursorPos;
    const char* label_end = ImGui::FindRenderedTextEnd(label);
    const float tw = draw::TextWidth(c.fonts.regular, BodySize(), label, label_end);
    const ImRect bb(pos, ImVec2(pos.x + tw, pos.y + S(c.m.row_h)));
    ImGui::ItemSize(bb, 0.0f);
    if (!ImGui::ItemAdd(bb, id)) return;
    const bool hovered = ImGui::IsItemHovered();
    const float hot = Animate(id, "hot", hovered ? 1.0f : 0.0f, 18.0f, 0.0f);
    {
        SearchScope scope(label);
        draw::TextCapCentered(window->DrawList, c.fonts.regular, BodySize(), pos.x, bb.GetCenter().y,
                              draw::Col(draw::Mix(c.pal.text_soft, c.pal.text, hot)), label, label_end);
    }
    AutoTooltip(label);
    char dot_id[64];
    ImFormatString(dot_id, IM_ARRAYSIZE(dot_id), "%.*s##dot", static_cast<int>(label_end - label), label);
    ColorDot(dot_id, color, alpha);
}

bool Button(const char* label, float width, bool accent)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    Context& c = g_ctx;
    const ImGuiID id = window->GetID(label);
    const ImVec2 pos = window->DC.CursorPos;
    const float w = width > 0.0f ? width : ContentRight(window) - pos.x;
    const ImRect bb(pos, ImVec2(pos.x + w, pos.y + S(c.m.button_h)));
    ImGui::ItemSize(bb, 0.0f);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    const float hot = Animate(id, "hot", hovered ? 1.0f : 0.0f, 18.0f, 0.0f);
    const float down = Animate(id, "down", held ? 1.0f : 0.0f, 26.0f, 0.0f);

    SearchScope scope(label);
    ImDrawList* dl = window->DrawList;
    const float r = Px(c.m.frame_rounding);
    if (accent) {
        draw::Glow(dl, bb.Min, bb.Max, r, draw::Col(c.pal.accent, 0.18f + 0.14f * hot), Px(8.0f));
        draw::RectGradient(dl, bb.Min, bb.Max, draw::Col(draw::Mix(c.pal.accent_top, c.pal.accent_end, hot * 0.25f)),
                           draw::Col(c.pal.accent_bottom), r, 0, false);
    } else {
        dl->AddRectFilled(bb.Min, bb.Max, draw::Col(draw::Mix(draw::Mix(c.pal.frame, c.pal.frame_hover, hot), c.pal.frame_active, down)), r);
    }
    const char* end = ImGui::FindRenderedTextEnd(label);
    const float fs = BodySize();
    const float tw = draw::TextWidth(c.fonts.regular, fs, label, end);
    const ImVec4 tc = accent ? c.pal.text : draw::Mix(c.pal.text_soft, c.pal.text, hot);
    draw::TextCapCentered(dl, c.fonts.regular, fs, bb.GetCenter().x - tw * 0.5f, bb.GetCenter().y + down, draw::Col(tc), label, end);
    ImGui::RenderNavCursor(bb, id);
    return pressed;
}

bool InputText(const char* id, const char* hint, char* buf, size_t size)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    Context& c = g_ctx;
    const float fs = BodySize();
    const float h = S(c.m.button_h);
    ImGui::PushFont(c.fonts.regular, fs);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(S(c.m.frame_text_x), ImMax(0.0f, (h - fs) * 0.5f)));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, Px(c.m.frame_rounding));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, c.pal.text_dim);
    ImGui::SetNextItemWidth(ContentRight(window) - window->DC.CursorPos.x);
    const bool changed = ImGui::InputTextWithHint(id, hint, buf, size);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    ImGui::PopFont();
    return changed;
}

bool ListRow(const char* label, const char* detail, bool selected)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    Context& c = g_ctx;
    const ImGuiID id = window->GetID(label);
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, ImVec2(ContentRight(window), pos.y + S(c.m.list_row_h)));
    ImGui::ItemSize(bb, 0.0f);
    if (!ImGui::ItemAdd(bb, id)) return false;
    bool hovered = false, held = false;
    const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    const float hot = Animate(id, "hot", hovered ? 1.0f : 0.0f, 18.0f, 0.0f);
    const float sel = Animate(id, "sel", selected ? 1.0f : 0.0f, 16.0f, selected ? 1.0f : 0.0f);

    SearchScope scope(label);
    ImDrawList* dl = window->DrawList;
    const float r = Px(c.m.frame_rounding);
    dl->AddRectFilled(bb.Min, bb.Max, draw::Col(draw::Mix(c.pal.frame, c.pal.frame_hover, ImMax(hot, sel * 0.6f)), 0.55f + 0.45f * ImMax(hot, sel)), r);
    if (sel > 0.01f) {
        const ImVec2 a(bb.Min.x, bb.Min.y + S(7.0f)), b(bb.Min.x + S(3.0f), bb.Max.y - S(7.0f));
        draw::Glow(dl, a, b, S(1.5f), draw::Col(c.pal.accent, 0.35f * sel), Px(6.0f));
        draw::RectGradient(dl, a, b, draw::Col(c.pal.accent, sel), draw::Col(c.pal.accent_end, sel), S(1.5f), 0, false);
    }
    const float fs = BodySize();
    const float x = bb.Min.x + S(12.0f) + S(4.0f) * sel;
    draw::TextCapCentered(dl, c.fonts.regular, fs, x, bb.GetCenter().y,
                          draw::Col(draw::Mix(draw::Mix(c.pal.text_soft, c.pal.text, hot), c.pal.text, sel)), label,
                          ImGui::FindRenderedTextEnd(label));
    if (detail != nullptr && detail[0] != '\0') {
        const float dw = draw::TextWidth(c.fonts.regular, fs, detail);
        draw::TextCapCentered(dl, c.fonts.regular, fs, bb.Max.x - S(12.0f) - dw, bb.GetCenter().y, draw::Col(c.pal.text_dim), detail);
    }
    ImGui::RenderNavCursor(bb, id);
    return pressed;
}

void Separator()
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;
    Context& c = g_ctx;
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, ImVec2(ContentRight(window), pos.y + 1.0f));
    ImGui::ItemSize(bb, 0.0f);
    if (!ImGui::ItemAdd(bb, 0)) return;
    window->DrawList->AddRectFilled(bb.Min, bb.Max, draw::Col(c.pal.card));
}

void Tooltip(const char* text)
{
    if (text != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_Stationary)) ShowTooltip(text);
}

bool ColorDot(const char* id_str, ColorValue* color, bool alpha)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    Context& c = g_ctx;
    const ImGuiID id = window->GetID(id_str);
    const float d = S(c.m.dot);
    const ImVec2 center = AccessoryCenter(id, d);
    const float hit = d * 0.5f + S(3.0f);
    const ImRect bb(ImVec2(center.x - hit, center.y - hit), ImVec2(center.x + hit, center.y + hit));
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered = false, held = false;
    const ImGuiID popup_id = ImHashStr("##picker", 0, id);
    if (ImGui::ButtonBehavior(bb, id, &hovered, &held) && !ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_None)) ImGui::OpenPopupEx(popup_id);
    const bool open = ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_None);
    const float hot = Animate(id, "hot", (hovered || open) ? 1.0f : 0.0f, 18.0f, 0.0f);

    {
        SearchScope scope(nullptr, true);
        ImDrawList* dl = window->DrawList;
        const ImVec4 col = color->Resolve(c.time);
        const float r = d * 0.5f + Px(1.0f) * hot;
        draw::Glow(dl, ImVec2(center.x - r, center.y - r), ImVec2(center.x + r, center.y + r), r,
                   draw::Col(ImVec4(col.x, col.y, col.z, 1.0f), 0.28f * ImSaturate(col.w + 0.15f)), Px(c.m.glow));
        dl->AddCircleFilled(center, r, draw::Col(col));
        if (col.w < 0.97f) dl->AddCircle(center, r - 0.5f, draw::Col(ImVec4(col.x, col.y, col.z, ImMin(1.0f, col.w + 0.22f))), 0, 1.0f);
    }
    ImGui::RenderNavCursor(bb, id);
    PickerPopup(popup_id, color, alpha, center);
    return false;
}

bool BeginSettings(const char* id_str, SettingsIcon icon, float width)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    Context& c = g_ctx;
    const ImGuiID id = window->GetID(id_str);
    const float d = S(icon == SettingsIcon::Gear ? 16.0f : c.m.dot);
    const ImVec2 center = AccessoryCenter(id, d);
    const float hit = d * 0.5f + S(3.0f);
    const ImRect bb(ImVec2(center.x - hit, center.y - hit), ImVec2(center.x + hit, center.y + hit));
    const ImGuiID popup_id = ImHashStr("##settings", 0, id);
    const ImGuiID was_key = ImHashStr("open_prev", 0, id);
    const bool was_open = c.state.GetBool(was_key, false);

    bool open = ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_None);
    if (ImGui::ItemAdd(bb, id)) {
        bool hovered = false, held = false;
        if (ImGui::ButtonBehavior(bb, id, &hovered, &held) && !open && !was_open) {
            ImGui::OpenPopupEx(popup_id);
            open = true;
        }
        const float hot = Animate(id, "hot", hovered ? 1.0f : 0.0f, 18.0f, 0.0f);
        const float act = Animate(id, "act", open ? 1.0f : 0.0f, 16.0f, 0.0f);

        SearchScope scope(nullptr, true);
        ImDrawList* dl = window->DrawList;
        if (icon == SettingsIcon::Plus) {
            const float r = d * 0.5f;
            const ImVec4 fill = draw::Mix(c.pal.accent, c.pal.accent_end, ImMax(hot * 0.55f, act * 0.75f));
            draw::Glow(dl, ImVec2(center.x - r, center.y - r), ImVec2(center.x + r, center.y + r), r,
                       draw::Col(fill, c.m.glow_alpha + 0.08f * hot), Px(c.m.glow));
            dl->AddCircleFilled(center, r, draw::Col(fill));
            draw::Plus(dl, center, S(5.0f), S(2.0f), draw::Col(c.pal.text));
        } else {
            c.icons.Draw(dl, Icon::GearSmall, center, draw::Col(draw::Mix(c.pal.text_soft, c.pal.text, ImMax(hot, act))));
        }
        ImGui::RenderNavCursor(bb, id);
    }
    c.state.SetBool(was_key, open);

    const float reveal = PopupReveal(popup_id, open);
    if (!open) return false;

    const float w = width > 0.0f ? width : S(c.m.popup_w);
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float last_h = c.state.GetFloat(ImHashStr("popup_h", 0, popup_id), S(120.0f));
    ImVec2 pos(center.x - w * 0.5f, center.y + d * 0.5f + S(c.m.popup_offset) - (1.0f - Ease(reveal)) * S(6.0f));
    pos.x = ImClamp(pos.x, vp->WorkPos.x + S(4.0f), vp->WorkPos.x + vp->WorkSize.x - w - S(4.0f));
    if (pos.y + last_h > vp->WorkPos.y + vp->WorkSize.y) pos.y = center.y - d * 0.5f - S(c.m.popup_offset) - last_h;

    ImGui::SetNextWindowPos(draw::Snap(pos));
    ImGui::SetNextWindowSizeConstraints(ImVec2(w, 0.0f), ImVec2(w, FLT_MAX));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * reveal);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(c.m.popup_pad_x), S(c.m.popup_pad_y)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(S(8.0f), S(c.m.item_spacing)));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0, 0, 0, 0));
    if (!ImGui::BeginPopupEx(popup_id, kPopupFlags | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(4);
        return false;
    }
    ImGuiWindow* popup = ImGui::GetCurrentWindow();
    c.state.SetFloat(ImHashStr("popup_h", 0, popup_id), popup->Size.y);
    PopupBackground(popup);
    return true;
}

void EndSettings()
{
    ImGui::EndPopup();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
}

bool CardCombo(const char* id_str, int* current, const char* const items[], int count, const ImRect& frame)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems || count <= 0) return false;
    Context& c = g_ctx;
    const ImGuiID id = window->GetID(id_str);
    *current = ImClamp(*current, 0, count - 1);
    if (!ImGui::ItemAdd(frame, id)) return false;

    const ImGuiID popup_id = ImHashStr("##dropdown", 0, id);
    bool hovered = false, held = false;
    const bool open = ImGui::IsPopupOpen(popup_id, ImGuiPopupFlags_None);
    if (ImGui::ButtonBehavior(frame, id, &hovered, &held) && !open) ImGui::OpenPopupEx(popup_id);
    const float hot = Animate(id, "hot", hovered ? 1.0f : 0.0f, 18.0f, 0.0f);

    ImDrawList* dl = window->DrawList;
    const float r = Px(c.m.frame_rounding);
    dl->AddRectFilled(frame.Min, frame.Max, draw::Col(draw::Mix(c.pal.frame, c.pal.frame_hover, hot * 0.7f)), r);
    const float tx = S(c.m.card_text_x);
    const ImU32 tc = draw::Col(draw::Mix(c.pal.text_soft, c.pal.text, hot * 0.5f));
    dl->PushClipRect(frame.Min, ImVec2(frame.Max.x - S(26.0f), frame.Max.y), true);
    draw::TextCapCentered(dl, c.fonts.regular, BodySize(), frame.Min.x + tx, frame.GetCenter().y, tc, items[*current]);
    dl->PopClipRect();
    draw::Chevron(dl, ImVec2(draw::Snap(frame.Max.x - S(c.m.card_chevron_right)), draw::Snap(frame.GetCenter().y) + 0.5f), Px(9.0f),
                  Px(4.5f), false, tc, Px(1.5f));
    ImGui::RenderNavCursor(frame, id);

    return Dropdown(popup_id, frame, items, count, current, nullptr, items[*current], tx);
}

void DrawBindsOverlay(float alpha)
{
    Context& c = g_ctx;
    int shown = 0;
    for (const auto& entry : c.binds)
        if (entry.second.show_in_list) ++shown;
    if (shown == 0 || alpha <= 0.01f) return;

    const float w = S(190.0f);
    const float head = S(26.0f);
    const float row = S(22.0f);
    ImGui::SetNextWindowPos(ImVec2(S(24.0f), S(220.0f)), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(w, head + row * static_cast<float>(shown) + S(6.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##astral_binds", nullptr, flags)) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImDrawList* dl = window->DrawList;
        const ImVec2 o = window->Pos;
        const ImVec2 o_max(o.x + window->Size.x, o.y + window->Size.y);
        const float r = Px(c.m.window_rounding);
        dl->PushClipRectFullScreen();
        draw::Glow(dl, o, o_max, r, draw::Col(Rgb(0x000000), 0.3f), Px(14.0f), ImVec2(0, Px(4.0f)));
        dl->PopClipRect();
        dl->AddRectFilled(o, o_max, draw::Col(c.pal.panel), r);
        draw::RectGradient(dl, o, ImVec2(o_max.x, o.y + head), draw::Col(c.pal.title_left), draw::Col(c.pal.title_right), r,
                           ImDrawFlags_RoundCornersTop, true);
        const float fs = BodySize();
        draw::TextCapCentered(dl, c.fonts.bold, fs, o.x + S(10.0f), o.y + head * 0.5f, draw::Col(c.pal.text), "Binds");
        float y = o.y + head + S(3.0f);
        static const char* const kModeNames[] = { "hold", "toggle", "always" };
        for (const auto& entry : c.binds) {
            const Bind& b = entry.second;
            if (!b.show_in_list) continue;
            const ImVec4 tc = b.active ? c.pal.text : c.pal.text_dim;
            draw::TextCapCentered(dl, c.fonts.regular, fs, o.x + S(10.0f), y + row * 0.5f, draw::Col(tc), b.label);
            char mode[32];
            ImFormatString(mode, IM_ARRAYSIZE(mode), "[%s]", kModeNames[static_cast<int>(b.mode)]);
            const float mw = draw::TextWidth(c.fonts.regular, fs, mode);
            draw::TextCapCentered(dl, c.fonts.regular, fs, o_max.x - S(10.0f) - mw, y + row * 0.5f,
                                  draw::Col(b.active ? c.pal.accent : c.pal.text_dim), mode);
            y += row;
        }
        ImGui::Dummy(window->Size);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace astral::ui

namespace astral {

ImVec4 ColorValue::Resolve(double time) const
{
    ImVec4 c = color;
    if (rainbow > 0.0f) {
        float h = 0.0f, s = 0.0f, v = 0.0f;
        ImGui::ColorConvertRGBtoHSV(c.x, c.y, c.z, h, s, v);
        const double cycles = time * static_cast<double>(rainbow) / 1000.0;
        h = static_cast<float>(std::fmod(static_cast<double>(h) + cycles, 1.0));
        if (s < 0.05f) s = 0.75f;
        if (v < 0.05f) v = 1.0f;
        ImGui::ColorConvertHSVtoRGB(h, s, v, c.x, c.y, c.z);
    }
    if (pulse > 0.0f) {
        const float wave = 0.5f + 0.5f * static_cast<float>(std::sin(time * 3.0));
        c.w *= 1.0f - (pulse / 100.0f) * 0.85f * wave;
    }
    return c;
}

} // namespace astral
