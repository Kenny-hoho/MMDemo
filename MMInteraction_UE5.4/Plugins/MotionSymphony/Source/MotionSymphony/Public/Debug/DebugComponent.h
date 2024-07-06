// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MotionMatchingDebugInfo.h"
#include "DebugComponent.generated.h"


UCLASS(BlueprintType, Blueprintable, Category = "Motion Matching", meta = (BlueprintSpawnableComponent))
class MOTIONSYMPHONY_API UDebugComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UDebugComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	AMotionMatchingDebugInfo* MMDebugInfo;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
};
