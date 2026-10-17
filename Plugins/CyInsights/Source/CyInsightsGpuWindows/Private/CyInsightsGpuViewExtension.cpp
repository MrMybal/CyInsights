// CyInsightsGpuWindows — Unreal plugin. Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT

#include "CyInsightsGpuViewExtension.h"

#if CYINSIGHTS_WINDOWS_GPU_ENABLED

#include "CyInsightsGpuWindows.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneView.h"
#include "UnrealClient.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"

#include "CyInsightsGpuApi.h"

FCyInsightsGpuViewExtension::FCyInsightsGpuViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
}

bool FCyInsightsGpuViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	const FCyInsightsGpuWindowsModule* Module = FCyInsightsGpuWindowsModule::Get();
	return Module != nullptr && Context.Viewport != nullptr && Module->IsAddonPresent() &&
		Context.Viewport == Module->GetMainViewport();
}

void FCyInsightsGpuViewExtension::AddMarker(FRDGBuilder& GraphBuilder, TRefCountPtr<IPooledRenderTarget>& Pooled, const TCHAR* Name)
{
	// Created once, then kept: the add-on recognises a marker by its native resource, which must
	// therefore be the same every frame. The name also reaches the GPU objects (SetName), where
	// the add-on reads it back into the capture.
	FRDGTextureRef Texture = Pooled.IsValid()
		? GraphBuilder.RegisterExternalTexture(Pooled)
		: GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(FIntPoint(1, 1), PF_B8G8R8A8, FClearValueBinding::Black,
			TexCreate_RenderTargetable | TexCreate_ShaderResource), Name);
	AddClearRenderTargetPass(GraphBuilder, Texture, FLinearColor::Black);
	// Extracted every frame, which also keeps the graph from culling a pass whose result nothing
	// else reads.
	GraphBuilder.QueueTextureExtraction(Texture, &Pooled);
}

void FCyInsightsGpuViewExtension::PublishScope(const FSceneViewFamily& InViewFamily)
{
	auto NativeOf = [](const TRefCountPtr<IPooledRenderTarget>& Pooled) -> uint64
	{
#if ENGINE_MAJOR_VERSION >= 5
		return Pooled.IsValid() && Pooled->GetRHI() != nullptr ? reinterpret_cast<uint64>(Pooled->GetRHI()->GetNativeResource()) : 0;
#else
        FRHITexture* Texture = Pooled.IsValid() ? Pooled->GetRenderTargetItem().TargetableTexture.GetReference() : nullptr;
        return Texture ? reinterpret_cast<uint64>(Texture->GetNativeResource()) : 0;
#endif
	};
	const uint64 Begin = NativeOf(BeginMarker);
	const uint64 End = NativeOf(EndMarker);
	uint64 Final = 0;
	if (InViewFamily.RenderTarget != nullptr)
	{
		if (FRHITexture* Target = InViewFamily.RenderTarget->GetRenderTargetTexture().GetReference())
		{
			Final = reinterpret_cast<uint64>(Target->GetNativeResource());
		}
	}
	if (Begin == PublishedBegin && End == PublishedEnd && Final == PublishedFinal)
	{
		return;
	}

	// Looked up each time, and only when something changed: ReShade reloads its add-ons as
	// devices come and go, and this runs on the render thread, apart from the module's cache.
	HMODULE Addon = GetModuleHandleW(TEXT("CyInsightsGpu.addon64"));
	const auto SetScope = Addon != nullptr
		? reinterpret_cast<PFN_CyInsightsGpu_SetViewportScope>(reinterpret_cast<void*>(GetProcAddress(Addon, CYGI_EXPORT_SET_VIEWPORT_SCOPE)))
		: nullptr;
	if (SetScope == nullptr)
	{
		return;
	}
	CygiViewportScope Scope = {};
	Scope.size = sizeof(Scope);
	// Both markers or neither: a capture waits for a frame that has the two.
	Scope.begin_marker = Begin != 0 && End != 0 ? Begin : 0;
	Scope.end_marker = Begin != 0 && End != 0 ? End : 0;
	Scope.final_image = Final;
	if (SetScope(&Scope) == 1)
	{
		PublishedBegin = Begin;
		PublishedEnd = End;
		PublishedFinal = Final;
	}
}

#if ENGINE_MAJOR_VERSION >= 5
void FCyInsightsGpuViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	// The markers of the previous frames exist by now: say where they are before this frame's.
	PublishScope(InViewFamily);
	AddMarker(GraphBuilder, BeginMarker, TEXT("CyInsightsGpuWindows.ViewportBegin"));
}

void FCyInsightsGpuViewExtension::PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	AddMarker(GraphBuilder, EndMarker, TEXT("CyInsightsGpuWindows.ViewportEnd"));
}
#else
void FCyInsightsGpuViewExtension::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
    PublishScope(InViewFamily);
    FRDGBuilder GraphBuilder(RHICmdList);
    AddMarker(GraphBuilder, BeginMarker, TEXT("CyInsightsGpuWindows.ViewportBegin"));
    GraphBuilder.Execute();
}
void FCyInsightsGpuViewExtension::PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
    FRDGBuilder GraphBuilder(RHICmdList);
    AddMarker(GraphBuilder, EndMarker, TEXT("CyInsightsGpuWindows.ViewportEnd"));
    GraphBuilder.Execute();
}
#endif

#endif
