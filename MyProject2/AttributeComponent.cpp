// Fill out your copyright notice in the Description page of Project Settings.

#include "AttributeComponent.h"

// Sets default values for this component's properties
UAttributeComponent::UAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

// Called when the game starts
void UAttributeComponent::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void UAttributeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Auto stamina regeneration
	StaminaRegenerate(DeltaTime);
}

/* ---------------- Health ---------------- */
void UAttributeComponent::ReceiveDamage(float Damage)
{
	Health = FMath::Clamp(Health - Damage, 0.f, MaxHealth);
	UE_LOG(LogTemp, Warning, TEXT("Damage Received: %.2f | Current Health: %.2f"), Damage, Health);
}

float UAttributeComponent::GetHealth() const { return Health; }
float UAttributeComponent::GetMaxHealth() const { return MaxHealth; }
float UAttributeComponent::GetHealthPercent() const { return MaxHealth > 0 ? Health / MaxHealth : 0.f; }
bool UAttributeComponent::IsAlive() const { return Health > 0.f; }

/* ---------------- Stamina ---------------- */
float UAttributeComponent::GetStamina() const { return Stamina; }
float UAttributeComponent::GetMaxStamina() const { return MaxStamina; }
float UAttributeComponent::GetStaminaPercent() const { return MaxStamina > 0 ? Stamina / MaxStamina : 0.f; }
bool UAttributeComponent::HasStamina() const { return Stamina > 0.f; }

void UAttributeComponent::ReduceStamina(float StaminaAmount)
{
	Stamina = FMath::Clamp(Stamina - StaminaAmount, 0.f, MaxStamina);
}

bool UAttributeComponent::HasEnoughStamina(float RequiredAmount) const
{
	return Stamina >= RequiredAmount;
}

void UAttributeComponent::StaminaRegenerate(float DeltaTime)
{
	Stamina = FMath::Clamp(Stamina + (StaminaRegenRate * DeltaTime), 0.f, MaxStamina);
}

/* ---------------- Attributes ---------------- */
void UAttributeComponent::AddGold(int32 GoldAmount) { Gold += GoldAmount; }
void UAttributeComponent::AddSoul(int32 SoulAmount) { Soul += SoulAmount; }
int32 UAttributeComponent::GetGold() const { return Gold; }
int32 UAttributeComponent::GetSoul() const { return Soul; }
