//-----------------------------------------------------------------------------
// DEAR IMGUI COMPILE-TIME OPTIONS (astral-remake)
// Every translation unit including imgui.h must see the same options.
//-----------------------------------------------------------------------------
#pragma once

//---- FreeType rasterizer (misc/freetype/imgui_freetype.cpp + third_party/freetype, static lib built with /MD)
#define IMGUI_ENABLE_FREETYPE
#define IMGUI_FREETYPE_INTERPRETER_VERSION 35   // 35 = classic full hinting (crisp stems), 40 = minimal hinting (softer)

//#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
//#define IMGUI_DEFINE_MATH_OPERATORS
