#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GeoBlock.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/** A plain coloured block (or cylinder) used to build the course at runtime. */
UCLASS()
class AGeoBlock : public AActor
{
	GENERATED_BODY()

public:
	AGeoBlock();

	/** Size in cm and colour. Call right after spawning. */
	void Init(const FVector& SizeCm, const FLinearColor& Color, bool bCylinder = false);

	UPROPERTY(VisibleAnywhere, Category = "Block")
	TObjectPtr<UStaticMeshComponent> Mesh;

protected:
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;
};
