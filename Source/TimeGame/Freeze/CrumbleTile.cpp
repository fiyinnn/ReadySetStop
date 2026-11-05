#include "CrumbleTile.h"
#include "FreezeGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ACrumbleTile::ACrumbleTile()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = CubeFinder.Object;
	BaseMaterial = MatFinder.Object;
}

void ACrumbleTile::BeginPlay()
{
	Super::BeginPlay();

	Home = GetActorLocation();
	Mesh->SetStaticMesh(CubeMesh);
	Mesh->SetWorldScale3D(FVector(1.94f, 1.94f, 0.5f));	// 194 x 194 x 50 cm, small gaps show the tiles
	if (BaseMaterial)
	{
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.62f, 0.5f, 0.33f));
		Mesh->SetMaterial(0, Mat);
	}

	if (AFreezeGameMode* GM = GetWorld()->GetAuthGameMode<AFreezeGameMode>())
	{
		GM->RegisterFreezable(this);
	}
}

void ACrumbleTile::ResetTile()
{
	bTriggered = false;
	Timer = 0.f;
	FallSpeed = 0.f;
	SetActorLocation(Home);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
}

void ACrumbleTile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);	// DeltaSeconds is ~0 while frozen, so the crumble pauses

	AFreezeGameMode* GM = GetWorld()->GetAuthGameMode<AFreezeGameMode>();
	if (!GM) return;

	if (!bTriggered)
	{
		if (ACharacter* Player = GM->GetPlayerCharacter())
		{
			const FVector P = Player->GetActorLocation();
			const FVector Rel = P - Home;
			const bool bAbove = FMath::Abs(Rel.X) < 110.f && FMath::Abs(Rel.Y) < 110.f && Rel.Z > 0.f && Rel.Z < 160.f;
			if (bAbove) bTriggered = true;
		}
		return;
	}

	Timer += DeltaSeconds;
	if (Timer < Delay)
	{
		// Shake as a warning
		const float Shake = 4.f * FMath::Sin(Timer * 80.f);
		SetActorLocation(Home + FVector(Shake, -Shake, 0.f));
		return;
	}

	FallSpeed += 1800.f * DeltaSeconds;
	AddActorWorldOffset(FVector(0.f, 0.f, -FallSpeed * DeltaSeconds));
	if (GetActorLocation().Z < Home.Z - 1500.f)
	{
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
	}
}
