#include "FreezeGameMode.h"
#include "FreezeHUD.h"
#include "GeoBlock.h"
#include "CrumbleTile.h"
#include "Hourglass.h"
#include "Sentinel.h"
#include "TimeMover.h"
#include "Components/InputComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AFreezeGameMode::AFreezeGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	static ConstructorHelpers::FClassFinder<APawn> PawnFinder(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PawnFinder.Succeeded())
	{
		DefaultPawnClass = PawnFinder.Class;
	}
	static ConstructorHelpers::FClassFinder<APlayerController> ControllerFinder(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController"));
	if (ControllerFinder.Succeeded())
	{
		PlayerControllerClass = ControllerFinder.Class;
	}
	HUDClass = AFreezeHUD::StaticClass();
}

void AFreezeGameMode::StartPlay()
{
	TimeLeft = StartTime;
	KillZ = CourseOrigin.Z - 700.f;

	// Blue, desaturated look while time is frozen
	FreezeLook = GetWorld()->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity);
	if (FreezeLook)
	{
		FreezeLook->bUnbound = true;
		FreezeLook->BlendWeight = 0.f;
		FreezeLook->Settings.bOverride_ColorSaturation = true;
		FreezeLook->Settings.ColorSaturation = FVector4(0.25f, 0.25f, 0.25f, 1.f);
		FreezeLook->Settings.bOverride_ColorGain = true;
		FreezeLook->Settings.ColorGain = FVector4(0.75f, 0.95f, 1.25f, 1.f);
		FreezeLook->Settings.bOverride_VignetteIntensity = true;
		FreezeLook->Settings.VignetteIntensity = 0.9f;
	}

	EnsureLighting();
	BuildCourse();

	Super::StartPlay();
}

void AFreezeGameMode::EnsureLighting()
{
	// If the level has no sun (e.g. an empty level), bring our own sun and sky
	UWorld* World = GetWorld();
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		return;
	}

	if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector::ZeroVector, FRotator(-40.f, 30.f, 0.f)))
	{
		if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetIntensity(8.f);
			Light->SetAtmosphereSunLight(true);
		}
	}
	World->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FTransform::Identity);
	if (ASkyLight* Sky = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FTransform::Identity))
	{
		if (USkyLightComponent* SkyComp = Sky->GetLightComponent())
		{
			SkyComp->SetMobility(EComponentMobility::Movable);
			SkyComp->SetIntensity(1.f);
			SkyComp->RecaptureSky();
		}
	}
}

ACharacter* AFreezeGameMode::GetPlayerCharacter() const
{
	return UGameplayStatics::GetPlayerCharacter(this, 0);
}

void AFreezeGameMode::RegisterFreezable(AActor* Actor)
{
	if (Actor)
	{
		Freezables.Add(Actor);
		if (bFrozen) Actor->CustomTimeDilation = 0.0001f;
	}
}

// ---------------------------------------------------------------------------
// Course
// ---------------------------------------------------------------------------

AGeoBlock* AFreezeGameMode::Block(const FVector& Center, const FVector& Size, const FLinearColor& Color, EGeoShape Shape, const FRotator& Rotation)
{
	AGeoBlock* B = GetWorld()->SpawnActor<AGeoBlock>(AGeoBlock::StaticClass(), W(Center), Rotation);
	if (B) B->Init(Size, Color, Shape);
	return B;
}

void AFreezeGameMode::Floor(float X0, float X1, float Y0, float Y1, float Top)
{
	Block(FVector((X0 + X1) * 0.5f, (Y0 + Y1) * 0.5f, Top - 25.f), FVector(X1 - X0, Y1 - Y0, 50.f), FreezeColors::Stone);
}

void AFreezeGameMode::SideWalls(float X0, float X1, float HalfWidth, float Height, float Top)
{
	const FVector Size(X1 - X0, 40.f, Height);
	const float CX = (X0 + X1) * 0.5f;
	Block(FVector(CX, HalfWidth + 20.f, Top + Height * 0.5f), Size, FreezeColors::Wall);
	Block(FVector(CX, -HalfWidth - 20.f, Top + Height * 0.5f), Size, FreezeColors::Wall);
}

AHourglass* AFreezeGameMode::SpawnHourglass(const FVector& Location, float Bonus)
{
	AHourglass* H = GetWorld()->SpawnActor<AHourglass>(AHourglass::StaticClass(), W(Location), FRotator::ZeroRotator);
	if (H) H->Bonus = Bonus;
	return H;
}

ASentinel* AFreezeGameMode::SpawnSentinel(const FVector& Location, const FVector2D& AreaMin, const FVector2D& AreaMax, float Speed, float ChaseRadius, bool bBoss)
{
	const FTransform T(FRotator(0.f, 180.f, 0.f), W(Location));
	ASentinel* S = GetWorld()->SpawnActorDeferred<ASentinel>(ASentinel::StaticClass(), T, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!S) return nullptr;
	S->AreaMin = FVector2D(CourseOrigin.X, CourseOrigin.Y) + AreaMin;
	S->AreaMax = FVector2D(CourseOrigin.X, CourseOrigin.Y) + AreaMax;
	S->Speed = Speed;
	S->ChaseRadius = ChaseRadius;
	S->bBoss = bBoss;
	S->FinishSpawning(T);
	if (bBoss) S->SetActorScale3D(FVector(1.3f));
	return S;
}

