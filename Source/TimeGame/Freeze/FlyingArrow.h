#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FlyingArrow.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/** The visible arrow: flies from the bow to where the shot landed, then disappears. */
UCLASS()
class AFlyingArrow : public AActor
{
	GENERATED_BODY()

public:
	AFlyingArrow();
	virtual void Tick(float DeltaSeconds) override;

	FVector Target = FVector::ZeroVector;
	float Speed = 3500.f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Shaft;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Tip;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Feathers;
	UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;
};
