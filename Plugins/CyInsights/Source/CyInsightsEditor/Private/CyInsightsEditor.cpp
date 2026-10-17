// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#include "CyInsights.h"
#include "ToolMenus.h"
#include "Framework/Commands/UIAction.h"
#include "ISettingsModule.h"
#if PLATFORM_WINDOWS
#include "CyInsightsGpuWindows.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#endif
class FCyInsightsEditorModule : public IModuleInterface {
public:
    virtual void StartupModule() override {
#if PLATFORM_WINDOWS
        if(auto* Gpu=FCyInsightsGpuWindowsModule::Get())Gpu->SetMainViewportResolver([]()->FViewport* {
            if(GEditor && GEditor->PlayWorld)if(auto* Viewport=GEditor->GetPIEViewport())return Viewport;
            if(auto* Level=FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
                if(auto Viewport=Level->GetFirstActiveLevelViewport())return Viewport->GetActiveViewport();
            return nullptr;
        });
#endif
        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this,&FCyInsightsEditorModule::Register));
    }
    virtual void ShutdownModule() override {
#if PLATFORM_WINDOWS
        if(auto* Gpu=FCyInsightsGpuWindowsModule::Get())Gpu->SetMainViewportResolver(nullptr);
#endif
        UToolMenus::UnRegisterStartupCallback(this); UToolMenus::UnregisterOwner(this);
    }
    void Register() {
        FToolMenuOwnerScoped Owner(this);
#if ENGINE_MAJOR_VERSION >= 5
        const FName MenuName("LevelEditor.MainMenu.Tools");
#else
        const FName MenuName("LevelEditor.MainMenu.Window");
#endif
        auto& Section=UToolMenus::Get()->ExtendMenu(MenuName)->FindOrAddSection("CyInsights");
        Section.Label=FText::FromString("CyInsights");
        Section.AddMenuEntry("CyInsightsOptions",FText::FromString("Capture settings"),FText::FromString("Project defaults: basic capture, extra stat groups and explicit GPU readbacks"),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([](){if(auto* Settings=FModuleManager::GetModulePtr<ISettingsModule>("Settings"))Settings->ShowViewer("Project","Plugins","CyInsights");})));
#if PLATFORM_WINDOWS
        Section.AddMenuEntry("CyInsightsWindowsGpuOptions",FText::FromString("Optional Windows GPU collector"),FText::FromString("Enable the internal collector at startup; native utrace GPU groups work without it"),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([](){if(auto* Settings=FModuleManager::GetModulePtr<ISettingsModule>("Settings"))Settings->ShowViewer("Project","Plugins","CyInsightsWindowsGpu");})));
#endif
        Section.AddMenuEntry("CyInsightsStart",FText::FromString("Start native capture"),FText::FromString("Record native Unreal trace and synchronization anchors"),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([](){FModuleManager::GetModuleChecked<FCyInsightsModule>("CyInsights").StartCapture();})));
        Section.AddMenuEntry("CyInsightsGpu",FText::FromString("Capture GPU frame"),FText::FromString("Request a detailed frame from the Windows collector"),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([](){FModuleManager::GetModuleChecked<FCyInsightsModule>("CyInsights").CaptureGpu(1);})));
        Section.AddMenuEntry("CyInsightsStop",FText::FromString("Stop native capture"),FText::FromString("Close the native trace; staging is in Saved/CyInsights"),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([](){FModuleManager::GetModuleChecked<FCyInsightsModule>("CyInsights").StopCapture();})));
    }
};
IMPLEMENT_MODULE(FCyInsightsEditorModule,CyInsightsEditor)
