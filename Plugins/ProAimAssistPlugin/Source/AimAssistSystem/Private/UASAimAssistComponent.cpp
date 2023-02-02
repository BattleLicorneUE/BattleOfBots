// Copyright 2022 Dmitriy Vergasov All Rights Reserved.

#include "UASAimAssistComponent.h"

#include "Algo/MinElement.h"
#include "Algo/NoneOf.h"
#include "DrawDebugHelpers.h"
#include "Engine/Canvas.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "Misc/App.h"
#include "UASAimAssistConfigDataAsset.h"
#include "UASAimAssistTargetComponent.h"

DEFINE_STAT(STAT_HandleTargets);
DEFINE_STAT(STAT_UpdateTargets);
DEFINE_STAT(STAT_HandleAutoAim);
DEFINE_STAT(STAT_HandleCurrentTarget);

FVector2D FUASAimAssistTargetData::GetLocationOnTheScreen(APlayerController* Controller) const
{
	FVector2D location;
	UGameplayStatics::ProjectWorldToScreen(Controller, GetSocketLocation(), location);
	return location;
}

FVector FUASAimAssistTargetData::GetSocketLocation() const
{
	if (IsValid())
	{
		return TargetComponent->GetMesh()->GetSocketLocation(SocketName);
	}

	return FVector::ZeroVector;
}

bool FUASAimAssistTargetData::IsValid() const
{
	return TargetComponent.IsValid() && TargetComponent->GetMesh() != nullptr;
}

UUASAimAssistComponent::UUASAimAssistComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UUASAimAssistComponent::BeginPlay()
{
	Super::BeginPlay();
	
	PlayerController = GetTypedOuter<APlayerController>();

	if (!PlayerController.IsValid())
	{
		PlayerController = Cast<APlayerController>(GetOwner()->GetInstigatorController());
	}
	
	if (!CanUseAssist())
	{
		PlayerController = nullptr;
		return;
	}

	SetAimAssistDataAsset(AimAssistDataAsset);
	
	if (PlayerController.IsValid())
	{
		CrosshairPosition = GetScreenCenter();
		CrosshairPositionDebug = CrosshairPosition;
		
		FViewport::ViewportResizedEvent.AddLambda([&](FViewport* Viewport, uint32) {
			if (this && PlayerController.IsValid() && GetWorld() && GetWorld()->GetGameViewport() && GetWorld()->GetGameViewport()->Viewport == Viewport && AimAssistDataAsset)
			{
				CrosshairPositionDebug = GetScreenCenter();
			}
		});

#if !UE_BUILD_SHIPPING
		if (PlayerController->GetHUD() != nullptr && PlayerController->GetHUD()->bShowHUD && FApp::CanEverRender())
		{
			AHUD::OnHUDPostRender.AddUObject(this, &UUASAimAssistComponent::DrawHudDebug);
		}
#endif
	}
}

void UUASAimAssistComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (AimAssistDataAsset == nullptr || !PlayerController.IsValid() || PlayerController->GetPawn() == nullptr)
	{
		return;
	}

	HandleCurrentTarget();
	UpdateCrosshair(DeltaTime);
	UpdateZonesScaling(DeltaTime);
	HandleAutoAim(DeltaTime);

#if !UE_BUILD_SHIPPING
	DrawDebug(DeltaTime);
#endif
}

FRotator UUASAimAssistComponent::GetRotationToCrosshairDirection(const FVector& From, FName TraceProfileName, float Distance) const
{
	if (PlayerController.IsValid())
	{
		FVector worldOrigin;
		FVector worldDirection;

		const auto crosshairPosition = (AimAssistDataAsset && CurrentTargetData.IsValid()) ? CrosshairPosition : GetScreenCenter();
		
		if (UGameplayStatics::DeprojectScreenToWorld(PlayerController.Get(), crosshairPosition, worldOrigin, worldDirection))
		{
			auto target = FVector::ZeroVector;
			if (CurrentTargetData.IsValid() && bMagnetismAreaActive)
			{
				target = worldOrigin + worldDirection * (worldOrigin - CurrentTargetData.GetSocketLocation()).Size();
			}
			else
			{
				FHitResult hitResult;

				UKismetSystemLibrary::LineTraceSingleByProfile(this,
				                                               worldOrigin,
				                                               worldOrigin + worldDirection * Distance,
				                                               TraceProfileName,
				                                               false,
				                                               { GetOwner() },
				                                               EDrawDebugTrace::None,
				                                               hitResult,
				                                               true);

				target = hitResult.bBlockingHit ? hitResult.Location : FVector::ZeroVector;
			}

			if (!target.IsZero())
			{
				return UKismetMathLibrary::FindLookAtRotation(From, target);
			}

			return worldDirection.Rotation();
		}
	}

	return FRotator::ZeroRotator;
}