void AFreezeGameMode::AddCheckpoint(float TriggerX, const FVector& Spawn, const FString& Hint, float InHintEndX)
{
	FCourseCheckpoint C;
	C.TriggerX = TriggerX;
	C.Spawn = Spawn;
	C.Hint = Hint;
	C.HintEndX = InHintEndX;
	Checkpoints.Add(C);
}

void AFreezeGameMode::BuildCourse()
{
	UWorld* World = GetWorld();

	// Spawns a mover, lets the caller set it up, then finishes spawning so BeginPlay sees the settings
	auto MakeMover = [&](const FVector& Local, TFunctionRef<void(ATimeMover&)> Configure) -> ATimeMover*
	{
		const FTransform T(W(Local));
		ATimeMover* M = World->SpawnActorDeferred<ATimeMover>(ATimeMover::StaticClass(), T);
		if (!M) return nullptr;
		Configure(*M);
		M->FinishSpawning(T);
		return M;
	};

	auto MakeMoverRot = [&](const FVector& Local, const FRotator& Rot, TFunctionRef<void(ATimeMover&)> Configure) -> ATimeMover*
	{
		const FTransform T(Rot, W(Local));
		ATimeMover* M = World->SpawnActorDeferred<ATimeMover>(ATimeMover::StaticClass(), T);
		if (!M) return nullptr;
		Configure(*M);
		M->FinishSpawning(T);
		return M;
	};

	auto Blade = [&](float X, float Top, float Period, float Phase, bool bGold)
	{
		MakeMover(FVector(X, 0.f, Top + 650.f), [&](ATimeMover& M)
		{
			M.Mode = ETimeMoveMode::Swing;
			M.Deadly = ETimeDeadly::Always;
			M.Size = FVector(60.f, 400.f, 220.f);
			M.SwingLength = 520.f;
			M.SwingAngle = 25.f;
			M.Period = Period;
			M.Phase = Phase;
			M.bTimeless = bGold;
			M.Color = FreezeColors::Danger;
			M.HitMessage = bGold ? TEXT("The gold blade got you.") : TEXT("The blade got you.");
		});
	};

	auto Bridge = [&](float CenterX, float Top, float Range, float Period, float Phase) -> ATimeMover*
	{
		return MakeMover(FVector(CenterX, 0.f, Top - 20.f), [&](ATimeMover& M)
		{
			M.Mode = ETimeMoveMode::PingPong;
			M.Size = FVector(1200.f, 400.f, 40.f);
			M.OffsetA = FVector(0.f, -Range, 0.f);
			M.OffsetB = FVector(0.f, Range, 0.f);
			M.Period = Period;
			M.Phase = Phase;
			M.Color = FreezeColors::Platform;
		});
	};

	const float H = 300.f;	// height of the upper half of the course

	// --- 0. Start ---------------------------------------------------------
	Floor(0.f, 800.f, -350.f, 350.f);
	SideWalls(0.f, 800.f, 350.f, 250.f);
	Block(FVector(-20.f, 0.f, 125.f), FVector(40.f, 780.f, 250.f), FreezeColors::Wall);
	AddCheckpoint(-1000.f, FVector(150.f, 0.f, 100.f),
		TEXT("Reach the clock tower before your time runs out. Move with WASD, look with the mouse, jump with Space."), 800.f);

	// --- 1. Learn to freeze: the sliding bridge (nothing here can hurt you) ---
	Floor(800.f, 1200.f, -350.f, 350.f);
	Floor(2400.f, 2800.f, -350.f, 350.f);
	if (ATimeMover* B = Bridge(1800.f, 0.f, 600.f, 8.f, 0.f))
	{
		if (AHourglass* Bonus = SpawnHourglass(B->GetActorLocation() - CourseOrigin + FVector(250.f, 0.f, 100.f)))
		{
			Bonus->AttachToActor(B, FAttachmentTransformRules::KeepWorldTransform);
		}
	}
	AddCheckpoint(850.f, FVector(950.f, 0.f, 100.f),
		TEXT("Press E to freeze time. Frozen things stay where they are. Freeze the bridge when it lines up with the path, then run across. Each freeze costs 5 seconds."), 2800.f);

	// --- 2. Frozen things can't hurt you: one blade -------------------------
	Floor(2800.f, 4400.f, -350.f, 350.f);
	SideWalls(2800.f, 4400.f, 350.f, 450.f);
	Blade(3600.f, 0.f, 2.4f, 0.f, false);
	SpawnHourglass(FVector(4250.f, 250.f, 80.f));
	AddCheckpoint(2850.f, FVector(2950.f, 0.f, 100.f),
		TEXT("Frozen things can't hurt you. Freeze the blade, then walk around it. Press E again to unfreeze early."), 4400.f);

	// --- 3. Freeze the stepping stones into a staircase ----------------------
	Floor(4400.f, 4800.f, -350.f, 350.f);
	Block(FVector(5600.f, 0.f, H - 150.f), FVector(400.f, 700.f, 300.f), FreezeColors::Stone);	// ledge
	{
		// Three stones bob up and down out of step. Once per cycle their tops line up
		// at roughly 90 / 180 / 270 cm, a staircase up to the 300 cm ledge.
		const float StoneX[3] = { 4900.f, 5100.f, 5300.f };
		const float StonePhase[3] = { 0.2913f, 0.3360f, 0.3900f };
		for (int32 i = 0; i < 3; ++i)
		{
			MakeMover(FVector(StoneX[i], 0.f, 0.f), [&](ATimeMover& M)
			{
				M.Mode = ETimeMoveMode::PingPong;
				M.Size = FVector(180.f, 400.f, 60.f);
				M.OffsetA = FVector(0.f, 0.f, -350.f - 30.f);
				M.OffsetB = FVector(0.f, 0.f, 350.f - 30.f);
				M.Period = 6.f;
				M.Phase = StonePhase[i];
				M.Color = FreezeColors::Platform;
			});
		}
	}
	AddCheckpoint(4450.f, FVector(4550.f, 0.f, 100.f),
		TEXT("The ledge is too high to jump. Freeze the stepping stones when they line up like stairs, then hop up them to the ledge."), 5500.f);

	// --- 4. Ride the pendulum, freeze it at the far end --------------------
	{
		const float X0 = 5400.f;
		Block(FVector(X0 + 180.f, 0.f, H + 100.f), FVector(160.f, 700.f, 200.f), FreezeColors::Stone);	// step up to the jump-off point
		MakeMoverRot(FVector(X0 + 1000.f, 0.f, H + 880.f), FRotator(0.f, 90.f, 0.f), [&](ATimeMover& M)
		{
			M.Mode = ETimeMoveMode::Swing;
			M.Size = FVector(300.f, 300.f, 40.f);
			M.SwingLength = 900.f;
			M.SwingAngle = 40.f;
			M.Period = 5.f;
			M.Color = FreezeColors::Platform;
		});
		Block(FVector(X0 + 1870.f, 0.f, H + 100.f), FVector(260.f, 700.f, 200.f), FreezeColors::Stone);	// far ledge
		AddCheckpoint(X0 + 50.f, FVector(X0 + 60.f, 0.f, H + 100.f),
			TEXT("Jump onto the swinging platform and ride it. Freeze it at the far end of its swing, then step off."), X0 + 2000.f);
	}

	// --- 5. Gold ignores your freeze ------------------------------------------
	{
		const float X0 = 7400.f;
		Floor(X0, X0 + 1600.f, -350.f, 350.f, H);
		SideWalls(X0, X0 + 1600.f, 350.f, 500.f, H);
		Blade(X0 + 500.f, H, 3.0f, 0.f, true);
		Blade(X0 + 1000.f, H, 1.8f, 0.f, false);
		Blade(X0 + 1250.f, H, 2.8f, 0.25f, true);
		AddCheckpoint(X0 + 50.f, FVector(X0 + 150.f, 0.f, H + 100.f),
			TEXT("Gold things are outside of time. Your freeze won't stop them, so time your run instead."), X0 + 1600.f);
	}

	// --- 6. The Sentinel ------------------------------------------------------
	{
		const float X0 = 9000.f;
		Floor(X0, X0 + 2000.f, -700.f, 700.f, H);
		SideWalls(X0, X0 + 2000.f, 700.f, 500.f, H);
		for (float EndX : { X0, X0 + 2000.f })
		{
			Block(FVector(EndX, 525.f, H + 250.f), FVector(40.f, 350.f, 500.f), FreezeColors::Wall);
			Block(FVector(EndX, -525.f, H + 250.f), FVector(40.f, 350.f, 500.f), FreezeColors::Wall);
		}
		Block(FVector(X0 + 700.f, 350.f, H + 200.f), FVector(200.f, 200.f, 400.f), FreezeColors::Wall);
		Block(FVector(X0 + 700.f, -350.f, H + 200.f), FVector(200.f, 200.f, 400.f), FreezeColors::Wall);
		Block(FVector(X0 + 1350.f, 0.f, H + 200.f), FVector(200.f, 200.f, 400.f), FreezeColors::Wall);
		SpawnSentinel(FVector(X0 + 1100.f, 0.f, H + 100.f), FVector2D(X0 + 50.f, -680.f), FVector2D(X0 + 1950.f, 680.f), 380.f, 1400.f, false);
		SpawnHourglass(FVector(X0 + 1750.f, 550.f, H + 80.f));
		AddCheckpoint(X0 + 50.f, FVector(X0 + 100.f, 0.f, H + 100.f),
			TEXT("A Sentinel guards this hall. If it touches you, you're sent back. Frozen, it's just a statue."), X0 + 2000.f);
	}

	// --- 7. Archery range: learn to shoot ------------------------------------
	{
		const float X0 = 11000.f;
		Floor(X0, X0 + 1400.f, -700.f, 700.f, H);
		SideWalls(X0, X0 + 1400.f, 700.f, 500.f, H);
		Block(FVector(X0, 525.f, H + 250.f), FVector(40.f, 350.f, 500.f), FreezeColors::Wall);
		Block(FVector(X0, -525.f, H + 250.f), FVector(40.f, 350.f, 500.f), FreezeColors::Wall);

		// The bow, waiting on a little stand
		Block(FVector(X0 + 250.f, 0.f, H + 40.f), FVector(80.f, 80.f, 80.f), FreezeColors::Stone);
		BowPickup = Block(FVector(X0 + 250.f, 0.f, H + 130.f), FVector(12.f, 12.f, 110.f), FLinearColor(0.45f, 0.25f, 0.1f), EGeoShape::Cylinder);
		BowSpot = FVector(X0 + 250.f, 0.f, H + 100.f);

		// Two targets darting across the back wall, too fast to hit unless frozen
		for (int32 i = 0; i < 2; ++i)
		{
			MakeMover(FVector(X0 + 1250.f, 0.f, H + 200.f + 200.f * i), [&](ATimeMover& M)
			{
				M.Mode = ETimeMoveMode::PingPong;
				M.Size = FVector(30.f, 140.f, 140.f);
				M.OffsetA = FVector(0.f, -550.f, 0.f);
				M.OffsetB = FVector(0.f, 550.f, 0.f);
				M.Period = 0.9f + 0.3f * i;
				M.Phase = 0.37f * i;
				M.Color = FLinearColor(0.9f, 0.1f, 0.1f);
				M.bIsTarget = true;
			});
		}
		Gates.Add(Block(FVector(X0 + 1400.f, 0.f, H + 250.f), FVector(60.f, 1400.f, 500.f), FreezeColors::Danger));
		AddCheckpoint(X0 + 50.f, FVector(X0 + 120.f, 0.f, H + 100.f),
			TEXT("Pick up the bow. Press F to shoot where you're aiming. The targets are too fast to hit, so freeze them first. Hit both to open the gate."), X0 + 1400.f);
	}

	// --- 8. Archery arena: three Sentinels -----------------------------------
	{
		const float X0 = 12400.f;
		Floor(X0, X0 + 1800.f, -800.f, 800.f, H);
		SideWalls(X0, X0 + 1800.f, 800.f, 500.f, H);
		Block(FVector(X0 + 600.f, 400.f, H + 150.f), FVector(150.f, 150.f, 300.f), FreezeColors::Wall);
		Block(FVector(X0 + 900.f, -350.f, H + 150.f), FVector(150.f, 150.f, 300.f), FreezeColors::Wall);
		for (float SY : { -500.f, 0.f, 500.f })
		{
			if (ASentinel* S = SpawnSentinel(FVector(X0 + 1550.f, SY, H + 100.f), FVector2D(X0 + 50.f, -780.f), FVector2D(X0 + 1750.f, 780.f), 330.f, 2500.f, false))
			{
				S->bArena = true;
			}
		}
		Gates.Add(Block(FVector(X0 + 1800.f, 0.f, H + 250.f), FVector(60.f, 1600.f, 500.f), FreezeColors::Danger));
		AddCheckpoint(X0 + 50.f, FVector(X0 + 120.f, 0.f, H + 100.f),
			TEXT("Shoot all three Sentinels to open the gate. Frozen Sentinels make easy targets."), X0 + 1800.f);
	}

	// --- 9. Everything together: crushers, crumbling floor, bridge -----------
	{
		const float X0 = 14200.f;
		Floor(X0, X0 + 800.f, -350.f, 350.f, H);
		SideWalls(X0, X0 + 280.f, 350.f, 500.f, H);
		SideWalls(X0 + 520.f, X0 + 800.f, 350.f, 500.f, H);
		for (float Side : { 1.f, -1.f })
		{
			MakeMover(FVector(X0 + 400.f, 525.f * Side, H + 200.f), [&](ATimeMover& M)
			{
				M.Mode = ETimeMoveMode::PingPong;
				M.Deadly = ETimeDeadly::MovingToB;
				M.Size = FVector(200.f, 350.f, 400.f);
				M.OffsetA = FVector::ZeroVector;
				M.OffsetB = FVector(0.f, -350.f * Side, 0.f);
				M.Period = 3.2f;
				M.Color = FreezeColors::Danger;
				M.HitMessage = TEXT("Crushed!");
			});
		}
		SpawnSentinel(FVector(X0 + 750.f, 0.f, H + 100.f), FVector2D(X0 + 520.f, -340.f), FVector2D(X0 + 2000.f, 340.f), 360.f, 1200.f, false);

		// Crumbling floor: each tile drops shortly after you step on it
		for (float TX = X0 + 900.f; TX < X0 + 2000.f; TX += 200.f)
		{
			for (float TY : { -200.f, 0.f, 200.f })
			{
				GetWorld()->SpawnActor<ACrumbleTile>(ACrumbleTile::StaticClass(), W(FVector(TX, TY, H - 25.f)), FRotator::ZeroRotator);
			}
		}
		Blade(X0 + 1500.f, H, 2.8f, 0.f, true);

		Bridge(X0 + 2600.f, H, 700.f, 8.f, 0.3f);
		Floor(X0 + 3200.f, X0 + 3600.f, -350.f, 350.f, H);
		SpawnHourglass(FVector(X0 + 3450.f, 0.f, H + 80.f));
		AddCheckpoint(X0 + 50.f, FVector(X0 + 100.f, 0.f, H + 100.f),
			TEXT("The next floor crumbles a moment after you step on it. Freezing time stops it falling. Keep moving!"), X0 + 2000.f);
	}

	// --- 10. Final room: crush the boss with the gold press ------------------
	{
		const float X0 = 17800.f;
		Floor(X0, X0 + 2000.f, -900.f, 900.f, H);
		SideWalls(X0, X0 + 2000.f, 900.f, 600.f, H);
		Block(FVector(X0, 625.f, H + 300.f), FVector(40.f, 550.f, 600.f), FreezeColors::Wall);
		Block(FVector(X0, -625.f, H + 300.f), FVector(40.f, 550.f, 600.f), FreezeColors::Wall);
		MakeMover(FVector(X0 + 1000.f, 0.f, H), [&](ATimeMover& M)
		{
			M.Mode = ETimeMoveMode::PingPong;
			M.Deadly = ETimeDeadly::MovingToB;
			M.Size = FVector(500.f, 500.f, 300.f);
			M.OffsetA = FVector(0.f, 0.f, 900.f);
			M.OffsetB = FVector(0.f, 0.f, 150.f);
			M.Period = 3.2f;
			M.bTimeless = true;
			M.bCrushesSentinels = true;
			M.HitMessage = TEXT("The press flattened you.");
		});
		SpawnSentinel(FVector(X0 + 1600.f, 500.f, H + 120.f), FVector2D(X0 + 50.f, -880.f), FVector2D(X0 + 1950.f, 880.f), 430.f, 3000.f, true);
		Gate = Block(FVector(X0 + 2000.f, 0.f, H + 300.f), FVector(60.f, 1800.f, 600.f), FreezeColors::Danger);
		AddCheckpoint(X0 + 50.f, FVector(X0 + 150.f, 0.f, H + 100.f),
			TEXT("This Sentinel's gold armour stops arrows. Gold things keep moving while you freeze. Lead it under the gold press."), X0 + 2000.f);
	}

	// --- Goal: the clock tower ------------------------------------------------
	Floor(19800.f, 20800.f, -500.f, 500.f, H);
	{
		const FLinearColor TowerStone(0.55f, 0.47f, 0.38f);
		const FLinearColor Roof(0.45f, 0.12f, 0.12f);
		const FLinearColor Wood(0.35f, 0.2f, 0.1f);
		const FLinearColor Cream(0.92f, 0.88f, 0.78f);
		const FLinearColor Dark(0.08f, 0.08f, 0.1f);
		const float TX = 20500.f;		// tower centre
		const float Front = TX - 300.f;	// face of the tower the player walks up to

		// Body of the tower with a plinth and a ledge under the belfry
		Block(FVector(TX, 0.f, H + 60.f), FVector(700.f, 700.f, 120.f), FreezeColors::Stone);
		Block(FVector(TX, 0.f, H + 800.f), FVector(600.f, 600.f, 1600.f), TowerStone);
		Block(FVector(TX, 0.f, H + 1620.f), FVector(680.f, 680.f, 40.f), FreezeColors::Stone);

		// Open belfry: four corner pillars, a bell and a pointed roof
		for (float PX : { -260.f, 260.f })
		{
			for (float PY : { -260.f, 260.f })
			{
				Block(FVector(TX + PX, PY, H + 1840.f), FVector(80.f, 80.f, 400.f), TowerStone);
			}
		}
		Block(FVector(TX, 0.f, H + 2060.f), FVector(700.f, 700.f, 40.f), FreezeColors::Stone);
		Block(FVector(TX, 0.f, H + 2380.f), FVector(760.f, 760.f, 600.f), Roof, EGeoShape::Cone);
		Block(FVector(TX, 0.f, H + 1890.f), FVector(240.f, 240.f, 240.f), FreezeColors::Gold, EGeoShape::Cone);
		Block(FVector(TX, 0.f, H + 1760.f), FVector(50.f), Dark, EGeoShape::Sphere);

		// Clock face on the front, with hands that stop when you freeze time
		Block(FVector(Front - 6.f, 0.f, H + 1250.f), FVector(460.f, 460.f, 12.f), Dark, EGeoShape::Cylinder, FRotator(90.f, 0.f, 0.f));
		Block(FVector(Front - 14.f, 0.f, H + 1250.f), FVector(420.f, 420.f, 12.f), Cream, EGeoShape::Cylinder, FRotator(90.f, 0.f, 0.f));
		for (int32 Tick = 0; Tick < 12; ++Tick)
		{
			const float A = 2.f * PI * Tick / 12.f;
			Block(FVector(Front - 22.f, 175.f * FMath::Sin(A), H + 1250.f + 175.f * FMath::Cos(A)), FVector(6.f, 14.f, 30.f), Dark,
				EGeoShape::Cube, FRotator(0.f, 0.f, -FMath::RadiansToDegrees(A)));
		}
		for (int32 Hand = 0; Hand < 2; ++Hand)
		{
			MakeMover(FVector(Front - 30.f, 0.f, H + 1250.f), [&](ATimeMover& M)
			{
				M.Mode = ETimeMoveMode::Spin;
				M.Size = Hand == 0 ? FVector(8.f, 14.f, 170.f) : FVector(8.f, 22.f, 110.f);
				M.Period = Hand == 0 ? 12.f : 144.f;
				M.Color = Dark;
			});
		}
		Block(FVector(Front - 36.f, 0.f, H + 1250.f), FVector(30.f), Dark, EGeoShape::Sphere);

		// Wooden door, and the bell rope hanging down the front: ring it to finish
		Block(FVector(Front - 10.f, 0.f, H + 260.f), FVector(20.f, 220.f, 280.f), Wood);
		Block(FVector(Front - 40.f, 200.f, H + 1500.f), FVector(80.f, 40.f, 30.f), Wood);
		Block(FVector(Front - 70.f, 200.f, H + 800.f), FVector(10.f, 10.f, 1400.f), FLinearColor(0.75f, 0.65f, 0.4f), EGeoShape::Cylinder);
		Block(FVector(Front - 70.f, 200.f, H + 100.f), FVector(40.f), FreezeColors::Danger, EGeoShape::Sphere);
		BellRope = FVector(Front - 70.f, 200.f, H + 100.f);
	}
}

