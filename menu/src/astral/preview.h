// Astral-Remake - made by kdt255 (https://github.com/kdt255/Astral-Remake)
// Free software: MIT + Commons Clause, selling is not permitted. If you paid for this, you got scammed.
#pragma once
#include "astral/config.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace astral {

// Live ESP preview: box, skeleton, name, health, weapon and flag overlays of `v`, laid out around an invisible reference body.
void DrawEspPreview(ImDrawList* dl, const ImRect& area, const PlayerVisuals& v, double time);

} // namespace astral