void UUASAimAssistComponent::SetAimAssistDataAsset(UUASAimAssistConfigDataAsset* DataAsset)
{
	if (PlayerController.IsValid())
	{
		AimAssistDataAsset = DataAsset != nullptr ? DuplicateObject<UUASAimAssistConfigDataAsset>(DataAsset, this) : nullptr;
		OnAimDataAssetChangedDelegate.Broadcast(AimAssistDataAsset);

		GetWorld()->GetTimerManager().ClearTimer(UpdateTargetsTimerHandle);
		
		if (AimAssistDataAsset != nullptr)
		{
			GetWorld()->GetTimerManager().SetTimer(UpdateTargetsTimerHandle, FTimerDelegate::CreateUObject(this, &UUASAimAssistComponent::UpdateAssist), AimAssistDataAsset->UpdateTargetsRate, true);
		}
	
	}
}

void UUASAimAssistComponent::UpdateAssist()
{
	if (AimAssistDataAsset == nullptr || !PlayerController.IsValid() || PlayerController->GetPawn() == nullptr)
	{
		return;
	}
	
	UpdateTargets();
	HandleTargets();
}

bool UUASAimAssistComponent::CanUseAssist() const
{
	if (PlayerController.IsValid() && PlayerController->IsLocalController()
		&& ((PlayerController->GetLocalRole() == ENetRole::ROLE_AutonomousProxy && GetWorld()->GetNetMode() == NM_Client)
		|| (PlayerController->GetLocalRole() == ENetRole::ROLE_Authority && GetWorld()->GetNetMode() != NM_Client && GetWorld()->GetNetMode() != NM_DedicatedServer))
		)
	{
		return true;
	}

	return false;
}

void UUASAimAssistComponent::UpdateTargets()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateTargets);

	TArray<FOverlapResult> overlapResults;
	FCollisionQueryParams collisionParams;
	collisionParams.AddIgnoredActor(PlayerController->GetPawn());

	GetWorld()->OverlapMultiByProfile(
	    overlapResults,
	    GetOverlapLocation(),
	    GetOverlapRotation().Quaternion(),
	    TargetsDetectionProfileName,
	    FCollisionShape::MakeBox(GetOverlapExtents()),
	    collisionParams);

	TArray<TWeakObjectPtr<AActor>> targetsToRemove;
	TargetComponents.GenerateKeyArray(targetsToRemove);

	for (const auto& result : overlapResults)
	{
		if (TargetComponents.Contains(result.GetActor()))
		{
			targetsToRemove.Remove(result.GetActor());
			continue;
		}

		TInlineComponentArray<UUASAimAssistTargetComponent*> targetComponents;
		result.GetActor()->GetComponents(targetComponents);

		if (targetComponents.Num() != 0)
		{
			auto& val = TargetComponents.Add(TWeakObjectPtr<AActor>(result.GetActor()));

			result.GetActor()->OnDestroyed.AddDynamic(this, &UUASAimAssistComponent::OnTargetDestroyed);

			for (const auto component : targetComponents)
			{
				val.Add(component);
			}
		}
	}

	for (const auto actor : targetsToRemove)
	{
		actor->OnDestroyed.RemoveAll(this);
		TargetComponents.Remove(actor);
	}
}

void UUASAimAssistComponent::OnTargetDestroyed(AActor* DestroyedActor)
{
	TargetComponents.Remove(DestroyedActor);
}

