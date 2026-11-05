#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GeoBlock.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

UENUM()
enum class EGeoShape : uint8 { Cube, Cylinder, Cone, Sphere };

/** A plain coloured block (or cylinder) used to build the course at runtime. */
UCLASS()
class AGeoBlock : public AActor
{
	GENERATED_BODY()

public:
	AGeoBlock();

	/** Size in cm and colour. Call right after spawning. */
	void Init(const FVector& SizeCm, const FLinearColor& Color, EGeoShape Shape = EGeoShape::Cube);

	UPROPERTY(VisibleAnywhere, Category = "Block")
	TObjectPtr<UStaticMeshComponent> Mesh;

protected:
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> ConeMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;
};
