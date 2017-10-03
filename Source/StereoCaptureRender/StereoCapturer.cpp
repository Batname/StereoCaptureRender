// Fill out your copyright notice in the Description page of Project Settings.

#include "StereoCapturer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Interfaces/IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "UnrealEngine.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "TextureResource.h"

#include "StereoCaptureRenderCharacter.h"


UStereoCapturer::UStereoCapturer(FVTableHelper& Helper)
	: Super(Helper)
	, bIsTicking(false)
	, ImageWrapperModule(FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper")))
{}

UStereoCapturer::UStereoCapturer()
	: bIsTicking(false)
	, ImageWrapperModule(FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper")))
{
	CaptureWidth = 1920;
	CaptureHeight = 2160;

	float HFov = 90.f;
	float VFov = 90.f;

	eyeSeparation = 6.4f;

	OutputDir = FString("D:/Windows/UE4/StereoCaptureRender/Captures");

	FrameDescriptors = TEXT("FrameNumber, GameClock, TimeTaken(s)" LINE_TERMINATOR);

	LeftEyeCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("LeftEyeCaptureComponent"));
	RigntEyeCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("RigntEyeCaptureComponent"));

	InitCaptureComponent(LeftEyeCaptureComponent, HFov, VFov, EStereoscopicPass::eSSP_LEFT_EYE);
	InitCaptureComponent(RigntEyeCaptureComponent, HFov, VFov, EStereoscopicPass::eSSP_RIGHT_EYE);
}

UWorld * UStereoCapturer::GetTickableGameObjectWorld() const
{
	if (LeftEyeCaptureComponent)
	{
		return LeftEyeCaptureComponent->GetWorld();
	}

	return nullptr; 
}

void UStereoCapturer::SetInitialState(AStereoCaptureRenderCharacter* StereoCaptureRenderChar)
{
	UE_LOG(LogTemp, Warning, TEXT("SetInitialState"));

	if (bIsTicking)
	{
		UE_LOG(LogTemp, Warning, TEXT("Already capturing a scene; concurrent captures are not allowed"));
		return;
	}

	LeftEyeCaptureComponent->RegisterComponentWithWorld(GWorld);
	RigntEyeCaptureComponent->RegisterComponentWithWorld(GWorld);

	LeftEyeCaptureComponent->AddToRoot();
	RigntEyeCaptureComponent->AddToRoot();

	CapturePlayerController = UGameplayStatics::GetPlayerController(GWorld, 0);
	CaptureGameMode = UGameplayStatics::GetGameMode(GWorld);
	StereoCaptureRenderCharacter = StereoCaptureRenderChar;

	if (CaptureGameMode == NULL || CapturePlayerController == NULL)
	{
		UE_LOG(LogTemp, Warning, TEXT("Missing GameMode or PlayerController"));
		return;
	}


	Timestamp = FString::Printf(TEXT("%s"), *FDateTime::Now().ToString());

	StartTime = FDateTime::UtcNow();
	OverallStartTime = StartTime;
	bIsTicking = true;
}

void UStereoCapturer::InitCaptureComponent(USceneCaptureComponent2D * EyeCaptureComponent, float HFov, float VFov, EStereoscopicPass InStereoPass)
{
	EyeCaptureComponent->SetVisibility( true );
	EyeCaptureComponent->SetHiddenInGame( false );

	//EyeCaptureComponent->bUseCustomProjectionMatrix = true;

	EyeCaptureComponent->CaptureStereoPass = InStereoPass;
	EyeCaptureComponent->FOVAngle = FMath::Max( HFov, VFov );
	EyeCaptureComponent->bCaptureEveryFrame = false;
	EyeCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

	const FName TargetName = MakeUniqueObjectName(this, UTextureRenderTarget2D::StaticClass(), TEXT("SceneCaptureTextureTarget"));
	EyeCaptureComponent->TextureTarget = NewObject<UTextureRenderTarget2D>(this, TargetName);

	EyeCaptureComponent->TextureTarget->InitCustomFormat(CaptureWidth, CaptureHeight, PF_A16B16G16R16, false);
	EyeCaptureComponent->TextureTarget->ClearColor = FLinearColor::Red;
}

void UStereoCapturer::ReadCaptureComponent(USceneCaptureComponent2D * EyeCaptureComponent, TArray<FColor>& Buffer)
{
	EyeCaptureComponent->CaptureScene();

	FTextureRenderTargetResource* RenderTarget = EyeCaptureComponent->TextureTarget->GameThread_GetRenderTargetResource();

	Buffer.AddUninitialized(CaptureWidth * CaptureHeight);

	// Read pixels
	FIntRect Area(0, 0, CaptureWidth, CaptureHeight);
	auto readSurfaceDataFlags = FReadSurfaceDataFlags();
	readSurfaceDataFlags.SetLinearToGamma(false);
	RenderTarget->ReadPixelsPtr(Buffer.GetData(), readSurfaceDataFlags, Area);
}