void UUASAimAssistComponent::HandleTargets()
{
	SCOPE_CYCLE_COUNTER(STAT_HandleTargets);

	LastTargetData.Empty(TargetComponents.Num());

	const FVector viewLocation = GetCameraLocation();

	for (const auto& target : TargetComponents)
	{
		if (!target.Key.IsValid())
		{
			continue;
		}

		for (const auto component : target.Value)
		{
			if (!component.IsValid() || !component->IsTargetActive())
			{
				continue;
			}

			FHitResult hitResult;

			for (const auto& socketData : component->GetAimTargetSocketLocations())
			{
				UKismetSystemLibrary::LineTraceSingleByProfile(this,
				                                               viewLocation,
				                                               socketData.Location,
				                                               ObstacleCheckProfileName,
				                                               false,
				                                               { GetOwner(), target.Key.Get() },
				                                               bDebugTargetTraces ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None,
				                                               hitResult,
				                                               true);

				if (hitResult.bBlockingHit)
				{
					continue;
				}

				LastTargetData.Add({ component, socketData.SocketName });
			}
		}
	}

	if (LastTargetData.Num() != 0)
	{
		CurrentTargetData = *Algo::MinElementBy(LastTargetData, [&](const FUASAimAssistTargetData& Data) {
			auto location = Data.GetLocationOnTheScreen(PlayerController.Get());
			return (location - GetScreenCenter()).Size();
		});
	}
	else
	{
		CurrentTargetData = {};
	}
}

FVector UUASAimAssistComponent::GetOverlapExtents() const
{
	if (AimAssistDataAsset)
	{
		auto extents = AimAssistDataAsset->AimAreaExtents;
		extents.X *= 0.5f;
		return extents;
	}

	return FVector::ZeroVector;
}

FVector UUASAimAssistComponent::GetOverlapLocation() const
{
	FVector cameraLocation;
	FRotator cameraRotation;

	PlayerController->GetPlayerViewPoint(cameraLocation, cameraRotation);

	return cameraLocation + cameraRotation.Vector() * AimAssistDataAsset->AimAreaExtents.X * 0.5f;
}

FRotator UUASAimAssistComponent::GetOverlapRotation() const
{
	FVector viewLocation;
	FRotator viewRotation;

	PlayerController->GetPlayerViewPoint(viewLocation, viewRotation);

	return viewRotation;
}

FVector UUASAimAssistComponent::GetCameraLocation() const
{
	FVector viewLocation;
	FRotator viewRotation;

	PlayerController->GetPlayerViewPoint(viewLocation, viewRotation);

	return viewLocation;
}

FVector2D UUASAimAssistComponent::GetScreenCenter() const
{
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D size;
		GEngine->GameViewport->GetViewportSize(size);

		return size * 0.5f + (AimAssistDataAsset ? AimAssistDataAsset->CrosshairOffset : FVector2D::ZeroVector);
	}

	return {};
}

void UUASAimAssistComponent::HandleCurrentTarget()
{
	SCOPE_CYCLE_COUNTER(STAT_HandleCurrentTarget);

	bStickinessAreaActive = false;
	bMagnetismAreaActive = false;
	bAutoAimAreaActive = false;

	if (!CurrentTargetData.IsValid() || !PlayerController.IsValid())
	{
		return;
	}

	const auto distance = (GetScreenCenter() - CurrentTargetData.GetLocationOnTheScreen(PlayerController.Get())).Size();

	const auto stickinessRadius = GetScaledZoneRadius(AimAssistDataAsset->StickinessZoneConfig.Radius);
	if (distance <= stickinessRadius && IsStickinessEnabled())
	{
		bStickinessAreaActive = true;
	}

	if (distance <= GetScaledZoneRadius(AimAssistDataAsset->MagnetismZoneConfig.StartRadius) && IsMagtetismEnabled())
	{
		bMagnetismAreaActive = true;
	}

	if (distance <= GetScaledZoneRadius(AimAssistDataAsset->AutoAimConfig.AutoAimZoneRadius)
	    && IsAutoAimEnabled() && (CurrentTargetData.GetSocketLocation() - PlayerController->GetPawn()->GetActorLocation()).Size() <= AimAssistDataAsset->AutoAimConfig.ActivationDistance)
	{
		bAutoAimAreaActive = true;
	}
}

