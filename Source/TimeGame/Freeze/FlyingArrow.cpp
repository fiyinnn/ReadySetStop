#include "FlyingArrow.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AFlyingArrow::AFlyingArrow()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = MatFinder.Object;

	// The arrow points along local X
	Shaft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shaft"));
	Shaft->SetupAttachment(Root);
	Shaft->SetStaticMesh(CylFinder.Object);
	Shaft->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	Shaft->SetRelativeScale3D(FVector(0.05f, 0.05f, 0.9f));
	Shaft->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Tip = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tip"));
	Tip->SetupAttachment(Root);
	Tip->SetStaticMesh(ConeFinder.Object);
	Tip->SetRelativeLocation(FVector(50.f, 0.f, 0.f));
	Tip->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	Tip->SetRelativeScale3D(FVector(0.12f, 0.12f, 0.16f));
	Tip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AFlyingArrow::BeginPlay()
{
	Super::BeginPlay();
	if (BaseMaterial)
	{
		UMaterialInstanceDynamic* Wood = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Wood->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.85f, 0.65f, 0.3f));
		Shaft->SetMaterial(0, Wood);
		UMaterialInstanceDynamic* Steel = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Steel->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.8f, 0.8f, 0.85f));
		Tip->SetMaterial(0, Steel);
	}
	SetActorRotation((Target - GetActorLocation()).Rotation());
	SetLifeSpan(2.f);
}

void AFlyingArrow::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const FVector ToTarget = Target - GetActorLocation();
	const float Step = Speed * DeltaSeconds;
	if (ToTarget.Size() <= Step)
	{
		Destroy();
		return;
	}
	SetActorLocation(GetActorLocation() + ToTarget.GetSafeNormal() * Step);
}
