#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GeoBlock.h"
#include "FreezeGameMode.generated.h"

class ACharacter;
class APostProcessVolume;
class ASentinel;
class AGeoBlock;

namespace FreezeColors
{
	inline const FLinearColor Gold(1.0f, 0.68f, 0.08f);
	inline const FLinearColor Danger(0.75f, 0.12f, 0.08f);
	inline const FLinearColor Platform(0.18f, 0.42f, 0.9f);
	inline const FLinearColor Crate(0.62f, 0.36f, 0.14f);
	inline const FLinearColor Stone(0.33f, 0.33f, 0.38f);
	inline const FLinearColor Wall(0.2f, 0.2f, 0.25f);
	inline const FLinearColor Metal(0.45f, 0.45f, 0.5f);
	inline const FLinearColor Ice(0.35f, 0.9f, 1.0f);
	inline const FLinearColor Tower(0.38f, 0.2f, 0.55f);
}

UENUM()
enum class EFreezeState : uint8 { Title, Playing, Won, Lost };

struct FCourseCheckpoint
{
	float TriggerX = 0.f;	// reached when the player walks past this X
	FVector Spawn = FVector::ZeroVector;
	FString Hint;			// empty = no tip for this section
	float HintEndX = 0.f;	// tip disappears once the player passes this X
};

struct FFreezeMessage
{
	FString Text;
	FLinearColor Color = FLinearColor::White;
	float Life = 3.f;
	bool bWarning = false;	// big and red
};

/**
 * Runs the whole game: builds the course, owns the clock and the freeze ability,
 * checkpoints, respawning, winning and losing.
 */
UCLASS()
class AFreezeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFreezeGameMode();

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// --- Tuning (changed between playtests) ---
	float StartTime = 480.f;	// seconds on the clock at the start
	float FreezeCost = 5.f;		// seconds taken off the clock per freeze
	float FreezeDuration = 4.f;	// how long a freeze lasts
	float FreezeCooldown = 1.f;	// wait after a freeze ends
	float HitPenalty = 10.f;	// seconds lost when hit, caught or falling

	// --- Called by course actors ---
	void RegisterFreezable(AActor* Actor);
	void PlayerHit(const FString& Reason, bool bByGold = false);
	void AddTime(float Seconds);
	void OnSentinelDefeated(ASentinel* Sentinel);

	bool IsFrozen() const { return bFrozen; }
	bool IsPlaying() const { return State == EFreezeState::Playing; }
	ACharacter* GetPlayerCharacter() const;

	// --- Read by the HUD ---
	EFreezeState State = EFreezeState::Title;
	float TimeLeft = 0.f;
	bool bFrozen = false;
	float FreezeLeft = 0.f;
	float CooldownLeft = 0.f;
	FString HintText;
	TArray<FFreezeMessage> Messages;
	int32 Freezes = 0;
	bool bHasBow = false;
	bool bAimOnTarget = false;	// crosshair turns red
	int32 Hits = 0;
	float PlaySeconds = 0.f;

	void AddMessage(const FString& Text, const FLinearColor& Color = FLinearColor::White);
	void AddWarning(const FString& Text);
	void ShowGoldWarning(const FString& Text);

	/** Plays /Game/Audio/<Name> if that sound has been imported; silently does nothing otherwise. */
	void PlaySfx(const TCHAR* Name, float Volume = 1.f, float MaxSeconds = 2.f);
	class USoundBase* FindSound(const TCHAR* Name);

protected:
	void BuildCourse();
	void EnsureLighting();
	void InitPlayer(ACharacter* Player);
	void SetFrozen(bool bNewFrozen);
	void RespawnPlayer();
	void Win();
	void Lose();

	void OnFreezePressed();
	void OnCloseHint();
	void OnRestartPressed();
	void OnShootPressed();
	void DebugNextCheckpoint();
	void DebugPreviousCheckpoint();
	void DebugJumpTo(int32 Index);
	void DebugRingBell();
	void UpdateLowTimeTicking();
	void StopTicking();
	void DuckMusicFor(float Seconds);
	AActor* FindAimTarget(FVector& OutHitPoint) const;
	void UpdateArrowKeys(ACharacter* Player);
	void OpenGate(int32 Index, const FString& Message);

	// Course building helpers (all positions relative to the course origin)
	AGeoBlock* Block(const FVector& Center, const FVector& Size, const FLinearColor& Color, EGeoShape Shape = EGeoShape::Cube, const FRotator& Rotation = FRotator::ZeroRotator);
	void Floor(float X0, float X1, float Y0, float Y1, float Top = 0.f);
	void SideWalls(float X0, float X1, float HalfWidth, float Height, float Top = 0.f);
	class AHourglass* SpawnHourglass(const FVector& Location, float Bonus = 20.f);
	ASentinel* SpawnSentinel(const FVector& Location, const FVector2D& AreaMin, const FVector2D& AreaMax, float Speed, float ChaseRadius, bool bBoss);
	void AddCheckpoint(float TriggerX, const FVector& Spawn, const FString& Hint, float HintEndX);
	FVector W(const FVector& Local) const { return CourseOrigin + Local; }

	FVector CourseOrigin = FVector(0.f, 0.f, 2000.f);
	float KillZ = 0.f;
	FVector BellRope = FVector::ZeroVector;	// touch the bell rope to win

	TArray<FCourseCheckpoint> Checkpoints;
	int32 CurrentCheckpoint = -1;
	float HintEndX = 0.f;
	float InvulnerableLeft = 0.f;
	int32 GoldLessonCheckpoint = -1;
	int32 RangeCheckpoint = 1000;
	int32 ArenaCheckpoint = 1000;
	int32 BossCheckpoint = 1000;
	TMap<int32, int32> GoldWarningsShown;
	int32 TargetsHit = 0;
	int32 ArenaDefeated = 0;
	float ShotCooldown = 0.f;
	FVector BowSpot = FVector::ZeroVector;

	bool bPlayerReady = false;

	UPROPERTY() TArray<TWeakObjectPtr<AActor>> Freezables;
	UPROPERTY() TObjectPtr<APostProcessVolume> FreezeLook;
	UPROPERTY() TObjectPtr<AGeoBlock> Gate;
	UPROPERTY() TObjectPtr<AGeoBlock> BowPickup;
	UPROPERTY() TMap<FName, TObjectPtr<class USoundBase>> SoundCache;
	UPROPERTY() TObjectPtr<class UAudioComponent> Music;
	UPROPERTY() TObjectPtr<class UAudioComponent> TickLoop;
	int32 LastTickSecond = -1;
	float MusicDuckedUntil = 0.f;
	bool bMusicDucked = false;
	UPROPERTY() TArray<TObjectPtr<AGeoBlock>> Gates;	// 0 = archery range, 1 = archery arena
};