void UUASAimAssistComponent::HandleAutoAim(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_HandleAutoAim);

	if (!CurrentTargetData.IsValid() || PlayerController->GetPawn() == nullptr || !PlayerController.IsValid() || !bAutoAimAreaActive || !IsAutoAimEnabled())
	{
		LastRotationInput = FRotator::ZeroRotator;
		return;
	}

	const auto delta = UKismetMathLibrary::NormalizedDeltaRotator(PlayerController->GetControlRotation(), LastRotationInput);
	if (delta.IsNearlyZero(0.01f))
	{
		TimeWithoutRotationInput += DeltaTime;
	}
	else
	{
		TimeWithoutRotationInput = 0.f;
	}

	LastRotationInput = PlayerController->GetControlRotation();

	if (!PlayerController->GetPawn()->GetMovementComponent()->GetLastInputVector().IsNearlyZero())
	{
		TimeWithMovementInput += DeltaTime;
	}
	else
	{
		TimeWithMovementInput = 0.f;
	}

	auto& timerManager = GetWorld()->GetTimerManager();

	if (!FMath::IsNearlyZero(AimAssistDataAsset->AutoAimConfig.TimeToBlockAfterChangeTarget))
	{
		if (CurrentTargetData.TargetComponent->GetOwner() != LastAutoAimTarget)
		{
			auto prevTarget = LastAutoAimTarget;
			LastAutoAimTarget = CurrentTargetData.TargetComponent->GetOwner();

			timerManager.ClearTimer(WaitAfterChangeTargetToAutoAimTimerHandle);

			if (prevTarget.IsValid())
			{
				timerManager.SetTimer(WaitAfterChangeTargetToAutoAimTimerHandle, AimAssistDataAsset->AutoAimConfig.TimeToBlockAfterChangeTarget, false);
			}
		}
	}

	if (TimeWithoutRotationInput >= AimAssistDataAsset->AutoAimConfig.TimeWithoutCameraInputToEnableAutoAim
	    && TimeWithMovementInput >= AimAssistDataAsset->AutoAimConfig.TimeWithMovementInputToEnableAutoAim
	    && (!AimAssistDataAsset->AutoAimConfig.bUseOnlyWithInactiveMagnetism || (!bMagnetismAreaActive || !IsMagtetismEnabled()))
	    && !timerManager.IsTimerActive(WaitAfterChangeTargetToAutoAimTimerHandle))
	{
		const auto targetLocation = CurrentTargetData.GetSocketLocation();

		auto targetRotation = UKismetMathLibrary::FindLookAtRotation(GetCameraLocation(), targetLocation);

		targetRotation = UKismetMathLibrary::RInterpTo(PlayerController->GetControlRotation(), targetRotation, DeltaTime, AimAssistDataAsset->AutoAimConfig.Speed);

		const auto appliedDelta = UKismetMathLibrary::NormalizedDeltaRotator(PlayerController->GetControlRotation(), targetRotation);
		PlayerController->SetControlRotation(targetRotation);

		LastRotationInput = UKismetMathLibrary::NormalizedDeltaRotator(LastRotationInput, appliedDelta);
	}
}

void UUASAimAssistComponent::UpdateCrosshair(float DeltaTime)
{
	if (!PlayerController.IsValid())
	{
		return;
	}

	FVector2D targetCrosshairPosition = GetScreenCenter();

	if (IsMagtetismEnabled() && bMagnetismAreaActive && CurrentTargetData.IsValid())
	{
		const auto targetLocation = CurrentTargetData.GetLocationOnTheScreen(PlayerController.Get());

		const auto lenght = (targetLocation - targetCrosshairPosition).Size();

		const auto magnetismRadius = GetScaledZoneRadius(AimAssistDataAsset->MagnetismZoneConfig.AimZoneRadius);

		if (lenght > magnetismRadius)
		{
			targetCrosshairPosition = targetCrosshairPosition - (targetCrosshairPosition - targetLocation).GetSafeNormal() * magnetismRadius;
		}
		else
		{
			targetCrosshairPosition = targetLocation;
		}
	}

	CrosshairPosition = targetCrosshairPosition;
	CrosshairPositionDebug = FMath::Vector2DInterpConstantTo(CrosshairPositionDebug, CrosshairPosition, DeltaTime, 350.f);
}

