// EnemyQLearningComponent1.cpp

#include "EnemyQLearningComponent1.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "AttributeComponent.h"
#include "Engine/World.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include <limits>

UEnemyQLearningComponent1::UEnemyQLearningComponent1()
{
	PrimaryComponentTick.bCanEverTick = true;

	// sensible defaults (if not set in header / editor)
	DistanceBins = FMath::Max(1, DistanceBins);
	HealthBins = FMath::Max(1, HealthBins);
	NumAction = FMath::Max(1, NumAction);

	Alpha = (Alpha <= 0.f) ? 0.1f : Alpha;
	Gamma = (Gamma <= 0.f) ? 0.95f : Gamma;
	Epsilon = (Epsilon <= 0.f) ? 1.0f : Epsilon;
	MinEpsilon = (MinEpsilon <= 0.f) ? 0.05f : MinEpsilon;
	EpsilonDecay = (EpsilonDecay <= 0.f) ? 0.995f : EpsilonDecay;

	DecisionInterval = (DecisionInterval <= 0.f) ? 0.25f : DecisionInterval;
	AttackCooldown = (AttackCooldown <= 0.f) ? 1.0f : AttackCooldown;

	LastState = -1;
	LastAction = -1;
	LastDistance = -1.f;
	TimeSinceDecision = 0.f;
	TimeSinceAttack = 10000.f;
	ExternalReward = 0.f;
}

void UEnemyQLearningComponent1::RegisterReward(float InReward)
{
	ExternalReward += InReward;
}

void UEnemyQLearningComponent1::BeginPlay()
{
	Super::BeginPlay();

	PawnOwner = Cast<APawn>(GetOwner());

	// ensure sensible sizes
	DistanceBins = FMath::Max(1, DistanceBins);
	HealthBins = FMath::Max(1, HealthBins);
	NumAction = FMath::Max(1, NumAction);

	NumStates = DistanceBins * HealthBins;

	// init Q table
	QTable.Init(0.f, NumStates * NumAction);

	// default target: player 0 if not set
	if (!TargetPawn && GetWorld())
	{
		TargetPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	}

	// initialize timers
	TimeSinceDecision = 0.f;
	TimeSinceAttack = 10000.f;
	LastState = -1;
	LastAction = -1;
	LastDistance = -1.f;

	// optionally, try to load precomputed qtable (if you want)
	// FString QPath = FPaths::ProjectSavedDir() / TEXT("qtable_init.qtab");
	// if (FPaths::FileExists(QPath)) LoadQTableFromFile(QPath);
}

void UEnemyQLearningComponent1::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bTraining)
		Step_Learning(DeltaTime);
}

int32 UEnemyQLearningComponent1::GetStateIndex()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !TargetPawn) return 0;

	float Dist = FVector::Dist(OwnerActor->GetActorLocation(), TargetPawn->GetActorLocation());
	int32 dBin = GetDistanceToBin(Dist);

	float HealthPercent = 1.f;
	if (ACharacter* Char = Cast<ACharacter>(OwnerActor))
	{
		if (UAttributeComponent* Attr = Char->FindComponentByClass<UAttributeComponent>())
		{
			HealthPercent = Attr->GetHealthPercent();
		}
	}

	int32 hBin = HealthToBin(HealthPercent);
	int32 idx = dBin * HealthBins + hBin;
	idx = FMath::Clamp(idx, 0, NumStates - 1);
	return idx;
}

int32 UEnemyQLearningComponent1::ChooseAction(int32 StateIndex)
{
	// guard
	if (NumAction <= 0) return 0;

	// epsilon-greedy
	if (FMath::FRand() < Epsilon) {
		return FMath::RandRange(0, NumAction - 1);
	}

	// Greedy selection with tie-breaking
	float Best = TNumericLimits<float>::Lowest();
	TArray<int32> BestActions;
	for (int32 i = 0; i < NumAction; i++) {
		float q = QValue(StateIndex, i);
		if (q > Best + KINDA_SMALL_NUMBER) {
			Best = q;
			BestActions.Empty();
			BestActions.Add(i);
		}
		else if (FMath::IsNearlyEqual(q, Best)) {
			BestActions.Add(i);
		}
	}

	if (BestActions.Num() == 0) return 0;
	int32 Pick = BestActions[FMath::RandRange(0, BestActions.Num() - 1)];
	return Pick;
}

