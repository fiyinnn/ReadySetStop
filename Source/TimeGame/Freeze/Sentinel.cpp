#include "Sentinel.h"
#include "FreezeGameMode.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ASentinel::ASentinel()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = Speed;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshFinder(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimFinder(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	if (MeshFinder.Succeeded())
	{
		GetMesh()->SetSkeletalMeshAsset(MeshFinder.Object);
	}
	if (AnimFinder.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(AnimFinder.Class);
	}
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -96.f));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = MatFinder.Object;

	// Red visor over the head so it reads as an enemy, not a second player
	Eye = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Eye"));
	Eye->SetupAttachment(GetMesh(), TEXT("head"));
	Eye->SetStaticMesh(SphereFinder.Object);
	Eye->SetRelativeScale3D(FVector(0.32f));
	Eye->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(GetCapsuleComponent());
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
	Glow->SetIntensity(6000.f);
	Glow->SetAttenuationRadius(450.f);
	Glow->SetLightColor(FLinearColor(1.f, 0.15f, 0.1f));
	Glow->SetCastShadows(false);
}

void ASentinel::BeginPlay()
{
	Super::BeginPlay();

	Home = GetActorLocation();
	GetCharacterMovement()->MaxWalkSpeed = Speed;

	if (BaseMaterial)
	{
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Mat->SetVectorParameterValue(TEXT("Color"), bBoss ? FreezeColors::Gold : FLinearColor(1.f, 0.05f, 0.03f));
		Eye->SetMaterial(0, Mat);
	}
	if (bBoss)
	{
		Glow->SetLightColor(FreezeColors::Gold);
	}

	if (AFreezeGameMode* GM = GetWorld()->GetAuthGameMode<AFreezeGameMode>())
	{
		GM->RegisterFreezable(this);
	}
}

bool ASentinel::IsInArea(const FVector& P) const
{
	return P.X >= AreaMin.X && P.X <= AreaMax.X && P.Y >= AreaMin.Y && P.Y <= AreaMax.Y;
}

void ASentinel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDefeated) return;

	AFreezeGameMode* GM = GetWorld()->GetAuthGameMode<AFreezeGameMode>();
	if (!GM || GM->IsFrozen() || !GM->IsPlaying()) return;

	// Fell off the course
	if (GetActorLocation().Z < Home.Z - 600.f)
	{
		Defeat();
		return;
	}

	ACharacter* Player = GM->GetPlayerCharacter();
	if (!Player) return;

	const FVector Me = GetActorLocation();
	const FVector Target = Player->GetActorLocation();
	const FVector ToPlayer = Target - Me;
	const bool bPlayerLevel = FMath::Abs(ToPlayer.Z) < 250.f;

	if (bPlayerLevel && IsInArea(Target) && ToPlayer.Size2D() < ChaseRadius)
	{
		AddMovementInput(ToPlayer.GetSafeNormal2D(), 1.f);

		const float CatchDistance = GetCapsuleComponent()->GetScaledCapsuleRadius() + 70.f;
		if (ToPlayer.Size2D() < CatchDistance)
		{
			GM->PlayerHit(TEXT("The Guard caught you."));
		}
	}
	else
	{
		const FVector ToHome = Home - Me;
		if (ToHome.Size2D() > 60.f)
		{
			AddMovementInput(ToHome.GetSafeNormal2D(), 0.6f);
		}
	}
}

void ASentinel::ResetToHome()
{
	if (bDefeated) return;
	SetActorLocation(Home, false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->StopMovementImmediately();
}

void ASentinel::Defeat()
{
	if (bDefeated) return;
	bDefeated = true;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorTickEnabled(false);

	if (AFreezeGameMode* GM = GetWorld()->GetAuthGameMode<AFreezeGameMode>())
	{
		GM->OnSentinelDefeated(this);
	}
	SetLifeSpan(0.5f);
}
