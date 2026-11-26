#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

class UTexture2D;

/** All texture regions used by the first UI screen live in one place. */
struct FSirrohUIResources
{
	FSirrohUIResources(
		UTexture2D* InFont,
		UTexture2D* InBattlefield,
		UTexture2D* InBloodMoon,
		UTexture2D* InTitle,
		UTexture2D* InNormalFrame,
		UTexture2D* InRedFrame,
		UTexture2D* InPanorama,
		UTexture2D* InDeco,
		UTexture2D* InOtherDeco,
		UTexture2D* InSliderHead,
		UTexture2D* InSliderLine,
		UTexture2D* InCharacterFrame,
		UTexture2D* InSmallSquare,
		UTexture2D* InSmallSquareSelected);

	
	static FSlateBrush MakeWholeTextureBrush(UTexture* Texture);
	static FSlateBrush MakeAtlasBrush(UTexture2D* Texture, const FIntRect& PixelRegion);

	UTexture2D* FontTexture = nullptr;
	FSlateBrush BattlefieldBrush;
	FSlateBrush BloodMoonBrush;
	FSlateBrush TitleBrush;
	FSlateBrush PanoramaBrush;
	FSlateBrush ButtonNormalBrush;
	FSlateBrush ButtonHoveredBrush;
	FSlateBrush ButtonPressedBrush;
	FSlateBrush CreatorPanelBrush;
	FSlateBrush CharacterArchBrush;
	FSlateBrush CharacterFrameBrush;
	FSlateBrush SmallSquareBrush;
	FSlateBrush SmallSquareSelectedBrush;
	FSlateBrush SmallFieldBrush;
	FSlateBrush SmallFieldActiveBrush;
	FSlateBrush SliderBarBrush;
	FSlateBrush SliderThumbBrush;
};
