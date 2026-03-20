// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyQLearningComponent.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
// Sets default values
AEnemyQLearningComponent::AEnemyQLearningComponent()
{
 	
	PrimaryActorTick.bCanEverTick = true;
	
}

// Called when the game starts or when spawned
void AEnemyQLearningComponent::BeginPlay()
{
	Super::BeginPlay();
	PawnOwner = Cast<APawn>(GetOwner());

}

int32 AEnemyQLearningComponent::GetStateIndex()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !TargetPawn) return 0;

	float Dist = FVector::Dist(OwnerActor->GetActorLocation(), TargetPawn->GetActorLocation());
	int32 dBin = GetDistanceToBin(Dist);

	float HealthPercent = 1.f;
	if (ACharacter* Char = Cast<ACharacter>(OwnerActor))
	{
		// Example: hook your health system
	}

	int32 hBin = HealthToBin(HealthPercent);
	return dBin * HealthBins + hBin;
}


int32 AEnemyQLearningComponent::ChooseAction(int32 StateIndex)
{
	if (FMath::FRand() < Epsilon) {
		return FMath::RandRange(0, NumAction - 1);
	}

	//Greedy
	float Best = -BIG_NUMBER;
	int32 BestAct = 0;
	for (int32 i = 0; i < NumAction; i++) {
		float q = QValue(StateIndex, i);
		if (q > Best) {
			Best = q;
			BestAct = i;
		}
	}

	return BestAct;
}

int32 AEnemyQLearningComponent::GetDistanceToBin(float distance)
{
	float MaxDistance = 2000.f;
	float Norm = FMath::Clamp(distance / MaxDistance, 0.f, 1.f);
	int32 Bin = FMath::FloorToInt(Norm * DistanceBins);
	return FMath::Clamp(Bin, 0, DistanceBins-1);
}

int32 AEnemyQLearningComponent::HealthToBin(float HealthPercent)
{
	float norm = FMath::Clamp(HealthPercent, 0.f, 1.f);

	int32 Bin = FMath::FloorToInt(norm * HealthBins);

	return FMath::Clamp(Bin, 0, HealthBins - 1);
}

// Called every frame

void AEnemyQLearningComponent::Step_Learning(float DeltaTime)
{
	if (!GetOwner() || !TargetPawn) return;

	TimeSinceDecision += DeltaTime;
	TimeSinceAttack += DeltaTime;

	if (TimeSinceDecision < DecisionInterval) return;
	TimeSinceDecision = 0.f;

	// --- 1. Get current state ---
	int32 State = GetStateIndex();

	// --- 2. Choose action (ε-greedy) ---
	int32 Action = ChooseAction(State);

	// --- 3. Perform action ---
	AActor* OwnerActor = GetOwner();
	PerformAction(Action, OwnerActor);

	// --- 4. Calculate reward ---
	float Reward = -0.01f; // small penalty to discourage doing nothing
	float CurrentDist = FVector::Dist(OwnerActor->GetActorLocation(), TargetPawn->GetActorLocation());


	// Distance-based shaping
	if (LastDistance > 0.f)  // make sure it's not the first frame
	{
		if (Action == (int32)EEnemyAction::Chase)
			Reward += (LastDistance - CurrentDist) * 0.1f;  // positive if closing gap
		else if (Action == (int32)EEnemyAction::Retreat)
			Reward += (CurrentDist - LastDistance) * 0.05f; // positive if increasing gap
	}

	// Attack reward
	if (Action == (int32)EEnemyAction::Attack)
	{
		if (CurrentDist < 200.f)
			Reward += 1.0f;   // successful attack range
		else
			Reward -= 0.3f;   // punished for useless attack
	}

	// --- 5. Get next state ---
	int32 NextState = GetStateIndex();

	// --- 6. Update Q table ---
	if (LastState >= 0 && LastAction >= 0)  // skip on very first frame
		UpdateQ(LastState, LastAction, Reward, NextState);

	// --- 7. Save for next iteration ---
	LastState = State;
	LastAction = Action;
	LastDistance = CurrentDist;
}


int32 AEnemyQLearningComponent::PerformAction(int32& Action, AActor* OwnerActor)
{
	if (EEnemyAction(Action) == EEnemyAction::Attack && TimeSinceAttack < AttackCooldown) {
		UE_LOG(LogTemp, Warning, TEXT("Attack skipped due to cooldown"));
		return (int32)EEnemyAction::Idle;
	}

	if (!OwnerActor) return Action;

	switch (EEnemyAction(Action))
	{
	case EEnemyAction::Idle:
		break;

	case EEnemyAction::Chase:
		UE_LOG(LogTemp, Log, TEXT("Action: Chase"));
		if (PawnOwner)
		{
			if (AAIController* AICon = Cast<AAIController>(PawnOwner->GetController()))
				AICon->MoveToActor(TargetPawn, 250.f);
		}
		break;

	case EEnemyAction::Attack:
		TimeSinceAttack = 0.f; // reset cooldown
		break;

	case EEnemyAction::Retreat:
		if (PawnOwner)
		{
			if (AAIController* AICon = Cast<AAIController>(PawnOwner->GetController()))
			{
				FVector OwnerLoc = OwnerActor->GetActorLocation();
				FVector TargetLoc = TargetPawn->GetActorLocation();
				FVector RetreatDirection = (OwnerLoc - TargetLoc).GetSafeNormal();
				FVector RetreatDestination = OwnerLoc + RetreatDirection * 600.f;

				FNavLocation NavLocation;
				if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
				{
					if (NavSys->ProjectPointToNavigation(RetreatDestination, NavLocation))
						AICon->MoveToLocation(NavLocation.Location);
				}
			}
		}
		break;
	}
	return Action;
}

bool AEnemyQLearningComponent::SaveQTable(const FString& SlotName)
{
	FString FilePath = FPaths::ProjectSavedDir() / (SlotName + TEXT(".qtab"));
	FString Out;
	for (float v : QTable) {
		Out += FString::SanitizeFloat(v) + TEXT("\n");

	}
	return FFileHelper::SaveStringToFile(Out, *FilePath);
}

bool AEnemyQLearningComponent::LoadQTable(const FString& SlotName)
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
void AEnemyQLearningComponent::UpdateQ(int32 State, int32 Action, float Reward, int32 NextState)
{
	float MaxNextQ = -BIG_NUMBER;
	for (int32 a = 0; a < NumAction; a++)
		MaxNextQ = FMath::Max(MaxNextQ, QValue(NextState, a));

	int32 Index = State * NumAction + Action;
	QTable[Index] = QTable[Index] + Alpha * (Reward + Gamma * MaxNextQ - QTable[Index]);
}
float AEnemyQLearningComponent::QValue(int32 State, int32 Action) const
{
	return QTable[State * NumAction + Action];
}

