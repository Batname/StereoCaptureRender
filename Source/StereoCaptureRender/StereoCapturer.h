// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "Components/SceneCaptureComponent2D.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/IImageWrapper.h"
#include "StereoCapturer.generated.h"

class IImageWrapperModule;

/**
 * 
 */
UCLASS()
class STEREOCAPTURERENDER_API UStereoCapturer : public UObject, public FTickableGameObject
{
	GENERATED_BODY()
	
public:
	UStereoCapturer();

	// For Image wrapper module
	UStereoCapturer(FVTableHelper& Helper);

	virtual void Tick(float DeltaTime) override;

	virtual bool IsTickable() const override
	{
		return true;
	}

	virtual bool IsTickableWhenPaused() const override
	{
		return bIsTicking;
	}

	virtual UWorld* GetTickableGameObjectWorld() const override;

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(USceneCapturer, STATGROUP_Tickables);
	}

public:
	void SetInitialState(class AStereoCaptureRenderCharacter* StereoCaptureRenderChar);
	void Reset();
	void SetPositionAndRotation(USceneCaptureComponent2D* EyeCaptureComponent, FVector Location, FRotator Rotation);
	void SetCustomProjectionMatrix(USceneCaptureComponent2D* EyeCaptureComponent);
	void InitCaptureComponent(USceneCaptureComponent2D* EyeCaptureComponent, float HFov, float VFov, EStereoscopicPass InStereoPass);
	void ReadCaptureComponent(USceneCaptureComponent2D* EyeCaptureComponent, TArray<FColor>& Buffer);

	void SaveFrame(const TArray<FColor>& Buffer, const FString& Name, EImageFormat::Type ImageFormat);
	
private:
	IImageWrapperModule& ImageWrapperModule;

	bool bIsTicking = false;

	float eyeSeparation;

	class APlayerController* CapturePlayerController;
	class AGameModeBase* CaptureGameMode;
	class AStereoCaptureRenderCharacter* StereoCaptureRenderCharacter;

	class USceneCaptureComponent2D* LeftEyeCaptureComponent;
	class USceneCaptureComponent2D* RigntEyeCaptureComponent;


	FDateTime OverallStartTime;
	FDateTime StartTime;

	FString Timestamp;

	int32 CaptureWidth;
	int32 CaptureHeight;

	float HFov;
	float VFov;

	FString OutputDir;

	FString FrameDescriptors;

};
