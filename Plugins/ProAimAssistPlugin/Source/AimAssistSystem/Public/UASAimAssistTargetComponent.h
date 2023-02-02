// Copyright 2022 Dmitriy Vergasov All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "UASAimAssistTargetComponent.generated.h"

USTRUCT()
struct AIMASSISTSYSTEM_API FUASSocketData
{
	GENERATED_BODY()
public:
	FName SocketName;
	FVector Location;
};

class UMeshComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AIMASSISTSYSTEM_API UUASAimAssistTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUASAimAssistTargetComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "AimAssistTargetComponent")
	void Init(UMeshComponent* Mesh);

	TArray<FUASSocketData> GetAimTargetSocketLocations() const;

	void SetAimAssistTargetActive(bool bValue) { bIsAimAssistActive = bValue; };

	bool IsTargetActive() const;

	UMeshComponent* GetMesh() const;

protected:
	UPROPERTY()
	UMeshComponent* MeshComponent;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AimAssistTargetComponent")
	bool bIsAimAssistActive = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AimAssistTargetComponent")
	TArray<FName> AimTargetSocketNames;
};
