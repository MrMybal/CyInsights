// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Async/Future.h"
#include "CyInsightsTrace.h"
#include "CyInsightsCapturePolicy.h"

class CYINSIGHTS_API FCyInsightsModule : public IModuleInterface
{
public:
    FCyInsightsModule();
    virtual ~FCyInsightsModule() override;

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	bool StartCapture(const FString& NetworkHost = FString(), uint16 NetworkPort = 0,const cycapture::Options* Override = nullptr);
	void StopCapture();
	bool CaptureGpu(int32 Frames);
	bool CaptureGpu(const cycapture::Options& Options);
	bool CanCaptureGpu() const;
private:
	friend class FCyInsightsRemoteControl;
	TSharedPtr<class FJsonObject> CaptureManifest() const;
	TSharedPtr<class FJsonObject> FinalManifest;
	void FinishNetworkStart();
	void EnableCaptureNetwork();
	void RestoreCaptureNetwork();
	uint32 OwnedNetworkTraceLevel = 0, CaptureNetworkTraceLevel = 0;
	void RecordFrame();
	bool Tick(float Delta);
	bool WriteManifest();
	bool bCapturing = false;
	bool bStopRequested = false;
	bool bNetworkPending = false, bNetworkQueued = false, bCancelNetwork = false;
	FString NetworkHost, NetworkError;
	uint16 NetworkPort = 0;
	double NetworkRequestedAt = 0;
	TFuture<bool> NetworkStartTask;
	TUniquePtr<class FCyInsightsRemoteControl> RemoteControl;
	TUniquePtr<class FCyInsightsStatsCapture> StatsCapture;
	TUniquePtr<class FCyInsightsGpuCapture> GpuCapture;
	cycapture::Options CaptureOptions;
	FString GpuCaptureMessage;
	void BeginCaptureCollectors();
	void ConfigureQuestCapture(bool Apply);
	FString SessionId, Directory, CreatedUtc, CaptureChannels;
	uint64 StartCycles = 0;
	double StopAtSeconds = 0;
	TUniquePtr<FArchive> SyncWriter;
	FDelegateHandle FrameHandle;
	FCyInsightsTickerHandle TickHandle;
	TArray<class IConsoleObject*> Commands;
};
