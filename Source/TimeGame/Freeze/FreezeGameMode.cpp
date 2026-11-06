#include "FreezeGameMode.h"
#include "FreezeHUD.h"
#include "GeoBlock.h"
#include "CrumbleTile.h"
#include "BowModel.h"
#include "FlyingArrow.h"
#include "Hourglass.h"
#include "Sentinel.h"
#include "TimeMover.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
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

void AFreezeGameMode::AddCheckpoint(float TriggerX, const FVector& Spawn, const FString& Hint, float InHintEndX, float TriggerZ)
{
	FCourseCheckpoint C;
	C.TriggerX = TriggerX;
	C.TriggerZ = TriggerZ;
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
	Blade(3600.f, 0.f, 1.8f, 0.f, false);
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
		Blade(X0 + 1000.f, H, 1.3f, 0.f, false);
		Blade(X0 + 1250.f, H, 2.8f, 0.25f, true);
		GoldLessonCheckpoint = Checkpoints.Num();
		AddCheckpoint(X0 + 50.f, FVector(X0 + 150.f, 0.f, H + 100.f),
			TEXT("Gold things are outside of time. Your freeze won't stop them, so time your run instead."), X0 + 1600.f);
	}

	// --- 6. Moving blocks: climb to the exit at the top ----------------------
	{
		const float X0 = 9000.f;
		const FLinearColor Wood(0.42f, 0.27f, 0.14f);
		Floor(X0, X0 + 1600.f, -500.f, 500.f, H);
		SideWalls(X0, X0 + 1600.f, 500.f, 1700.f, H);
		Block(FVector(X0, 425.f, H + 850.f), FVector(40.f, 150.f, 1700.f), FreezeColors::Wall);
		Block(FVector(X0, -425.f, H + 850.f), FVector(40.f, 150.f, 1700.f), FreezeColors::Wall);
		Block(FVector(X0, 0.f, H + 1100.f), FVector(40.f, 700.f, 1200.f), FreezeColors::Wall);
		Block(FVector(X0 + 1600.f, 0.f, H + 700.f), FVector(40.f, 1000.f, 1400.f), FreezeColors::Wall);
		Block(FVector(X0 + 1700.f, 0.f, H + 1380.f), FVector(300.f, 500.f, 40.f), FreezeColors::Stone);	// exit balcony

		auto Ledge = [&](float X, float Y, float Top, float SX, float SY)
		{
			Block(FVector(X, Y, H + Top - 20.f), FVector(SX, SY, 40.f), Wood);
		};
		auto Lift = [&](float X, float Y, float Top, float SX, float SY, const FVector& Travel, float Period, float Phase)
		{
			MakeMover(FVector(X, Y, H + Top - 20.f), [&](ATimeMover& M)
			{
				M.Mode = ETimeMoveMode::PingPong;
				M.Size = FVector(SX, SY, 40.f);
				M.OffsetA = FVector::ZeroVector;
				M.OffsetB = Travel;
				M.Period = Period;
				M.Phase = Phase;
				M.Color = FreezeColors::Platform;
			});
		};
		// Up the south side, back along the north side, then a long ride to the exit
		Ledge(X0 + 400.f, -250.f, 200.f, 200.f, 200.f);
		Lift(X0 + 700.f, -250.f, 200.f, 200.f, 200.f, FVector(0.f, 0.f, 300.f), 3.f, 0.f);
		Ledge(X0 + 1000.f, -250.f, 500.f, 200.f, 200.f);
		Lift(X0 + 1150.f, -250.f, 700.f, 200.f, 160.f, FVector(300.f, 0.f, 0.f), 3.f, 0.25f);
		Ledge(X0 + 1450.f, 250.f, 850.f, 200.f, 400.f);
		Lift(X0 + 1150.f, 250.f, 850.f, 200.f, 200.f, FVector(0.f, 0.f, 300.f), 3.f, 0.5f);
		Ledge(X0 + 850.f, 250.f, 1150.f, 200.f, 200.f);
		Lift(X0 + 600.f, 250.f, 1300.f, 200.f, 200.f, FVector(-300.f, 0.f, 0.f), 3.f, 0.f);
		Ledge(X0 + 150.f, 0.f, 1350.f, 200.f, 600.f);
		Lift(X0 + 400.f, -250.f, 1400.f, 300.f, 200.f, FVector(900.f, 0.f, 0.f), 7.f, 0.f);
		Ledge(X0 + 1500.f, -150.f, 1400.f, 200.f, 500.f);

		AddCheckpoint(X0 + 50.f, FVector(X0 + 80.f, 0.f, H + 100.f),
			TEXT("Get to the top to leave this room. Ride the blue moving blocks, and freeze them to make the jumps easier."), X0 + 1600.f);
	}

	// --- 7. The Sentinel ------------------------------------------------------
	{
		const float X0 = 10600.f;
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
		SpawnSentinel(FVector(X0 + 1100.f, 0.f, H + 100.f), FVector2D(X0 + 50.f, -680.f), FVector2D(X0 + 1950.f, 680.f), 450.f, 1600.f, false);
		SpawnHourglass(FVector(X0 + 1750.f, 550.f, H + 80.f));
		AddCheckpoint(X0 + 50.f, FVector(X0 + 100.f, 0.f, H + 100.f),
			TEXT("A Guard guards this hall. If it touches you, you're sent back. Frozen, it's just a statue."), X0 + 2000.f);
	}

	// --- 7. Archery range: learn to shoot ------------------------------------
	{
		const float X0 = 12600.f;
		Floor(X0, X0 + 1400.f, -700.f, 700.f, H);
		SideWalls(X0, X0 + 1400.f, 700.f, 500.f, H);
		Block(FVector(X0, 525.f, H + 250.f), FVector(40.f, 350.f, 500.f), FreezeColors::Wall);
		Block(FVector(X0, -525.f, H + 250.f), FVector(40.f, 350.f, 500.f), FreezeColors::Wall);

		// The bow, waiting on a little stand
		Block(FVector(X0 + 250.f, 0.f, H + 40.f), FVector(80.f, 80.f, 80.f), FreezeColors::Stone);
		BowPickup = GetWorld()->SpawnActor<ABowModel>(ABowModel::StaticClass(), W(FVector(X0 + 250.f, 0.f, H + 140.f)), FRotator::ZeroRotator);
		BowSpot = FVector(X0 + 250.f, 0.f, H + 100.f);

		// Two targets darting across the back wall, too fast to hit unless frozen
		for (int32 i = 0; i < 2; ++i)
		{
			MakeMover(FVector(X0 + 1250.f, 0.f, H + 180.f + 260.f * i), [&](ATimeMover& M)
			{
				M.Mode = ETimeMoveMode::PingPong;
				M.Size = FVector(30.f, 220.f, 220.f);
				M.OffsetA = FVector(0.f, -500.f, 0.f);
				M.OffsetB = FVector(0.f, 500.f, 0.f);
				M.Period = 2.0f + 0.6f * i;
				M.Phase = 0.37f * i;
				M.Color = FLinearColor(0.9f, 0.1f, 0.1f);
				M.bIsTarget = true;
			});
		}
		Gates.Add(Block(FVector(X0 + 1400.f, 0.f, H + 250.f), FVector(60.f, 1400.f, 500.f), FreezeColors::Danger));
		RangeCheckpoint = Checkpoints.Num();
		SpotlightCheckpoint = RangeCheckpoint;
		AddCheckpoint(X0 + 50.f, FVector(X0 + 120.f, 0.f, H + 100.f),
			TEXT("Pick up the bow. Aim with the mouse: the + in the middle of the screen turns red when it's on a target. Left-click or press F to shoot. Freeze the targets first to make it easy. Hit both to open the gate."), X0 + 1400.f);
	}

	// --- 8. Archery arena: three Sentinels -----------------------------------
	{
		const float X0 = 14000.f;
		Floor(X0, X0 + 1800.f, -800.f, 800.f, H);
		SideWalls(X0, X0 + 1800.f, 800.f, 500.f, H);
		Block(FVector(X0 + 600.f, 400.f, H + 150.f), FVector(150.f, 150.f, 300.f), FreezeColors::Wall);
		Block(FVector(X0 + 900.f, -350.f, H + 150.f), FVector(150.f, 150.f, 300.f), FreezeColors::Wall);
		// Four Guards: three fast ones at the back, a slower fourth from the far corner so they don't all arrive at once
		struct FArenaGuard { FVector Spot; float Speed; };
		const FArenaGuard ArenaGuards[] = {
			{ FVector(X0 + 1550.f, -500.f, H + 100.f), 450.f },
			{ FVector(X0 + 1550.f,    0.f, H + 100.f), 450.f },
			{ FVector(X0 + 1550.f,  500.f, H + 100.f), 450.f },
			{ FVector(X0 + 1700.f,  700.f, H + 100.f), 350.f },
		};
		for (const FArenaGuard& G : ArenaGuards)
		{
			if (ASentinel* S = SpawnSentinel(G.Spot, FVector2D(X0 + 50.f, -780.f), FVector2D(X0 + 1750.f, 780.f), G.Speed, 2500.f, false))
			{
				S->bArena = true;
				ArenaGuardCount++;
			}
		}
		Gates.Add(Block(FVector(X0 + 1800.f, 0.f, H + 250.f), FVector(60.f, 1600.f, 500.f), FreezeColors::Danger));
		ArenaCheckpoint = Checkpoints.Num();
		AddCheckpoint(X0 + 50.f, FVector(X0 + 120.f, 0.f, H + 100.f),
			TEXT("Shoot all four Guards to open the gate. A moving Guard takes two arrows, but a frozen Guard goes down in one."), X0 + 1800.f);
	}

	// --- 9. Everything together: crushers, crumbling floor, bridge -----------
	{
		const float X0 = 15800.f;
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
				M.Period = 2.4f;
				M.Color = FreezeColors::Danger;
				M.HitMessage = TEXT("Crushed!");
			});
		}
		SpawnSentinel(FVector(X0 + 750.f, 0.f, H + 100.f), FVector2D(X0 + 520.f, -340.f), FVector2D(X0 + 2000.f, 340.f), 440.f, 1400.f, false);

		// Crumbling floor: each tile drops shortly after you step on it
		for (float TX = X0 + 900.f; TX < X0 + 2000.f; TX += 200.f)
		{
			for (float TY : { -200.f, 0.f, 200.f })
			{
				GetWorld()->SpawnActor<ACrumbleTile>(ACrumbleTile::StaticClass(), W(FVector(TX, TY, H - 25.f)), FRotator::ZeroRotator);
			}
		}
		Blade(X0 + 1500.f, H, 2.8f, 0.f, true);

		Bridge(X0 + 2600.f, H, 700.f, 6.f, 0.3f);
		Floor(X0 + 3200.f, X0 + 3600.f, -350.f, 350.f, H);
		SpawnHourglass(FVector(X0 + 3450.f, 0.f, H + 80.f));
		AddCheckpoint(X0 + 50.f, FVector(X0 + 100.f, 0.f, H + 100.f),
			TEXT("The next floor crumbles a moment after you step on it. Freezing time stops it falling. Keep moving!"), X0 + 2000.f);
	}

	// --- 10. Final room: crush the boss with the gold press ------------------
	{
		const float X0 = 19400.f;
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
		BossCheckpoint = Checkpoints.Num();
		AddCheckpoint(X0 + 50.f, FVector(X0 + 150.f, 0.f, H + 100.f),
			TEXT("This Guard's gold armour stops arrows. Gold things keep moving while you freeze. Lead it under the gold press."), X0 + 2000.f);
	}

	// --- Goal: the clock tower ------------------------------------------------
	Floor(21400.f, 22400.f, -500.f, 500.f, H);
	{
		const FLinearColor TowerStone(0.55f, 0.47f, 0.38f);
		const FLinearColor Roof(0.45f, 0.12f, 0.12f);
		const FLinearColor Wood(0.35f, 0.2f, 0.1f);
		const FLinearColor Cream(0.92f, 0.88f, 0.78f);
		const FLinearColor Dark(0.08f, 0.08f, 0.1f);
		const float TX = 22100.f;		// tower centre
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

		// Wooden door
		Block(FVector(Front - 10.f, 0.f, H + 260.f), FVector(20.f, 220.f, 280.f), Wood);

		// Climb the tower's side on ledges and moving platforms to reach the bell
		auto Ledge = [&](float DX, float Y, float Top)
		{
			Block(FVector(TX + DX, Y, H + Top - 20.f), FVector(300.f, 240.f, 40.f), Wood);
		};
		auto Lift = [&](float DX, float Y, float Top, const FVector& Travel, float Period)
		{
			MakeMover(FVector(TX + DX, Y, H + Top - 20.f), [&](ATimeMover& M)
			{
				M.Mode = ETimeMoveMode::PingPong;
				M.Size = FVector(300.f, 240.f, 40.f);
				M.OffsetA = FVector::ZeroVector;
				M.OffsetB = Travel;
				M.Period = Period;
				M.Color = FreezeColors::Platform;
			});
		};
		Ledge(-320.f, -430.f, 200.f);
		Lift(0.f, -430.f, 200.f, FVector(0.f, 0.f, 350.f), 4.f);
		Ledge(320.f, -430.f, 550.f);
		Lift(320.f, -680.f, 750.f, FVector(-640.f, 0.f, 0.f), 6.f);
		Ledge(-320.f, -430.f, 950.f);
		Lift(0.f, -680.f, 1000.f, FVector(0.f, 0.f, 350.f), 4.f);
		Ledge(100.f, -430.f, 1450.f);
		AddCheckpoint(21450.f, FVector(21500.f, 0.f, H + 100.f),
			TEXT("Climb the tower's side and ring the bell at the top. Freeze the blue platforms to make the jumps easier."), 0.f);
		AddCheckpoint(21450.f, FVector(TX - 320.f, -430.f, H + 1050.f), TEXT("Halfway up! A fall now only sends you back here."), 0.f, H + 900.f);

		BellRope = FVector(TX, 0.f, H + 1890.f);	// the bell in the belfry
	}
}

