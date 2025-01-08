// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SplineComponent.h"
#include "PathFollowMatching.generated.h"


UCLASS(ClassGroup = (Custom), Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent))
class MOTIONSYMPHONY_API UPathFollowMatching : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UPathFollowMatching();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PathFollow")
	AActor* SplineActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PathFollow")
	USplineComponent* SplineComp;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
};
