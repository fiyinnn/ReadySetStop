#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CrumbleTile.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/** A floor tile that shakes and falls a moment after the player stands on it. Freezing time pauses it. */
UCLASS()
class ACrumbleTile : public AActor
{
	GENERATED_BODY()

public:
	ACrumbleTile();

	virtual void Tick(float DeltaSeconds) override;

	/** Put the tile back (called when the player respawns). */
	void ResetTile();

	UPROPERTY(EditAnywhere, Category = "Crumble") float Delay = 0.6f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;

	FVector Home;
	bool bTriggered = false;
	float Timer = 0.f;
	float FallSpeed = 0.f;
};
