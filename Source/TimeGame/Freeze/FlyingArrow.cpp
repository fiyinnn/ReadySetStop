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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = MatFinder.Object;

	// The arrow points along local X
	Shaft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shaft"));
	Shaft->SetupAttachment(Root);
	Shaft->SetStaticMesh(CylFinder.Object);
	Shaft->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	Shaft->SetRelativeScale3D(FVector(0.025f, 0.025f, 1.1f));
	Shaft->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Tip = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tip"));
	Tip->SetupAttachment(Root);
	Tip->SetStaticMesh(ConeFinder.Object);
	Tip->SetRelativeLocation(FVector(62.f, 0.f, 0.f));
	Tip->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	Tip->SetRelativeScale3D(FVector(0.09f, 0.03f, 0.16f));	// a flat, pointed arrowhead
	Tip->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// "Historic Medieval Weapon Arrow Wood 01" by Quixel Megascans (Fab). If missing, the shapes below are used.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ArrowFinder(TEXT("/Game/Fab/Megascans/3D/Historic_Medieval_Weapon_Arrow_Wood_01_xjthdfs/High/SM_xjthdfs_tier_1/StaticMeshes/SM_xjthdfs_tier_1.SM_xjthdfs_tier_1"));
	Model = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Model"));
	Model->SetupAttachment(Root);
	Model->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (ArrowFinder.Succeeded())
	{
		Model->SetStaticMesh(ArrowFinder.Object);
	}

	// Three feathers (fletching) at the back, spaced around the shaft
	const TCHAR* FeatherNames[] = { TEXT("Feather0"), TEXT("Feather1"), TEXT("Feather2") };
	for (int32 i = 0; i < 3; ++i)
	{
		UStaticMeshComponent* Feather = CreateDefaultSubobject<UStaticMeshComponent>(FeatherNames[i]);
		Feather->SetupAttachment(Root);
		Feather->SetStaticMesh(CubeFinder.Object);
		Feather->SetRelativeRotation(FRotator(0.f, 0.f, 120.f * i));
		Feather->SetRelativeLocation(FRotator(0.f, 0.f, 120.f * i).RotateVector(FVector(-44.f, 0.f, 5.f)));
		Feather->SetRelativeScale3D(FVector(0.2f, 0.008f, 0.08f));
		Feather->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Feathers.Add(Feather);
	}
}

void AFlyingArrow::BeginPlay()
{
	Super::BeginPlay();

	if (Model && Model->GetStaticMesh())
	{
		// Real arrow: turn its long side to point forward (+X), size it to about 100 cm, hide the stand-in shapes
		const FVector Extent = Model->GetStaticMesh()->GetBounds().BoxExtent;
		FRotator Turn = FRotator::ZeroRotator;
		float Longest = Extent.X;
		if (Extent.Y > Longest) { Longest = Extent.Y; Turn = FRotator(0.f, -90.f, 0.f); }
		if (Extent.Z > Longest) { Longest = Extent.Z; Turn = FRotator(-90.f, 0.f, 0.f); }
		if (bFlipModel) Turn += FRotator(0.f, 180.f, 0.f);
		Model->SetRelativeRotation(Turn);
		if (Longest > KINDA_SMALL_NUMBER)
		{
			Model->SetRelativeScale3D(FVector(100.f / (Longest * 2.f)));
		}
		Shaft->SetVisibility(false);
		Tip->SetVisibility(false);
		for (UStaticMeshComponent* Feather : Feathers)
		{
			if (Feather) Feather->SetVisibility(false);
		}
		SetActorRotation((Target - GetActorLocation()).Rotation());
		SetLifeSpan(2.f);
		return;
	}
	if (Model) Model->SetVisibility(false);
	if (BaseMaterial)
	{
		UMaterialInstanceDynamic* Wood = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Wood->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.85f, 0.65f, 0.3f));
		Shaft->SetMaterial(0, Wood);
		UMaterialInstanceDynamic* Steel = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Steel->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.8f, 0.8f, 0.85f));
		Tip->SetMaterial(0, Steel);
		UMaterialInstanceDynamic* Red = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Red->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.85f, 0.1f, 0.08f));
		for (UStaticMeshComponent* Feather : Feathers)
		{
			if (Feather) Feather->SetMaterial(0, Red);
		}
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
