// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Async/Future.h"

class CYINSIGHTS_API FCyInsightsModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	bool StartCapture(const FString& NetworkHost = FString(), uint16 NetworkPort = 0);
	void StopCapture();
	bool CaptureGpu(int32 Frames);
private:
	friend class FCyInsightsRemoteControl;
	TSharedPtr<class FJsonObject> CaptureManifest() const;
	TSharedPtr<class FJsonObject> FinalManifest;
	void FinishNetworkStart();
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
	FString SessionId, Directory, CreatedUtc;
	uint64 StartCycles = 0;
	double StopAtSeconds = 0;
	TUniquePtr<FArchive> SyncWriter;
	FDelegateHandle FrameHandle;
	FTSTicker::FDelegateHandle TickHandle;
	TArray<class IConsoleObject*> Commands;
};
