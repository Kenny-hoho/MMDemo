// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interface_Interact.h"
#include "InteractionGPComponent.generated.h"


UCLASS( ClassGroup=(Custom), Blueprintable, BlueprintType, meta=(BlueprintSpawnableComponent) )
class MOTIONSYMPHONY_API UInteractionGPComponent : public UActorComponent, public IInterface_Interact
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UInteractionGPComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	AActor* CurrentHitActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FVector InteractPointLocation;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable)
	const FVector GetCurrentInteractPointLocation() const;

	UFUNCTION(BlueprintCallable)
	void SetCurrentInteractPointLocation(FVector inLocation);

	UFUNCTION(BlueprintCallable)
	void ActiveInteract_Implementation(FVector InLocation, AActor* InActor);
};