void UUASAimAssistComponent::UpdateZonesScaling(float DeltaTime)
{
	if (!PlayerController.IsValid() || !IsScalingEnabled())
	{
		return;
	}

	float target = 1.f;

	if (CurrentTargetData.IsValid())
	{
		const auto distanceToTarget = (CurrentTargetData.GetSocketLocation() - PlayerController->GetPawn()->GetActorLocation()).Size();
		const auto curveValue = GetCurveValue(AimAssistDataAsset->ZonesScalingConfig.ZonesScalingCurve, distanceToTarget);
		target = curveValue;
	}

	ZonesScaleMultiplier = target;

#if !UE_BUILD_SHIPPING
	ZonesScaleMultiplierDebug = FMath::FInterpConstantTo(ZonesScaleMultiplierDebug, target, DeltaTime, 1.5f);
#endif
}

float UUASAimAssistComponent::GetCurveValue(const FRuntimeFloatCurve& Curve, float Power) const
{
	if (Curve.ExternalCurve != nullptr)
	{
		return Curve.ExternalCurve->GetFloatValue(Power);
	}
	else if (Curve.GetRichCurveConst()->GetNumKeys() != 0)
	{
		return Curve.GetRichCurveConst()->Eval(Power);
	}

	return 1.f;
}

float UUASAimAssistComponent::GetScaledZoneRadius(float From) const
{
	auto val = From;
	
	if (AimAssistDataAsset != nullptr)
	{
		val *= (IsScalingEnabled() ? ZonesScaleMultiplier : 1.f);
	}

	if (bScalingByDPI && GEngine && GEngine->GameViewport)
	{
		val *= GEngine->GameViewport->GetDPIScale();
	}
	
	return val;
}

void UUASAimAssistComponent::GetControlMultipliers(float& Pitch, float& Yaw) const
{
	if (!CurrentTargetData.IsValid() || !PlayerController.IsValid() || !IsStickinessEnabled() || !bStickinessAreaActive)
	{
		Pitch = 1.f;
		Yaw = 1.f;
		return;
	}

	const auto distance = (GetScreenCenter() - CurrentTargetData.GetLocationOnTheScreen(PlayerController.Get())).Size();

	const auto stickinessRadius = GetScaledZoneRadius(AimAssistDataAsset->StickinessZoneConfig.Radius);

	const auto power = 1.f - distance / stickinessRadius;

	auto multiplierPitch = GetCurveValue(AimAssistDataAsset->StickinessZoneConfig.StickinessMultiplierCurvePitch, power);
	auto multiplierYaw = GetCurveValue(AimAssistDataAsset->StickinessZoneConfig.StickinessMultiplierCurveYaw, power);

	multiplierPitch *= AimAssistDataAsset->StickinessZoneConfig.Multiplier;
	multiplierYaw *= AimAssistDataAsset->StickinessZoneConfig.Multiplier;

	Pitch = multiplierPitch;
	Yaw = multiplierYaw;
}

bool UUASAimAssistComponent::IsMagtetismEnabled() const
{
	if (AimAssistDataAsset != nullptr)
	{
		return bEnableMagnetism && AimAssistDataAsset->bMagnetismZoneConfig;
	}

	return false;
}

bool UUASAimAssistComponent::IsStickinessEnabled() const
{
	if (AimAssistDataAsset != nullptr)
	{
		return bEnableStickiness && AimAssistDataAsset->bStickinessZoneConfig;
	}

	return false;
}

bool UUASAimAssistComponent::IsScalingEnabled() const
{
	if (AimAssistDataAsset != nullptr)
	{
		return bEnableScaling && AimAssistDataAsset->bScalingZoneConfig;
	}

	return false;
}

bool UUASAimAssistComponent::IsAutoAimEnabled() const
{
	if (AimAssistDataAsset != nullptr)
	{
		return bEnableAutoAim && AimAssistDataAsset->bAutoAimConfig;
	}

	return false;
}

