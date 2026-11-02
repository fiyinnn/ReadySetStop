#include "Hourglass.h"
#include "FreezeGameMode.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AHourglass::AHourglass()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Movable);

	Spinner = CreateDefaultSubobject<USceneComponent>(TEXT("Spinner"));
	Spinner->SetupAttachment(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = MatFinder.Object;

	// Two cones tip to tip make an hourglass
	Bottom = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bottom"));
	Bottom->SetupAttachment(Spinner);
	Bottom->SetStaticMesh(ConeFinder.Object);
	Bottom->SetRelativeLocation(FVector(0.f, 0.f, -22.f));
	Bottom->SetRelativeScale3D(FVector(0.45f, 0.45f, 0.45f));
	Bottom->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Top = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Top"));
	Top->SetupAttachment(Spinner);
	Top->SetStaticMesh(ConeFinder.Object);
	Top->SetRelativeLocation(FVector(0.f, 0.f, 22.f));
	Top->SetRelativeRotation(FRotator(180.f, 0.f, 0.f));
	Top->SetRelativeScale3D(FVector(0.45f, 0.45f, 0.45f));
	Top->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Root);
	Glow->SetIntensity(4000.f);
	Glow->SetAttenuationRadius(350.f);
	Glow->SetLightColor(FreezeColors::Ice);
	Glow->SetCastShadows(false);
}

void AHourglass::BeginPlay()
{
	Super::BeginPlay();
	if (BaseMaterial)
	{
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Mat->SetVectorParameterValue(TEXT("Color"), FreezeColors::Ice);
		Top->SetMaterial(0, Mat);
		Bottom->SetMaterial(0, Mat);
	}
}

void AHourglass::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	Spinner->SetRelativeRotation(FRotator(0.f, Age * 90.f, 0.f));
	Spinner->SetRelativeLocation(FVector(0.f, 0.f, 10.f * FMath::Sin(Age * 2.5f)));

	AFreezeGameMode* GM = GetWorld()->GetAuthGameMode<AFreezeGameMode>();
	if (!GM || !GM->IsPlaying()) return;

	if (ACharacter* Player = GM->GetPlayerCharacter())
	{
		if (FVector::Dist(Player->GetActorLocation(), GetActorLocation()) < 140.f)
		{
			GM->AddTime(Bonus);
			Destroy();
		}
	}
}