void UStereoCapturer::SaveFrame(const TArray<FColor>& Buffer, const FString & Name, EImageFormat::Type ImageFormat)
{
	FString FrameString = FString::Printf(TEXT("%s.png"), *Name);
	FString Filepath = OutputDir / Timestamp / FrameString;

	//Read Whole Capture Buffer
	IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
	ImageWrapper->SetRaw(Buffer.GetData(), Buffer.GetAllocatedSize(), CaptureWidth, CaptureHeight, ERGBFormat::BGRA, 8);
	const TArray<uint8>& ImageData = ImageWrapper->GetCompressed(100);
	FFileHelper::SaveArrayToFile(ImageData, *Filepath);
}

void UStereoCapturer::Reset()
{
	LeftEyeCaptureComponent->SetVisibility(false);
	LeftEyeCaptureComponent->SetHiddenInGame(true);
	LeftEyeCaptureComponent->RemoveFromRoot();

	RigntEyeCaptureComponent->SetVisibility(false);
	RigntEyeCaptureComponent->SetHiddenInGame(true);
	RigntEyeCaptureComponent->RemoveFromRoot();
}

void UStereoCapturer::SetPositionAndRotation(USceneCaptureComponent2D * EyeCaptureComponent, FVector Location, FRotator Rotation)
{
	float EyeOffset = 3.20000005f;
	const float PassOffset = (EyeCaptureComponent->CaptureStereoPass == eSSP_LEFT_EYE) ? EyeOffset : -EyeOffset;

	EyeCaptureComponent->SetWorldLocationAndRotation(Location - FVector(0.0f, PassOffset, 0.0f), Rotation);
}

void UStereoCapturer::SetCustomProjectionMatrix(USceneCaptureComponent2D * EyeCaptureComponent)
{
	FMatrix CustomProjectionMatrix;
	const float ProjectionCenterOffset = 0.151976421f;
	const float PassProjectionOffset = (EyeCaptureComponent->CaptureStereoPass == eSSP_LEFT_EYE) ? ProjectionCenterOffset : -ProjectionCenterOffset;

	const float HalfFov = 2.19686294f / 2.f;
	const float InWidth = 640.f;
	const float InHeight = 480.f;
	const float XS = 1.0f / tan(HalfFov);
	const float YS = InWidth / tan(HalfFov) / InHeight;

	const float InNearZ = GNearClippingPlane;
	CustomProjectionMatrix = FMatrix(
		FPlane(XS, 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, YS, 0.0f, 0.0f),
		FPlane(0.0f, 0.0f, 0.0f, 1.0f),
		FPlane(0.0f, 0.0f, InNearZ, 0.0f))

		* FTranslationMatrix(FVector(PassProjectionOffset, 0, 0));

	EyeCaptureComponent->CustomProjectionMatrix = CustomProjectionMatrix;
}


void UStereoCapturer::Tick(float DeltaTime)
{
	if (!bIsTicking  || !CapturePlayerController)
	{
		return;
	}

	TArray<FColor> LeftEyeBuffer;
	TArray<FColor> RightEyeBuffer;

	// Set position
	FVector Location;
	FRotator Rotation;
	CapturePlayerController->GetPlayerViewPoint(Location, Rotation);

	UE_LOG(LogTemp, Warning, TEXT("Location, %s"), *Location.ToString());
	UE_LOG(LogTemp, Warning, TEXT("Rotation, %s"), *Rotation.ToString());

	SetPositionAndRotation(LeftEyeCaptureComponent, Location, Rotation);
	SetPositionAndRotation(RigntEyeCaptureComponent, Location, Rotation);

	// Set Matrices
	//SetCustomProjectionMatrix(LeftEyeCaptureComponent);
	//SetCustomProjectionMatrix(RigntEyeCaptureComponent);

	// Read data from texture;
	ReadCaptureComponent(LeftEyeCaptureComponent, LeftEyeBuffer);
	ReadCaptureComponent(RigntEyeCaptureComponent, RightEyeBuffer);

	UE_LOG(LogTemp, Warning, TEXT("LeftEyeBuffer.GetAllocatedSize, %d"), LeftEyeBuffer.GetAllocatedSize());
	UE_LOG(LogTemp, Warning, TEXT("RightEyeBuffer.GetAllocatedSize, %d"), RightEyeBuffer.GetAllocatedSize());
	UE_LOG(LogTemp, Warning, TEXT("LeftEyeBuffer.Num, %d"), LeftEyeBuffer.Num());
	UE_LOG(LogTemp, Warning, TEXT("RightEyeBuffer.Num, %d"), RightEyeBuffer.Num());

	// Save Image
	SaveFrame(LeftEyeBuffer, TEXT("Left"), EImageFormat::PNG);
	SaveFrame(RightEyeBuffer, TEXT("Right"), EImageFormat::PNG);

	// Dump out how long the process took
	FDateTime EndTime = FDateTime::UtcNow();
	FTimespan Duration = EndTime - StartTime;
	UE_LOG(LogTemp, Log, TEXT("Duration: %g seconds"), Duration.GetTotalSeconds());
	StartTime = EndTime;

	// Clean up
	LeftEyeBuffer.Empty();
	RightEyeBuffer.Empty();
	StereoCaptureRenderCharacter->Cleanup();

	bIsTicking = false;
}

