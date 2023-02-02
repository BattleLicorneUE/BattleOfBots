// Copyright 2022 Dmitriy Vergasov All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Engine/Canvas.h"
#include "GameFramework/HUD.h"

#include "UASAimAssistComponent.generated.h"

class UUASAimAssistTargetComponent;
class UUASAimAssistConfigDataAsset;

USTRUCT()
struct AIMASSISTSYSTEM_API FUASAimAssistTargetData
{
public:
	GENERATED_BODY()
public:
	bool operator==(const FUASAimAssistTargetData& A) const { return A.TargetComponent == TargetComponent && A.SocketName == SocketName; }

	FVector2D GetLocationOnTheScreen(APlayerController* Controller) const;
	FVector GetSocketLocation() const;

	bool IsValid() const;
	TWeakObjectPtr<UUASAimAssistTargetComponent> TargetComponent;
	FName SocketName;
};

DECLARE_STATS_GROUP(TEXT("AimAssist"), STATGROUP_AimAssist, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("HandleCurrentTarget"), STAT_HandleCurrentTarget, STATGROUP_AimAssist, AIMASSISTSYSTEM_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("HandleAutoAim"), STAT_HandleAutoAim, STATGROUP_AimAssist, AIMASSISTSYSTEM_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("UpdateTargets"), STAT_UpdateTargets, STATGROUP_AimAssist, AIMASSISTSYSTEM_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("HandleTargets"), STAT_HandleTargets, STATGROUP_AimAssist, AIMASSISTSYSTEM_API);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUASOnAimDataAssetChangedDelegate, UUASAimAssistConfigDataAsset*, NewAsset);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AIMASSISTSYSTEM_API UUASAimAssistComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUASAimAssistComponent();
	
	virtual  void BeginPlay() override;
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, meta = (AdvancedDisplay = "Distance"), Category = "AimAssistComponent")
	FRotator GetRotationToCrosshairDirection(const FVector& From, FName TraceProfileName, float Distance = 100000.f) const;

	UFUNCTION(BlueprintPure, Category = "AimAssistComponent")
	const FVector2D& GetCrosshairPosition() const { return CrosshairPosition; }

	UFUNCTION(BlueprintCallable, Category = "AimAssistComponent")
	void SetAimAssistDataAsset(UUASAimAssistConfigDataAsset* DataAsset);

	UFUNCTION(BlueprintPure, Category = "AimAssistComponent")
	void GetControlMultipliers(float& Pitch, float& Yaw) const;

protected:
	bool CanUseAssist() const;

	UFUNCTION()
	void OnTargetDestroyed(AActor* DestroyedActor);

	void UpdateAssist();

	void UpdateTargets();

	void HandleTargets();

	FVector GetOverlapExtents() const;

	FVector GetOverlapLocation() const;
	FRotator GetOverlapRotation() const;

	FVector GetCameraLocation() const;

	FVector2D GetScreenCenter() const;

	void HandleCurrentTarget();

	void HandleAutoAim(float DeltaTime);

	void UpdateCrosshair(float DeltaTime);

	void UpdateZonesScaling(float DeltaTime);

	float GetCurveValue(const FRuntimeFloatCurve& Curve, float Power) const;

	float GetScaledZoneRadius(float From) const;

#if !UE_BUILD_SHIPPING
	float GetScaledZoneRadiusForDebug(float From) const;
	void DrawDebug(float DeltaTime) const;
	void DrawHudDebug(AHUD* HUD, UCanvas* Canvas) const;
	void DrawCircle(AHUD* HUD, float Radius, float LineThickness, FColor Color) const;

	void DrawCrosshair(AHUD* HUD, const FVector2D& Position) const;
#endif

	bool IsMagtetismEnabled() const;
	bool IsStickinessEnabled() const;
	bool IsScalingEnabled() const;
	bool IsAutoAimEnabled() const;

public:
	UPROPERTY(BlueprintAssignable, Category = "AimAssistComponent")
	FUASOnAimDataAssetChangedDelegate OnAimDataAssetChangedDelegate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistComponent")
	bool bEnableStickiness = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistComponent")
	bool bEnableMagnetism = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistComponent")
	bool bEnableScaling = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistComponent")
	bool bScalingByDPI = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistComponent")
	bool bEnableAutoAim = true;
protected:
	UPROPERTY(EditAnywhere, Category = "AimAssistComponent|Debug")
	bool bDrawCircles = false;

	UPROPERTY(EditAnywhere, Category = "AimAssistComponent|Debug")
	bool bDrawCrosshair = false;

	UPROPERTY(EditAnywhere, Category = "AimAssistComponent|Debug")
	bool bDebugOverlayBox = false;

	UPROPERTY(EditAnywhere, Category = "AimAssistComponent|Debug")
	bool bShowValidTargetSockets = false;

	UPROPERTY(EditAnywhere, Category = "AimAssistComponent|Debug")
	bool bDebugTargetTraces = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimAssistComponent")
	UUASAimAssistConfigDataAsset* AimAssistDataAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistComponent")
	FName TargetsDetectionProfileName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistComponent")
	FName ObstacleCheckProfileName;

	FTimerHandle UpdateTargetsTimerHandle;

	TWeakObjectPtr<APlayerController> PlayerController;

	TMap<TWeakObjectPtr<AActor>, TArray<TWeakObjectPtr<UUASAimAssistTargetComponent>>> TargetComponents;

	FUASAimAssistTargetData CurrentTargetData;
	TArray<FUASAimAssistTargetData> LastTargetData;

	bool bStickinessAreaActive = false;
	bool bMagnetismAreaActive = false;
	bool bAutoAimAreaActive = false;
	float ZonesScaleMultiplier = 1.f;
	float ZonesScaleMultiplierDebug = 1.f;

	TWeakObjectPtr<AActor> LastAutoAimTarget;
	FTimerHandle WaitAfterChangeTargetToAutoAimTimerHandle;
	float TimeWithoutRotationInput = 0.f;
	float TimeWithMovementInput = 0.f;
	FVector2D CrosshairPosition = FVector2D::ZeroVector;
	FVector2D CrosshairPositionDebug = FVector2D::ZeroVector;
	FRotator LastRotationInput = FRotator::ZeroRotator;
};