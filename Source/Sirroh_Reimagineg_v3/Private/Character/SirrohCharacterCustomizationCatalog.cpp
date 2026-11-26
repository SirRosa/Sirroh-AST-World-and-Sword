#include "Character/SirrohCharacterCustomizationCatalog.h"








const USirrohCharacterCustomizationCatalog* USirrohCharacterCustomizationCatalog::Get()
{
        return GetDefault<USirrohCharacterCustomizationCatalog>();
}








const FSirrohCharacterBodyPreset* USirrohCharacterCustomizationCatalog::FindBodyPreset(const FName Id) const
{
        return BodyPresets.FindByPredicate([Id](const FSirrohCharacterBodyPreset& Preset)
        {
                return Preset.Id == Id;
        });
}








FName USirrohCharacterCustomizationCatalog::SanitizePresetId(const FName Id) const
{
        if (FindBodyPreset(Id))
        {
                return Id;
        }
        if (FindBodyPreset(FName(TEXT("Default"))))
        {
                return FName(TEXT("Default"));
        }
        return BodyPresets.Num() > 0 ? BodyPresets[0].Id : NAME_None;
}