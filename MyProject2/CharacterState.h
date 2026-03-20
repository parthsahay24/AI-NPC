// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"


UENUM(BlueprintType)
enum class EEnemyAction : uint8
{
	Idle    UMETA(DisplayName = "Idle"),
	Chase   UMETA(DisplayName = "Chase"),
	Attack  UMETA(DisplayName = "Attack"),
	Retreat UMETA(DisplayName = "Retreat"),
	EES_NoState       UMETA(DisplayName = "NoState"),
	EES_Dead          UMETA(DisplayName = "Dead")
};
UENUM(BlueprintType)
enum class EEnemyState :uint8
{
	EES_Death UMETA(DisplayName = "Death"),
	EES_Patrolling  UMETA(DisplayName = "Patrollin"),
	EES_Chasing  UMETA(DisplayName = "Chasing"),
	EES_Attacking  UMETA(DisplayName = "Attacking"),
	EES_Retreat UMETA(DisplayName = "Retreat"),
	EES_Engaged UMETA(DisplayName = "Enagaged"),
	EES_NoState UMETA(DisplayName = "No State")
};