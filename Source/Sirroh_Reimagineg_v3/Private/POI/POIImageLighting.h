#pragma once
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Texture.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

// The POI PNG contract is ImageTexture + Tint + UVRect. Do not replace
// unrelated emissive effects, translucent overlays, fire, or arbitrary shaders.
namespace POIImageLighting
{
    inline UMaterialInterface* LitImageMaterial()
    {
        return LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/POIKit/M_KitImageFit_LitBackground.M_KitImageFit_LitBackground"));
    }
    inline bool IsUnlit(UMaterialInterface* Material)
    {
        return Material && Material->GetMaterial() && Material->GetMaterial()->GetShadingModels().HasShadingModel(MSM_Unlit);
    }
    inline int32 ApplyToActor(AActor* Actor)
    {
        if (!IsValid(Actor)) return 0;
        auto* Lit=LitImageMaterial();
        if (!Lit) return 0;
        int32 Changed=0;
        TArray<UStaticMeshComponent*> Meshes;
        Actor->GetComponents<UStaticMeshComponent>(Meshes,true);
        const FMaterialParameterInfo ImageParam(FName(TEXT("ImageTexture")));
        const FMaterialParameterInfo TintParam(FName(TEXT("Tint")));
        const FMaterialParameterInfo RectParam(FName(TEXT("UVRect")));
        for (auto* Mesh:Meshes) if (IsValid(Mesh))
            for (int32 Slot=0;Slot<Mesh->GetNumMaterials();++Slot)
            {
                auto* Source=Mesh->GetMaterial(Slot);
                if (!IsUnlit(Source) || (Source->GetBlendMode()!=BLEND_Masked && Source->GetBlendMode()!=BLEND_Opaque)) continue;
                const FName Parent=Source->GetMaterial()->GetFName();
                const bool Known=Parent==FName(TEXT("M_KitImageFit")) || Parent==FName(TEXT("M_KitImage")) || Parent==FName(TEXT("M_POIImage"));
                UTexture* Image=nullptr;
                FLinearColor Tint=FLinearColor::White,Rect(0,0,1,1);
                const bool HasImage=Source->GetTextureParameterValue(ImageParam,Image);
                const bool HasTint=Source->GetVectorParameterValue(TintParam,Tint);
                const bool HasRect=Source->GetVectorParameterValue(RectParam,Rect);
                // Also accept custom authored PNG cards implementing the full standard contract.
                if (!Known && !(HasImage && Image && HasTint && HasRect)) continue;
                auto* MID=UMaterialInstanceDynamic::Create(Lit,Mesh);
                if (!MID) continue;
                if (HasImage && Image) MID->SetTextureParameterValue(ImageParam.Name,Image);
                if (HasTint) MID->SetVectorParameterValue(TintParam.Name,Tint);
                if (HasRect) MID->SetVectorParameterValue(RectParam.Name,Rect);
                Mesh->SetMaterial(Slot,MID);
                ++Changed;
            }
        return Changed;
    }
}
