#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "PWGEnvironmentV2.generated.h"

class UPostProcessComponent;
class ADirectionalLight;
class UStaticMeshComponent;
class FNumericProperty;
class AActor;
class UDirectionalLightComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class USceneComponent;
class UTexture2D;

USTRUCT(BlueprintType)
struct FPWGEnvironmentLayerV2
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Landscape")
	TSoftObjectPtr<UTexture2D> Texture;

	/** Positive Y distance behind the centre of the terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Landscape", meta=(ClampMin="100.0", Units="cm"))
	float Depth = 1000.0f;

	/** Apparent horizontal movement relative to the camera: 1=foreground, 0=screen-stationary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Landscape", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Parallax = 0.55f;

	/** Maximum fraction of the visible screen height occupied by this panorama. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Landscape", meta=(ClampMin="0.05", ClampMax="0.25"))
	float ScreenHeightFraction = 0.25f;

	/** Screen-space lift. Mid defaults to +100 px and Far to +200 px. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Landscape")
	float VerticalOffsetPixels = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Atmosphere")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Atmosphere", meta=(ClampMin="0.0", ClampMax="4.0"))
	float Brightness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter", meta=(ClampMin="0", ClampMax="64"))
	int32 TreeCount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter", meta=(ClampMin="0", ClampMax="64"))
	int32 RockCount = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter_UNUSED", meta=(ClampMin="0", ClampMax="64", DisplayName="Bush Count (UNUSED)"))
	int32 BushCount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter", meta=(ClampMin="0.01"))
	FVector2D TreeHeightRange = FVector2D(260.0f, 620.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter", meta=(ClampMin="0.01"))
	FVector2D RockHeightRange = FVector2D(70.0f, 180.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter_UNUSED", meta=(ClampMin="0.01", DisplayName="Bush Height Range (UNUSED)"))
	FVector2D BushHeightRange = FVector2D(90.0f, 210.0f);

	/** Additional artistic scale. Far layers should use smaller silhouettes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter", meta=(ClampMin="0.01", ClampMax="2.0"))
	float ScatterScale = 1.0f;
};

UENUM(BlueprintType)
enum class EPWGVisualMode : uint8
{
    Legacy,
    Underground_Stylized
};

// Display-referred per-plane atmosphere; texture intensity is applied in the lit rock material.
USTRUCT(BlueprintType)
struct FPWGCavePlanGrade
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cave Plan", meta=(ClampMin="0.0",ClampMax="3.0"))
    float Brightness=1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cave Plan", meta=(ClampMin="0.4",ClampMax="2.5"))
    float Contrast=1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cave Plan", meta=(ClampMin="0.0",ClampMax="1.0"))
    float Saturation=1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cave Plan", meta=(ClampMin="0.0",ClampMax="1.0"))
    float Haze=.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cave Plan", meta=(ClampMin="0.0",ClampMax="1.0"))
    float TextureIntensity=.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cave Plan")
    FLinearColor Tint=FLinearColor::White;
    FPWGCavePlanGrade()=default;
    FPWGCavePlanGrade(float B,float C,float S,float H,float T,FLinearColor Color):
        Brightness(B),Contrast(C),Saturation(S),Haze(H),TextureIntensity(T),Tint(Color) {}
    bool operator==(const FPWGCavePlanGrade& O) const
    {return Brightness==O.Brightness && Contrast==O.Contrast && Saturation==O.Saturation && Haze==O.Haze && TextureIntensity==O.TextureIntensity && Tint==O.Tint;}
};

/** Rendering-only settings, never part of FPWGv2WorldRow or a world save. */
USTRUCT(BlueprintType)
struct FPWGVisualProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual Profile")
    EPWGVisualMode Mode = EPWGVisualMode::Underground_Stylized;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual Profile", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Strength = 1.0f;

    /** Extra margin beyond the outer edge of all three gameplay lanes. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual Profile", meta=(ClampMin="0.0", ClampMax="100.0", Units="cm"))
    float NeutralPadding = 10.0f;

    /** Distance after the protected zone at which the ramp reaches its full effect. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual Profile", meta=(ClampMin="20.0", ClampMax="2000.0", Units="cm"))
    float BackRange = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual Profile", meta=(ClampMin="20.0", ClampMax="2000.0", Units="cm"))
    float FrontRange = 130.0f;

    /** Background palette multiplier, independent of physical lane. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual Profile")
    FLinearColor BackgroundTint = FLinearColor(0.90f, 0.86f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual Profile", meta=(ClampMin="0.0", ClampMax="1.0"))
    float FogStrength = 0.40f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual Profile", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Desaturation = 0.65f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual Profile")
    FLinearColor ForegroundTint = FLinearColor(0.78f, 0.66f, 0.90f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual Profile", meta=(ClampMin="0.0", ClampMax="1.0"))
    float ForegroundDarkness = 0.35f;

    /** Display-referred gain on classified rear walls only. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual Profile", meta=(ClampMin="0.0", ClampMax="12.0"))
    float BackgroundBrightness = 4.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual Profile", meta=(ClampMin="0.4", ClampMax="2.5"))
    float BackgroundContrast = 0.85f;

    /** Artistic ambient floor on rear walls only; zero allows a fully black room. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual Profile", meta=(ClampMin="0.0", ClampMax="0.4"))
    float BackgroundAmbient = 0.075f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual Profile")
    FLinearColor AtmosphereColor = FLinearColor(0.48f,0.43f,0.58f,1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual Profile", meta=(ClampMin="0.0", ClampMax="3.0"))
    float GameplayBrightness = 1.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual Profile", meta=(ClampMin="0.4", ClampMax="2.5"))
    float GameplayContrast = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual Profile")
    FLinearColor GameplayTint = FLinearColor(.96f,.91f,1.f,1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Depth Layers")
    bool bEnableDepthGrading = true;

    /** Exact Stage 1 geometry and grading without changing the individual experiment controls. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Depth Layers")
    bool bCompareStage1 = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Depth Layers", meta=(ClampMin="0.0", ClampMax="2000.0", Units="cm"))
    float DepthStart = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Depth Layers", meta=(ClampMin="20.0", ClampMax="4000.0", Units="cm"))
    float DepthRange = 450.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Depth Layers", meta=(ClampMin="0.0", ClampMax="3.0"))
    float DepthBrightness = 1.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Depth Layers", meta=(ClampMin="0.4", ClampMax="2.5"))
    float DepthContrast = .85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Depth Layers", meta=(ClampMin="0.0", ClampMax="1.0"))
    float DepthDesaturation = .5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Depth Layers", meta=(ClampMin="0.0", ClampMax="1.0"))
    float DepthTintStrength = .45f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Depth Layers", meta=(ClampMin="0.0", ClampMax="1.0"))
    float DepthFog = .75f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Depth Layers")
    FLinearColor DepthTint = FLinearColor(.78f,.84f,1.f,1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Depth Layers")
    bool bEnableDepthLayers = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Depth Layers", meta=(ClampMin="0", ClampMax="2"))
    int32 DepthFormationCount = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Depth Layers")
    bool bShowDepthNear = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Depth Layers")
    bool bShowDepthMid = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Depth Layers")
    bool bShowDepthBacking = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Procedural Depth Layers")
    bool bVisualizeDepthLayers = false;

    // Opt-in, pinned reference fragment only; no world-wide rollout yet.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Independent Cave Reference")
    bool bCaveReference=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Independent Cave Reference")
    bool bUseReliefCheckpoint=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Independent Cave Reference",meta=(ClampMin="5.0",ClampMax="50.0",Units="m"))
    float CaveVisualDepthMetres=22.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Independent Cave Reference")
    bool bShowCaveNear=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Independent Cave Reference")
    bool bShowCaveMid=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Independent Cave Reference")
    bool bShowCaveFar=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Independent Cave Reference")
    bool bShowCaveClosure=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Independent Cave Reference")
    FPWGCavePlanGrade CaveNear=FPWGCavePlanGrade(.53f,1.32f,.72f,.24f,.90f,FLinearColor(.72f,.83f,.92f,1));
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Independent Cave Reference")
    FPWGCavePlanGrade CaveMid=FPWGCavePlanGrade(.85f,1.08f,.55f,.56f,.48f,FLinearColor(.84f,.91f,1.f,1));
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Independent Cave Reference")
    FPWGCavePlanGrade CaveFar=FPWGCavePlanGrade(1.14f,.88f,.30f,.83f,.16f,FLinearColor(.93f,.98f,1.f,1));
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Independent Cave Reference")
    FPWGCavePlanGrade CaveClosure=FPWGCavePlanGrade(1.37f,.80f,.12f,.98f,.025f,FLinearColor::White);
    const FPWGCavePlanGrade& CaveGrade(int32 I) const
    {return I==0?CaveNear:I==1?CaveMid:I==2?CaveFar:CaveClosure;}

    // Artistic lift is never applied to gameplay geometry. Character/items remain ungraded.
    static FPWGVisualProfile DemoPreset(int32 Index)
    {
        FPWGVisualProfile P;
        if(Index==0)
        {
            P.BackgroundBrightness=1.f; P.BackgroundContrast=1.f; P.BackgroundAmbient=0.f;
            P.bDisableBackgroundAmbient=true; P.FogStrength=0.f; P.Desaturation=.10f;
            P.BackgroundTint=FLinearColor::White; P.AtmosphereColor=FLinearColor(.35f,.36f,.42f,1.f);
            P.GameplayBrightness=1.f; P.GameplayContrast=1.f; P.GameplayTint=FLinearColor::White; P.ForegroundDarkness=.40f;
            P.DepthBrightness=1.f; P.DepthContrast=1.f; P.DepthFog=0.f; P.DepthTintStrength=0.f;
            P.CaveNear=FPWGCavePlanGrade(1.f,1.f,1.f,0.f,.90f,FLinearColor::White);
            P.CaveMid=FPWGCavePlanGrade(.48f,1.f,1.f,0.f,.48f,FLinearColor::White);
            P.CaveFar=FPWGCavePlanGrade(.20f,1.f,1.f,0.f,.16f,FLinearColor::White);
            P.CaveClosure=FPWGCavePlanGrade(.10f,1.f,1.f,0.f,.025f,FLinearColor::White);
        }
        else if(Index==2)
        {
            P.BackgroundBrightness=3.8f; P.BackgroundContrast=.95f; P.BackgroundAmbient=.065f;
            P.BackgroundTint=FLinearColor(1.f,.82f,.62f,1.f); P.AtmosphereColor=FLinearColor(.70f,.46f,.28f,1.f);
            P.Desaturation=.45f; P.FogStrength=.30f; P.ForegroundTint=FLinearColor(.82f,.69f,.56f,1.f);
            P.GameplayTint=FLinearColor(1.f,.88f,.72f,1.f); P.GameplayBrightness=1.15f;
            P.DepthTint=FLinearColor(1.f,.77f,.55f,1.f);
        }
        else if(Index==3)
        {
            P.bCaveReference=true; P.bUseReliefCheckpoint=false;
            P.AtmosphereColor=FLinearColor(.33f,.47f,.55f,1.f);
            P.BackgroundAmbient=.095f; P.FogStrength=.56f;
            P.CaveNear=FPWGCavePlanGrade(.70f,1.12f,.78f,.13f,.98f,FLinearColor(.76f,.84f,.90f,1));
            P.CaveMid=FPWGCavePlanGrade(.94f,1.02f,.55f,.39f,.70f,FLinearColor(.84f,.91f,1.f,1));
            P.CaveFar=FPWGCavePlanGrade(1.13f,.92f,.30f,.73f,.25f,FLinearColor(.93f,.98f,1.f,1));
            P.CaveClosure=FPWGCavePlanGrade(1.30f,.90f,.12f,.95f,.025f,FLinearColor::White);
        }
        return P;
    }

    /** Blackout is scoped to the artistic background floor, never to real lights. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual Profile")
    bool bDisableBackgroundAmbient = false;

    /** Cyan = rear, magenta = cutaway, yellow = gameplay geometry, green = other. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual Profile")
    bool bVisualizeMasks = false;

    /** Blend into V2; Legacy and surface switch back immediately for exact A/B. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual Profile", meta=(ClampMin="0.0", ClampMax="5.0", Units="s"))
    float BlendSeconds = 0.25f;

    FPWGVisualProfile Sanitized() const
    {
        const FPWGVisualProfile Defaults;
        FPWGVisualProfile Out = *this;
        const auto Safe = [](float V, float Fallback, float Min, float Max)
        {
            return FMath::IsFinite(V) ? FMath::Clamp(V, Min, Max) : Fallback;
        };
        const auto Color = [&Safe](FLinearColor V, const FLinearColor& Fallback)
        {
            return FLinearColor(Safe(V.R,Fallback.R,0.f,1.f), Safe(V.G,Fallback.G,0.f,1.f),
                Safe(V.B,Fallback.B,0.f,1.f), 1.f);
        };
        if (Out.Mode != EPWGVisualMode::Underground_Stylized) Out.Mode = EPWGVisualMode::Legacy;
        Out.Strength = Safe(Strength,Defaults.Strength,0.f,1.f);
        Out.NeutralPadding = Safe(NeutralPadding,Defaults.NeutralPadding,0.f,100.f);
        Out.BackRange = Safe(BackRange,Defaults.BackRange,20.f,2000.f);
        Out.FrontRange = Safe(FrontRange,Defaults.FrontRange,20.f,2000.f);
        Out.BackgroundTint = Color(BackgroundTint,Defaults.BackgroundTint);
        Out.FogStrength = Safe(FogStrength,Defaults.FogStrength,0.f,1.f);
        Out.Desaturation = Safe(Desaturation,Defaults.Desaturation,0.f,1.f);
        Out.ForegroundTint = Color(ForegroundTint,Defaults.ForegroundTint);
        Out.ForegroundDarkness = Safe(ForegroundDarkness,Defaults.ForegroundDarkness,0.f,1.f);
        Out.BackgroundBrightness = Safe(BackgroundBrightness,Defaults.BackgroundBrightness,0.f,12.f);
        Out.BackgroundContrast = Safe(BackgroundContrast,Defaults.BackgroundContrast,0.4f,2.5f);
        Out.BackgroundAmbient = Safe(BackgroundAmbient,Defaults.BackgroundAmbient,0.f,0.4f);
        Out.AtmosphereColor = Color(AtmosphereColor,Defaults.AtmosphereColor);
        Out.GameplayBrightness = Safe(GameplayBrightness,Defaults.GameplayBrightness,0.f,3.f);
        Out.GameplayContrast = Safe(GameplayContrast,Defaults.GameplayContrast,0.4f,2.5f);
        Out.GameplayTint = Color(GameplayTint,Defaults.GameplayTint);
        Out.BlendSeconds = Safe(BlendSeconds,Defaults.BlendSeconds,0.f,5.f);
        Out.DepthStart = Safe(DepthStart,Defaults.DepthStart,0.f,2000.f);
        Out.DepthRange = Safe(DepthRange,Defaults.DepthRange,20.f,4000.f);
        Out.DepthBrightness = Safe(DepthBrightness,Defaults.DepthBrightness,0.f,3.f);
        Out.DepthContrast = Safe(DepthContrast,Defaults.DepthContrast,.4f,2.5f);
        Out.DepthDesaturation = Safe(DepthDesaturation,Defaults.DepthDesaturation,0.f,1.f);
        Out.DepthTintStrength = Safe(DepthTintStrength,Defaults.DepthTintStrength,0.f,1.f);
        Out.DepthFog = Safe(DepthFog,Defaults.DepthFog,0.f,1.f);
        Out.DepthTint = Color(DepthTint,Defaults.DepthTint);
        Out.DepthFormationCount = FMath::Clamp(DepthFormationCount,0,2);
        Out.CaveVisualDepthMetres=Safe(CaveVisualDepthMetres,Defaults.CaveVisualDepthMetres,5.f,50.f);
        auto Grade=[&](FPWGCavePlanGrade& V,const FPWGCavePlanGrade& D)
        {
            V.Brightness=Safe(V.Brightness,D.Brightness,0.f,3.f);
            V.Contrast=Safe(V.Contrast,D.Contrast,.4f,2.5f);
            V.Saturation=Safe(V.Saturation,D.Saturation,0.f,1.f);
            V.Haze=Safe(V.Haze,D.Haze,0.f,1.f);
            V.TextureIntensity=Safe(V.TextureIntensity,D.TextureIntensity,0.f,1.f);
            V.Tint=Color(V.Tint,D.Tint);
        };
        Grade(Out.CaveNear,Defaults.CaveNear); Grade(Out.CaveMid,Defaults.CaveMid);
        Grade(Out.CaveFar,Defaults.CaveFar); Grade(Out.CaveClosure,Defaults.CaveClosure);
        return Out;
    }

    EPWGVisualMode ResolveMode(bool bUnderground, bool bMaterialAvailable) const
    {
        return Mode == EPWGVisualMode::Underground_Stylized && bUnderground && bMaterialAvailable
            ? EPWGVisualMode::Underground_Stylized : EPWGVisualMode::Legacy;
    }

    float ProtectedHalfWidth(float LaneSpacing) const
    {
        // Generator lane centres are -Spacing, 0, +Spacing; each bank is 100 cm wide.
        return (FMath::IsFinite(LaneSpacing) ? FMath::Max(0.f,LaneSpacing) : 100.f)
            + 50.f + (FMath::IsFinite(NeutralPadding) ? FMath::Clamp(NeutralPadding,0.f,100.f) : 10.f);
    }

    bool SameShaderValues(const FPWGVisualProfile& Other) const
    {
        return Strength == Other.Strength && NeutralPadding == Other.NeutralPadding
            && BackRange == Other.BackRange && FrontRange == Other.FrontRange
            && BackgroundTint == Other.BackgroundTint && FogStrength == Other.FogStrength
            && Desaturation == Other.Desaturation && ForegroundTint == Other.ForegroundTint
            && ForegroundDarkness == Other.ForegroundDarkness
            && BackgroundBrightness == Other.BackgroundBrightness && BackgroundContrast == Other.BackgroundContrast
            && BackgroundAmbient == Other.BackgroundAmbient && AtmosphereColor == Other.AtmosphereColor
            && GameplayBrightness == Other.GameplayBrightness && GameplayContrast == Other.GameplayContrast && GameplayTint == Other.GameplayTint
            && bDisableBackgroundAmbient == Other.bDisableBackgroundAmbient && bVisualizeMasks == Other.bVisualizeMasks
            && bCompareStage1 == Other.bCompareStage1 && bEnableDepthGrading == Other.bEnableDepthGrading && DepthStart == Other.DepthStart && DepthRange == Other.DepthRange
            && DepthBrightness == Other.DepthBrightness && DepthContrast == Other.DepthContrast
            && DepthDesaturation == Other.DepthDesaturation && DepthTintStrength == Other.DepthTintStrength
            && DepthFog == Other.DepthFog && DepthTint == Other.DepthTint && bVisualizeDepthLayers == Other.bVisualizeDepthLayers
            && CaveNear==Other.CaveNear && CaveMid==Other.CaveMid && CaveFar==Other.CaveFar && CaveClosure==Other.CaveClosure;
    }
};

/**
 * Completely independent replacement for the original PWG Environment code.
 * One instance belongs to the streaming root generator; chunks never create it.
 */
