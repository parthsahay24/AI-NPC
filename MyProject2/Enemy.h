// Enemy.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Perception/PawnSensingComponent.h"
#include "Animation/AnimMontage.h"
#include "CharacterState.h"
#include "Enemy.generated.h"

class UHealthBar;
class UAttributeComponent;
class AWeapon;
class UPawnSensingComponent;

UCLASS()
class MYPROJECT2_API AEnemy : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemy();

	/** AActor overrides */
	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	virtual void Destroyed() override;

	/** Hit interface */
	virtual void GetHit_Implementation(const FVector& ImpactPoint, AActor* Hitter);

protected:
	virtual void BeginPlay() override;

	/** Enemy-specific */
	void Die();
	virtual void Die_Implementation(); // called when dying
	virtual void Attack();
	virtual bool CanAttack();

	virtual void HandleDamage(float DamageAmount);

	/* State */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "State")
	EEnemyState EnemyState = EEnemyState::EES_Patrolling;

private:
	/** AI / Behavior helpers */
	void InitializeEnemy();
	void CheckPatrolTarget();
	void CheckCombatTarget();
	void PatrolTimerFinished();
	void HideHealthBar();
	void ShowHealthBar();
	void LoseInterest();
	void StartPatrolling();
	void ChaseTarget();
	bool IsOutsideCombatRadius();
	bool IsOutsideAttackRadius();
	bool IsInsideAttackRadius();
	bool IsChasing();
	bool IsAttacking();
	bool IsDead();
	bool IsEngaged();
	void ClearPatrolTimer();
	void StartAttackTimer();
	void ClearAttackTimer();
	bool InTargetRange(AActor* Target, double Radius);
	void MoveToTarget(AActor* Target);
	AActor* ChoosePatrolTarget();
	void SpawnDefaultWeapon();

	UFUNCTION()
	void PawnSeen(APawn* SeenPawn);

	/* Components */
	UPROPERTY(VisibleAnywhere)
	UHealthBar* HealthBarWidget;

	UPROPERTY(VisibleAnywhere)
	UPawnSensingComponent* PawnSensing;

	UPROPERTY(VisibleAnywhere)
	UAttributeComponent* Attributes;

	UPROPERTY(EditAnywhere, Category = Combat)
	TSubclassOf<class AWeapon> WeaponClass;

	UPROPERTY()
	class AAIController* EnemyController;

	// Patrol / Navigation
	UPROPERTY(EditInstanceOnly, Category = "AI Navigation")
	AActor* PatrolTarget;

	UPROPERTY(EditInstanceOnly, Category = "AI Navigation")
	TArray<AActor*> PatrolTargets;

	UPROPERTY(EditAnywhere)
	double PatrolRadius = 200.f;

	FTimerHandle PatrolTimer;

	UPROPERTY(EditAnywhere, Category = "AI Navigation")
	float PatrolWaitMin = 5.f;

	UPROPERTY(EditAnywhere, Category = "AI Navigation")
	float PatrolWaitMax = 10.f;

	UPROPERTY(EditAnywhere, Category = Combat)
	float PatrollingSpeed = 125.f;

	FTimerHandle AttackTimer;

	UPROPERTY(EditAnywhere, Category = Combat)
	float AttackMin = 0.5f;

	UPROPERTY(EditAnywhere, Category = Combat)
	float AttackMax = 1.f;

	UPROPERTY(EditAnywhere, Category = Combat)
	float ChasingSpeed = 300.f;

	UPROPERTY(EditAnywhere, Category = Combat)
	float DeathLifeSpan = 8.f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float CombatRadius = 500.f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float AttackRadius = 150.f;

	UPROPERTY(EditAnywhere, Category = "AI|Movement")
	float AcceptanceRadius = 50.f;

	UPROPERTY(VisibleAnywhere)
	AWeapon* EquippedWeapon;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void AttackEnd();

	// Animations
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	UAnimMontage* AttackMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	UAnimMontage* HitReactMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	UAnimMontage* DeathMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	TArray<FName> AttackMontageSections;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (AllowPrivateAccess = "true"))
	TArray<FName> DeathMontageSections;

	// Effects
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	UParticleSystem* HitParticles;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	USoundBase* HitSound;

	// Combat target (player)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	APawn* CombatTarget;

	// --- ?? ADDED FOR LATENCY SUPERVISION ---
	// Enemy.h (inside class AEnemy)
protected:
	// Latency / velocity smoothing parameters (tweak as needed)
	UPROPERTY(EditAnywhere, Category = "Latency")
	float MaxLatencyVelocity = 1200.0f;        // clamp raw velocity to this

	UPROPERTY(EditAnywhere, Category = "Latency")
	float LatencyVelocityAlpha = 0.08f;        // smoothing factor [0..1] (smaller = smoother)

	// Runtime values (not exposed)
	float SmoothedLatencyVelocity = 0.0f;
	float RawLatencyVelocity = 0.0f;

private:
	/** CSV logging for supervised training */
	void InitLogger();
	/** EventType, Damage optional, DeltaTime optional (pass Tick DeltaTime when calling from Tick) */
	void LogEnemyEvent(const FString& EventType, float Damage = 0.f, float DeltaTime = 0.f);

	/** shared CSV file for all enemies */
	static FString GEnemyLogFilePath;

	/** Previous frame's Latency (distance) value */
	float PrevLatency = -1.f;

	/** Time accumulator for throttled perception logging */
	float PerceptionLogAccumulator = 0.f;

	/** How often to log perception/latency data (seconds) */
	UPROPERTY(EditAnywhere, Category = "Logging")
	float PerceptionLogInterval = 0.1f;

	/** Clamp threshold for ?Latency (cm/s) to avoid physics spikes */
	UPROPERTY(EditAnywhere, Category = "Logging")
	float MaxReasonableDelta = 2000.f;

	/** Movement threshold (cm/s) — if target approaches faster, mark as Approaching */
	UPROPERTY(EditAnywhere, Category = "Logging")
	float ApproachSpeedThreshold = 150.f;
	// --- ?? END LATENCY ADDITIONS ---

private:
	/** Helpers from BaseCharacter */
	void PlayHitReactMontage(const FName& SectionName);
	void DirectionalHitReact(const FVector& ImpactPoint);
	void PlayHitSound(const FVector& ImpactPoint);
	void SpawnHitParticles(const FVector& ImpactPoint);
	void HandleDamage_BaseCharacter(float DamageAmount);
	int32 PlayRandomMontageSection(UAnimMontage* Montage, const TArray<FName>& SectionNames);
	int32 PlayAttackMontage();
	int32 PlayDeathMontage();
	void StopAttackMontage();
	void SetWeaponCollisionEnabled(ECollisionEnabled::Type CollisionEnabled);

public:
	// Debug helpers
	bool IsAlive() const;
	void DisableCapsule();
	void DisableMeshCollision();
};
