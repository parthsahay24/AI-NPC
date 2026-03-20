// Enemy.cpp
#include "Enemy.h"
#include "AIController.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Perception/PawnSensingComponent.h"
#include "AttributeComponent.h"
#include "HealthBarUW.h"
#include "Weapon.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "HealthBar.h"
#include "Components/BoxComponent.h"
#include "CharacterState.h"
#include "Navigation/PathFollowingComponent.h"
#include "Components/CapsuleComponent.h"
#define LOCTEXT_NAMESPACE "Enemy"

// static member definition
FString AEnemy::GEnemyLogFilePath;

/* ---------- Constructor ---------- */
AEnemy::AEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create attribute component (copied from BaseCharacter)
	Attributes = CreateDefaultSubobject<UAttributeComponent>(TEXT("Attributes"));

	// Health bar component (teacher uses UHealthBarComponent)
	HealthBarWidget = CreateDefaultSubobject<UHealthBar>(TEXT("HealthBar"));
	HealthBarWidget->SetupAttachment(GetRootComponent());

	// Pawn sensing
	PawnSensing = CreateDefaultSubobject<UPawnSensingComponent>(TEXT("PawnSensing"));
	if (PawnSensing)
	{
		PawnSensing->SightRadius = 4000.f;
		PawnSensing->SetPeripheralVisionAngle(45.f);
	}

	// Movement defaults
	GetCharacterMovement()->bOrientRotationToMovement = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// initial state: use the real enum
	EnemyState = EEnemyState::EES_Patrolling;

	PatrolRadius = 200.f;
	PatrolWaitMin = 5.f;
	PatrolWaitMax = 10.f;
	PatrollingSpeed = 125.f;
	AttackMin = 0.5f;
	AttackMax = 1.f;
	ChasingSpeed = 300.f;
	DeathLifeSpan = 8.f;

	// --- Latency / logging initialization ---
	PrevLatency = -1.f;
	PerceptionLogAccumulator = 0.f;
	// PerceptionLogInterval, MaxReasonableDelta, ApproachSpeedThreshold are set from header defaults (editable in editor)
}

/* ---------- Logger (recording) ---------- */
void AEnemy::InitLogger()
{
	if (!GEnemyLogFilePath.IsEmpty()) return;

	FString Dir = FPaths::ProjectSavedDir() / TEXT("TrainingLogs");
	IFileManager::Get().MakeDirectory(*Dir, true);
	GEnemyLogFilePath = Dir / TEXT("EnemyTrainingLog.csv");

	if (!FPaths::FileExists(GEnemyLogFilePath))
	{
		// Header now includes State column
		FString Header = TEXT("Timestamp,EnemyName,Event,State,HealthPercent,Distance,Latency,DeltaLatency,Damage\n");
		FFileHelper::SaveStringToFile(Header, *GEnemyLogFilePath);
	}
}

void AEnemy::LogEnemyEvent(const FString& EventType, float Damage, float DeltaTime)
{
	// DeltaTime here should be the accumulated time over which you compute velocity (e.g., PerceptionLogAccumulator)
	float dt = DeltaTime; // passed into LogEnemyEvent

	// compute current distance
	const bool bHasTarget = (CombatTarget != nullptr);
	float CurrDistance = bHasTarget ? FVector::Dist(GetActorLocation(), CombatTarget->GetActorLocation()) : 0.f;

	// compute raw latency velocity safely
	float rawVel = 0.f;
	if (bHasTarget && PrevLatency >= 0.f && dt > KINDA_SMALL_NUMBER)
	{
		rawVel = (PrevLatency - CurrDistance) / dt; // positive => closing
	}
	else
	{
		rawVel = 0.f;
	}

	// clamp outliers
	if (FMath::Abs(rawVel) > MaxLatencyVelocity)
	{
		rawVel = FMath::Clamp(rawVel, -MaxLatencyVelocity, MaxLatencyVelocity);
	}

	// smoothing (EMA)
	SmoothedLatencyVelocity = LatencyVelocityAlpha * rawVel + (1.0f - LatencyVelocityAlpha) * SmoothedLatencyVelocity;

	// set boolean
	bool bIsApproaching = (SmoothedLatencyVelocity > ApproachSpeedThreshold);

	// save into class members and CSV as needed
	RawLatencyVelocity = rawVel;
	PrevLatency = bHasTarget ? CurrDistance : -1.f;

}

