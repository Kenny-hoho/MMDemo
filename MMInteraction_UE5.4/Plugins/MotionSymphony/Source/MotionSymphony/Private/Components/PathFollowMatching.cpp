// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/PathFollowMatching.h"

// Sets default values for this component's properties
UPathFollowMatching::UPathFollowMatching()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UPathFollowMatching::BeginPlay()
{
	Super::BeginPlay();

	// ...
	if (SplineActor) {
		if (USplineComponent* Comp = SplineActor->GetComponentByClass<USplineComponent>()) {
			SplineComp = Comp;
		}
	}
}


// Called every frame
void UPathFollowMatching::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

