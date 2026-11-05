#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimeMover.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UBoxComponent;
class UPointLightComponent;

UENUM()
enum class ETimeMoveMode : uint8
{
	PingPong,	// eases back and forth between OffsetA and OffsetB
	Loop,		// moves A -> B at constant speed, then jumps back to A
	Swing,		// pendulum swinging side to side under the actor's location
	Spin		// turns full circles around the X axis (clock hands)
};

UENUM()
enum class ETimeDeadly : uint8
{
	Never,
	Always,
	MovingToB	// only dangerous while travelling towards OffsetB (closing walls, slamming press)
};

/**
 * Anything in the course that moves on its own: blades, bridges, crates, crushers, the press.
 * Normal movers stop completely while time is frozen. Gold (timeless) movers ignore the freeze.
 */
UCLASS()
class ATimeMover : public AActor
{
	GENERATED_BODY()

public:
	ATimeMover();

	UPROPERTY(EditAnywhere, Category = "Mover") ETimeMoveMode Mode = ETimeMoveMode::PingPong;
	UPROPERTY(EditAnywhere, Category = "Mover") ETimeDeadly Deadly = ETimeDeadly::Never;
	UPROPERTY(EditAnywhere, Category = "Mover") FVector Size = FVector(100.f);
	UPROPERTY(EditAnywhere, Category = "Mover") FLinearColor Color = FLinearColor(0.8f, 0.15f, 0.1f);
	UPROPERTY(EditAnywhere, Category = "Mover") FVector OffsetA = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category = "Mover") FVector OffsetB = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category = "Mover") float Period = 2.f;
	UPROPERTY(EditAnywhere, Category = "Mover") float Phase = 0.f;
	UPROPERTY(EditAnywhere, Category = "Mover") float SwingLength = 500.f;
	UPROPERTY(EditAnywhere, Category = "Mover") float SwingAngle = 30.f;
	UPROPERTY(EditAnywhere, Category = "Mover") bool bTimeless = false;
	UPROPERTY(EditAnywhere, Category = "Mover") bool bCrushesSentinels = false;
	UPROPERTY(EditAnywhere, Category = "Mover") bool bIsTarget = false;	// archery target
	UPROPERTY(EditAnywhere, Category = "Mover") FString HitMessage = TEXT("You got hit.");

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	void ApplyMotion();
	bool IsDangerousNow() const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Arm;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> KillBox;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> GoldGlow;

	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;

	FVector Origin;
	FRotator BaseRotation;
	float Time = 0.f;
};