UCLASS(BlueprintType)
class SIRROH_REIMAGINEG_V3_API APWGEnvironmentV2 : public AActor
{
	GENERATED_BODY()

public:
	APWGEnvironmentV2();

	void Initialize(AActor* InGenerator, int32 InSeed);
    void SetUndergroundView(bool bUnderground);
    void SetRegionWeather(float CloudMultiplier, float WindStrength);
    bool IsDepthAtmosphereActive() const;
    void RefreshVisualProfile();
    void ResetVisualProfileToDemo() { ApplyVisualPreset(1); }
    void ApplyVisualPreset(int32 Index)
    {
        const FPWGVisualProfile Before=VisualProfile;
        VisualProfile=FPWGVisualProfile::DemoPreset(Index);
        if(Index!=3 && Before.bCaveReference)
        {
            VisualProfile.bCaveReference=true;
            VisualProfile.CaveVisualDepthMetres=Before.CaveVisualDepthMetres;
            VisualProfile.bShowCaveNear=Before.bShowCaveNear; VisualProfile.bShowCaveMid=Before.bShowCaveMid;
            VisualProfile.bShowCaveFar=Before.bShowCaveFar; VisualProfile.bShowCaveClosure=Before.bShowCaveClosure;
        }
        RefreshVisualProfile();
    }
    FString GetVisualProfileStatus() const;