// ---------------------------------------------------------------------------
// Player, freeze and clock
// ---------------------------------------------------------------------------

void AFreezeGameMode::InitPlayer(ACharacter* Player)
{
	bPlayerReady = true;

	if (UCharacterMovementComponent* Move = Player->GetCharacterMovement())
	{
		Move->JumpZVelocity = 700.f;
		Move->AirControl = 0.6f;
		Move->MaxWalkSpeed = 550.f;
	}

	if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
	{
		EnableInput(PC);
		if (InputComponent)
		{
			InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AFreezeGameMode::OnFreezePressed);
			InputComponent->BindKey(EKeys::X, IE_Pressed, this, &AFreezeGameMode::OnCloseHint);
			InputComponent->BindKey(EKeys::F, IE_Pressed, this, &AFreezeGameMode::OnShootPressed);
			InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AFreezeGameMode::OnRestartPressed);
			InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AFreezeGameMode::OnRestartPressed);
		}
	}

	CurrentCheckpoint = 0;
	if (Checkpoints.Num() > 0)
	{
		HintText = Checkpoints[0].Hint;
		HintEndX = Checkpoints[0].HintEndX;
	}
	RespawnPlayer();
}

void AFreezeGameMode::SetFrozen(bool bNewFrozen)
{
	bFrozen = bNewFrozen;
	const float Dilation = bFrozen ? 0.0001f : 1.f;
	for (int32 i = Freezables.Num() - 1; i >= 0; --i)
	{
		if (AActor* A = Freezables[i].Get())
		{
			A->CustomTimeDilation = Dilation;
		}
		else
		{
			Freezables.RemoveAtSwap(i);
		}
	}
	if (FreezeLook)
	{
		FreezeLook->BlendWeight = bFrozen ? 1.f : 0.f;
	}
}

