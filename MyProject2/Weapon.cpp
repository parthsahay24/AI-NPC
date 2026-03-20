// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SceneComponent.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Components/StaticMeshComponent.h"
// Sets default values
AWeapon::AWeapon()
{
	PrimaryActorTick.bCanEverTick = true;

	// Root
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Weapon collision box
	WeaponBox = CreateDefaultSubobject<UBoxComponent>(TEXT("WeaponBox"));
	WeaponBox->SetupAttachment(RootComponent);

	// Trace start & end points
	BoxTraceStart = CreateDefaultSubobject<USceneComponent>(TEXT("BoxTraceStart"));
	BoxTraceStart->SetupAttachment(RootComponent);

	BoxTraceEnd = CreateDefaultSubobject<USceneComponent>(TEXT("BoxTraceEnd"));
	BoxTraceEnd->SetupAttachment(RootComponent);

	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	ItemMesh->SetupAttachment(RootComponent);
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // unless you want collisions
	ItemMesh->SetHiddenInGame(false);
}

// Called when the game starts or when spawned
void AWeapon::BeginPlay()
{
	Super::BeginPlay();

	if (WeaponBox)
	{
		WeaponBox->OnComponentBeginOverlap.AddDynamic(this, &AWeapon::OnBoxOverlap);
	}
}

// Called every frame
void AWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// ---------------- Combat ----------------
void AWeapon::OnBoxOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == GetOwner()) return;

	// Only proceed if weapon collision is enabled (you should toggle it during attack)
	if (!WeaponBox || WeaponBox->GetCollisionEnabled() == ECollisionEnabled::NoCollision) return;

	FHitResult BoxHit;
	if (BoxTrace(BoxHit))
	{
		AActor* HitActor = BoxHit.GetActor();
		if (!HitActor) return;

		// avoid double-hitting same actor (per-swing)
		if (IgnoreActors.Contains(HitActor)) return;

		// if same type (e.g., enemy hitting enemy), skip
		if (ActorIsSameType(HitActor)) return;

		IgnoreActors.Add(HitActor);

		// Apply damage
		ExecuteGetHit(BoxHit);
	}
}


// Perform box trace between start and end
bool AWeapon::BoxTrace(FHitResult& BoxHit)
{
	if (!BoxTraceStart || !BoxTraceEnd) return false;

	const FVector Start = BoxTraceStart->GetComponentLocation();
	const FVector End = BoxTraceEnd->GetComponentLocation();
	const FCollisionShape Box = FCollisionShape::MakeBox(BoxTraceExtent);

	// Setup params to ignore weapon and owner
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	if (GetOwner()) Params.AddIgnoredActor(GetOwner());

	// Add any additional actors you want ignored
	for (AActor* A : IgnoreActors)
	{
		if (A) Params.AddIgnoredActor(A);
	}

	bool bHit = GetWorld()->SweepSingleByChannel(
		BoxHit,
		Start,
		End,
		FQuat::Identity,
		ECC_Pawn, // make sure targets respond to this channel
		Box,
		Params
	);

	if (bShowBoxDebug)
	{
		const FVector Center = (Start + End) * 0.5f;
		const FVector Extent = BoxTraceExtent;
		DrawDebugBox(GetWorld(), Center, Extent, FQuat::Identity, bHit ? FColor::Green : FColor::Red, false, 1.5f);
		DrawDebugLine(GetWorld(), Start, End, FColor::Yellow, false, 1.5f, 0, 1.f);
	}

	return bHit;
}
// Handle hit logic (damage, effects, etc.)
void AWeapon::ExecuteGetHit(FHitResult& BoxHit)
{
	AActor* HitActor = BoxHit.GetActor();
	if (!HitActor) return;

	// Use a damage type if you have one
	TSubclassOf<UDamageType> DType = UDamageType::StaticClass();

	AController* InstigatorController = GetInstigatorController();

	UE_LOG(LogTemp, Log, TEXT("[Weapon] Hitting %s with Damage=%.2f InstigatorController=%s"),
		*GetNameSafe(HitActor), Damage, InstigatorController ? *GetNameSafe(InstigatorController->GetOwner()) : TEXT("null"));

	UGameplayStatics::ApplyDamage(HitActor, Damage, InstigatorController, this, DType);

	// Optional blueprint event / VFX
	CreateFields(BoxHit.ImpactPoint);
}

// Check if target is same type as owner (e.g., enemy hitting enemy)
bool AWeapon::ActorIsSameType(AActor* OtherActor)
{
	if (!OtherActor || !GetOwner()) return false;

	// Prefer tags: if you tag enemies with "Enemy", compare tags
	if (GetOwner()->ActorHasTag(TEXT("Enemy")) && OtherActor->ActorHasTag(TEXT("Enemy")))
	{
		return true;
	}

	// Fallback: same class check (may be too strict depending on inheritance)
	return OtherActor->GetClass() == GetOwner()->GetClass();
}
// ---------------- Optional effects ----------------
void AWeapon::DeactivateEmbers()
{
	// Placeholder: deactivate Niagara component if any
}

void AWeapon::DisableSphereCollision()
{
	// Placeholder: disable sphere collision if any
}

// ---------------- Equip ----------------
void AWeapon::Equip(USceneComponent* InParent, FName InSocketName, AActor* NewOwner, APawn* NewInstigator)
{
	AttachMeshToSocket(InParent, InSocketName);
	SetOwner(NewOwner);
	SetInstigator(NewInstigator);
	PlayEquipSound();
}

void AWeapon::AttachMeshToSocket(USceneComponent* InParent, const FName& InSocketName)
{
	if (InParent)
	{
		AttachToComponent(InParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, InSocketName);
	}
}

void AWeapon::PlayEquipSound()
{
	if (EquipSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, EquipSound, GetActorLocation());
	}
}
UBoxComponent* AWeapon::GetWeaponBox() const
{
	return WeaponBox;
}