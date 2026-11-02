#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Hourglass.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInterface;

/** Pickup that adds time back to the clock. */
UCLASS()
class AHourglass : public AActor
{
	GENERATED_BODY()

public:
	AHourglass();

	UPROPERTY(EditAnywhere, Category = "Hourglass") float Bonus = 20.f;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Spinner;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Top;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Bottom;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Glow;
	UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;

	float Age = 0.f;
};
