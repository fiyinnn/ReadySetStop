#include "BowModel.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ABowModel::ABowModel()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = MatFinder.Object;

	// The bow stands upright along Z and bows out towards +X; the string runs straight down the back
	WoodParts.Add(AddPart(TEXT("Grip"), FVector(0.f, 0.f, 0.f), FRotator::ZeroRotator, FVector(0.06f, 0.06f, 0.22f)));
	WoodParts.Add(AddPart(TEXT("UpperLimb"), FVector(-6.f, 0.f, 27.f), FRotator(-14.f, 0.f, 0.f), FVector(0.045f, 0.045f, 0.36f)));
	WoodParts.Add(AddPart(TEXT("LowerLimb"), FVector(-6.f, 0.f, -27.f), FRotator(14.f, 0.f, 0.f), FVector(0.045f, 0.045f, 0.36f)));
	WoodParts.Add(AddPart(TEXT("UpperTip"), FVector(-16.f, 0.f, 46.f), FRotator(-40.f, 0.f, 0.f), FVector(0.035f, 0.035f, 0.1f)));
	WoodParts.Add(AddPart(TEXT("LowerTip"), FVector(-16.f, 0.f, -46.f), FRotator(40.f, 0.f, 0.f), FVector(0.035f, 0.035f, 0.1f)));
	String = AddPart(TEXT("String"), FVector(-19.f, 0.f, 0.f), FRotator::ZeroRotator, FVector(0.008f, 0.008f, 0.96f));

	// "Cool Bow" by lncg5764 (Fab). If it is missing, the simple shape bow above is used instead.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BowFinder(TEXT("/Game/Fab/Cool_Bow/cool_bow/StaticMeshes/cool_bow.cool_bow"));
	Model = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Model"));
	Model->SetupAttachment(Root);
	Model->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (BowFinder.Succeeded())
	{
		Model->SetStaticMesh(BowFinder.Object);
	}
}

UStaticMeshComponent* ABowModel::AddPart(const TCHAR* Name, const FVector& Location, const FRotator& Rotation, const FVector& Scale)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Part->SetupAttachment(Root);
	Part->SetStaticMesh(CylFinder.Object);
	Part->SetRelativeLocation(Location);
	Part->SetRelativeRotation(Rotation);
	Part->SetRelativeScale3D(Scale);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	return Part;
}

void ABowModel::BeginPlay()
{
	Super::BeginPlay();

	if (Model && Model->GetStaticMesh())
	{
		// Use the real model: size it to about 110 cm tall and hide the stand-in shapes
		const FVector Extent = Model->GetStaticMesh()->GetBounds().BoxExtent;
		const float Longest = FMath::Max3(Extent.X, Extent.Y, Extent.Z) * 2.f;
		if (Longest > KINDA_SMALL_NUMBER)
		{
			Model->SetRelativeScale3D(FVector(110.f / Longest));
		}
		for (UStaticMeshComponent* Part : WoodParts)
		{
			if (Part) Part->SetVisibility(false);
		}
		if (String) String->SetVisibility(false);
		return;
	}
	if (Model) Model->SetVisibility(false);
	if (!BaseMaterial) return;

	UMaterialInstanceDynamic* Wood = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	Wood->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.42f, 0.22f, 0.08f));
	for (UStaticMeshComponent* Part : WoodParts)
	{
		if (Part) Part->SetMaterial(0, Wood);
	}
	UMaterialInstanceDynamic* Cord = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	Cord->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.92f, 0.9f, 0.82f));
	if (String) String->SetMaterial(0, Cord);
}