/* ---------- BeginPlay ---------- */
void AEnemy::BeginPlay()
{
	Super::BeginPlay();

	// Init logger for supervised recording
	InitLogger();

	if (PawnSensing) PawnSensing->OnSeePawn.AddDynamic(this, &AEnemy::PawnSeen);
	InitializeEnemy();
	Tags.Add(FName("Enemy"));
}

/* ---------- Tick ---------- */
void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (IsDead()) return;

	// Throttle perception logging to reduce I/O and compute ΔLatency over the interval
	PerceptionLogAccumulator += DeltaTime;
	if (PerceptionLogAccumulator >= PerceptionLogInterval)
	{
		// Log only when there is a combat target (avoids many zero-distance rows)
		if (CombatTarget)
		{
			// pass the accumulated time so ΔLatency is computed correctly over the interval
			LogEnemyEvent(TEXT("PerceptionTick"), 0.f, PerceptionLogAccumulator);
		}
		PerceptionLogAccumulator = 0.f;
	}

	if (EnemyState > EEnemyState::EES_Patrolling)
	{
		CheckCombatTarget();
	}
	else
	{
		CheckPatrolTarget();
	}
}


float AEnemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	HandleDamage(DamageAmount);

	// Log damage taken
	LogEnemyEvent(TEXT("DamageTaken"), DamageAmount, 0.f);

	// teacher sets CombatTarget to the instigator pawn
	if (EventInstigator)
	{
		CombatTarget = EventInstigator->GetPawn();

		// Initialize PrevLatency immediately to avoid a large Δ on next tick
		if (CombatTarget)
		{
			PrevLatency = FVector::Dist(GetActorLocation(), CombatTarget->GetActorLocation());
		}
	}

	// change state depending on range
	if (IsInsideAttackRadius())
	{
		EnemyState = EEnemyState::EES_Attacking;
	}
	else if (IsOutsideAttackRadius())
	{
		ChaseTarget();
	}

	return DamageAmount;
}

void AEnemy::HandleDamage(float DamageAmount)
{
	// call BaseCharacter-like handler and update healthbar
	HandleDamage_BaseCharacter(DamageAmount);

	if (Attributes && HealthBarWidget)
	{
		HealthBarWidget->SetPercent(Attributes->GetHealthPercent());
	}
	if (Attributes && !Attributes->IsAlive())
	{
		Die();
		return; // stop further actions
	}
}

void AEnemy::HandleDamage_BaseCharacter(float DamageAmount)
{
	if (Attributes)
	{
		Attributes->ReceiveDamage(DamageAmount);
	}
}

/* ---------- Destroyed ---------- */
void AEnemy::Destroyed()
{
	// log final cleanup
	LogEnemyEvent(TEXT("Destroyed"), 0.f, 0.f);

	if (EquippedWeapon)
	{
		EquippedWeapon->Destroy();
	}
	Super::Destroyed();
}

/* ---------- Hit / GetHit ---------- */
void AEnemy::GetHit_Implementation(const FVector& ImpactPoint, AActor* Hitter)
{
	// replicate teacher behavior
	if (!IsDead()) ShowHealthBar();
	ClearPatrolTimer();
	ClearAttackTimer();
	SetWeaponCollisionEnabled(ECollisionEnabled::NoCollision);

	StopAttackMontage();
	DirectionalHitReact(ImpactPoint);
	PlayHitSound(ImpactPoint);
	SpawnHitParticles(ImpactPoint);

	if (IsInsideAttackRadius())
	{
		if (!IsDead()) StartAttackTimer();
	}
}