void AFreezeGameMode::OnFreezePressed()
{
	if (!IsPlaying()) return;

	// Pressing E again ends the freeze early (the cost was already paid)
	if (bFrozen)
	{
		SetFrozen(false);
		CooldownLeft = FreezeCooldown;
		return;
	}
	if (CooldownLeft > 0.f)
	{
		AddMessage(TEXT("Freeze is recharging..."), FLinearColor(0.7f, 0.7f, 0.7f));
		return;
	}
	Freezes++;
	TimeLeft -= FreezeCost;
	FreezeLeft = FreezeDuration;
	SetFrozen(true);

	// Remind the player (the first few times) when gold things nearby keep moving
	if (GoldReminders < 3)
	{
		if (ACharacter* Player = GetPlayerCharacter())
		{
			for (TActorIterator<ATimeMover> It(GetWorld()); It; ++It)
			{
				if (It->bTimeless && FVector::Dist2D(It->GetActorLocation(), Player->GetActorLocation()) < 1500.f)
				{
					AddMessage(TEXT("Gold things ignore your freeze!"), FreezeColors::Gold);
					GoldReminders++;
					break;
				}
			}
		}
	}
	if (TimeLeft <= 0.f)
	{
		TimeLeft = 0.f;
		Lose();
	}
}

void AFreezeGameMode::OnCloseHint()
{
	HintText.Empty();
}

