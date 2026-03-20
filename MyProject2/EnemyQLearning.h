#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Perception/PawnSensingComponent.h"
#include "Components/WidgetComponent.h"
#include "CharacterState.h"
#include "AIController.h"
#include "EnemyQLearningComponent1.h"
#include "EnemyQLearning.generated.h"

class UAttributeComponent;
class AWeapon;
class USoundBase;
class UParticleSystem;
class UEnemyQLearningComponent1;
class UHealthBar;

UCLASS()
class MYPROJECT2_API AEnemyQLearning : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemyQLearning();

protected:
	virtual void BeginPlay() override;

	/* Components */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UAttributeComponent* AttributeComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UPawnSensingComponent* PawnSensingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UHealthBar* Healthbar;

	/* Q-Learning brain */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UEnemyQLearningComponent1* QLearningComp;

	/* Weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TSubclassOf<AWeapon> WeaponClass;

	UPROPERTY()
	AWeapon* EquipWeapon;

	/* AI */
	UPROPERTY()
	AAIController* EnemyController;

	/* State & target */
	UPROPERTY(BlueprintReadOnly)
	EEnemyAction EnemyState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	APawn* CombatTarget;

	/* Anim montages */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
	UAnimMontage* AttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
	TArray<FName> AttackMontageSections;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
	UAnimMontage* HitReactMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
	UAnimMontage* DeathMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
	TArray<FName> DeathMontageSections;

	/* FX */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FX")
	USoundBase* HitSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FX")
	UParticleSystem* HitParticles;

	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float AttackRange = 200.f;

	
	float SurvivalTimer = 0.f;

	
	void EnsureQLearningTarget();
	void ChooseAndPerformQLAction();

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// Damage interface
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	// Helper functions
	UFUNCTION(BlueprintCallable)
	bool IsDead() const;

	UFUNCTION(BlueprintCallable)
	void HandleDamage(float DamageAmount);

	UFUNCTION(BlueprintCallable)
	void Die();

	UFUNCTION(BlueprintCallable)
	bool IsOutsideAttackRadius() const;

	UFUNCTION(BlueprintCallable)
	void ChaseTarget();

	UFUNCTION(BlueprintCallable)
	void MoveToTarget(APawn* TargetPawn);

	UFUNCTION(BlueprintCallable)
	void ShowHealthBar();

	UFUNCTION(BlueprintCallable)
	void PlayHitSound(const FVector& Location);

	UFUNCTION(BlueprintCallable)
	void SpawnHitParticle(const FVector& Location);

	UFUNCTION(BlueprintCallable)
	void GetHit_Implementation(const FVector& HitImpact, AActor* Hitter);

	// ---------------- Q-Learning Hooks ----------------
	void RegisterSurvival(float DeltaTime);
	void RegisterHitReward(bool bSuccess);
	void RegisterDeathPenalty();
};