void AEnemy::Die()
{
	LogEnemyEvent(TEXT("DeathStart"), 0.f, 0.f);
	EnemyState = EEnemyState::EES_Death;
	ClearAttackTimer();
	HideHealthBar();
	DisableCapsule();
	SetLifeSpan(DeathLifeSpan);
	GetCharacterMovement()->bOrientRotationToMovement = false;
	SetWeaponCollisionEnabled(ECollisionEnabled::NoCollision);
	PlayDeathMontage();
	// Log death (DeltaTime = 0)
	LogEnemyEvent(TEXT("Died"), 0.f, 0.f);
}

/* ---------- Die ---------- */
void AEnemy::Die_Implementation()
{
	LogEnemyEvent(TEXT("DeathStart"), 0.f, 0.f);
	EnemyState = EEnemyState::EES_Death;
	ClearAttackTimer();
	HideHealthBar();
	DisableCapsule();
	SetLifeSpan(DeathLifeSpan);
	GetCharacterMovement()->bOrientRotationToMovement = false;
	SetWeaponCollisionEnabled(ECollisionEnabled::NoCollision);
	PlayDeathMontage();
	// Log death (DeltaTime = 0)
	LogEnemyEvent(TEXT("Died"), 0.f, 0.f);
}

/* ---------- Attack flow ---------- */
void AEnemy::Attack()
{
	// teacher: set engaged and play montage
	if (CombatTarget == nullptr) return;

	EnemyState = EEnemyState::EES_Engaged;
	PlayAttackMontage();

	// log attack start (DeltaTime = 0 here)
	LogEnemyEvent(TEXT("AttackStart"), 0.f, 0.f);
}

bool AEnemy::CanAttack()
{
	bool bCanAttack = IsInsideAttackRadius() && !IsAttacking() && !IsEngaged() && !IsDead();
	return bCanAttack;
}

void AEnemy::AttackEnd()
{
	EnemyState = EEnemyState::EES_NoState;
	CheckCombatTarget();

	// log attack end
	LogEnemyEvent(TEXT("AttackEnd"), 0.f, 0.f);
}

/* ---------- Initialize / AI helpers ---------- */
void AEnemy::InitializeEnemy()
{
	EnemyController = Cast<AAIController>(GetController());
	MoveToTarget(PatrolTarget);
	HideHealthBar();
	SpawnDefaultWeapon();
}

void AEnemy::CheckPatrolTarget()
{
	if (InTargetRange(PatrolTarget, PatrolRadius))
	{
		PatrolTarget = ChoosePatrolTarget();
		const float WaitTime = FMath::RandRange(PatrolWaitMin, PatrolWaitMax);
		GetWorldTimerManager().SetTimer(PatrolTimer, this, &AEnemy::PatrolTimerFinished, WaitTime);
	}
}

void AEnemy::CheckCombatTarget()
{
	if (IsOutsideCombatRadius())
	{
		ClearAttackTimer();
		LoseInterest();
		if (!IsEngaged()) StartPatrolling();
	}
	else if (IsOutsideAttackRadius() && !IsChasing())
	{
		ClearAttackTimer();
		if (!IsEngaged()) ChaseTarget();
	}
	else if (CanAttack())
	{
		StartAttackTimer();
	}
}

void AEnemy::PatrolTimerFinished()
{
	MoveToTarget(PatrolTarget);
}

void AEnemy::HideHealthBar()
{
	if (HealthBarWidget)
	{
		HealthBarWidget->SetVisibility(false);
	}
}

void AEnemy::ShowHealthBar()
{
	if (HealthBarWidget)
	{
		HealthBarWidget->SetVisibility(true);
	}
}