void AFreezeGameMode::OnRestartPressed()
{
	if (IsPlaying()) return;
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

void AFreezeGameMode::AddTime(float Seconds)
{
	TimeLeft += Seconds;
	AddMessage(FString::Printf(TEXT("+%d seconds"), FMath::RoundToInt(Seconds)), FreezeColors::Ice);
}

void AFreezeGameMode::AddMessage(const FString& Text, const FLinearColor& Color)
{
	FFreezeMessage M;
	M.Text = Text;
	M.Color = Color;
	Messages.Add(M);
	if (Messages.Num() > 4) Messages.RemoveAt(0);
}

void AFreezeGameMode::PlayerHit(const FString& Reason)
{
	if (!IsPlaying() || InvulnerableLeft > 0.f) return;

	Hits++;
	TimeLeft = FMath::Max(0.f, TimeLeft - HitPenalty);
	AddMessage(FString::Printf(TEXT("%s  -%ds"), *Reason, FMath::RoundToInt(HitPenalty)), FLinearColor(1.f, 0.35f, 0.3f));
	RespawnPlayer();

	if (TimeLeft <= 0.f) Lose();
}

void AFreezeGameMode::RespawnPlayer()
{
	SetFrozen(false);
	CooldownLeft = 0.f;
	InvulnerableLeft = 1.f;

	ACharacter* Player = GetPlayerCharacter();
	if (!Player || !Checkpoints.IsValidIndex(CurrentCheckpoint)) return;

	Player->SetActorLocationAndRotation(W(Checkpoints[CurrentCheckpoint].Spawn), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
	Player->GetCharacterMovement()->StopMovementImmediately();
	if (AController* C = Player->GetController())
	{
		C->SetControlRotation(FRotator(-10.f, 0.f, 0.f));
	}

	for (TActorIterator<ASentinel> It(GetWorld()); It; ++It)
	{
		It->ResetToHome();
	}
	for (TActorIterator<ACrumbleTile> It(GetWorld()); It; ++It)
	{
		It->ResetTile();
	}
}

void AFreezeGameMode::OnSentinelDefeated(ASentinel* Sentinel)
{
	if (!Sentinel) return;

	if (Sentinel->bBoss)
	{
		AddMessage(TEXT("The Sentinel is crushed!"), FreezeColors::Gold);
		if (Gate)
		{
			Gate->Destroy();
			Gate = nullptr;
			AddMessage(TEXT("The gate is open! Ring the clock tower bell."), FreezeColors::Gold);
		}
		return;
	}

	if (Sentinel->bArena)
	{
		ArenaDefeated++;
		AddMessage(FString::Printf(TEXT("Sentinel down! %d of 3"), ArenaDefeated), FreezeColors::Gold);
		if (ArenaDefeated >= 3) OpenGate(1, TEXT("All three Sentinels are down. The gate is open!"));
		return;
	}

	AddMessage(TEXT("Sentinel down!"), FreezeColors::Gold);
}

void AFreezeGameMode::OpenGate(int32 Index, const FString& Message)
{
	if (Gates.IsValidIndex(Index) && Gates[Index])
	{
		Gates[Index]->Destroy();
		Gates[Index] = nullptr;
		AddMessage(Message, FreezeColors::Gold);
	}
}

void AFreezeGameMode::OnShootPressed()
{
	if (!IsPlaying() || !bHasBow || ShotCooldown > 0.f) return;

	ACharacter* Player = GetPlayerCharacter();
	APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
	if (!PC) return;
	ShotCooldown = 0.35f;

	FVector ViewLoc;
	FRotator ViewRot;
	PC->GetPlayerViewPoint(ViewLoc, ViewRot);
	const FVector Dir = ViewRot.Vector();
	const FVector Start = ViewLoc + Dir * 150.f;
	const FVector End = ViewLoc + Dir * 8000.f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ReadySetStopArrow), false, Player);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_Pawn);

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, Objects, Params);
	const FVector HitPoint = bHit ? Hit.ImpactPoint : End;

	// Arrow streak from the player to where it landed
	const FVector Muzzle = Player->GetActorLocation() + FVector(0.f, 0.f, 50.f) + Player->GetActorForwardVector() * 40.f;
	const FVector Path = HitPoint - Muzzle;
	if (Path.Size() > 10.f)
	{
		AGeoBlock* Streak = GetWorld()->SpawnActor<AGeoBlock>(AGeoBlock::StaticClass(), Muzzle + Path * 0.5f, FRotationMatrix::MakeFromZ(Path.GetSafeNormal()).Rotator());
		if (Streak)
		{
			Streak->SetActorEnableCollision(false);
			Streak->Init(FVector(9.f, 9.f, Path.Size()), FLinearColor(1.f, 0.85f, 0.2f), EGeoShape::Cylinder);
			Streak->SetLifeSpan(0.4f);
		}
	}

	AActor* HitActor = bHit ? Hit.GetActor() : nullptr;
	if (!Cast<ASentinel>(HitActor) && !(Cast<ATimeMover>(HitActor) && Cast<ATimeMover>(HitActor)->bIsTarget))
	{
		AddMessage(TEXT("Missed!"), FLinearColor(0.8f, 0.8f, 0.85f));
		return;
	}

	if (ASentinel* S = Cast<ASentinel>(HitActor))
	{
		if (S->bBoss)
		{
			AddMessage(TEXT("Arrows bounce off its gold armour!"), FreezeColors::Gold);
		}
		else
		{
			S->Defeat();
		}
		return;
	}

	if (ATimeMover* M = Cast<ATimeMover>(HitActor))
	{
		if (M->bIsTarget)
		{
			M->Destroy();
			TargetsHit++;
			AddMessage(FString::Printf(TEXT("Target hit! %d of 2"), TargetsHit), FreezeColors::Gold);
			if (TargetsHit >= 2) OpenGate(0, TEXT("Both targets hit. The gate is open!"));
		}
	}
}

