// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#include "CyInsights.h"
#include "ToolMenus.h"
#include "Framework/Commands/UIAction.h"
class FCyInsightsEditorModule : public IModuleInterface {
public:
    virtual void StartupModule() override {
        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this,&FCyInsightsEditorModule::Register));
    }
    virtual void ShutdownModule() override {
        UToolMenus::UnRegisterStartupCallback(this); UToolMenus::UnregisterOwner(this);
    }
    void Register() {
        FToolMenuOwnerScoped Owner(this);
        auto& Section=UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools")->AddSection("CyInsights",FText::FromString("CyInsights"));
        Section.AddMenuEntry("CyInsightsStart",FText::FromString("Start native capture"),FText::FromString("Record native Unreal trace and synchronization anchors"),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([](){FModuleManager::GetModuleChecked<FCyInsightsModule>("CyInsights").StartCapture();})));
        Section.AddMenuEntry("CyInsightsGpu",FText::FromString("Capture GPU frame"),FText::FromString("Request a detailed frame from the Windows collector"),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([](){FModuleManager::GetModuleChecked<FCyInsightsModule>("CyInsights").CaptureGpu(1);})));
        Section.AddMenuEntry("CyInsightsStop",FText::FromString("Stop native capture"),FText::FromString("Close the native trace; staging is in Saved/CyInsights"),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([](){FModuleManager::GetModuleChecked<FCyInsightsModule>("CyInsights").StopCapture();})));
    }
};
IMPLEMENT_MODULE(FCyInsightsEditorModule,CyInsightsEditor)
