#pragma once

#include "imgui_toggle.h"

// ImGuiTogglePresets: A few canned configurations for various presets OOTB.
namespace ImGuiTogglePresets
{
    // The default, unmodified style.
    IMGUI_API ImGuiToggleConfig DefaultStyle();

    // A style similar to default, but with rectangular knob and frame.
    IMGUI_API ImGuiToggleConfig RectangleStyle();

    // A style that uses a shadow to appear to glow while it's on.
    IMGUI_API ImGuiToggleConfig GlowingStyle();

    // A style that emulates what a toggle on iOS looks like.
    IMGUI_API ImGuiToggleConfig iOSStyle(float size_scale = 1.0f, bool light_mode = false);

    // A style that emulates what a Material Design toggle looks like.
    IMGUI_API ImGuiToggleConfig MaterialStyle(float size_scale = 1.0f);

    // A style that emulates what a toggle close to one from Minecraft.
    IMGUI_API ImGuiToggleConfig MinecraftStyle(float size_scale = 1.0f);
}
