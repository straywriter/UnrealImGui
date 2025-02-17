#include "PropertyWatcher.h"
#include "EngineUtils.h"
#include "imgui_internal.h"

#include "Kismet/KismetSystemLibrary.h"
#include "Engine/StaticMeshActor.h"

#include "GameFramework/PlayerController.h"

#include "UObject/Stack.h"
#include "Engine/Level.h"

#include "GameFramework/Actor.h"

#include "Misc/ExpressionParser.h"
#include "Internationalization/Regex.h"

#include "Misc/TextFilterUtils.h"
#include <inttypes.h> // For printing address.

struct InputTextCharCallbackUserData
{
    TArray<char>& Str;
    ImGuiInputTextCallback ChainCallback;
    void* ChainCallbackUserData;
};

static int InputTextCharCallback(ImGuiInputTextCallbackData* data)
{
    InputTextCharCallbackUserData* user_data = (InputTextCharCallbackUserData*)data->UserData;
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
        // Resize string callback
        // If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
        TArray<char>& str = user_data->Str;

        str.SetNum(data->BufTextLen);
        if (str.Max() < data->BufSize + 1)
            str.Reserve(data->BufSize + 1);

        data->Buf = str.GetData();
    }
    else if (user_data->ChainCallback)
    {
        // Forward to user callback, if any
        data->UserData = user_data->ChainCallbackUserData;
        return user_data->ChainCallback(data);
    }
    return 0;
}

bool ImGuiAddon::InputText(const char* label, TArray<char>& str, ImGuiInputTextFlags flags,
                           ImGuiInputTextCallback callback, void* user_data)
{
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCharCallbackUserData UserData = {str, callback, user_data};
    return ImGui::InputText(label, str.GetData(), str.Max() + 1, flags, InputTextCharCallback, &UserData);
}

bool ImGuiAddon::InputTextWithHint(const char* label, const char* hint, TArray<char>& str, ImGuiInputTextFlags flags,
                                   ImGuiInputTextCallback callback, void* user_data)
{
    flags |= ImGuiInputTextFlags_CallbackResize;

    InputTextCharCallbackUserData UserData = {str, callback, user_data};
    return ImGui::InputTextWithHint(label, hint, str.GetData(), str.Max() + 1, flags, InputTextCharCallback, &UserData);
}

bool ImGuiAddon::InputString(FString Label, FString& String, TArray<char>& StringBuffer, ImGuiInputTextFlags flags)
{
    StringBuffer.Empty();
    StringBuffer.Append(ImGui_StoA(*String), String.Len() + 1);
    if (ImGuiAddon::InputText(ImGui_StoA(*Label), StringBuffer, flags))
    {
        String = FString(StringBuffer);
        return true;
    }
    return false;
}

bool ImGuiAddon::InputStringWithHint(FString Label, FString Hint, FString& String, TArray<char>& StringBuffer,
                                     ImGuiInputTextFlags flags)
{
    StringBuffer.Empty();
    StringBuffer.Append(ImGui_StoA(*String), String.Len() + 1);
    if (ImGuiAddon::InputTextWithHint(ImGui_StoA(*Label), ImGui_StoA(*Hint), StringBuffer, flags))
    {
        String = FString(StringBuffer);
        return true;
    }
    return false;
}

void ImGuiAddon::QuickTooltip(FString TooltipText, ImGuiHoveredFlags Flags)
{
    if (ImGui::IsItemHovered(Flags))
    {
        ImGui::BeginTooltip();
        defer { ImGui::EndTooltip(); };
        ImGui::Text(ImGui_StoA(*TooltipText));
    }
}
