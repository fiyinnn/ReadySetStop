#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RisingSand.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/**
 * Gold sand that starts rising once the player walks into its room. It is gold, so freezing time
 * does not stop it. Touching it sends the player back.
 */
UCLASS()
class ARisingSand : public AActor
{
	GENERATED_BODY()

public:
	ARisingSand();

	virtual void Tick(float DeltaSeconds) override;
	void ResetSand();

	// World-space room the sand fills
	FVector2D AreaMin = FVector2D::ZeroVector;
	FVector2D AreaMax = FVector2D::ZeroVector;
	float StartZ = 0.f;		// top of the sand before it starts
	float MaxZ = 0.f;		// where it stops
	float Speed = 25.f;		// cm per second
	float TriggerX = 0.f;	// starts rising when the player passes this X

protected:
	virtual void BeginPlay() override;
	void PlaceSurface();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;

	float SandTop = 0.f;
	bool bRising = false;
};
