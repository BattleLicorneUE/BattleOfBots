// Copyright 2022 Dmitriy Vergasov All Rights Reserved.

#include "UASAimAssistTargetComponent.h"

#include "Components/MeshComponent.h"

UUASAimAssistTargetComponent::UUASAimAssistTargetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UUASAimAssistTargetComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UUASAimAssistTargetComponent::Init(UMeshComponent* Mesh)
{
	MeshComponent = Mesh;
}

TArray<FUASSocketData> UUASAimAssistTargetComponent::GetAimTargetSocketLocations() const
{
	TArray<FUASSocketData> locations;

	if (IsTargetActive())
	{
		for (auto& socket : AimTargetSocketNames)
		{
			locations.Add({ socket, MeshComponent->GetSocketLocation(socket) });
		}
	}

	return locations;
}

bool UUASAimAssistTargetComponent::IsTargetActive() const
{
	return MeshComponent != nullptr && bIsAimAssistActive;
}

UMeshComponent* UUASAimAssistTargetComponent::GetMesh() const
{
	return MeshComponent;
}
