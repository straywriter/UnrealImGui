#pragma once

#include "imgui.h"

namespace ImGui
{
    IMGUI_API bool GradientButton(const char* label, const ImVec2& size, ImU32 text_color, ImU32 bg_color_1, ImU32 bg_color_2);
}