int32 UEnemyQLearningComponent1::GetDistanceToBin(float Distance)
{
	float MaxDistance = 2000.f;
	float Norm = FMath::Clamp(Distance / MaxDistance, 0.f, 1.f);
	int32 Bin = FMath::FloorToInt(Norm * DistanceBins);
	return FMath::Clamp(Bin, 0, DistanceBins - 1);
}

int32 UEnemyQLearningComponent1::HealthToBin(float HealthPercent)
{
	float Norm = FMath::Clamp(HealthPercent, 0.f, 1.f);
	int32 Bin = FMath::FloorToInt(Norm * HealthBins);
	return FMath::Clamp(Bin, 0, HealthBins - 1);
}

void UEnemyQLearningComponent1::Step_Learning(float DeltaTime)
{
	if (!GetOwner() || !TargetPawn) return;

	TimeSinceDecision += DeltaTime;
	TimeSinceAttack += DeltaTime;

	if (TimeSinceDecision < DecisionInterval) return;
	TimeSinceDecision = 0.f;

	// --- 1. Current state ---
	int32 State = GetStateIndex();

	// --- 2. Choose action ---
	int32 ChosenAction = ChooseAction(State);

	// --- 3. Perform action ---
	AActor* OwnerActor = GetOwner();
	int32 ActualAction = PerformAction(ChosenAction, OwnerActor);

	// --- 4. Reward ---
	float Reward = -0.01f; // small time penalty
	float CurrentDist = FVector::Dist(OwnerActor->GetActorLocation(), TargetPawn->GetActorLocation());

	// combine ExternalReward (if any)
	Reward += ExternalReward;
	ExternalReward = 0.f;

	// Interpret action using enum for clarity
	EEnemyAction ActionEnum = static_cast<EEnemyAction>(ActualAction);

	if (LastDistance > 0.f)
	{
		if (ActionEnum == EEnemyAction::Chase)
			Reward += (LastDistance - CurrentDist) * 0.1f;
		else if (ActionEnum == EEnemyAction::Retreat)
			Reward += (CurrentDist - LastDistance) * 0.05f;
	}

	if (ActionEnum == EEnemyAction::Attack)
	{
		if (CurrentDist < 200.f)
			Reward += 1.0f;
		else
			Reward -= 0.3f;
	}

	// if NoState or Dead, neutral small penalty (already -0.01), nothing extra

	// --- 5. Next state ---
	int32 NextState = GetStateIndex();

	// --- 6. Update Q ---
	if (LastState >= 0 && LastAction >= 0)
		UpdateQ(LastState, LastAction, Reward, NextState);

	// --- 7. Store for next iteration ---
	LastState = State;
	LastAction = ActualAction;
	LastDistance = CurrentDist;

	// Epsilon decay (per decision)
	Epsilon = FMath::Max(MinEpsilon, Epsilon * EpsilonDecay);
}