    /** Runtime override; not serialized into the world plan. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category="Environment V2|Visual Profiles")
    FPWGVisualProfile VisualProfile;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Sky")
	TSoftObjectPtr<UTexture2D> SkyTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Sky", meta=(ClampMin="1000.0", Units="cm"))
	float SkyDepth = 7000.0f;

	/** Vertical screen-space adjustment. Negative values move the panorama down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Sky")
	float SkyVerticalOffsetPixels = -400.0f;

	/** Extra screen coverage while preserving the source aspect ratio. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Sky", meta=(ClampMin="1.0", ClampMax="2.0"))
	float SkyCoverageMultiplier = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Landscape")
	FPWGEnvironmentLayerV2 LandscapeClose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Landscape")
	FPWGEnvironmentLayerV2 LandscapeMid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Landscape")
	FPWGEnvironmentLayerV2 LandscapeFar;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Scatter")
	TArray<TSoftObjectPtr<UTexture2D>> TreeSprites;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Scatter")
	TArray<TSoftObjectPtr<UTexture2D>> RockSprites;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Scatter_UNUSED", meta=(DisplayName="Bush Atlas (UNUSED - use PWG Generator)"))
	TSoftObjectPtr<UTexture2D> BushAtlas;

	/** Explicit switches are preferable to clearing an asset list, because empty lists use the fallback assets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Scatter")
	bool bEnableTrees = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Scatter")
	bool bEnableRocks = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Scatter_UNUSED", meta=(DisplayName="Enable Bushes (UNUSED - use PWG Generator)"))
	bool bEnableBushes = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Scatter", meta=(ClampMin="0.0", ClampMax="1.0"))
	float LandscapeAlphaThreshold = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Scatter", meta=(ClampMin="0.0"))
	FVector2D RootBurialPixels = FVector2D(10.0f, 30.0f);

	/** Hard safety limit preventing a background sprite from covering the view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Scatter", meta=(ClampMin="0.01", ClampMax="0.3"))
	float MaxScatterScreenHeightFraction = 0.16f;

	/** Places roots behind the opaque part of their landscape instead of fighting its depth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Scatter", meta=(ClampMin="1.0", Units="cm"))
	float ScatterBehindLandscapeDepth = 24.0f;

	/** Stable per-card separation; prevents overlapping masked cards from flickering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Scatter", meta=(ClampMin="0.01", Units="cm"))
	float ScatterDepthStep = 0.35f;

	/** Without a readable alpha silhouette, skip scatter instead of placing it in mid-air. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Scatter")
	bool bRequireLandscapeAlphaForScatter = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Clouds")
	TSoftObjectPtr<UTexture2D> CloudAtlas;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Clouds")
	bool bEnableClouds = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Clouds", meta=(ClampMin="0", ClampMax="64"))
	int32 CloudCount = 18;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Clouds", meta=(ClampMin="0", ClampMax="64"))
	int32 MaxRenderedClouds = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Clouds")
	FVector2D CloudHeightRange = FVector2D(180.0f, 420.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Clouds", meta=(ClampMin="0.02", ClampMax="0.3"))
	float MaxCloudScreenHeightFraction = 0.14f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Clouds", meta=(ClampMin="0.01", Units="cm"))
	float CloudDepthStep = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Clouds", meta=(Units="cm/s"))
	FVector2D CloudSpeedRange = FVector2D(3.0f, 15.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Celestial")
	TSoftObjectPtr<UTexture2D> SunTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Celestial")
	TSoftObjectPtr<UTexture2D> MoonTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Celestial", meta=(Units="s", ClampMin="10.0"))
	float DayDuration = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Celestial")
	FVector2D OrbitRadius = FVector2D(1800.0f, 1050.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Celestial")
	FVector2D CelestialSpriteSize = FVector2D(320.0f, 320.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Lighting")
	bool bDisableExistingDirectionalLights = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Lighting", meta=(ClampMin="0.0"))
	float SunIntensity = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Lighting", meta=(ClampMin="0.0"))
	float MoonIntensity = 0.35f;

	/** 0.30 = 30% of the currently visible sun/moon light. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Lighting", meta=(ClampMin="0.0", ClampMax="2.0"))
	float CameraLightFraction = 0.30f;

	/** Keeps the sun aimed sufficiently down at the terrain even near the horizon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Lighting", meta=(ClampMin="0.05", ClampMax="1.0"))
	float MinimumLightDownwardComponent = 0.45f;

	/** Side component aimed at the visible X/Z faces of the 2.5D world. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Lighting", meta=(ClampMin="-1.0", ClampMax="1.0"))
	float LightSideComponent = 0.30f;

	/** Shadowless opposite light preventing most of the terrain from becoming black. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Lighting", meta=(ClampMin="0.0"))
	float DayFillLightIntensity = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Lighting")
	FLinearColor SunLightColor = FLinearColor(1.0f, 0.72f, 0.43f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Lighting")
	FLinearColor MoonLightColor = FLinearColor(0.32f, 0.48f, 1.0f);

	/** Cheap material lighting for sky, landscapes, clouds and their scatter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Lighting|Background")
	bool bEnableBackgroundDayNightTint = true;

	/** Colour multiplier reached at full night. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Lighting|Background")
	FLinearColor NightBackgroundTint = FLinearColor(0.16f, 0.24f, 0.48f, 1.0f);

	/** Brightness multiplier reached at full night. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Lighting|Background", meta=(ClampMin="0.0", ClampMax="1.0"))
	float NightBackgroundBrightness = 0.32f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Lighting|Terrain")
	FLinearColor NightTerrainTint = FLinearColor(0.20f, 0.30f, 0.68f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment V2|Lighting|Terrain", meta=(ClampMin="0.0", ClampMax="1.0"))
	float NightTerrainBrightness = 0.38f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
#if !UE_BUILD_SHIPPING
    void VerifySkySequence();
    void VerifySemanticVisuals();
    int32 VisualProbeStep=0;
    int32 VisualProbeFrames=0;
    FVector VisualProbeCamera=FVector::ZeroVector;
    FRotator VisualProbeRotation=FRotator::ZeroRotator;
    float VisualProbeWidth=0.f;
    TWeakObjectPtr<AActor> VisualProbeTorch;
    int32 VisualProbeScene=0;
    FVector VisualProbeFocus=FVector::ZeroVector;
    FVector VisualProbeTarget=FVector::ZeroVector;
    FVector VisualProbePawn=FVector::ZeroVector;
    FString VisualProbeLabel;
    float VisualProbeLaneY=0.f;
    bool bVisualProbeDisableMaskPass=false;
    TArray<float> VisualProbeGPU;
    TArray<float> VisualProbeCPU;
    double VisualProbeLastFrame=0.0;
    int32 SkyVerificationFrame = 0;
#endif
    void StartDynamicSky();
    float SyncStyledSky();
    UPROPERTY(Transient) TObjectPtr<AActor> DynamicSky;
    UPROPERTY(Transient) TObjectPtr<AActor> DynamicWeather;
    UPROPERTY(Transient) TObjectPtr<AActor> StyledSky;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> StyledClouds;
    double StyleDomeRadius = 0.0;
    FProperty* SkyTime = nullptr;
    FProperty* StyleTime = nullptr;
    UPROPERTY(Transient) TObjectPtr<UFunction> ReadCloudCoverage;
    FNumericProperty* CoverageOutput = nullptr;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> StyleDiscs;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> CameraDiscs;
    float WeatherCloudAmount = 0.f;
    UFunction* UpdateStyle = nullptr;

	/** True only when CloudAtlas points at the 4096x4096 biome master layout. */
	bool bCloudUsesBiomeMasterAtlas = false;
    bool bUndergroundView = false;
    float RegionWindMultiplier = 1;
	struct FMeshData
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FColor> Colors;
		TArray<FProcMeshTangent> Tangents;
	};

	struct FCloud
	{
		float X = 0.0f;
		float Z = 0.0f;
		float Width = 0.0f;
		float Height = 0.0f;
		float Speed = 0.0f;
		int32 Frame = 0;
	};

    void UpdateDepthAtmosphere(float SunAmount);
    UPROPERTY(Transient) TObjectPtr<UPostProcessComponent> DepthPostProcess;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> DepthMaterial;
    UPROPERTY(EditDefaultsOnly, Category="Environment V2|Visual Profiles")
    TSoftObjectPtr<UMaterialInterface> StylizedDepthAsset;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> StylizedDepthMaterial;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ActiveDepthMaterial;
    bool bTriedStylizedDepthAsset = false;
    bool bHasDepthPrototypeAsset = false;
    bool bHasIndependentCaveAsset=false;
    bool bLastCaveReferenceActive=false;
    float LastCaveStart=-1.f, LastCaveDepth=-1.f;
    float LastDepthLayerSpan=-1.f;
    bool bLastDepthLayerActive=false;
    bool bProfileUniformsInitialized = false;
    FPWGVisualProfile LastProfileUniforms;
    FVector LastProfileOrigin = FVector::ZeroVector;
    float LastProfileNeutralWidth = 0.f;
    float ProfileMix = 0.f;
    float LastDepthSunAmount = 1.f;
    float DepthGroundZ = 0;
    bool bDepthGroundInitialized = false;
    float DepthWeather = 1;
	void BuildAll();
	void BuildSky(float ViewWidth, float ViewHeight);
	void BuildLandscape(int32 LayerIndex, float ViewWidth, float ViewHeight);
	void BuildClouds(float ViewWidth, float ViewHeight);
	void BuildCelestial();
	void UpdateTransforms(const FVector& ViewLocation, const FRotator& ViewRotation, float ViewWidth, float ViewHeight);
	void UpdateCloudMesh(float ViewWidth);
	void UpdateCelestial(float DeltaSeconds, const FVector& ViewLocation, const FRotator& ViewRotation);
	void DisableOldDirectionalLights();
	void AddQuad(FMeshData& Data, float X0, float X1, float Z0, float Z1, float Y,
		const FVector2D& UV0 = FVector2D(0.0f, 0.0f), const FVector2D& UV1 = FVector2D(1.0f, 1.0f), bool bMirror = false) const;
	void CommitSection(UProceduralMeshComponent* Mesh, int32 Section, const FMeshData& Data,
		UTexture2D* Texture, const FLinearColor& Tint, float Brightness, bool bAffectByDayNight = true);
	/** Reads the first opaque pixel inside a vertical UV slice of a texture. */
	const TArray<float>* GetSilhouette(UTexture2D* Texture, float UVMinY = 0.0f,
		float UVMaxY = 1.0f) const;
	UMaterialInterface* GetMaterialFor(UTexture2D* Texture) const;
	bool GetView(FVector& OutLocation, FRotator& OutRotation, float& OutWidth, float& OutHeight) const;

	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> SkyMesh;
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> LandscapeMeshes;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> CloudMesh;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> SunMesh;
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> MoonMesh;
	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> SunLight;
	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> MoonLight;
	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> CameraFillLight;
	UPROPERTY(Transient)
	TObjectPtr<AActor> Generator;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> MaterialV2;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> MaterialV2VT;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;
	TArray<FLinearColor> DynamicMaterialBaseTints;
	TArray<float> DynamicMaterialBaseBrightness;

	TArray<FCloud> Clouds;
	TArray<float> LandscapeWrapWidths;
	FRandomStream Random;
	FVector InitialViewLocation = FVector::ZeroVector;
	float BuiltViewWidth = 0.0f;
	float BuiltViewHeight = 0.0f;
	float DayClock = 0.0f;
	int32 EnvironmentSeed = 137;
	bool bInitialized = false;
};

