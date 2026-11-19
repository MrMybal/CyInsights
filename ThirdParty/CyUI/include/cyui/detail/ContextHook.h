#pragma once
#include <imgui_internal.h>

namespace cyui::detail {
// ImGui registers the IO atlas first and unregisters it before shutdown hooks.
inline bool hasContextAtlas(const ImGuiContext* context)
{
    return context && !context->FontAtlases.empty() && context->FontAtlases[0] == context->IO.Fonts;
}

// ImGui dispatches hooks with a range-for over its vector. First use of CyUI
// can occur inside a host callback, including when every slot is occupied.
// Keep that dispatch's iterators valid until context teardown. A remaining
// callback observes the live registration (e.g. a removal earlier in the loop).
inline void addShutdownHook(ImGuiContext* context, ImGuiContextHookCallback callback,
                            ImVector<ImGuiContextHook>& retired)
{
    auto& hooks = context->Hooks;
    if (hooks.Size && hooks.Size == hooks.Capacity) {
        retired.swap(hooks);
        hooks = retired;
        for (auto& previous : retired) {
            previous.Callback = [](ImGuiContext* ctx, ImGuiContextHook* prior) {
                for (auto& live : ctx->Hooks)
                    if (live.HookId == prior->HookId && live.Type == prior->Type) {
                        live.Callback(ctx, &live);
                        return;
                    }
            };
        }
    }
    ImGuiContextHook hook;
    hook.Type = ImGuiContextHookType_Shutdown;
    hook.Callback = callback;
    ImGui::AddContextHook(context, &hook);
}
} // namespace cyui::detail