void AEnemy::LoseInterest()
{
	CombatTarget = nullptr;
	HideHealthBar();

	// reset PrevLatency sentinel so future chases don't compute wrong ΔLatency
	PrevLatency = -1.f;

	LogEnemyEvent(TEXT("LostInterest"), 0.f, 0.f);
}

void AEnemy::StartPatrolling()
{
	EnemyState = EEnemyState::EES_Patrolling;
	GetCharacterMovement()->MaxWalkSpeed = PatrollingSpeed;
	MoveToTarget(PatrolTarget);
	LogEnemyEvent(TEXT("StartPatrolling"), 0.f, 0.f);
}

void AEnemy::ChaseTarget()
{
	EnemyState = EEnemyState::EES_Chasing;
	GetCharacterMovement()->MaxWalkSpeed = ChasingSpeed;
	MoveToTarget(CombatTarget);
	LogEnemyEvent(TEXT("StartChasing"), 0.f, 0.f);
}

bool AEnemy::IsOutsideCombatRadius()
{
	return !InTargetRange(CombatTarget, CombatRadius);
}

bool AEnemy::IsOutsideAttackRadius()
{
	return !InTargetRange(CombatTarget, AttackRadius);
}

bool AEnemy::IsInsideAttackRadius()
{
	return InTargetRange(CombatTarget, AttackRadius);
}

bool AEnemy::IsChasing()
{
	return EnemyState == EEnemyState::EES_Chasing;
}

bool AEnemy::IsAttacking()
{
	return EnemyState == EEnemyState::EES_Attacking;
}

bool AEnemy::IsDead()
{
	return EnemyState == EEnemyState::EES_Death;
}

bool AEnemy::IsEngaged()
{
	return EnemyState == EEnemyState::EES_Engaged;
}

void AEnemy::ClearPatrolTimer()
{
	GetWorldTimerManager().ClearTimer(PatrolTimer);
}

void AEnemy::StartAttackTimer()
{
	EnemyState = EEnemyState::EES_Attacking;
	const float AttackTime = FMath::RandRange(AttackMin, AttackMax);
	GetWorldTimerManager().SetTimer(AttackTimer, this, &AEnemy::Attack, AttackTime);
}

void AEnemy::ClearAttackTimer()
{
	GetWorldTimerManager().ClearTimer(AttackTimer);
}

bool AEnemy::InTargetRange(AActor* Target, double Radius)
{
	if (Target == nullptr) return false;
	const double DistanceToTarget = (Target->GetActorLocation() - GetActorLocation()).Size();
	return DistanceToTarget <= Radius;
}

void AEnemy::MoveToTarget(AActor* Target)
{
	if (EnemyController == nullptr || Target == nullptr) return;
	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalActor(Target);
	MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
	EnemyController->MoveTo(MoveRequest);
}

AActor* AEnemy::ChoosePatrolTarget()
{
	TArray<AActor*> ValidTargets;
	for (AActor* Target : PatrolTargets)
	{
		if (Target != PatrolTarget)
		{
			ValidTargets.AddUnique(Target);
		}
	}

	const int32 NumPatrolTargets = ValidTargets.Num();
	if (NumPatrolTargets > 0)
	{
		const int32 TargetSelection = FMath::RandRange(0, NumPatrolTargets - 1);
		return ValidTargets[TargetSelection];
	}
	return nullptr;
}

void AEnemy::SpawnDefaultWeapon()
{
	UWorld* World = GetWorld();
	if (World && WeaponClass)
	{
		AWeapon* DefaultWeapon = World->SpawnActor<AWeapon>(WeaponClass);
		if (DefaultWeapon)
		{
			DefaultWeapon->Equip(GetMesh(), FName("WeaponSocket"), this, this);
			EquippedWeapon = DefaultWeapon;
		}
	}
}

