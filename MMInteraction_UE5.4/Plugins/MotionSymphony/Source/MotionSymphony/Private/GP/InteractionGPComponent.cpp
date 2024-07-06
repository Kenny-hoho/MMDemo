// Fill out your copyright notice in the Description page of Project Settings.


#include "GP/InteractionGPComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#define TraceChannel ECC_GameTraceChannel1

// Sets default values for this component's properties
UInteractionGPComponent::UInteractionGPComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UInteractionGPComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UInteractionGPComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

const FVector UInteractionGPComponent::GetCurrentInteractPointLocation() const
{
	if (CurrentHitActor) {
		return InteractPointLocation;
	}
	return FVector();
}

void UInteractionGPComponent::SetCurrentInteractPointLocation(FVector inLocation)
{
	// 需要把传入地址转换到角色的根骨骼的坐标系
	InteractPointLocation = inLocation;
}

void UInteractionGPComponent::ActiveInteract_Implementation(FVector InLocation, AActor* InActor)
{
	UE_LOG(LogTemp, Warning, TEXT("ActiveInteract_Implementation!"));
}


