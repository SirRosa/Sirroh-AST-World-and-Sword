#include "POI/POIArtPool.h"
#include "Engine/Texture2D.h"

void UPOIArtPool::BakeAlphaBounds()
{
#if WITH_EDITOR
    for (auto& Pair:Images)
    {
        Pair.SideUV=APOIKitBlock::GetImageAlphaBounds(Pair.Side.LoadSynchronous());
        Pair.TopUV=APOIKitBlock::GetImageAlphaBounds(Pair.Top.LoadSynchronous());
    }
    MarkPackageDirty();
#endif
}
#if WITH_EDITOR
void UPOIArtPool::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    Super::PostEditChangeProperty(Event);
    BakeAlphaBounds();
}
#endif
UPOIArtTheme* UPOIArtTheme::ForLayer(int32 Layer)
{
    return LoadObject<UPOIArtTheme>(nullptr,*FString::Printf(TEXT("/Game/POIKit/Themes/DA_Shaft_%d.DA_Shaft_%d"),Layer,Layer));
}
void UPOIArtTheme::Apply(FPOIKitSettings& S,FRandomStream& Random) const
{
    const bool Back=S.Kind==EPOIKitKind::Background;
    if (!Back && S.Kind==EPOIKitKind::SideWall) return;
    const UPOIArtPool* Pool=Back?Backgrounds:Platforms;
    if (!Pool || Pool->Images.IsEmpty()) return;
    const auto& Pair=Pool->Images[Random.RandRange(0,Pool->Images.Num()-1)];
    if (Pair.Side.IsNull()) return;
    S.Image=Pair.Side; S.ImageUVRect=Pair.SideUV;
    S.TopImage=Back?TSoftObjectPtr<UTexture2D>():Pair.Top; S.TopImageUVRect=Pair.TopUV;
    S.bFitImageToAlpha=true; S.Tint=FLinearColor::White;
    S.Material=Material.IsNull()?TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/POIKit/M_KitImageFit.M_KitImageFit"))):Material;
}