// ---------------------------------------------------------------------------
// Player, freeze and clock
// ---------------------------------------------------------------------------

void AFreezeGameMode::InitPlayer(ACharacter* Player)
{
	bPlayerReady = true;
	if (APlayerController* StartPC = Cast<APlayerController>(Player->GetController()))
	{
		StartPC->SetIgnoreMoveInput(true);	// wait on the title screen until Enter
	}

	if (UCharacterMovementComponent* Move = Player->GetCharacterMovement())
	{
		Move->JumpZVelocity = 910.f;
		Move->GravityScale = 1.8f;	// falls back down faster; jump is a little lower than before
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
			InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AFreezeGameMode::OnShootPressed);
			InputComponent->BindKey(EKeys::F, IE_Pressed, this, &AFreezeGameMode::OnShootPressed);
#if WITH_EDITOR
			// Testing only (not in builds): N jumps to the next checkpoint, B to the previous one
			InputComponent->BindKey(EKeys::N, IE_Pressed, this, &AFreezeGameMode::DebugNextCheckpoint);
			InputComponent->BindKey(EKeys::B, IE_Pressed, this, &AFreezeGameMode::DebugPreviousCheckpoint);
			InputComponent->BindKey(EKeys::RightBracket, IE_Pressed, this, &AFreezeGameMode::DebugRingBell);
			{
				// 1-9 = sections 1-9, 0 = 10, minus = 11, equals = 12
				const FKey SectionKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six,
					EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero, EKeys::Hyphen, EKeys::Equals };
				for (int32 k = 0; k < UE_ARRAY_COUNT(SectionKeys); ++k)
				{
					const int32 Target = k + 1;
					FInputKeyBinding Binding(FInputChord(SectionKeys[k]), IE_Pressed);
					Binding.KeyDelegate.GetDelegateForManualSet().BindLambda([this, Target]() { DebugJumpTo(Target); });
					InputComponent->KeyBindings.Emplace(MoveTemp(Binding));
				}
			}
