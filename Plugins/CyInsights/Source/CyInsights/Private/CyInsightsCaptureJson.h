// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include "CyInsightsCapturePolicy.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace CyInsightsOptions {
inline TSharedPtr<FJsonObject> Write(const cycapture::Options& O) {
    auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("version"),1);
#define CY_BOOL(Name) J->SetBoolField(TEXT(#Name),O.Name)
    CY_BOOL(cpu);CY_BOOL(gpu);CY_BOOL(frame_stats);CY_BOOL(object_counts);CY_BOOL(memory_stats);CY_BOOL(stat_groups_enabled);
    CY_BOOL(tasks);CY_BOOL(network);CY_BOOL(file_io);CY_BOOL(loadtime);CY_BOOL(logs);CY_BOOL(memtags);CY_BOOL(quest_metrics);
    CY_BOOL(quest_gpu_completion);CY_BOOL(gpu_passes);CY_BOOL(gpu_details);CY_BOOL(gpu_bindings);CY_BOOL(gpu_barriers);CY_BOOL(gpu_per_command);CY_BOOL(gpu_readbacks);
#undef CY_BOOL
    J->SetStringField(TEXT("stat_groups"),UTF8_TO_TCHAR(O.stat_groups.c_str()));J->SetNumberField(TEXT("quest_hz"),O.quest_hz);J->SetNumberField(TEXT("gpu_frames"),O.gpu_frames);return J;
}
inline bool Read(const TSharedPtr<FJsonObject>& J,cycapture::Options& O,FString& Error) {
    if(!J){Error=TEXT("Capture options must be an object");return false;}
    double Version=0;if(!J->TryGetNumberField(TEXT("version"),Version)||Version!=1){Error=TEXT("Unsupported capture-options version");return false;}
    // Parse into a copy: malformed fields never change the running profile.
    auto Result=O;
    for(const auto& Pair:J->Values){
        const FString Key(Pair.Key);const auto& Value=Pair.Value;if(Key==TEXT("version"))continue;
#define CY_BOOL(Name) if(Key==TEXT(#Name)){if(Value->Type!=EJson::Boolean){Error=TEXT("Expected boolean: ")+Key;return false;}Result.Name=Value->AsBool();continue;}
        CY_BOOL(cpu);CY_BOOL(gpu);CY_BOOL(frame_stats);CY_BOOL(object_counts);CY_BOOL(memory_stats);CY_BOOL(stat_groups_enabled);
        CY_BOOL(tasks);CY_BOOL(network);CY_BOOL(file_io);CY_BOOL(loadtime);CY_BOOL(logs);CY_BOOL(memtags);CY_BOOL(quest_metrics);
        CY_BOOL(quest_gpu_completion);CY_BOOL(gpu_passes);CY_BOOL(gpu_details);CY_BOOL(gpu_bindings);CY_BOOL(gpu_barriers);CY_BOOL(gpu_per_command);CY_BOOL(gpu_readbacks);
#undef CY_BOOL
        if(Key==TEXT("stat_groups")){if(Value->Type!=EJson::String){Error=TEXT("Expected stat group string");return false;}Result.stat_groups=TCHAR_TO_UTF8(*Value->AsString());continue;}
        if(Key==TEXT("quest_hz")||Key==TEXT("gpu_frames")){
            if(Value->Type!=EJson::Number){Error=TEXT("Expected integer: ")+Key;return false;}
            const double N=Value->AsNumber();if(!FMath::IsFinite(N)||N<(Key==TEXT("quest_hz")?0:1)||N>120||N!=FMath::FloorToDouble(N)){Error=TEXT("Invalid integer: ")+Key;return false;}
            if(Key==TEXT("quest_hz"))Result.quest_hz=static_cast<int>(N);else Result.gpu_frames=static_cast<int>(N);continue;
        }
        Error=TEXT("Unknown capture option: ")+Key;return false;
    }
    const auto Invalid=Result.Validate();if(!Invalid.empty()){Error=UTF8_TO_TCHAR(Invalid.c_str());return false;}O=MoveTemp(Result);return true;
}
}
