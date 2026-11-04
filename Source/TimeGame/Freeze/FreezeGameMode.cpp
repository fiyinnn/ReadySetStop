#include "FreezeGameMode.h"
#include "FreezeHUD.h"
#include "GeoBlock.h"
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

AGeoBlock* AFreezeGameMode::Block(const FVector& Center, const FVector& Size, const FLinearColor& Color, bool bCylinder)
{
	AGeoBlock* B = GetWorld()->SpawnActor<AGeoBlock>(AGeoBlock::StaticClass(), W(Center), FRotator::ZeroRotator);
	if (B) B->Init(Size, Color, bCylinder);
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

	// --- 3. Freeze a falling crate to use it as a step ----------------------
	Floor(4400.f, 5000.f, -350.f, 350.f);
	Block(FVector(6500.f, 0.f, H - 150.f), FVector(2200.f, 700.f, 300.f), FreezeColors::Stone);	// ledge + section 4 floor
	for (float CratePhase : { 0.f, 0.5f })
	{
		MakeMover(FVector(5200.f, 0.f, 0.f), [&](ATimeMover& M)
		{
			M.Mode = ETimeMoveMode::Loop;
			M.Size = FVector(300.f, 360.f, 300.f);
			M.OffsetA = FVector(0.f, 0.f, 1100.f);
			M.OffsetB = FVector(0.f, 0.f, -1100.f);
			M.Period = 14.f;
			M.Phase = CratePhase;
			M.Color = FreezeColors::Crate;
		});
	}
	AddCheckpoint(4450.f, FVector(4550.f, 0.f, 100.f),
		TEXT("The ledge is too high to jump. When a falling crate's top is about level with your chest, freeze, jump onto it, then jump up to the ledge."), 5500.f);

	// --- 4. Gold ignores your freeze ------------------------------------------
	SideWalls(6000.f, 7600.f, 350.f, 500.f, H);
	Blade(6500.f, H, 3.0f, 0.f, true);
	Blade(7000.f, H, 1.8f, 0.f, false);
	Blade(7250.f, H, 2.8f, 0.25f, true);
	AddCheckpoint(6050.f, FVector(6150.f, 0.f, H + 100.f),
		TEXT("Gold things are outside of time. Your freeze won't stop them, so time your run instead."), 7600.f);

	// --- 5. The Sentinel ------------------------------------------------------
	Floor(7600.f, 9600.f, -700.f, 700.f, H);
	SideWalls(7600.f, 9600.f, 700.f, 500.f, H);
	for (float EndX : { 7600.f, 9600.f })
	{
		Block(FVector(EndX, 525.f, H + 250.f), FVector(40.f, 350.f, 500.f), FreezeColors::Wall);
		Block(FVector(EndX, -525.f, H + 250.f), FVector(40.f, 350.f, 500.f), FreezeColors::Wall);
	}
	Block(FVector(8300.f, 350.f, H + 200.f), FVector(200.f, 200.f, 400.f), FreezeColors::Wall);
	Block(FVector(8300.f, -350.f, H + 200.f), FVector(200.f, 200.f, 400.f), FreezeColors::Wall);
	Block(FVector(8950.f, 0.f, H + 200.f), FVector(200.f, 200.f, 400.f), FreezeColors::Wall);
	SpawnSentinel(FVector(8700.f, 0.f, H + 100.f), FVector2D(7650.f, -680.f), FVector2D(9550.f, 680.f), 380.f, 1400.f, false);
	SpawnHourglass(FVector(9350.f, 550.f, H + 80.f));
	AddCheckpoint(7650.f, FVector(7700.f, 0.f, H + 100.f),
		TEXT("A Sentinel guards this hall. If it touches you, you're sent back. Frozen, it's just a statue."), 9600.f);

	// --- 6. Everything together (no tips from here) -----------------------
	Floor(9600.f, 10400.f, -350.f, 350.f, H);
	SideWalls(9600.f, 9880.f, 350.f, 500.f, H);
	SideWalls(10120.f, 10400.f, 350.f, 500.f, H);
	for (float Side : { 1.f, -1.f })
	{
		MakeMover(FVector(10000.f, 525.f * Side, H + 200.f), [&](ATimeMover& M)
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
	SpawnSentinel(FVector(10300.f, 0.f, H + 100.f), FVector2D(10120.f, -340.f), FVector2D(10390.f, 340.f), 400.f, 1000.f, false);
	Bridge(11000.f, H, 700.f, 8.f, 0.3f);
	Blade(11000.f, H, 2.8f, 0.f, true);
	Floor(11600.f, 12000.f, -350.f, 350.f, H);
	SpawnHourglass(FVector(11850.f, 0.f, H + 80.f));
	AddCheckpoint(9650.f, FVector(9700.f, 0.f, H + 100.f), FString(), 0.f);

	// --- 7. Final room: crush the Sentinel with the gold press ---------------
	Floor(12000.f, 14000.f, -900.f, 900.f, H);
	SideWalls(12000.f, 14000.f, 900.f, 600.f, H);
	Block(FVector(12000.f, 625.f, H + 300.f), FVector(40.f, 550.f, 600.f), FreezeColors::Wall);
	Block(FVector(12000.f, -625.f, H + 300.f), FVector(40.f, 550.f, 600.f), FreezeColors::Wall);
	MakeMover(FVector(13000.f, 0.f, H), [&](ATimeMover& M)
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
	SpawnSentinel(FVector(13600.f, 500.f, H + 120.f), FVector2D(12050.f, -880.f), FVector2D(13950.f, 880.f), 430.f, 3000.f, true);
	Gate = Block(FVector(14000.f, 0.f, H + 300.f), FVector(60.f, 1800.f, 600.f), FreezeColors::Danger);
	AddCheckpoint(12050.f, FVector(12150.f, 0.f, H + 100.f),
		TEXT("The gate opens once the Sentinel is gone. Gold things keep moving while you freeze. Lead it under the gold press."), 14000.f);

	// --- Goal: the clock tower ------------------------------------------------
	Floor(14000.f, 15000.f, -500.f, 500.f, H);
	Block(FVector(14700.f, 0.f, H + 800.f), FVector(600.f, 600.f, 1600.f), FreezeColors::Tower, true);
	Block(FVector(14395.f, 0.f, H + 150.f), FVector(20.f, 200.f, 300.f), FreezeColors::Ice);
	GoalX = 14250.f;
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
}

void AFreezeGameMode::OnSentinelDefeated(ASentinel* Sentinel)
{
	AddMessage(TEXT("The Sentinel is crushed!"), FreezeColors::Gold);
	if (Sentinel && Sentinel->bBoss && Gate)
	{
		Gate->Destroy();
		Gate = nullptr;
		AddMessage(TEXT("The gate is open!"), FreezeColors::Gold);
	}
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

	if (LocalX > GoalX)
	{
		Win();
	}
}
