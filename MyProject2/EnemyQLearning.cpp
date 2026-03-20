#include "EnemyQLearning.h"
#include "Weapon.h"
#include "HealthBar.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AttributeComponent.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "AIController.h"
#include "EnemyQLearningComponent1.h"
#include "CharacterState.h"
// If your enum lives in a separate header, include it here; otherwise remove this line.


AEnemyQLearning::AEnemyQLearning()
{
	PrimaryActorTick.bCanEverTick = true;

	AttributeComp = CreateDefaultSubobject<UAttributeComponent>(TEXT("AttributeComp"));
	Healthbar = CreateDefaultSubobject<UHealthBar>(TEXT("Healthbar"));
	Healthbar->SetupAttachment(RootComponent);

	PawnSensingComponent = CreateDefaultSubobject<UPawnSensingComponent>(TEXT("PawnSensing"));
	PawnSensingComponent->SightRadius = 2000.f;
	PawnSensingComponent->SetPeripheralVisionAngle(60.f);

	QLearningComp = CreateDefaultSubobject<UEnemyQLearningComponent1>(TEXT("QLearningComp"));

	CombatTarget = nullptr;
	EquipWeapon = nullptr;
	AttackRange = 200.f;
	EnemyState = EEnemyAction::Idle;
	SurvivalTimer = 0.f;
}

void AEnemyQLearning::BeginPlay()
{
	Super::BeginPlay();
	EnemyController = Cast<AAIController>(GetController());

	if (WeaponClass)
	{
		EquipWeapon = GetWorld()->SpawnActor<AWeapon>(WeaponClass);
		if (EquipWeapon)
		{
			EquipWeapon->AttachToComponent(GetMesh(),
				FAttachmentTransformRules::SnapToTargetIncludingScale,
				FName("WeaponSocket"));
		}
	}

	EnsureQLearningTarget();

	if (QLearningComp)
	{
		QLearningComp->bTraining = true;
		QLearningComp->Epsilon = 1.0f;
	}

	if (Healthbar && AttributeComp)
	{
		Healthbar->SetPercent(AttributeComp->GetHealthPercent());
		Healthbar->SetVisibility(false);
	}
}

void AEnemyQLearning::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (IsDead()) return;

	RegisterSurvival(DeltaTime);
	EnsureQLearningTarget();

	// Let Q-learning choose behavior
	ChooseAndPerformQLAction();

	// Reward based on proximity
	if (QLearningComp && CombatTarget)
	{
		if (!IsOutsideAttackRadius())
			QLearningComp->RegisterReward(10.f);
		else
			QLearningComp->RegisterReward(-1.f);
	}
}

void AEnemyQLearning::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

// --- Core AI Behavior from Q-Learning ---
void AEnemyQLearning::ChooseAndPerformQLAction()
{
	if (!QLearningComp) return;

	int32 State = QLearningComp->GetStateIndex();
	int32 ActionIndex = QLearningComp->ChooseAction(State);
	EEnemyAction Chosen = static_cast<EEnemyAction>(ActionIndex);

	// If the chosen action corresponds to a state-like enum (NoState/Dead), handle carefully
	if (Chosen == EEnemyAction::EES_Dead)
	{
		// If Q-learning suggests 'Dead', ensure enemy is flagged dead (but actual death mechanics should be authoritative)
		EnemyState = EEnemyAction::EES_Dead;
		UE_LOG(LogTemp, Warning, TEXT("[QL-Enemy] Chosen Dead state from Q - treating as No-op"));
		return;
	}

	if (Chosen == EEnemyAction::EES_NoState)
	{
		// NoState — treat as Idle / do nothing
		EnemyState = EEnemyAction::EES_NoState;
		return;
	}

	switch (Chosen)
	{
	case EEnemyAction::Idle:
		EnemyState = EEnemyAction::Idle;
		break;

	case EEnemyAction::Chase:
		if (CombatTarget)
		{
			EnemyState = EEnemyAction::Chase;
			GetCharacterMovement()->MaxWalkSpeed = 300.f;
			MoveToTarget(CombatTarget);
		}
		else
		{
			// fallback
			EnemyState = EEnemyAction::EES_NoState;
		}
		break;

	case EEnemyAction::Attack:
		if (!IsOutsideAttackRadius() && AttackMontage)
		{
			EnemyState = EEnemyAction::Attack;
			PlayAnimMontage(AttackMontage);
			// NOTE: RegisterHitReward(true) awards the RL signal that an attack was attempted.
			// If you want to only reward on successful hits, call RegisterHitReward in the hit callback.
			RegisterHitReward(true);
		}
		else
		{
			EnemyState = EEnemyAction::Chase;
			ChaseTarget();
		}
		break;

	case EEnemyAction::Retreat:
		if (CombatTarget)
		{
			FVector RetreatDir = (GetActorLocation() - CombatTarget->GetActorLocation()).GetSafeNormal();
			FVector RetreatDest = GetActorLocation() + RetreatDir * 600.f;
			if (EnemyController)
				EnemyController->MoveToLocation(RetreatDest);
			EnemyState = EEnemyAction::Idle;
		}
		else
		{
			EnemyState = EEnemyAction::EES_NoState;
		}
		break;

		// handle fallthrough for any other enum (e.g., new ones)
	default:
		EnemyState = EEnemyAction::EES_NoState;
		break;
	}

	UE_LOG(LogTemp, Log, TEXT("[QL-Enemy] Action: %d | State: %d"), ActionIndex, State);
}

