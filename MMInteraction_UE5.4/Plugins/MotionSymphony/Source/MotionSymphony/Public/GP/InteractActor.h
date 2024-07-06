// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface_Interact.h"
#include "InteractActor.generated.h"

UCLASS(Blueprintable, BlueprintType, Category = "Interaction")
class MOTIONSYMPHONY_API AInteractActor : public AActor, public IInterface_Interact
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AInteractActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	void InteractPrepare_Implementation(AActor* Investigator);

	UFUNCTION(BlueprintCallable)
	void PassiveInteract_Implementation();

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector MoveToLocation;
};
