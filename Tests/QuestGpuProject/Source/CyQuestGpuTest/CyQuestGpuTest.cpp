// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#include "CyQuestGpuTest.h"
#include "Modules/ModuleManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ProfilingDebugging/MiscTrace.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, CyQuestGpuTest, "CyQuestGpuTest");

ACyQuestGpuTestMode::ACyQuestGpuTestMode() {
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = nullptr;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Cube = Mesh.Object;
}
void ACyQuestGpuTestMode::StartPlay() {
    Super::StartPlay();
    auto* World = GetWorld();
    for(int32 i=0;i<65;++i) {
        const bool Floor=i==64;
        auto* Object=World->SpawnActor<AStaticMeshActor>();
        Object->SetMobility(EComponentMobility::Movable);
        Object->GetStaticMeshComponent()->SetStaticMesh(Cube);
        Object->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Object->SetActorLocation(Floor?FVector(450,0,-100):FVector(150+(i/8)*100,(i%8-4)*100,0));
        Object->SetActorScale3D(Floor?FVector(20,20,.2):FVector(.6));
        if(!Floor)Objects.Add(Object);
    }
    auto* Light=World->SpawnActor<ADirectionalLight>();
    Light->SetActorRotation(FRotator(-45,-35,0));
    Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Light->GetLightComponent()->SetIntensity(6);
    auto* Sky=World->SpawnActor<ASkyLight>();
    Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    auto* Camera=World->SpawnActor<ACameraActor>();
    Camera->SetActorLocation(FVector(-250,0,150));
    Camera->SetActorRotation(FRotator(-12,0,0));
    if(auto* Player=World->GetFirstPlayerController())Player->SetViewTarget(Camera);
    Target=NewObject<UTextureRenderTarget2D>(this);
    Target->InitAutoFormat(512,512);
    ExtraView=World->SpawnActor<ASceneCapture2D>(Camera->GetActorLocation(),Camera->GetActorRotation());
    ExtraView->GetCaptureComponent2D()->TextureTarget=Target;
    ExtraView->GetCaptureComponent2D()->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;
    UE_LOG(LogTemp,Display,TEXT("CyQuestGpuTest ready: 64 cubes, movable light; extra rendered view alternates every 10 seconds"));
}
void ACyQuestGpuTestMode::Tick(float DeltaSeconds) {
    Super::Tick(DeltaSeconds);
    for(auto* Object:Objects)Object->AddActorLocalRotation(FRotator(0,DeltaSeconds*25,0));
    const int32 NextPhase=(int32(GetWorld()->GetTimeSeconds())/10)%2;
    if(NextPhase!=Phase&&ExtraView) {
        Phase=NextPhase;
        auto* Capture=ExtraView->GetCaptureComponent2D();
        Capture->bCaptureEveryFrame=Phase!=0;
        Capture->bCaptureOnMovement=false;
        TRACE_BOOKMARK(TEXT("CyQuestGpuTest extra_view=%d"),Phase);
        UE_LOG(LogTemp,Display,TEXT("CyQuestGpuTest extra_view=%d"),Phase);
    }
}
