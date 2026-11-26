#include "UI/SirrohUIResources.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"

FSirrohUIResources::FSirrohUIResources(
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
	UTexture2D* InSmallSquareSelected)
	: FontTexture(InFont)
	, BattlefieldBrush(MakeWholeTextureBrush(InBattlefield))
	, BloodMoonBrush(MakeWholeTextureBrush(InBloodMoon))
	, TitleBrush(MakeWholeTextureBrush(InTitle))
	, PanoramaBrush(MakeWholeTextureBrush(InPanorama))
	, ButtonNormalBrush(MakeWholeTextureBrush(InNormalFrame))
	, ButtonHoveredBrush(MakeWholeTextureBrush(InRedFrame))
	, ButtonPressedBrush(MakeWholeTextureBrush(InRedFrame))
	, CreatorPanelBrush(MakeAtlasBrush(InDeco, FIntRect(20, 17, 641, 606)))
	, CharacterArchBrush(MakeAtlasBrush(InDeco, FIntRect(1125, 18, 1647, 606)))
	, CharacterFrameBrush(MakeWholeTextureBrush(InCharacterFrame))
	, SmallSquareBrush(MakeWholeTextureBrush(InSmallSquare))
	, SmallSquareSelectedBrush(MakeWholeTextureBrush(InSmallSquareSelected))
	, SmallFieldBrush(MakeAtlasBrush(InOtherDeco, FIntRect(40, 918, 473, 1013)))
	, SmallFieldActiveBrush(MakeAtlasBrush(InOtherDeco, FIntRect(485, 918, 949, 1013)))
	, SliderBarBrush(MakeAtlasBrush(InSliderLine, FIntRect(154, 285, 2018, 392)))
	, SliderThumbBrush(MakeAtlasBrush(InSliderHead, FIntRect(415, 425, 842, 827)))
{
	// Use the UI design size for nine-slice calculations. Leaving the original
	// 2172x724 image size here makes Slate treat each edge as taller than the
	// entire 157px button, so the top and bottom slices overlap the black centre.
	const FVector2f ButtonDesignSize(470.0f, 157.0f);
	ButtonNormalBrush.SetImageSize(ButtonDesignSize);
	ButtonHoveredBrush.SetImageSize(ButtonDesignSize);
	ButtonPressedBrush.SetImageSize(ButtonDesignSize);

	// Preserve the decorated ends and stretch only the broad, dark centre.
	ButtonNormalBrush.DrawAs = ESlateBrushDrawType::Box;
	ButtonHoveredBrush.DrawAs = ESlateBrushDrawType::Box;
	ButtonPressedBrush.DrawAs = ESlateBrushDrawType::Box;
	ButtonNormalBrush.Margin = FMargin(0.12f, 0.0f, 0.12f, 0.0f);
	ButtonHoveredBrush.Margin = FMargin(0.12f, 0.0f, 0.12f, 0.0f);
	ButtonPressedBrush.Margin = FMargin(0.12f, 0.0f, 0.12f, 0.0f);

	CreatorPanelBrush.DrawAs = ESlateBrushDrawType::Box;
	CreatorPanelBrush.Margin = FMargin(0.055f);
	CharacterArchBrush.DrawAs = ESlateBrushDrawType::Box;
	CharacterArchBrush.Margin = FMargin(0.12f, 0.14f, 0.12f, 0.12f);
	SmallFieldBrush.DrawAs = ESlateBrushDrawType::Box;
	SmallFieldActiveBrush.DrawAs = ESlateBrushDrawType::Box;
	SmallFieldBrush.Margin = FMargin(0.10f, 0.0f, 0.10f, 0.0f);
	SmallFieldActiveBrush.Margin = FMargin(0.10f, 0.0f, 0.10f, 0.0f);
	SliderBarBrush.SetImageSize(FVector2f(360.0f, 20.0f));
	SliderThumbBrush.SetImageSize(FVector2f(30.0f, 30.0f));
}

FSlateBrush FSirrohUIResources::MakeWholeTextureBrush(UTexture* Texture)
{
	FSlateBrush Brush;

	// Zabezpieczenie przed b��dem, gdyby tekstura by�a pusta
	if (!Texture)
	{
		return Brush;
	}

	float SizeX = 0.f;
	float SizeY = 0.f;

	// Sprawdzamy, czy to zwyk�a tekstura 2D
	if (UTexture2D* Tex2D = Cast<UTexture2D>(Texture))
	{
		SizeX = Tex2D->GetSizeX();
		SizeY = Tex2D->GetSizeY();
	}
	// Sprawdzamy, czy to Render Target (np. podgl�d postaci z kamery)
	else if (UTextureRenderTarget2D* RT = Cast<UTextureRenderTarget2D>(Texture))
	{
		SizeX = RT->SizeX;
		SizeY = RT->SizeY;
	}

	// Przypisanie zasobu do p�dzla 
	Brush.SetResourceObject(Texture);

	// Ustawienie odpowiedniego rozmiaru (u�ywamy zmiennych zamiast GetSizeX())
	Brush.SetImageSize(FVector2f(SizeX, SizeY));

	return Brush;
}

FSlateBrush FSirrohUIResources::MakeAtlasBrush(UTexture2D* Texture, const FIntRect& PixelRegion)
{
	FSlateBrush Brush = MakeWholeTextureBrush(Texture);
	if (!Texture || Texture->GetSizeX() <= 0 || Texture->GetSizeY() <= 0)
	{
		return Brush;
	}

	const FVector2f TextureSize(Texture->GetSizeX(), Texture->GetSizeY());
	const FVector2f UVMin(
		static_cast<float>(PixelRegion.Min.X) / TextureSize.X,
		static_cast<float>(PixelRegion.Min.Y) / TextureSize.Y);
	const FVector2f UVMax(
		static_cast<float>(PixelRegion.Max.X) / TextureSize.X,
		static_cast<float>(PixelRegion.Max.Y) / TextureSize.Y);

	Brush.SetUVRegion(FBox2f(UVMin, UVMax));
	Brush.SetImageSize(FVector2f(PixelRegion.Width(), PixelRegion.Height()));
	return Brush;
}