void AEnemy::PawnSeen(APawn* SeenPawn)
{
	const bool bShouldChaseTarget =
		EnemyState != EEnemyState::EES_Death &&
		EnemyState != EEnemyState::EES_Chasing &&
		EnemyState < EEnemyState::EES_Attacking;

	if (bShouldChaseTarget)
	{
		CombatTarget = SeenPawn;

		// initialize PrevLatency immediately to avoid a large Δ on next tick
		if (CombatTarget)
		{
			PrevLatency = FVector::Dist(GetActorLocation(), CombatTarget->GetActorLocation());
		}

		ClearPatrolTimer();
		ChaseTarget();
	}
}

/* ---------- BaseCharacter helper functions copied ---------- */
void AEnemy::PlayHitReactMontage(const FName& SectionName)
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HitReactMontage)
	{
		AnimInstance->Montage_Play(HitReactMontage);
		AnimInstance->Montage_JumpToSection(SectionName, HitReactMontage);
	}
}

void AEnemy::DirectionalHitReact(const FVector& ImpactPoint)
{
	const FVector Forward = GetActorForwardVector();
	const FVector ImpactLowered(ImpactPoint.X, ImpactPoint.Y, GetActorLocation().Z);
	const FVector ToHit = (ImpactLowered - GetActorLocation()).GetSafeNormal();
	double CosTheta = FVector::DotProduct(Forward, ToHit);
	double Theta = FMath::Acos(CosTheta);
	Theta = FMath::RadiansToDegrees(Theta);
	const FVector CrossProduct = FVector::CrossProduct(Forward, ToHit);
	if (CrossProduct.Z < 0) Theta *= -1.f;

	FName Section("FromBack");
	if (Theta >= -45.f && Theta < 45.f) Section = FName("FromFront");
	else if (Theta >= -135.f && Theta < -45.f) Section = FName("FromLeft");
	else if (Theta >= 45.f && Theta < 135.f) Section = FName("FromRight");

	PlayHitReactMontage(Section);
}

void AEnemy::PlayHitSound(const FVector& ImpactPoint)
{
	if (HitSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, HitSound, ImpactPoint);
	}
}

void AEnemy::SpawnHitParticles(const FVector& ImpactPoint)
{
	if (HitParticles && GetWorld())
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), HitParticles, ImpactPoint);
	}
}

int32 AEnemy::PlayRandomMontageSection(UAnimMontage* Montage, const TArray<FName>& SectionNames)
{
	if (SectionNames.Num() <= 0) return -1;
	const int32 MaxSectionIndex = SectionNames.Num() - 1;
	const int32 Selection = FMath::RandRange(0, MaxSectionIndex);
	if (Montage)
	{
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance)
		{
			AnimInstance->Montage_Play(Montage);
			AnimInstance->Montage_JumpToSection(SectionNames[Selection], Montage);
		}
	}
	return Selection;
}

int32 AEnemy::PlayAttackMontage()
{
	return PlayRandomMontageSection(AttackMontage, AttackMontageSections);
}

int32 AEnemy::PlayDeathMontage()
{
	const int32 Selection = PlayRandomMontageSection(DeathMontage, DeathMontageSections);
	return Selection;
}

void AEnemy::StopAttackMontage()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && AttackMontage)
	{
		AnimInstance->Montage_Stop(0.25f, AttackMontage);
	}
}

void AEnemy::SetWeaponCollisionEnabled(ECollisionEnabled::Type CollisionEnabled)
{
	if (EquippedWeapon && EquippedWeapon->GetWeaponBox())
	{
		EquippedWeapon->GetWeaponBox()->SetCollisionEnabled(CollisionEnabled);
		EquippedWeapon->IgnoreActors.Empty();
	}
}

/* ---------- Utility / misc ---------- */
bool AEnemy::IsAlive() const
{
	return Attributes && Attributes->IsAlive();
}

void AEnemy::DisableCapsule()
{
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AEnemy::DisableMeshCollision()
{
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

#undef LOCTEXT_NAMESPACE
