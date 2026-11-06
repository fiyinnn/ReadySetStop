#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BowModel.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

/** A simple recurve bow built from basic shapes: curved limbs, a grip and a string. */
UCLASS()
class ABowModel : public AActor
{
	GENERATED_BODY()

public:
	ABowModel();

protected:
	virtual void BeginPlay() override;

	UStaticMeshComponent* AddPart(const TCHAR* Name, const FVector& Location, const FRotator& Rotation, const FVector& Scale);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> WoodParts;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> String;
	UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;
};
