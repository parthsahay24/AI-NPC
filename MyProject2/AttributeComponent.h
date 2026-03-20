// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttributeComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MYPROJECT2_API UAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Constructor
	UAttributeComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/* ---------------- Health ---------------- */
	void ReceiveDamage(float Damage);
	float GetHealth() const;
	float GetMaxHealth() const;
	float GetHealthPercent() const;
	bool IsAlive() const;

	/* ---------------- Stamina ---------------- */
	float GetStamina() const;
	float GetMaxStamina() const;
	float GetStaminaPercent() const;
	bool HasStamina() const;
	void ReduceStamina(float StaminaAmount);
	bool HasEnoughStamina(float RequiredAmount) const;
	void StaminaRegenerate(float DeltaTime);

	/* ---------------- Attributes ---------------- */
	void AddGold(int32 GoldAmount);
	void AddSoul(int32 SoulAmount);
	int32 GetGold() const;
	int32 GetSoul() const;

private:
	// Health
	UPROPERTY(EditAnywhere, Category = "Player|Health")
	float Health = 100.f;

	UPROPERTY(EditAnywhere, Category = "Player|Health")
	float MaxHealth = 100.f;

	// Stamina
	UPROPERTY(EditAnywhere, Category = "Player|Stamina")
	float Stamina = 100.f;

	UPROPERTY(EditAnywhere, Category = "Player|Stamina")
	float MaxStamina = 100.f;

	UPROPERTY(EditAnywhere, Category = "Player|Stamina")
	float StaminaRegenRate = 5.f;

	// Extra attributes
	UPROPERTY(EditAnywhere, Category = "Player|Attributes")
	int32 Gold = 0;

	UPROPERTY(EditAnywhere, Category = "Player|Attributes")
	int32 Soul = 0;
};
