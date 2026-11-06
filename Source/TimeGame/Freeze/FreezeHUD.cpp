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

	// --- Title screen ---
	if (GM->State == EFreezeState::Title)
	{
		DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.82f), 0.f, 0.f, SW, SH);
		float Y = SH * 0.16f;
		{
			// "Ready" yellow, "Set" green, "Stop" red, drawn side by side as one centred title
			const float TS = 3.2f * UI;
			float W1 = 0.f, W2 = 0.f, W3 = 0.f, TH = 0.f;
			GetTextSize(TEXT("Ready"), W1, TH, Big, TS);
			GetTextSize(TEXT("Set"), W2, TH, Big, TS);
			GetTextSize(TEXT("Stop"), W3, TH, Big, TS);
			float X = (SW - (W1 + W2 + W3)) * 0.5f;
			DrawText(TEXT("Ready"), FLinearColor(1.f, 0.85f, 0.1f), X, Y, Big, TS);
			X += W1;
			DrawText(TEXT("Set"), FLinearColor(0.2f, 0.85f, 0.3f), X, Y, Big, TS);
			X += W2;
			DrawText(TEXT("Stop"), FLinearColor(1.f, 0.2f, 0.15f), X, Y, Big, TS);
		}
		Y += 96.f * UI;
		DrawCentered(TEXT("Race the clock to the tower and ring its bell."), SW * 0.5f, Y, Small, 1.3f * UI, FLinearColor::White);
		Y += 34.f * UI;
		DrawCentered(TEXT("Freeze time to get past blades, Guards and moving platforms,"), SW * 0.5f, Y, Small, 1.1f * UI, FLinearColor(0.85f, 0.9f, 1.f));
		Y += 28.f * UI;
		DrawCentered(TEXT("but every freeze costs you time. Gold things ignore your freeze."), SW * 0.5f, Y, Small, 1.1f * UI, FLinearColor(0.85f, 0.9f, 1.f));
		Y += 56.f * UI;
		DrawCentered(TEXT("CONTROLS"), SW * 0.5f, Y, Small, 1.1f * UI, FLinearColor(0.75f, 0.8f, 0.9f));
		Y += 34.f * UI;
		const TCHAR* Lines[] = {
			TEXT("WASD or arrow keys   move"),
			TEXT("Mouse   look and aim"),
			TEXT("Space   jump"),
			TEXT("E   freeze time (press again to unfreeze)"),
			TEXT("Left-click   shoot, once you find the bow"),
			TEXT("X   hide tips") };
		for (const TCHAR* Line : Lines)
		{
			DrawCentered(Line, SW * 0.5f, Y, Small, 1.05f * UI, FLinearColor::White);
			Y += 28.f * UI;
		}
		Y += 30.f * UI;
		DrawCentered(TEXT("Press Enter to start"), SW * 0.5f, Y, Big, 1.4f * UI, FreezeColors::Ice);
		return;
	}

	// --- Clock (top centre) ---
	{
		const float W = 240.f * UI, H = 92.f * UI;
		const float X = (SW - W) * 0.5f, Y = Margin;
		DrawPanel(X, Y, W, H, FLinearColor(1.f, 1.f, 1.f, 0.9f));

		FLinearColor ClockColor(0.08f, 0.08f, 0.1f);
		if (GM->bFrozen) ClockColor = FLinearColor(0.05f, 0.4f, 0.75f);
		else if (GM->TimeLeft < 30.f) ClockColor = FLinearColor(0.85f, 0.1f, 0.08f);

		DrawCentered(FormatClock(GM->TimeLeft), SW * 0.5f, Y + 4.f * UI, Big, 2.2f * UI, ClockColor);

		// Freeze meter
		const float BarW = W - 30.f * UI, BarH = 10.f * UI;
		const float BarX = X + 15.f * UI, BarY = Y + H - 26.f * UI;
		DrawRect(FLinearColor(0.78f, 0.8f, 0.84f, 1.f), BarX, BarY, BarW, BarH);

		float Fill = 1.f;
		FLinearColor FillColor(0.1f, 0.6f, 0.9f);
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
		DrawCentered(Label, SW * 0.5f, BarY + BarH + 1.f * UI, Small, 0.8f * UI, FLinearColor(0.2f, 0.22f, 0.28f));
	}

	// --- Crosshair once the player has the bow ---
	if (GM->bHasBow && GM->IsPlaying())
	{
		const float CX = SW * 0.5f, CY = SH * 0.5f, L = (GM->bAimOnTarget ? 12.f : 8.f) * UI, T = 3.f * UI;
		const FLinearColor C = GM->bAimOnTarget ? FLinearColor(1.f, 0.15f, 0.1f, 1.f) : FLinearColor(1.f, 1.f, 1.f, 0.85f);
		DrawRect(C, CX - L, CY - T * 0.5f, L * 2.f, T);
		DrawRect(C, CX - T * 0.5f, CY - L, T, L * 2.f);
	}

	// --- Tip panel (top left); red warnings stack just under it ---
	float TipBottom = Margin;
	if (!GM->HintText.IsEmpty() && GM->IsPlaying())
	{
		const float W = 380.f * UI;
		const float TextScale = 1.18f * UI;
		const float Pad = 12.f * UI;
		TArray<FString> Lines = WrapText(GM->HintText, Small, TextScale, W - Pad * 2.f);
		float LW = 0.f, LH = 0.f;
		GetTextSize(TEXT("Ag"), LW, LH, Small, TextScale);
		const float H = Pad * 2.f + 26.f * UI + Lines.Num() * LH + 22.f * UI;
		float X = Margin, Y = Margin;
		if (GM->HintSpotlightLeft > 0.f)
		{
			// New idea: show the tip in the middle of the screen first, then it moves to the top left
			X = (SW - W) * 0.5f;
			Y = SH * 0.3f;
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f), 0.f, 0.f, SW, SH);
		}
		else
		{
			TipBottom = Y + H + 12.f * UI;
		}

		DrawPanel(X, Y, W, H, PanelColor);
		DrawText(TEXT("TIP"), FLinearColor(0.75f, 0.8f, 0.9f), X + Pad, Y + Pad, Small, 1.0f * UI);
		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			// Drawn twice, a hair apart, so the tip reads bold
			DrawText(Lines[i], FLinearColor::White, X + Pad, Y + Pad + 26.f * UI + i * LH, Small, TextScale);
			DrawText(Lines[i], FLinearColor::White, X + Pad + 0.9f * UI, Y + Pad + 26.f * UI + i * LH, Small, TextScale);
		}
		DrawText(TEXT("[X] hide"), FLinearColor(0.6f, 0.6f, 0.65f), X + Pad, Y + H - Pad - 14.f * UI, Small, 0.75f * UI);
	}

	// --- Messages under the clock; red warnings in the top left ---
	{
		float Y = Margin + 112.f * UI;
		float WarnY = TipBottom;
		for (int32 i = 0; i < GM->Messages.Num(); ++i)
		{
			const FFreezeMessage& M = GM->Messages[i];
			FLinearColor C = M.Color;
			C.A = FMath::Clamp(M.Life, 0.f, 1.f);
			const float Scale = (M.bWarning ? 1.3f : 1.1f) * UI;
			float TW = 0.f, TH = 0.f;
			GetTextSize(M.Text, TW, TH, Big, Scale);
			if (M.bWarning)
			{
				DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f * C.A), Margin, WarnY, TW + 24.f * UI, TH + 8.f * UI);
				DrawText(M.Text, C, Margin + 12.f * UI, WarnY + 4.f * UI, Big, Scale);
				WarnY += TH + 14.f * UI;
			}
			else
			{
				DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f * C.A), SW * 0.5f - TW * 0.5f - 12.f * UI, Y - 2.f * UI, TW + 24.f * UI, TH + 4.f * UI);
				DrawCentered(M.Text, SW * 0.5f, Y, Big, Scale, C);
				Y += TH + 10.f * UI;
			}
		}
	}

	// --- End screen ---
	if (GM->State == EFreezeState::Won || GM->State == EFreezeState::Lost)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.7f), 0.f, 0.f, SW, SH);
		const bool bWon = GM->State == EFreezeState::Won;
		const FString Title = bWon ? TEXT("You reached the clock tower!") : TEXT("Time's up!");
		DrawCentered(Title, SW * 0.5f, SH * 0.36f, Big, 2.4f * UI, bWon ? FreezeColors::Gold : FLinearColor(1.f, 0.35f, 0.3f));
		DrawCentered(TEXT("Press R to play again"), SW * 0.5f, SH * 0.36f + 90.f * UI, Small, 1.1f * UI, FLinearColor(0.8f, 0.85f, 1.f));
	}
}
