#include "TimeMover.h"
#include "FreezeGameMode.h"
#include "Sentinel.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ATimeMover::ATimeMover()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Movable);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetMobility(EComponentMobility::Movable);

	Arm = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Arm"));
	Arm->SetupAttachment(Root);
	Arm->SetMobility(EComponentMobility::Movable);
	Arm->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	KillBox = CreateDefaultSubobject<UBoxComponent>(TEXT("KillBox"));
	KillBox->SetupAttachment(Root);
	KillBox->SetMobility(EComponentMobility::Movable);
	KillBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	KillBox->SetCollisionResponseToAllChannels(ECR_Overlap);
	KillBox->SetGenerateOverlapEvents(false);

	// Gold (timeless) things glow so they stand out from things your freeze can stop
	GoldGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("GoldGlow"));
	GoldGlow->SetupAttachment(Body);
	GoldGlow->SetIntensity(8000.f);
	GoldGlow->SetAttenuationRadius(500.f);
	GoldGlow->SetLightColor(FLinearColor(1.f, 0.7f, 0.15f));
	GoldGlow->SetCastShadows(false);
	GoldGlow->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = CubeFinder.Object;
	CylinderMesh = CylFinder.Object;
	BaseMaterial = MatFinder.Object;
}

void ATimeMover::BeginPlay()
{
	Super::BeginPlay();

	Origin = GetActorLocation();
	BaseRotation = GetActorRotation();

	const FLinearColor BodyColor = bTimeless ? FreezeColors::Gold : Color;
	const FVector BodyOffset = (Mode == ETimeMoveMode::Swing) ? FVector(0.f, 0.f, -SwingLength) : FVector::ZeroVector;

	Body->SetStaticMesh(CubeMesh);
	Body->SetRelativeLocation(BodyOffset);
	Body->SetRelativeScale3D(Size / 100.f);

	KillBox->SetRelativeLocation(BodyOffset);
	KillBox->SetBoxExtent(Size * 0.5f + FVector(12.f));

	if (BaseMaterial)
	{
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Mat->SetVectorParameterValue(TEXT("Color"), BodyColor);
		Body->SetMaterial(0, Mat);

		UMaterialInstanceDynamic* ArmMat = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		ArmMat->SetVectorParameterValue(TEXT("Color"), bTimeless ? FreezeColors::Gold : FreezeColors::Metal);
		Arm->SetMaterial(0, ArmMat);
	}

	GoldGlow->SetVisibility(bTimeless);

	if (Mode == ETimeMoveMode::Swing)
	{
		Arm->SetStaticMesh(CylinderMesh);
		Arm->SetRelativeLocation(FVector(0.f, 0.f, -SwingLength * 0.5f));
		Arm->SetRelativeScale3D(FVector(0.15f, 0.15f, SwingLength / 100.f));
	}
	else
	{
		Arm->SetVisibility(false);
	}

	if (!bTimeless)
	{
		if (AFreezeGameMode* GM = GetWorld()->GetAuthGameMode<AFreezeGameMode>())
		{
			GM->RegisterFreezable(this);
		}
	}

	ApplyMotion();
}

void ATimeMover::ApplyMotion()
{
	const float SafePeriod = FMath::Max(Period, 0.1f);
	const float Cycle = Time / SafePeriod + Phase;

	switch (Mode)
	{
	case ETimeMoveMode::PingPong:
	{
		const float Alpha = 0.5f - 0.5f * FMath::Cos(2.f * PI * Cycle);
		SetActorLocation(Origin + FMath::Lerp(OffsetA, OffsetB, Alpha));
		break;
	}
	case ETimeMoveMode::Loop:
	{
		const float Alpha = FMath::Frac(Cycle);
		SetActorLocation(Origin + FMath::Lerp(OffsetA, OffsetB, Alpha));
		break;
	}
	case ETimeMoveMode::Swing:
	{
		const float Angle = SwingAngle * FMath::Sin(2.f * PI * Cycle);
		SetActorRotation(BaseRotation + FRotator(0.f, 0.f, Angle));
		break;
	}
	}
}

bool ATimeMover::IsDangerousNow() const
{
	switch (Deadly)
	{
	case ETimeDeadly::Always:
		return true;
	case ETimeDeadly::MovingToB:
		if (Mode == ETimeMoveMode::Loop) return true;
		// PingPong heads towards B while sin(2*pi*cycle) is positive
		return FMath::Sin(2.f * PI * (Time / FMath::Max(Period, 0.1f) + Phase)) > 0.05f;
	default:
		return false;
	}
}

void ATimeMover::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// DeltaSeconds is already 0 while this actor is frozen (CustomTimeDilation = 0)
	Time += DeltaSeconds;
	ApplyMotion();

	AFreezeGameMode* GM = GetWorld()->GetAuthGameMode<AFreezeGameMode>();
	if (!GM) return;

	const bool bFrozenNow = !bTimeless && GM->IsFrozen();
	if (bFrozenNow || !IsDangerousNow()) return;

	if (ACharacter* Player = GM->GetPlayerCharacter())
	{
		const UCapsuleComponent* Cap = Player->GetCapsuleComponent();
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(Cap->GetScaledCapsuleRadius(), Cap->GetScaledCapsuleHalfHeight());
		if (KillBox->OverlapComponent(Cap->GetComponentLocation(), Cap->GetComponentQuat(), Shape))
		{
			GM->PlayerHit(HitMessage);
		}
	}

	if (bCrushesSentinels)
	{
		for (TActorIterator<ASentinel> It(GetWorld()); It; ++It)
		{
			ASentinel* S = *It;
			if (!S || S->IsDefeated()) continue;
			const UCapsuleComponent* Cap = S->GetCapsuleComponent();
			const FCollisionShape Shape = FCollisionShape::MakeCapsule(Cap->GetScaledCapsuleRadius(), Cap->GetScaledCapsuleHalfHeight());
			if (KillBox->OverlapComponent(Cap->GetComponentLocation(), Cap->GetComponentQuat(), Shape))
			{
				S->Defeat();
			}
		}
	}
}