void AFreezeGameMode::UpdateArrowKeys(ACharacter* Player)
{
	APlayerController* PC = Cast<APlayerController>(Player->GetController());
	if (!PC) return;

	const float Forward = (PC->IsInputKeyDown(EKeys::Up) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::Down) ? 1.f : 0.f);
	const float Right = (PC->IsInputKeyDown(EKeys::Right) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::Left) ? 1.f : 0.f);
	if (Forward == 0.f && Right == 0.f) return;

	const FRotator Yaw(0.f, PC->GetControlRotation().Yaw, 0.f);
	Player->AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Forward);
	Player->AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Right);
}

void AFreezeGameMode::Win()
{
	State = EFreezeState::Won;
	SetFrozen(false);
	HintText.Empty();
}

void AFreezeGameMode::Lose()
{
	State = EFreezeState::Lost;
	SetFrozen(false);
	HintText.Empty();
}

void AFreezeGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	for (int32 i = Messages.Num() - 1; i >= 0; --i)
	{
		Messages[i].Life -= DeltaSeconds;
		if (Messages[i].Life <= 0.f) Messages.RemoveAt(i);
	}

	ACharacter* Player = GetPlayerCharacter();
	if (!Player) return;
	if (!bPlayerReady)
	{
		InitPlayer(Player);
		return;
	}
	if (!IsPlaying()) return;

	PlaySeconds += DeltaSeconds;
	InvulnerableLeft -= DeltaSeconds;
	ShotCooldown = FMath::Max(0.f, ShotCooldown - DeltaSeconds);
	UpdateArrowKeys(Player);

	// Pick up the bow
	if (!bHasBow && FVector::Dist(Player->GetActorLocation(), W(BowSpot)) < 160.f)
	{
		bHasBow = true;
		if (BowPickup)
		{
			BowPickup->Destroy();
			BowPickup = nullptr;
		}
		AddMessage(TEXT("You got the bow! Press F to shoot."), FreezeColors::Gold);
	}

	// The clock stops while time is frozen; the freeze is paid for up front
	if (bFrozen)
	{
		FreezeLeft -= DeltaSeconds;
		if (FreezeLeft <= 0.f)
		{
			SetFrozen(false);
			CooldownLeft = FreezeCooldown;
		}
	}
	else
	{
		CooldownLeft = FMath::Max(0.f, CooldownLeft - DeltaSeconds);
		TimeLeft -= DeltaSeconds;
	}

	if (TimeLeft <= 0.f)
	{
		TimeLeft = 0.f;
		Lose();
		return;
	}

	const FVector Loc = Player->GetActorLocation();
	const float LocalX = Loc.X - CourseOrigin.X;

	for (int32 i = Checkpoints.Num() - 1; i > CurrentCheckpoint; --i)
	{
		if (LocalX > Checkpoints[i].TriggerX)
		{
			CurrentCheckpoint = i;
			HintText = Checkpoints[i].Hint;
			HintEndX = Checkpoints[i].HintEndX;
			AddMessage(TEXT("Checkpoint"), FreezeColors::Ice);
			break;
		}
	}

	if (!HintText.IsEmpty() && HintEndX > 0.f && LocalX > HintEndX)
	{
		HintText.Empty();
	}

	if (Loc.Z < KillZ)
	{
		PlayerHit(TEXT("You fell."));
		return;
	}

	if (FVector::Dist2D(Loc, W(BellRope)) < 160.f)
	{
		AddMessage(TEXT("DONG!"), FreezeColors::Gold);
		Win();
	}
}
