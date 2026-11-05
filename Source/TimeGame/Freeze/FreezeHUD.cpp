#include "FreezeHUD.h"
#include "FreezeGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"

FString AFreezeHUD::FormatClock(float Seconds)
{
	const int32 Total = FMath::Max(0, FMath::CeilToInt(Seconds));
	return FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60);
}

void AFreezeHUD::DrawPanel(float X, float Y, float W, float H, const FLinearColor& Color)
{
	DrawRect(Color, X, Y, W, H);
}

void AFreezeHUD::DrawCentered(const FString& Text, float CenterX, float Y, UFont* Font, float Scale, const FLinearColor& Color)
{
	float TW = 0.f, TH = 0.f;
	GetTextSize(Text, TW, TH, Font, Scale);
	DrawText(Text, Color, CenterX - TW * 0.5f, Y, Font, Scale);
}

TArray<FString> AFreezeHUD::WrapText(const FString& Text, UFont* Font, float Scale, float MaxWidth)
{
	TArray<FString> Words;
	Text.ParseIntoArray(Words, TEXT(" "));

	TArray<FString> Lines;
	FString Line;
	for (const FString& Word : Words)
	{
		const FString Test = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
		float TW = 0.f, TH = 0.f;
		GetTextSize(Test, TW, TH, Font, Scale);
		if (TW > MaxWidth && !Line.IsEmpty())
		{
			Lines.Add(Line);
			Line = Word;
		}
		else
		{
			Line = Test;
		}
	}
	if (!Line.IsEmpty()) Lines.Add(Line);
	return Lines;
}

void AFreezeHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;

	AFreezeGameMode* GM = GetWorld()->GetAuthGameMode<AFreezeGameMode>();
	if (!GM) return;

	UFont* Big = GEngine->GetLargeFont();
	UFont* Small = GEngine->GetMediumFont();

	const float SW = Canvas->ClipX;
	const float SH = Canvas->ClipY;
	const float UI = FMath::Max(0.6f, SH / 720.f);
	const float Margin = 16.f * UI;
	const FLinearColor PanelColor(0.f, 0.f, 0.f, 0.55f);
	const FLinearColor Orange(1.f, 0.55f, 0.15f);

	// --- Clock (top centre) ---
	{
		const float W = 240.f * UI, H = 92.f * UI;
		const float X = (SW - W) * 0.5f, Y = Margin;
		DrawPanel(X, Y, W, H, PanelColor);

		FLinearColor ClockColor = FLinearColor::White;
		if (GM->bFrozen) ClockColor = FreezeColors::Ice;
		else if (GM->TimeLeft < 30.f) ClockColor = FLinearColor(1.f, 0.3f, 0.25f);

		DrawCentered(FormatClock(GM->TimeLeft), SW * 0.5f, Y + 4.f * UI, Big, 2.2f * UI, ClockColor);

		// Freeze meter
		const float BarW = W - 30.f * UI, BarH = 10.f * UI;
		const float BarX = X + 15.f * UI, BarY = Y + H - 26.f * UI;
		DrawRect(FLinearColor(0.15f, 0.15f, 0.18f, 0.9f), BarX, BarY, BarW, BarH);

		float Fill = 1.f;
		FLinearColor FillColor = FreezeColors::Ice;
		FString Label = FString::Printf(TEXT("E  FREEZE  (-%ds)"), FMath::RoundToInt(GM->FreezeCost));
		if (GM->bFrozen)
		{
			Fill = GM->FreezeLeft / FMath::Max(GM->FreezeDuration, 0.01f);
			Label = TEXT("TIME FROZEN   E to resume");
		}
		else if (GM->CooldownLeft > 0.f)
		{
			Fill = 1.f - GM->CooldownLeft / FMath::Max(GM->FreezeCooldown, 0.01f);
			FillColor = FLinearColor(0.45f, 0.45f, 0.5f);
			Label = TEXT("recharging");
		}
		DrawRect(FillColor, BarX, BarY, BarW * FMath::Clamp(Fill, 0.f, 1.f), BarH);
		DrawCentered(Label, SW * 0.5f, BarY + BarH + 1.f * UI, Small, 0.8f * UI, FLinearColor(0.85f, 0.9f, 1.f));
	}

	// --- Tip panel (top left) ---
	if (!GM->HintText.IsEmpty() && GM->IsPlaying())
	{
		const float W = 380.f * UI;
		const float TextScale = 1.0f * UI;
		const float Pad = 12.f * UI;
		TArray<FString> Lines = WrapText(GM->HintText, Small, TextScale, W - Pad * 2.f);
		float LW = 0.f, LH = 0.f;
		GetTextSize(TEXT("Ag"), LW, LH, Small, TextScale);
		const float H = Pad * 2.f + 26.f * UI + Lines.Num() * LH + 22.f * UI;
		const float X = Margin, Y = SH - Margin - H;

		DrawPanel(X, Y, W, H, PanelColor);
		DrawRect(Orange, X, Y, 4.f * UI, H);
		DrawText(TEXT("TIP"), Orange, X + Pad, Y + Pad, Small, 1.0f * UI);
		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			DrawText(Lines[i], FLinearColor::White, X + Pad, Y + Pad + 26.f * UI + i * LH, Small, TextScale);
		}
		DrawText(TEXT("[X] hide"), FLinearColor(0.6f, 0.6f, 0.65f), X + Pad, Y + H - Pad - 14.f * UI, Small, 0.75f * UI);
	}

	// --- Messages (bottom centre, newest at the bottom) ---
	{
		float Y = SH * 0.62f;
		for (int32 i = GM->Messages.Num() - 1; i >= 0; --i)
		{
			const FFreezeMessage& M = GM->Messages[i];
			FLinearColor C = M.Color;
			C.A = FMath::Clamp(M.Life, 0.f, 1.f);
			DrawCentered(M.Text, SW * 0.5f, Y, Big, 1.1f * UI, C);
			Y -= 34.f * UI;
		}
	}

	// --- End screen ---
	if (!GM->IsPlaying())
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.7f), 0.f, 0.f, SW, SH);
		const bool bWon = GM->State == EFreezeState::Won;
		const FString Title = bWon ? TEXT("You reached the clock tower!") : TEXT("Time's up!");
		DrawCentered(Title, SW * 0.5f, SH * 0.36f, Big, 2.4f * UI, bWon ? FreezeColors::Gold : FLinearColor(1.f, 0.35f, 0.3f));
		// PLAYTEST ONLY: remove these stats before submitting
		const FString Stats = FString::Printf(TEXT("Time left %s     Freezes used %d     Times hit %d     Played %s"),
			*FormatClock(GM->TimeLeft), GM->Freezes, GM->Hits, *FormatClock(GM->PlaySeconds));
		DrawCentered(Stats, SW * 0.5f, SH * 0.36f + 80.f * UI, Small, 1.1f * UI, FLinearColor::White);
		DrawCentered(TEXT("Press R to play again"), SW * 0.5f, SH * 0.36f + 130.f * UI, Small, 1.1f * UI, FLinearColor(0.8f, 0.85f, 1.f));
	}
}
