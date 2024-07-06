// Fill out your copyright notice in the Description page of Project Settings.


#include "GP/InteractActor.h"

// Sets default values
AInteractActor::AInteractActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AInteractActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AInteractActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AInteractActor::InteractPrepare_Implementation(AActor* Investigator)
{
	UE_LOG(LogTemp, Warning, TEXT("InteractPrepare_Implementation!"));
}

void AInteractActor::PassiveInteract_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("PassiveInteract_Implementation!"));
}