void AEnemyQLearning::EnsureQLearningTarget()
{
	if (!QLearningComp) return;

	APawn* TargetPawn = Cast<APawn>(CombatTarget);
	if (!TargetPawn && GetWorld())
	{
		TargetPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		if (TargetPawn)
			CombatTarget = TargetPawn;
	}
	QLearningComp->TargetPawn = TargetPawn;
}

// ---------------- Damage ----------------
float AEnemyQLearning::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	UE_LOG(LogTemp, Warning, TEXT("[ENEMY] TakeDamage: %.2f | Causer: %s"), DamageAmount, *GetNameSafe(DamageCauser));
	HandleDamage(DamageAmount);

	if (Healthbar && AttributeComp)
	{
		Healthbar->SetPercent(AttributeComp->GetHealthPercent());
		ShowHealthBar();
	}

	if (IsDead())
	{
		RegisterDeathPenalty();
		// mark state as dead for consistency
		EnemyState = EEnemyAction::EES_Dead;
		Die();
		return DamageAmount;
	}

	CombatTarget = EventInstigator ? EventInstigator->GetPawn() : nullptr;
	if (QLearningComp) QLearningComp->TargetPawn = CombatTarget;

	if (CombatTarget)
	{
		if (!IsOutsideAttackRadius())
			EnemyState = EEnemyAction::Attack;
		else
		{
			EnemyState = EEnemyAction::Chase;
			ChaseTarget();
		}

		MoveToTarget(CombatTarget);
		GetCharacterMovement()->MaxWalkSpeed = 300.f;
	}

	return DamageAmount;
}

bool AEnemyQLearning::IsDead() const
{
	return AttributeComp && AttributeComp->GetHealthPercent() <= 0.f;
}

void AEnemyQLearning::HandleDamage(float DamageAmount)
{
	if (AttributeComp)
		AttributeComp->ReceiveDamage(DamageAmount);
}

void AEnemyQLearning::Die()
{
	UE_LOG(LogTemp, Warning, TEXT("[ENEMY] Died"));
	// set to dead enum
	EnemyState = EEnemyAction::EES_Dead;

	if (DeathMontage)
		PlayAnimMontage(DeathMontage);

	if (Healthbar)
		Healthbar->SetVisibility(false);

	if (EquipWeapon)
		EquipWeapon->Destroy();

	Destroy();
}

bool AEnemyQLearning::IsOutsideAttackRadius() const
{
	if (!CombatTarget) return true;
	return FVector::Dist(GetActorLocation(), CombatTarget->GetActorLocation()) > AttackRange;
}

void AEnemyQLearning::ChaseTarget()
{
	if (EnemyController && CombatTarget)
		EnemyController->MoveToActor(CombatTarget, AttackRange);
}

void AEnemyQLearning::MoveToTarget(APawn* TargetPawn)
{
	if (EnemyController && TargetPawn)
		EnemyController->MoveToActor(TargetPawn, AttackRange);
}

void AEnemyQLearning::ShowHealthBar()
{
	if (Healthbar)
		Healthbar->SetVisibility(true);
}

void AEnemyQLearning::PlayHitSound(const FVector& Location)
{
	if (HitSound)
		UGameplayStatics::PlaySoundAtLocation(this, HitSound, Location);
}

void AEnemyQLearning::SpawnHitParticle(const FVector& Location)
{
	if (HitParticles)
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), HitParticles, Location);
}

void AEnemyQLearning::GetHit_Implementation(const FVector& HitImpact, AActor* Hitter)
{
	if (!IsDead())
	{
		ShowHealthBar();
		PlayHitSound(HitImpact);
		SpawnHitParticle(HitImpact);

		if (QLearningComp)
			QLearningComp->RegisterReward(-20.f);

		if (!IsOutsideAttackRadius())
			EnemyState = EEnemyAction::Attack;
		else
		{
			EnemyState = EEnemyAction::Chase;
			ChaseTarget();
		}
	}
	else
	{
		if (Healthbar)
			Healthbar->SetVisibility(false);

		RegisterDeathPenalty();
		EnemyState = EEnemyAction::EES_Dead;
		Die();
	}
}

// ---------------- Q-Learning Hooks ----------------
void AEnemyQLearning::RegisterSurvival(float DeltaTime)
{
	SurvivalTimer += DeltaTime;
	if (SurvivalTimer >= 1.f)
	{
		if (QLearningComp)
			QLearningComp->RegisterReward(+2.f);
		SurvivalTimer = 0.f;
	}
}

void AEnemyQLearning::RegisterHitReward(bool bSuccess)
{
	if (QLearningComp)
	{
		if (bSuccess)
			QLearningComp->RegisterReward(+30.f);
		else
			QLearningComp->RegisterReward(-10.f);
	}
}

void AEnemyQLearning::RegisterDeathPenalty()
{
	if (QLearningComp)
		QLearningComp->RegisterReward(-100.f);
}
 