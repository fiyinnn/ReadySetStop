#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Sentinel.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInterface;

/**
 * The clockwork soldier. Chases the player inside its area and sends them back on touch.
 * Freezing time turns it into a statue. The gold press in the final room can crush it.
 */
UCLASS()
class ASentinel : public ACharacter
{
	GENERATED_BODY()

public:
	ASentinel();

	/** Area the sentinel guards (world X/Y). It only chases while the player is inside. */
	UPROPERTY(EditAnywhere, Category = "Sentinel") FVector2D AreaMin = FVector2D(-1000.f);
	UPROPERTY(EditAnywhere, Category = "Sentinel") FVector2D AreaMax = FVector2D(1000.f);
	UPROPERTY(EditAnywhere, Category = "Sentinel") float ChaseRadius = 1400.f;
	UPROPERTY(EditAnywhere, Category = "Sentinel") float Speed = 380.f;
	UPROPERTY(EditAnywhere, Category = "Sentinel") bool bBoss = false;	// gold armour: arrows bounce off
	UPROPERTY(EditAnywhere, Category = "Sentinel") bool bArena = false;	// one of the Guards in the archery arena

	/** Arena Guards take two arrows while moving, one while frozen. Returns true if this arrow finished it. */
	bool TakeArrow(bool bTimeFrozen);

	virtual void Tick(float DeltaSeconds) override;

	void ResetToHome();
	void Defeat();
	bool IsDefeated() const { return bDefeated; }

protected:
	virtual void BeginPlay() override;

	bool IsInArea(const FVector& P) const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Eye;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Glow;
	UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;

	FVector Home;
	bool bDefeated = false;
	int32 ArrowHits = 0;
};
