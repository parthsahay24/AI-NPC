// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CharacterState.h"
#include "EnemyQLearningComponent.generated.h"

UCLASS()
class MYPROJECT2_API AEnemyQLearningComponent : public  ACharacter
{
	GENERATED_BODY()

public:
	
	AEnemyQLearningComponent();

protected:
	
	virtual void BeginPlay() override;
	int32 GetStateIndex();
	int32 ChooseAction(int32 StateIndex);


	void UpdateQ(int32 State, int32 Action, float Reward, int32 NextState);
	float QValue(int32 State, int32 Action) const;
	FORCEINLINE float& QValue (int32 StateIndex, int32 Action) { return QTable[StateIndex * NumState + Action]; }
	//Helpers
	int32 GetDistanceToBin(float distance);
	int32 HealthToBin(float HealthPercent); 

	//Last state/action memory
	int32 LastState = -1;
	int32 LastAction = -1;


	//Decision frequency
	UPROPERTY(EditAnywhere, Category = "QLearning")
	float DecisionInterval = 0.5f;
	UPROPERTY(EditAnywhere, Category = "QLearning")
	float TimeSinceDecision = 0.f;
	UPROPERTY(EditAnywhere, Category = "QLearning")
	float AttackCooldown = 2.0f;
	UPROPERTY(EditAnywhere, Category = "QLearning") 
	float TimeSinceAttack = 0.f;
public:	
	
	
	//Config
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning|Config")
	int32 DistanceBins = 3;// close, mid, far
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning|Config")
	int32 HealthBins = 3;// low, med, high
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning|Config")
	int32 NumAction = 4;// Idle, Chase, Attack, Retreat which represeabt state

	//HyperParameter

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning|Hyper")
	float Alpha = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning|Hyper")
	float Gamma = 0.95f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning|Hyper")
	float Epsilon = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning|Hyper")
	float MinEpsilon = 0.01f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning|Hyper")
	float EpsilonDeccay = 0.999f;

	//Runtime

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QLearning|Runtime")
	TArray<float> QTable;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QLearning|Runtime")
	int32 NumState;

	// Player pawn reference
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning")
	APawn* TargetPawn;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QLearning")
	bool bTraining = true;
	float LastDistance = -1.f;

	APawn* PawnOwner;
	//public API
	UFUNCTION(BlueprintCallable, Category = "QLearning")
	void Step_Learning(float DeltaTime);
	int32 PerformAction(int32& Action, AActor* Owner);
	UFUNCTION(BlueprintCallable, Category = "QLearning")
	FORCEINLINE void StopLearning() { bTraining = false; }
	UFUNCTION(BlueprintCallable, Category = "QLearning")
	FORCEINLINE void StartLearning() { bTraining = true; }

	UFUNCTION(BlueprintCallable, Category = "QLearning")
	bool SaveQTable(const FString& SlotName = TEXT("QTableSave"));
	UFUNCTION(BlueprintCallable, Category = "QLearning")
	bool LoadQTable(const FString& SlotName = TEXT("QTableSave"));
};
