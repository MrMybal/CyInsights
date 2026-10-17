// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CyQuestGpuTest.generated.h"

UCLASS()
class ACyQuestGpuTestMode : public AGameModeBase {
    GENERATED_BODY()
public:
    ACyQuestGpuTestMode();
    void StartPlay() override;
    void Tick(float DeltaSeconds) override;
private:
    UPROPERTY() UStaticMesh* Cube = nullptr;
    UPROPERTY() class ASceneCapture2D* ExtraView = nullptr;
    UPROPERTY() class UTextureRenderTarget2D* Target = nullptr;
    UPROPERTY() TArray<class AStaticMeshActor*> Objects;
    int32 Phase = -1;
};
