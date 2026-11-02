#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "FreezeHUD.generated.h"

class UFont;

/** Clock, freeze meter, tips, messages and the end screen, all drawn in code. */
UCLASS()
class AFreezeHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	void DrawPanel(float X, float Y, float W, float H, const FLinearColor& Color);
	void DrawCentered(const FString& Text, float CenterX, float Y, UFont* Font, float Scale, const FLinearColor& Color);
	TArray<FString> WrapText(const FString& Text, UFont* Font, float Scale, float MaxWidth);
	static FString FormatClock(float Seconds);
};