#endif
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
	if (Music)
	{
		Music->SetPaused(bFrozen);	// silence while frozen, back to the music after
	}
	LastTickSecond = -1;
	if (bFrozen) StopTicking();
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
		PlaySfx(TEXT("SFX_Denied"), 0.6f);
		return;
	}
	Freezes++;
	TimeLeft -= FreezeCost;
	FreezeLeft = FreezeDuration;
	SetFrozen(true);

	// Remind the player when gold things nearby keep moving
	{
		if (ACharacter* Player = GetPlayerCharacter())
		{
			for (TActorIterator<ATimeMover> It(GetWorld()); It; ++It)
			{
				if (It->bTimeless && FVector::Dist2D(It->GetActorLocation(), Player->GetActorLocation()) < 1500.f)
				{
					ShowGoldWarning(TEXT("Gold things ignore your freeze!"));
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

void AFreezeGameMode::DebugNextCheckpoint() { DebugJumpTo(CurrentCheckpoint + 1); }
void AFreezeGameMode::DebugPreviousCheckpoint() { DebugJumpTo(CurrentCheckpoint - 1); }

void AFreezeGameMode::DebugRingBell()
{
	// Testing only: drop the player beside the bell, which rings and wins
	if (!IsPlaying()) return;
	DebugJumpTo(Checkpoints.Num() - 1);
	if (ACharacter* Player = GetPlayerCharacter())
	{
		Player->SetActorLocation(W(BellRope) + FVector(0.f, -150.f, -150.f), false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void AFreezeGameMode::DebugJumpTo(int32 Index)
{
	if (!IsPlaying() || !Checkpoints.IsValidIndex(Index)) return;

	// Clear whatever would normally stand between you and that checkpoint
	if (Index > RangeCheckpoint && !bHasBow)
	{
		bHasBow = true;
		ACharacter* Player = GetPlayerCharacter();
		if (BowPickup && Player)
		{
			BowPickup->SetActorEnableCollision(false);
			BowPickup->AttachToComponent(Player->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("hand_l"));
		}
	}
	if (Index > RangeCheckpoint) OpenGate(0, TEXT("(test) gate opened"));
	if (Index > ArenaCheckpoint) OpenGate(1, TEXT("(test) gate opened"));
	if (Index > BossCheckpoint && Gate)
	{
		Gate->Destroy();
		Gate = nullptr;
	}

	CurrentCheckpoint = Index;
	HintText = Checkpoints[Index].Hint;
	HintEndX = Checkpoints[Index].HintEndX;
	RespawnPlayer();
	AddMessage(FString::Printf(TEXT("(test) checkpoint %d of %d"), Index, Checkpoints.Num() - 1), FreezeColors::Ice);
}

void AFreezeGameMode::OnCloseHint()
{
	HintText.Empty();
}

void AFreezeGameMode::OnRestartPressed()
{
	if (IsPlaying()) return;
	if (State == EFreezeState::Title)
	{
		State = EFreezeState::Playing;
		PlaySfx(TEXT("SFX_Start"));
		if (USoundBase* Track = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/MUS_Background.MUS_Background"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			// Keep the audio component alive after the track ends so it can be restarted (loops)
			Music = UGameplayStatics::SpawnSound2D(this, Track, 0.35f, 1.f, 0.f, nullptr, false, false);
		}
		if (ACharacter* Player = GetPlayerCharacter())
		{
			if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
			{
				PC->SetIgnoreMoveInput(false);
			}
		}
		return;
	}
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

void AFreezeGameMode::AddTime(float Seconds)
{
	TimeLeft += Seconds;
	PlaySfx(TEXT("SFX_Hourglass"));
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

void AFreezeGameMode::AddWarning(const FString& Text)
{
	FFreezeMessage M;
	M.Text = Text;
	M.Color = FLinearColor(1.f, 0.15f, 0.1f);
	M.Life = 4.f;
	M.bWarning = true;
	Messages.Add(M);
	if (Messages.Num() > 4) Messages.RemoveAt(0);
}

void AFreezeGameMode::ShowGoldWarning(const FString& Text)
{
	// Every time in the section that teaches gold; at most twice in each later section
	if (CurrentCheckpoint != GoldLessonCheckpoint)
	{
		int32& Shown = GoldWarningsShown.FindOrAdd(CurrentCheckpoint);
		if (Shown >= 2) return;
		Shown++;
	}
	AddWarning(Text);
}

void AFreezeGameMode::PlaySfx(const TCHAR* Name, float Volume, float MaxSeconds)
{
	USoundBase* Sound = FindSound(Name);
	if (!Sound) return;

	// One-off sounds are cut short (with a quick fade) if the file is longer than the moment needs
	const float Length = FMath::Min(Sound->GetDuration(), MaxSeconds);
	UAudioComponent* Voice = UGameplayStatics::SpawnSound2D(this, Sound, Volume);
	if (Voice && Sound->GetDuration() > MaxSeconds)
	{
		TWeakObjectPtr<UAudioComponent> WeakVoice(Voice);
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakVoice]()
		{
			if (WeakVoice.IsValid()) WeakVoice->FadeOut(0.25f, 0.f);
		}), MaxSeconds, false);
	}
	DuckMusicFor(Length);
}

void AFreezeGameMode::DuckMusicFor(float Seconds)
{
	// The music goes quiet while another sound plays, then comes back
	if (!Music || Seconds <= 0.f || Seconds > 20.f) return;
	const float Now = GetWorld()->GetTimeSeconds();
	MusicDuckedUntil = FMath::Max(MusicDuckedUntil, Now + Seconds);
	if (!bMusicDucked)
	{
		bMusicDucked = true;
		Music->AdjustVolume(0.15f, 0.f);
	}
}

USoundBase* AFreezeGameMode::FindSound(const TCHAR* Name)
{
	const FName Key(Name);
	TObjectPtr<USoundBase>* Cached = SoundCache.Find(Key);
	USoundBase* Sound = nullptr;
	if (Cached)
	{
		Sound = *Cached;
	}
	else
	{
		const FString Path = FString::Printf(TEXT("/Game/Audio/%s.%s"), Name, Name);
		Sound = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		SoundCache.Add(Key, Sound);
	}
	return Sound;
}

void AFreezeGameMode::UpdateLowTimeTicking()
{
	const bool bShouldTick = IsPlaying() && !bFrozen && TimeLeft < 30.f;
	if (!bShouldTick)
	{
		StopTicking();
		return;
	}
	USoundBase* Tick = FindSound(TEXT("SFX_Tick"));
	if (!Tick) return;

	if (Tick->GetDuration() <= 1.5f)
	{
		// A single tick: play it once a second
		const int32 Second = FMath::FloorToInt(TimeLeft);
		if (Second != LastTickSecond)
		{
			LastTickSecond = Second;
			PlaySfx(TEXT("SFX_Tick"), 0.6f);
		}
	}
	else
	{
		// A longer ticking clip: keep one copy running, and stop it when no longer needed
		if (!TickLoop) TickLoop = UGameplayStatics::SpawnSound2D(this, Tick, 0.6f, 1.f, 0.f, nullptr, false, false);
		else if (!TickLoop->IsPlaying()) TickLoop->Play();
		DuckMusicFor(0.5f);
	}
}

void AFreezeGameMode::StopTicking()
{
	if (TickLoop && TickLoop->IsPlaying()) TickLoop->Stop();
}

void AFreezeGameMode::PlayerHit(const FString& Reason, bool bByGold)
{
	if (!IsPlaying() || InvulnerableLeft > 0.f) return;

	Hits++;
	TimeLeft = FMath::Max(0.f, TimeLeft - HitPenalty);
	AddMessage(FString::Printf(TEXT("%s  -%ds"), *Reason, FMath::RoundToInt(HitPenalty)), FLinearColor(1.f, 0.35f, 0.3f));
	PlaySfx(TEXT("SFX_Getting_Hit"));
	if (bByGold) ShowGoldWarning(TEXT("Gold things ignore your freeze! Time your run instead."));
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
	PlaySfx(TEXT("SFX_SentinelDown"));

	if (Sentinel->bBoss)
	{
		AddMessage(TEXT("The Guard is crushed!"), FreezeColors::Gold);
		if (Gate)
		{
			PlaySfx(TEXT("SFX_GateOpen"));
			Gate->Destroy();
			Gate = nullptr;
			AddMessage(TEXT("The gate is open! Ring the clock tower bell."), FreezeColors::Gold);
		}
		return;
	}

	if (Sentinel->bArena)
	{
		ArenaDefeated++;
		AddMessage(FString::Printf(TEXT("Guard down! %d of %d"), ArenaDefeated, ArenaGuardCount), FreezeColors::Gold);
		if (ArenaDefeated >= ArenaGuardCount) OpenGate(1, TEXT("All the Guards are down. The gate is open!"));
		return;
	}

	AddMessage(TEXT("Guard down!"), FreezeColors::Gold);
}

void AFreezeGameMode::OpenGate(int32 Index, const FString& Message)
{
	if (Gates.IsValidIndex(Index) && Gates[Index])
	{
		PlaySfx(TEXT("SFX_GateOpen"));
		Gates[Index]->Destroy();
		Gates[Index] = nullptr;
		AddMessage(Message, FreezeColors::Gold);
	}
}

static bool IsShootable(AActor* A)
{
	if (ASentinel* S = Cast<ASentinel>(A)) return !S->IsDefeated();
	if (ATimeMover* M = Cast<ATimeMover>(A)) return M->bIsTarget;
	return false;
}

AActor* AFreezeGameMode::FindAimTarget(FVector& OutHitPoint) const
{
	ACharacter* Player = GetPlayerCharacter();
	APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
	if (!PC) return nullptr;

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
	OutHitPoint = bHit ? Hit.ImpactPoint : End;
	if (bHit && IsShootable(Hit.GetActor())) return Hit.GetActor();

	// Aim assist: accept anything shootable within a few degrees of the crosshair, if nothing blocks it
	AActor* Best = nullptr;
	float BestAngle = 6.f;
	auto Consider = [&](AActor* A)
	{
		if (!IsShootable(A)) return;
		const FVector To = A->GetActorLocation() - ViewLoc;
		if (To.Size() > 7000.f) return;
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Dir, To.GetSafeNormal())));
		if (Angle >= BestAngle) return;
		FHitResult Block;
		if (GetWorld()->LineTraceSingleByObjectType(Block, Start, A->GetActorLocation(), Objects, Params) && Block.GetActor() != A) return;
		Best = A;
		BestAngle = Angle;
	};
	for (TActorIterator<ATimeMover> It(GetWorld()); It; ++It) Consider(*It);
	for (TActorIterator<ASentinel> It(GetWorld()); It; ++It) Consider(*It);
	if (Best) OutHitPoint = Best->GetActorLocation();
	return Best;
}

void AFreezeGameMode::OnShootPressed()
{
	if (!IsPlaying() || !bHasBow || ShotCooldown > 0.f) return;

	ACharacter* Player = GetPlayerCharacter();
	if (!Player) return;
	ShotCooldown = 0.6f;	// a short reload, so you can't spray arrows

	FVector HitPoint;
	AActor* HitActor = FindAimTarget(HitPoint);

	// Turn to face the shot, then send a visible arrow from the bow hand to where it lands
	if (AController* C = Player->GetController())
	{
		Player->SetActorRotation(FRotator(0.f, C->GetControlRotation().Yaw, 0.f));
	}
	const FVector Muzzle = Player->GetMesh() ? Player->GetMesh()->GetSocketLocation(TEXT("hand_l")) : Player->GetActorLocation() + FVector(0.f, 0.f, 50.f);
	PlaySfx(TEXT("SFX_BowShoot"), 1.f, 0.8f);
	if ((HitPoint - Muzzle).Size() > 10.f)
	{
		const FTransform T(Muzzle);
		if (AFlyingArrow* Arrow = GetWorld()->SpawnActorDeferred<AFlyingArrow>(AFlyingArrow::StaticClass(), T))
		{
			Arrow->Target = HitPoint;
			Arrow->FinishSpawning(T);
		}
	}

	if (!HitActor)
	{
		AddMessage(TEXT("Missed!"), FLinearColor(0.8f, 0.8f, 0.85f));
		return;
	}

	PlaySfx(TEXT("SFX_ArrowHit"), 1.f, 0.8f);
	if (ASentinel* S = Cast<ASentinel>(HitActor))
	{
		if (!S->IsGuarding(Player->GetActorLocation()))
		{
			AddMessage(TEXT("Too far! Step into their room first."), FLinearColor(0.8f, 0.8f, 0.85f));
		}
		else if (S->bBoss)
		{
			AddMessage(TEXT("Arrows bounce off its gold armour!"), FreezeColors::Gold);
		}
		else if (!S->TakeArrow(IsFrozen()))
		{
			AddMessage(TEXT("Hit! One more arrow, or freeze it to finish it."), FreezeColors::Gold);
		}
		return;
	}

	if (ATimeMover* M = Cast<ATimeMover>(HitActor))
	{
		M->Destroy();
		TargetsHit++;
		AddMessage(FString::Printf(TEXT("Target hit! %d of 2"), TargetsHit), FreezeColors::Gold);
		if (TargetsHit >= 2) OpenGate(0, TEXT("Both targets hit. The gate is open!"));
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
	StopTicking();
	if (Music) Music->FadeOut(2.f, 0.f);
	SetFrozen(false);
	HintText.Empty();
}

void AFreezeGameMode::Lose()
{
	State = EFreezeState::Lost;
	StopTicking();
	PlaySfx(TEXT("SFX_GameOver"), 1.f, 5.f);
	if (Music) Music->FadeOut(1.f, 0.f);
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
	HintSpotlightLeft = FMath::Max(0.f, HintSpotlightLeft - DeltaSeconds);
	if (bHasBow)
	{
		FVector Ignored;
		AActor* Aimed = FindAimTarget(Ignored);
		const ASentinel* AimedGuard = Cast<ASentinel>(Aimed);
		const ACharacter* Me = GetPlayerCharacter();
		// crosshair turns red on a target (but not on Guards in a room you haven't entered yet)
		bAimOnTarget = Aimed && !(AimedGuard && Me && !AimedGuard->IsGuarding(Me->GetActorLocation()));
	}
	if (Music && !bFrozen && !Music->IsPlaying()) Music->Play();
	if (bMusicDucked && Music && GetWorld()->GetTimeSeconds() >= MusicDuckedUntil && !(TickLoop && TickLoop->IsPlaying()))
	{
		bMusicDucked = false;
		Music->AdjustVolume(0.4f, 1.f);
	}
	UpdateLowTimeTicking();
	UpdateArrowKeys(Player);

	// Pick up the bow
	if (!bHasBow && FVector::Dist(Player->GetActorLocation(), W(BowSpot)) < 160.f)
	{
		bHasBow = true;
		PlaySfx(TEXT("SFX_BowPickup"));
		if (BowPickup)
		{
			BowPickup->SetActorEnableCollision(false);
			BowPickup->AttachToComponent(Player->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("hand_l"));
		}
		AddMessage(TEXT("You got the bow! Left-click or press F to shoot."), FreezeColors::Gold);
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
		const bool bHighEnough = Checkpoints[i].TriggerZ <= -1e5f || Loc.Z - CourseOrigin.Z > Checkpoints[i].TriggerZ;
		if (LocalX > Checkpoints[i].TriggerX && bHighEnough)
		{
			CurrentCheckpoint = i;
			if (i == SpotlightCheckpoint) HintSpotlightLeft = 2.f;
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

	if (FVector::Dist(Loc, W(BellRope)) < 280.f)
	{
		AddMessage(TEXT("DONG!"), FreezeColors::Gold);
		PlaySfx(TEXT("SFX_Bell"), 1.f, 6.f);
		Win();
	}
}
