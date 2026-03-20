// EnemyQLearningComponent1.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterState.h"
#include "EnemyQLearningComponent1.generated.h"


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MYPROJECT2_API UEnemyQLearningComponent1 : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyQLearningComponent1();
	int32 GetStateIndex();
	int32 ChooseAction(int32 StateIndex);
protected:
	virtual void BeginPlay() override;
	float ExternalReward = 0.f;
	// Helpers
	
	int32 GetDistanceToBin(float Distance);
	int32 HealthToBin(float HealthPercent);

	void UpdateQ(int32 State, int32 Action, float Reward, int32 NextState);
	float QValue(int32 State, int32 Action) const;
	FORCEINLINE float& QValue(int32 StateIndex, int32 Action) { return QTable[StateIndex * NumAction + Action]; }

	// Last state/action memory
	int32 LastState = -1;
	int32 LastAction = -1;
	float LastDistance = -1.f;

	// Pawn owner
	APawn* PawnOwner;

	// Decision timers
	float TimeSinceDecision = 0.f;
	float TimeSinceAttack = 0.f;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ----------------- Config -----------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning|Config")
	int32 DistanceBins = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning|Config")
	int32 HealthBins = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning|Config")
	int32 NumAction = 4; // Idle, Chase, Attack, Retreat

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning|Config")
	float DecisionInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning|Config")
	float AttackCooldown = 2.0f;

	// ----------------- Hyperparameters -----------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning|Hyper")
	float Alpha = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning|Hyper")
	float Gamma = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning|Hyper")
	float Epsilon = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning|Hyper")
	float MinEpsilon = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning|Hyper")
	float EpsilonDecay = 0.999f;

	// ----------------- Runtime -----------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QLearning|Runtime")
	TArray<float> QTable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QLearning|Runtime")
	int32 NumStates;

	// ----------------- Target -----------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning")
	APawn* TargetPawn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning")
	bool bTraining = true;

	// ----------------- Public API -----------------
	UFUNCTION(BlueprintCallable, Category = "QLearning")
	void Step_Learning(float DeltaTime);

	int32 PerformAction(int32& Action, AActor* OwnerActor);

	UFUNCTION(BlueprintCallable, Category = "QLearning")
	FORCEINLINE void StopLearning() { bTraining = false; }

	UFUNCTION(BlueprintCallable, Category = "QLearning")
	FORCEINLINE void StartLearning() { bTraining = true; }

	UFUNCTION(BlueprintCallable, Category = "QLearning")
	bool SaveQTable(const FString& SlotName = TEXT("QTableSave"));

	UFUNCTION(BlueprintCallable, Category = "QLearning")
	bool LoadQTable(const FString& SlotName = TEXT("QTableSave"));
	
	UFUNCTION(BlueprintCallable, Category = "QLearning")
	void RegisterReward(float InReward);
};
