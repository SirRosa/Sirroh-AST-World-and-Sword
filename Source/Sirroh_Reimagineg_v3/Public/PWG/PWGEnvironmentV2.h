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

