#include "RisingSand.h"
#include "FreezeGameMode.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

static constexpr float SandDepth = 2500.f;

ARisingSand::ARisingSand()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = CubeFinder.Object;
	BaseMaterial = MatFinder.Object;
}

void ARisingSand::BeginPlay()
{
	Super::BeginPlay();
	Mesh->SetStaticMesh(CubeMesh);
	Mesh->SetWorldScale3D(FVector((AreaMax.X - AreaMin.X) / 100.f, (AreaMax.Y - AreaMin.Y) / 100.f, SandDepth / 100.f));
	if (BaseMaterial)
	{
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.72f, 0.2f));
		Mesh->SetMaterial(0, Mat);
	}
	ResetSand();
}

void ARisingSand::PlaceSurface()
{
	SetActorLocation(FVector((AreaMin.X + AreaMax.X) * 0.5f, (AreaMin.Y + AreaMax.Y) * 0.5f, SandTop - SandDepth * 0.5f));
}

void ARisingSand::ResetSand()
{
	bRising = false;
	SandTop = StartZ;
	PlaceSurface();
}

void ARisingSand::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);	// gold: never frozen

	AFreezeGameMode* GM = GetWorld()->GetAuthGameMode<AFreezeGameMode>();
	if (!GM || !GM->IsPlaying()) return;
	ACharacter* Player = GM->GetPlayerCharacter();
	if (!Player) return;

	const FVector P = Player->GetActorLocation();
	const bool bInRoom = P.X >= AreaMin.X && P.X <= AreaMax.X && P.Y >= AreaMin.Y && P.Y <= AreaMax.Y;

	if (!bRising)
	{
		if (bInRoom && P.X > TriggerX)
		{
			bRising = true;
			GM->PlaySfx(TEXT("SFX_SandRise"));
			GM->AddWarning(TEXT("The sand is rising! Climb!"));
		}
		return;
	}

	SandTop = FMath::Min(SandTop + Speed * DeltaSeconds, MaxZ);
	PlaceSurface();

	const float Feet = P.Z - Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (bInRoom && Feet < SandTop - 10.f)
	{
		GM->PlayerHit(TEXT("The sand swallowed you."), true);
	}
}
