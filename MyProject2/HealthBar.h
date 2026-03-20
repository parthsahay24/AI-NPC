// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "HealthBar.generated.h"

/**
 * 
 */
UCLASS()
class MYPROJECT2_API UHealthBar : public UWidgetComponent
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
	void SetPercent(float percent);
public:	
	UPROPERTY()
	class UHealthBarUW1* HealthBarUW;
};