int32 UEnemyQLearningComponent1::PerformAction(int32& Action, AActor* OwnerActor)
{
	// If Attack attempted but cooldown not ready, skip to Idle and return Idle
	if (static_cast<EEnemyAction>(Action) == EEnemyAction::Attack && TimeSinceAttack < AttackCooldown) {
		UE_LOG(LogTemp, Warning, TEXT("Attack skipped due to cooldown"));
		return static_cast<int32>(EEnemyAction::Idle);
	}

	if (!OwnerActor) return Action;

	switch (static_cast<EEnemyAction>(Action))
	{
	case EEnemyAction::Idle:
		return static_cast<int32>(EEnemyAction::Idle);

	case EEnemyAction::Chase:
		if (PawnOwner)
		{
			if (AAIController* AICon = Cast<AAIController>(PawnOwner->GetController()))
			{
				AICon->MoveToActor(TargetPawn, 250.f);
			}
		}
		return static_cast<int32>(EEnemyAction::Chase);

	case EEnemyAction::Attack:
		// perform attack: reset cooldown timer
		TimeSinceAttack = 0.f;
		UE_LOG(LogTemp, Log, TEXT("Action: Attack"));
		// call attack mechanics here (damage, anim) if desired
		return static_cast<int32>(EEnemyAction::Attack);

	case EEnemyAction::Retreat:
		if (PawnOwner)
		{
			if (AAIController* AICon = Cast<AAIController>(PawnOwner->GetController()))
			{
				FVector OwnerLoc = OwnerActor->GetActorLocation();
				FVector TargetLoc = TargetPawn->GetActorLocation();
				FVector RetreatDir = (OwnerLoc - TargetLoc).GetSafeNormal();
				FVector RetreatDest = OwnerLoc + RetreatDir * 600.f;

				FNavLocation NavLocation;
				if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
				{
					if (NavSys->ProjectPointToNavigation(RetreatDest, NavLocation))
					{
						AICon->MoveToLocation(NavLocation.Location);
						return static_cast<int32>(EEnemyAction::Retreat);
					}
				}
			}
		}
		// if retreat failed, fallback to Idle
		return static_cast<int32>(EEnemyAction::Idle);

		// If enum contains NoState/Dead values, treat them as Idle/No-op
	case EEnemyAction::EES_NoState:
	case EEnemyAction::EES_Dead:
	default:
		return static_cast<int32>(EEnemyAction::Idle);
	}
}

bool UEnemyQLearningComponent1::SaveQTable(const FString& SlotName)
{
	FString FilePath = FPaths::ProjectSavedDir() / (SlotName + TEXT(".qtab"));
	FString Out;
	for (float v : QTable) {
		Out += FString::SanitizeFloat(v) + TEXT("\n");
	}
	return FFileHelper::SaveStringToFile(Out, *FilePath);
}

bool UEnemyQLearningComponent1::LoadQTable(const FString& SlotName)
{
	FString FilePath = FPaths::ProjectSavedDir() / (SlotName + TEXT(".qtab"));
	FString In;
	if (!FFileHelper::LoadFileToString(In, *FilePath)) return false;

	TArray<FString> Lines;
	In.ParseIntoArrayLines(Lines);
	if (Lines.Num() != QTable.Num()) return false;

	for (int32 i = 0; i < QTable.Num(); ++i)
		QTable[i] = FCString::Atof(*Lines[i]);

	return true;
}

void UEnemyQLearningComponent1::UpdateQ(int32 State, int32 Action, float Reward, int32 NextState)
{
	// bounds check
	if (State < 0 || Action < 0) return;
	if (State >= NumStates) return;
	if (Action >= NumAction) return;
	if (QTable.Num() != NumStates * NumAction) return;

	// compute max next Q safely
	float MaxNextQ = TNumericLimits<float>::Lowest();
	for (int32 a = 0; a < NumAction; a++)
	{
		MaxNextQ = FMath::Max(MaxNextQ, QValue(NextState, a));
	}

	if (MaxNextQ == TNumericLimits<float>::Lowest())
	{
		// no valid next Qs (shouldn't happen) -> assume 0
		MaxNextQ = 0.f;
	}

	int32 Index = State * NumAction + Action;
	if (!QTable.IsValidIndex(Index)) return;

	QTable[Index] = QTable[Index] + Alpha * (Reward + Gamma * MaxNextQ - QTable[Index]);
}

float UEnemyQLearningComponent1::QValue(int32 State, int32 Action) const
{
	if (State < 0 || Action < 0) return 0.f;
	if (NumAction <= 0) return 0.f;
	if (QTable.Num() == 0) return 0.f;

	int32 Index = State * NumAction + Action;
	if (!QTable.IsValidIndex(Index)) return 0.f;
	return QTable[Index];
}
