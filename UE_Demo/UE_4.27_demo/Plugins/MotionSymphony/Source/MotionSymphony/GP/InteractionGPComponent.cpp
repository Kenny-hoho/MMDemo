// Fill out your copyright notice in the Description page of Project Settings.


#include "InteractionGPComponent.h"
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

	AActor* OwnerActor = GetOwner();
	FVector ActorLocation= OwnerActor->GetActorLocation();
	FHitResult HitResult;
	TArray<AActor*> MySelf;
	MySelf.Add(OwnerActor);
	bool bHit = UKismetSystemLibrary::SphereTraceSingle(this, ActorLocation, ActorLocation, 400, UEngineTypes::ConvertToTraceType(TraceChannel),false, MySelf, EDrawDebugTrace::None,HitResult,true);
	if (bHit)
	{
		FVector ActorForward = FVector{ OwnerActor->GetActorForwardVector().X ,OwnerActor->GetActorForwardVector().Y ,0 };
		FVector Dist = HitResult.Actor.Get()->GetActorLocation() - ActorLocation;
		FVector NormalizeDist = UKismetMathLibrary::Normal(Dist);
		FVector NormalizeDistXY = FVector{ NormalizeDist.X , NormalizeDist.Y , 0 };
		float Deg= UKismetMathLibrary::DegAcos(UKismetMathLibrary::Dot_VectorVector(NormalizeDistXY,ActorForward));
		if (Deg <= 45.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("%f"), Deg);
		}
		
	}
	else
	{

	}

	// ...
}

