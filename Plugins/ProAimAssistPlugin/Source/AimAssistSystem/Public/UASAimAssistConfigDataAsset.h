// Copyright 2022 Dmitriy Vergasov All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Engine/DataAsset.h"

#include "UASAimAssistConfigDataAsset.generated.h"

USTRUCT(BlueprintType)
struct AIMASSISTSYSTEM_API FUASStickinessZoneConfig
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistConfig|Stickiness")
	float Radius = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistConfig|Stickiness", meta = (ClampMin = 0.f, UIMin = 0.f))
	float Multiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistConfig|Stickiness")
	FRuntimeFloatCurve StickinessMultiplierCurvePitch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistConfig|Stickiness")
	FRuntimeFloatCurve StickinessMultiplierCurveYaw;
};

USTRUCT(BlueprintType)
struct AIMASSISTSYSTEM_API FUASMagnetismZoneConfig
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistConfig|Magnetism", meta = (ClampMin = 0.f, UIMin = 0.f))
	float StartRadius = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistConfig|Magnetism", meta = (ClampMin = 0.f, UIMin = 0.f))
	float AimZoneRadius = 30.f;
};

USTRUCT(BlueprintType)
struct AIMASSISTSYSTEM_API FUASZonesScalingConfig
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AimAssistConfig|Scaling")
	FRuntimeFloatCurve ZonesScalingCurve;
};

USTRUCT(BlueprintType)
struct AIMASSISTSYSTEM_API FUASAutoAimConfig
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistConfig|AutoAim", meta = (ClampMin = 0.f, UIMin = 0.f))
	float Speed = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistConfig|AutoAim", meta = (ClampMin = 0.f, UIMin = 0.f))
	float AutoAimZoneRadius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistConfig|AutoAim", meta = (ClampMin = 0.f, UIMin = 0.f))
	float ActivationDistance = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistConfig|AutoAim", meta = (ClampMin = 0.f, UIMin = 0.f))
	float TimeWithoutCameraInputToEnableAutoAim = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistConfig|AutoAim", meta = (ClampMin = 0.f, UIMin = 0.f))
	float TimeWithMovementInputToEnableAutoAim = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistConfig|AutoAim")
	bool bUseOnlyWithInactiveMagnetism = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistConfig|AutoAim", meta = (ClampMin = 0.f, UIMin = 0.f))
	float TimeToBlockAfterChangeTarget = 0.2f;
};

UCLASS(BlueprintType)
class AIMASSISTSYSTEM_API UUASAimAssistConfigDataAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistConfig")
	float UpdateTargetsRate = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistConfig")
	FVector AimAreaExtents = { 5000.f, 300.f, 300.f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (InlineEditConditionToggle), Category = "AimAssistConfig")
	bool bStickinessZoneConfig = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = bStickinessZoneConfig), Category = "AimAssistConfig")
	FUASStickinessZoneConfig StickinessZoneConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (InlineEditConditionToggle), Category = "AimAssistConfig")
	bool bMagnetismZoneConfig = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = bMagnetismZoneConfig), Category = "AimAssistConfig")
	FUASMagnetismZoneConfig MagnetismZoneConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (InlineEditConditionToggle), Category = "AimAssistConfig")
	bool bScalingZoneConfig = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = bScalingZoneConfig), Category = "AimAssistConfig")
	FUASZonesScalingConfig ZonesScalingConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (InlineEditConditionToggle), Category = "AimAssistConfig")
	bool bAutoAimConfig = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = bAutoAimConfig), Category = "AimAssistConfig")
	FUASAutoAimConfig AutoAimConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistConfig")
	FVector2D CrosshairOffset = FVector2D::ZeroVector;
};