#if !UE_BUILD_SHIPPING
float UUASAimAssistComponent::GetScaledZoneRadiusForDebug(float From) const
{
	auto val = From;
	
	if (AimAssistDataAsset != nullptr)
	{
		 val *= (IsScalingEnabled() ? ZonesScaleMultiplierDebug : 1.f);
	}

	if (bScalingByDPI && GEngine && GEngine->GameViewport)
	{
		val *= GEngine->GameViewport->GetDPIScale();
	}
	
	return val;
}

void UUASAimAssistComponent::DrawHudDebug(AHUD* HUD, UCanvas* Canvas) const
{
	if (!PlayerController.IsValid() || AimAssistDataAsset == nullptr || HUD == nullptr || PlayerController->GetPawn() == nullptr || HUD->GetOwner() != PlayerController)
	{
		return;
	}
	
	if (bDrawCrosshair)
	{
		DrawCrosshair(HUD, CrosshairPositionDebug);
	}

	if (!bDrawCircles)
	{
		return;
	}

	if (IsStickinessEnabled())
	{
		DrawCircle(HUD, GetScaledZoneRadiusForDebug(AimAssistDataAsset->StickinessZoneConfig.Radius), 2.f, FColor::Purple);
	}

	if (IsMagtetismEnabled())
	{
		DrawCircle(HUD, GetScaledZoneRadiusForDebug(AimAssistDataAsset->MagnetismZoneConfig.StartRadius), 2.f, FColor::Cyan);
		DrawCircle(HUD, GetScaledZoneRadiusForDebug(AimAssistDataAsset->MagnetismZoneConfig.AimZoneRadius), 2.f, FColor::Blue);
	}

	if (IsAutoAimEnabled())
	{
		DrawCircle(HUD, GetScaledZoneRadiusForDebug(AimAssistDataAsset->AutoAimConfig.AutoAimZoneRadius), bAutoAimAreaActive ? 4.f : 2.f, FColor::Red);
	}
}

void UUASAimAssistComponent::DrawCircle(AHUD* HUD, float Radius, float LineThickness, FColor Color) const
{
	const FVector2D center = GetScreenCenter();

	auto numSides = 30;

	const float AngleDelta = 2.0f * PI / numSides;
	FVector2D AxisX(1.f, 0.f);
	FVector2D AxisY(0.f, -1.f);
	FVector2D LastVertex = center + AxisX * Radius;
	for (int32 SideIndex = 0; SideIndex < numSides; SideIndex++)
	{
		const FVector2D Vertex = center + (AxisX * FMath::Cos(AngleDelta * (SideIndex + 1)) + AxisY * FMath::Sin(AngleDelta * (SideIndex + 1))) * Radius;
		HUD->DrawLine(LastVertex.X, LastVertex.Y, Vertex.X, Vertex.Y, Color, LineThickness);
		LastVertex = Vertex;
	}
}

void UUASAimAssistComponent::DrawCrosshair(AHUD* HUD, const FVector2D& Position) const
{
	const int32 size = 10.f;

	HUD->DrawLine(Position.X - size, Position.Y, Position.X + size, Position.Y, FColor::Red, 2.f);
	HUD->DrawLine(Position.X, Position.Y - size, Position.X, Position.Y + size, FColor::Red, 2.f);
}

void UUASAimAssistComponent::DrawDebug(float DeltaTime) const
{
	if (bDebugOverlayBox)
	{
		::DrawDebugBox(GetWorld(), GetOverlapLocation(), GetOverlapExtents(), GetOverlapRotation().Quaternion(), FColor::Orange, false, 0.f, 0, 1.5f);
	}

	if (bShowValidTargetSockets)
	{
		for (const auto& data : LastTargetData)
		{
			FColor color = FColor::Red;

			if (data == CurrentTargetData)
			{
				if (bMagnetismAreaActive)
				{
					color = FColor::Blue;
				}
				else if (bStickinessAreaActive)
				{
					color = FColor::Purple;
				}
			}

			::DrawDebugSphere(GetWorld(), data.GetSocketLocation(), 10.f, 5, color, false, 0.f, 1, 1.f);
		}
	}
}
#endif
