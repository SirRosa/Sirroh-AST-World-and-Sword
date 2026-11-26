#include "PWG/PWGEnvironmentV2.h"

#include "Camera/CameraTypes.h"
#include "Components/PostProcessComponent.h"
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "GameFramework/Pawn.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PixelFormat.h"
#include "GameFramework/Actor.h"
#include "SceneView.h"
#include "EngineUtils.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"

DEFINE_LOG_CATEGORY_STATIC(LogPWGEnvironmentV2, Log, All);

namespace
{
	constexpr int32 MasterWideCellColumns = 16;
	constexpr int32 MasterWideCellRows = 32;
	// Every landscape band has a transparent separator at its top. Start 20 px
	// below the nominal boundary so filtering/mips cannot pull pixels from the
	// preceding atlas region into the panorama's upper edge.
	constexpr float MasterLandscapeTopPaddingV = 20.0f/4096.0f;
	constexpr float MasterLandscapeCloseMinV = 0.5f+MasterLandscapeTopPaddingV; // Y 2068..2816
	constexpr float MasterLandscapeCloseMaxV = 11.0f/16.0f;
	constexpr float MasterLandscapeMidMinV = 11.0f/16.0f+MasterLandscapeTopPaddingV; // Y 2836..3328
	constexpr float MasterLandscapeMidMaxV = 13.0f/16.0f;
	constexpr float MasterLandscapeFarMinV = 13.0f/16.0f+MasterLandscapeTopPaddingV; // Y 3348..4096
	constexpr float MasterLandscapeFarMaxV = 1.0f;

	TSoftObjectPtr<UTexture2D> TextureRef(const TCHAR* Path)
	{
		return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Path));
	}

	void ConfigureVisualMesh(UProceduralMeshComponent* Mesh, USceneComponent* Parent, int32 Priority)
	{
		Mesh->SetupAttachment(Parent);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetReceivesDecals(false);
		Mesh->SetVisibleInRayTracing(false);
		Mesh->SetAffectDistanceFieldLighting(false);
		Mesh->SetAffectDynamicIndirectLighting(false);
		Mesh->TranslucencySortPriority = Priority;
	}
}

APWGEnvironmentV2::APWGEnvironmentV2()
{
	PrimaryActorTick.bCanEverTick = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("EnvironmentV2Root"));
	SetRootComponent(SceneRoot);
    DepthPostProcess=CreateDefaultSubobject<UPostProcessComponent>(TEXT("GlobalDepthAtmosphere"));
    DepthPostProcess->SetupAttachment(SceneRoot);
    DepthPostProcess->bUnbound=true;
    DepthPostProcess->Priority=20;
    DepthPostProcess->BlendWeight=0;
    // Config/DefaultGame.ini already always-cooks this PWG directory.
    StylizedDepthAsset=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
        TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_DepthAtmosphere_SemanticPOI.M_PWGv2_DepthAtmosphere_SemanticPOI")));


	SkyMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("EnvironmentV2_Sky"));
	ConfigureVisualMesh(SkyMesh, SceneRoot, -1000);
	for (int32 Layer = 0; Layer < 3; ++Layer)
	{
		UProceduralMeshComponent* Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(
			*FString::Printf(TEXT("EnvironmentV2_Landscape_%d"), Layer));
		ConfigureVisualMesh(Mesh, SceneRoot, -600 + Layer * 100);
		LandscapeMeshes.Add(Mesh);
	}
	CloudMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("EnvironmentV2_Clouds"));
	ConfigureVisualMesh(CloudMesh, SceneRoot, -800);
	SunMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("EnvironmentV2_Sun"));
	ConfigureVisualMesh(SunMesh, SceneRoot, -900);
	MoonMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("EnvironmentV2_Moon"));
	ConfigureVisualMesh(MoonMesh, SceneRoot, -900);

	SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("EnvironmentV2_SunLight"));
	SunLight->SetupAttachment(SceneRoot);
	SunLight->SetMobility(EComponentMobility::Movable);
	SunLight->SetCastShadows(true);
	SunLight->SetForwardShadingPriority(0);
	MoonLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("EnvironmentV2_MoonLight"));
	MoonLight->SetupAttachment(SceneRoot);
	MoonLight->SetMobility(EComponentMobility::Movable);
	MoonLight->SetCastShadows(false);
	MoonLight->SetForwardShadingPriority(0);
	CameraFillLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("EnvironmentV2_CameraFillLight"));
	CameraFillLight->SetupAttachment(SceneRoot);
	CameraFillLight->SetMobility(EComponentMobility::Movable);
	CameraFillLight->SetCastShadows(false);
	CameraFillLight->SetForwardShadingPriority(10);
	// Channel 1 is reserved for PWG vegetation cards. This prevents the camera
	// fill from competing with the sun/moon directional lights on channel 0.
	CameraFillLight->SetLightingChannels(false, true, false);
	CameraFillLight->SetSpecularScale(0.0f);

	SkyTexture = TextureRef(TEXT("/Game/Systems/Sprite2Bone/Textures/Sky/15.15"));
	LandscapeClose.Texture = TextureRef(TEXT("/Game/Systems/Sprite2Bone/Textures/Terrain/Landscape/Landscape_Close.Landscape_Close"));
	LandscapeClose.Depth = 1000.0f;
	LandscapeClose.Parallax = 0.55f;
	LandscapeClose.VerticalOffsetPixels = 0.0f;
	LandscapeClose.Tint = FLinearColor::White;
	LandscapeClose.TreeCount = 12;
	LandscapeClose.RockCount = 10;
	LandscapeClose.BushCount = 12;
	LandscapeClose.TreeHeightRange = FVector2D(120.0f, 280.0f);
	LandscapeClose.RockHeightRange = FVector2D(40.0f, 105.0f);
	LandscapeClose.BushHeightRange = FVector2D(50.0f, 115.0f);
	LandscapeClose.ScatterScale = 0.80f;

	LandscapeMid.Texture = TextureRef(TEXT("/Game/Systems/Sprite2Bone/Textures/Terrain/Landscape/Landscape_Mid.Landscape_Mid"));
	LandscapeMid.Depth = 3000.0f;
	LandscapeMid.Parallax = 0.16f;
	LandscapeMid.VerticalOffsetPixels = 100.0f;
	LandscapeMid.Tint = FLinearColor(0.78f, 0.80f, 0.86f, 1.0f);
	LandscapeMid.Brightness = 0.9f;
	LandscapeMid.TreeCount = 9;
	LandscapeMid.RockCount = 7;
	LandscapeMid.BushCount = 8;
	LandscapeMid.TreeHeightRange = FVector2D(100.0f, 230.0f);
	LandscapeMid.RockHeightRange = FVector2D(32.0f, 85.0f);
	LandscapeMid.BushHeightRange = FVector2D(42.0f, 95.0f);
	LandscapeMid.ScatterScale = 0.58f;

	LandscapeFar.Texture = TextureRef(TEXT("/Game/Systems/Sprite2Bone/Textures/Terrain/Landscape/Landscape_Far.Landscape_Far"));
	LandscapeFar.Depth = 6000.0f;
	LandscapeFar.Parallax = 0.035f;
	LandscapeFar.VerticalOffsetPixels = 200.0f;
	LandscapeFar.Tint = FLinearColor(0.58f, 0.64f, 0.76f, 1.0f);
	LandscapeFar.Brightness = 0.78f;
	LandscapeFar.TreeCount = 6;
	LandscapeFar.RockCount = 5;
	LandscapeFar.BushCount = 4;
	LandscapeFar.TreeHeightRange = FVector2D(75.0f, 170.0f);
	LandscapeFar.RockHeightRange = FVector2D(24.0f, 62.0f);
	LandscapeFar.BushHeightRange = FVector2D(30.0f, 72.0f);
	LandscapeFar.ScatterScale = 0.38f;

	CloudAtlas = TextureRef(TEXT("/Game/Systems/Sprite2Bone/Biomes/Meadow/Meadow_Clouds/Meadows_Clouds_Atlas_01.Meadows_Clouds_Atlas_01"));
	BushAtlas = TextureRef(TEXT("/Game/Systems/Sprite2Bone/Biomes/Meadow/Meadow_Bushes/Meadows_Bush_Atlas.Meadows_Bush_Atlas"));
	SunTexture = TextureRef(TEXT("/Game/Systems/Sprite2Bone/Biomes/Sky/Sun_Test.Sun_Test"));
	MoonTexture = TextureRef(TEXT("/Game/Systems/Sprite2Bone/Biomes/Sky/Moon_Test.Moon_Test"));

	for (int32 Index = 1; Index <= 12; ++Index)
	{
		TreeSprites.Add(TextureRef(*FString::Printf(
			TEXT("/Game/Systems/Sprite2Bone/Biomes/Meadow/Meadow_Trees/Tree_Meadow_%02d.Tree_Meadow_%02d"), Index, Index)));
	}
	for (int32 Index = 1; Index <= 12; ++Index)
	{
		RockSprites.Add(TextureRef(*FString::Printf(
			TEXT("/Game/Systems/Sprite2Bone/Biomes/Meadow/Meadow_Rocks/Meadow_Rock_%02d.Meadow_Rock_%02d"), Index, Index)));
	}
}

void APWGEnvironmentV2::BeginPlay()
{
	Super::BeginPlay();
	if (bDisableExistingDirectionalLights)
	{
		DisableOldDirectionalLights();
	}
}

namespace
{
    void SetSkyOption(AActor* Actor, const FName Name, const bool Value)
    {
        if (auto* Prop = FindFProperty<FBoolProperty>(Actor->GetClass(), Name))
            Prop->SetPropertyValue_InContainer(Actor, Value);
    }

    void SelectSkyOption(AActor* Actor, const FName Name, const FString& Label)
    {
        FProperty* Prop = Actor->GetClass()->FindPropertyByName(Name);
        if (!Prop) return;
        const auto* Byte = CastField<FByteProperty>(Prop);
        const auto* Typed = CastField<FEnumProperty>(Prop);
        const UEnum* Enum = Byte ? Byte->GetIntPropertyEnum() : Typed ? Typed->GetEnum() : nullptr;
        if (!Enum) return;
        for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
        {
            if (Enum->GetAuthoredNameStringByIndex(Index) == Label)
            {
                auto* Numeric = CastField<FNumericProperty>(Prop);
                if (auto* EnumProp = CastField<FEnumProperty>(Prop)) Numeric = EnumProp->GetUnderlyingProperty();
                if (Numeric) Numeric->SetIntPropertyValue(Prop->ContainerPtrToValuePtr<void>(Actor), Enum->GetValueByIndex(Index));
                return;
            }
        }
    }
}

void APWGEnvironmentV2::StartDynamicSky()
{
    if (DynamicSky) return;
    auto* SkyClass = LoadClass<AActor>(nullptr, TEXT("/Game/UltraDynamicSky/Blueprints/Ultra_Dynamic_Sky.Ultra_Dynamic_Sky_C"));
    auto* WeatherClass = LoadClass<AActor>(nullptr, TEXT("/Game/UltraDynamicSky/Blueprints/Ultra_Dynamic_Weather.Ultra_Dynamic_Weather_C"));
    auto* StyleClass = LoadClass<AActor>(nullptr, TEXT("/Game/StyleHex_Studio/Stylized_Sky_Lite/Blueprints/BP_Stylized_Sky_Lite_SoftFantasy.BP_Stylized_Sky_Lite_SoftFantasy_C"));
    if (!SkyClass || !WeatherClass || !StyleClass)
    {
        UE_LOG(LogPWGEnvironmentV2, Error, TEXT("Sky assets could not load. Keeping PWG panorama."));
        return;
    }
    const FTransform Origin(GetActorLocation());
    auto Spawn = [&](UClass* Class) { return GetWorld()->SpawnActorDeferred<AActor>(Class, Origin, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn); };
    DynamicSky = Spawn(SkyClass);
    if (!DynamicSky) return;
    SetSkyOption(DynamicSky, TEXT("Animate Time of Day"), true);
    SelectSkyOption(DynamicSky, TEXT("Sky Mode"), TEXT("No Clouds"));
    SetSkyOption(DynamicSky, TEXT("Apply Exposure Settings"), false);
    for (const auto& Setting : { TPair<FName,double>(TEXT("Sun Light Intensity"), SunIntensity),
                                 TPair<FName,double>(TEXT("Moon Light Intensity"), MoonIntensity),
                                 TPair<FName,double>(TEXT("Day Length"), FMath::Max(10.f, DayDuration) / 120.0),
                                 TPair<FName,double>(TEXT("Night Length"), FMath::Max(10.f, DayDuration) / 120.0) })
        if (auto* Prop = FindFProperty<FNumericProperty>(SkyClass, Setting.Key); Prop && Prop->IsFloatingPoint())
            Prop->SetFloatingPointPropertyValue(Prop->ContainerPtrToValuePtr<void>(DynamicSky.Get()), Setting.Value);
    DynamicSky->FinishSpawning(Origin);
    DynamicWeather = Spawn(WeatherClass);
    if (!DynamicWeather)
    {
        DynamicSky->Destroy();
        DynamicSky = nullptr;
        return;
    }
    SelectSkyOption(DynamicWeather, TEXT("Random Weather Variation"), TEXT("Random Interval"));
    DynamicWeather->FinishSpawning(Origin);
    StyledSky = Spawn(StyleClass);
    if (!StyledSky)
    {
        DynamicWeather->Destroy();
        DynamicSky->Destroy();
        DynamicWeather = nullptr;
        DynamicSky = nullptr;
        return;
    }
    SetSkyOption(StyledSky, TEXT("EnableDayNightCycle"), false);
    StyledSky->FinishSpawning(Origin);
    StyledSky->SetActorTickEnabled(false);
    StyledSky->GetRootComponent()->SetMobility(EComponentMobility::Movable);
    for (auto* Component : StyledSky->GetComponents())
    {
        if (auto* Post = Cast<UPostProcessComponent>(Component)) Post->bEnabled = false;
        if (Component->IsA<ULightComponentBase>() || Component->IsA<UExponentialHeightFogComponent>())
            CastChecked<USceneComponent>(Component)->SetVisibility(false);
        auto* SourceMesh = Cast<UStaticMeshComponent>(Component);
        if (SourceMesh && (SourceMesh->GetName() == TEXT("SunMesh") || SourceMesh->GetName() == TEXT("MoonMesh")))
        {
            // A side-on orthographic camera sees the authored orbital discs edge-on at noon.
            // Keep their animated materials and project their original orbit onto the camera plane.
            UStaticMeshComponent* Disc = NewObject<UStaticMeshComponent>(this);
            Disc->SetupAttachment(SceneRoot);
            Disc->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
            Disc->SetTranslucentSortPriority(1);
            Disc->SetMaterial(0, SourceMesh->GetMaterial(0));
            Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Disc->SetCastShadow(false);
            Disc->RegisterComponent();
            SourceMesh->SetHiddenInGame(true);
            StyleDiscs.Add(SourceMesh);
            CameraDiscs.Add(Disc);
        }
        if (auto* Dome = Cast<UStaticMeshComponent>(Component); Dome && Dome->GetName() == TEXT("SkyDome"))
            StyleDomeRadius = Dome->Bounds.BoxExtent.X / FMath::Max(0.001, StyledSky->GetActorScale3D().X);
        if (auto* Mesh = Cast<UStaticMeshComponent>(Component); Mesh && Mesh->GetName().Contains(TEXT("CloudsDome")))
            StyledClouds = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
    }
    for (auto* Component : DynamicSky->GetComponents())
        {
            if (auto* Mesh = Cast<UStaticMeshComponent>(Component)) Mesh->SetHiddenInGame(true);
            if (auto* Fog = Cast<UExponentialHeightFogComponent>(Component)) Fog->SetVisibility(false);
        }
    SkyTime = SkyClass->FindPropertyByName(TEXT("Time of Day"));
    StyleTime = StyleClass->FindPropertyByName(TEXT("TimeOfDay"));
    ReadCloudCoverage = DynamicSky->FindFunction(TEXT("Get Cloud Coverage 0-10"));
    if (ReadCloudCoverage && ReadCloudCoverage->NumParms == 1)
        for (TFieldIterator<FNumericProperty> It(ReadCloudCoverage); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
            if (It->IsFloatingPoint() && It->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm)) CoverageOutput = *It;
    UpdateStyle = StyledSky->FindFunction(TEXT("UpdateSky"));
    SunLight->SetVisibility(false);
    MoonLight->SetVisibility(false);
    UE_LOG(LogPWGEnvironmentV2, Display, TEXT("UDS/StyleHex time=%s styleTime=%s update=%s inputs=%d clouds=%s weather=%s"),
        *GetNameSafe(SkyTime), *GetNameSafe(StyleTime), *GetNameSafe(UpdateStyle), UpdateStyle ? UpdateStyle->ParmsSize : -1,
        *GetNameSafe(StyledClouds), *GetNameSafe(CoverageOutput));
}

float APWGEnvironmentV2::SyncStyledSky()
{
    const auto* Time = CastField<FNumericProperty>(SkyTime);
    const auto* Target = CastField<FNumericProperty>(StyleTime);
    const double Hours = Time ? Time->GetFloatingPointPropertyValue(Time->ContainerPtrToValuePtr<void>(DynamicSky.Get())) / 100.0 : 12.0;
    if (Target) Target->SetFloatingPointPropertyValue(Target->ContainerPtrToValuePtr<void>(StyledSky.Get()), Hours);
    if (UpdateStyle && UpdateStyle->ParmsSize == 0) StyledSky->ProcessEvent(UpdateStyle, nullptr);
    if (CoverageOutput && StyledClouds)
    {
        FStructOnScope Args(ReadCloudCoverage);
        DynamicSky->ProcessEvent(ReadCloudCoverage, Args.GetStructMemory());
        WeatherCloudAmount = FMath::Clamp(CoverageOutput->GetFloatingPointPropertyValue(CoverageOutput->ContainerPtrToValuePtr<void>(Args.GetStructMemory())) / 10.0, 0.0, 1.0);
        StyledClouds->SetScalarParameterValue(TEXT("Cloud Opacity"), WeatherCloudAmount);
    }
    return FMath::Clamp(0.5f - 2.f * FMath::Cos(Hours * UE_TWO_PI / 24.0), 0.0, 1.0);
}

void APWGEnvironmentV2::EndPlay(const EEndPlayReason::Type Reason)
{
    for (AActor* Actor : { StyledSky.Get(), DynamicWeather.Get(), DynamicSky.Get() })
        if (IsValid(Actor)) Actor->Destroy();
    Super::EndPlay(Reason);
}

void APWGEnvironmentV2::Initialize(AActor* InGenerator, const int32 InSeed)
{
	Generator = InGenerator;
    StartDynamicSky();
    if (!DepthMaterial)
    {
        auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_DepthAtmosphere.M_PWGv2_DepthAtmosphere"));
        if (Material)
        {
            DepthMaterial=UMaterialInstanceDynamic::Create(Material,this);
            DepthPostProcess->AddOrUpdateBlendable(DepthMaterial,1.f);
            ActiveDepthMaterial=DepthMaterial;
        }
        else UE_LOG(LogPWGEnvironmentV2,Warning,TEXT("Missing global depth material; run Content/Python/pwg_depth_atmosphere.py"));
    }
    UpdateDepthAtmosphere(1.f);
	// Enforce the runtime route even when the Blueprint was saved before the
	// dedicated vegetation lighting channel was introduced.
	CameraFillLight->SetLightingChannels(false, true, false);
	CameraFillLight->SetCastShadows(false);
	// Blueprintowe wartości zapisane na subkomponentach mogą nadpisać konstruktor.
	// Wymuszamy unikalny priorytet, aby renderer nigdy nie wybierał losowo
	// pomiędzy słońcem, księżycem i światłem wypełniającym z kamery.
	SunLight->SetForwardShadingPriority(0);
	MoonLight->SetForwardShadingPriority(0);
	CameraFillLight->SetForwardShadingPriority(10);
	CameraFillLight->SetVisibility(true, true);
	// Blueprint component transforms from an earlier iteration must not magnify
	// screen-space cards. Sizes below are already expressed in world centimetres.
	SetActorScale3D(FVector::OneVector);
	SceneRoot->SetRelativeScale3D(FVector::OneVector);
	for (UProceduralMeshComponent* Mesh : LandscapeMeshes)
	{
		if (Mesh) Mesh->SetRelativeScale3D(FVector::OneVector);
	}
	SkyMesh->SetRelativeScale3D(FVector::OneVector);
	CloudMesh->SetRelativeScale3D(FVector::OneVector);
	SunMesh->SetRelativeScale3D(FVector::OneVector);
	MoonMesh->SetRelativeScale3D(FVector::OneVector);
	EnvironmentSeed = InSeed == 0 ? 137 : InSeed;
	Random.Initialize(EnvironmentSeed);
	bCloudUsesBiomeMasterAtlas = false;
	
	// A Blueprint may intentionally clear or accidentally lose inherited arrays.
	// Keep the system functional and restore the documented Meadow fallbacks.
	if (TreeSprites.IsEmpty())
	{
		for (int32 Index = 1; Index <= 12; ++Index)
		{
			TreeSprites.Add(TextureRef(*FString::Printf(
				TEXT("/Game/Systems/Sprite2Bone/Biomes/Meadow/Meadow_Trees/Tree_Meadow_%02d.Tree_Meadow_%02d"), Index, Index)));
		}
	}
	if (RockSprites.IsEmpty())
	{
		for (int32 Index = 1; Index <= 12; ++Index)
		{
			RockSprites.Add(TextureRef(*FString::Printf(
				TEXT("/Game/Systems/Sprite2Bone/Biomes/Meadow/Meadow_Rocks/Meadow_Rock_%02d.Meadow_Rock_%02d"), Index, Index)));
		}
	}
	MaterialV2 = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Systems/Sprite2Bone/Materials/M_PWG_Environment_V2.M_PWG_Environment_V2"));
	MaterialV2VT = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Systems/Sprite2Bone/Materials/M_PWG_Environment_V2_VT.M_PWG_Environment_V2_VT"));
	if (!MaterialV2 && !MaterialV2VT)
	{
		UE_LOG(LogPWGEnvironmentV2, Error, TEXT("Environment V2 materials are missing."));
		return;
	}
	UE_LOG(LogPWGEnvironmentV2, Display,
		TEXT("Initialize: normal=%s VT=%s owner=%s seed=%d"),
		*GetNameSafe(MaterialV2), *GetNameSafe(MaterialV2VT), *GetNameSafe(Generator), InSeed);
	if (bDisableExistingDirectionalLights)
	{
		DisableOldDirectionalLights();
	}
	BuildAll();
	bInitialized = true;
	SetActorTickEnabled(true);
}

bool APWGEnvironmentV2::GetView(FVector& OutLocation, FRotator& OutRotation, float& OutWidth, float& OutHeight) const
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->PlayerCameraManager) return false;
	PC->GetPlayerViewPoint(OutLocation, OutRotation);
	const FMinimalViewInfo& View = PC->PlayerCameraManager->GetCameraCacheView();
	int32 PixelsX = 1920;
	int32 PixelsY = 1080;
	PC->GetViewportSize(PixelsX, PixelsY);
	const float Aspect = PixelsY > 0 ? static_cast<float>(PixelsX) / PixelsY : 16.0f / 9.0f;
	OutWidth = View.ProjectionMode == ECameraProjectionMode::Orthographic
		? FMath::Max(100.0f, View.OrthoWidth)
		: 3200.0f;
	OutHeight = OutWidth / FMath::Max(0.1f, Aspect);
	return true;
}

void APWGEnvironmentV2::BuildAll()
{
	FVector ViewLocation;
	FRotator ViewRotation;
	float ViewWidth = 0.0f;
	float ViewHeight = 0.0f;
	if (!GetView(ViewLocation, ViewRotation, ViewWidth, ViewHeight))
	{
		UE_LOG(LogPWGEnvironmentV2, Display, TEXT("Build deferred: player camera is not ready yet."));
		BuiltViewWidth = 0.0f;
		BuiltViewHeight = 0.0f;
		return;
	}
	InitialViewLocation = ViewLocation;
	// Rebuilding for a camera/aspect change must reproduce the same world.
	// Previously the stream continued and every decoration teleported.
	Random.Initialize(EnvironmentSeed);
	BuiltViewWidth = ViewWidth;
	BuiltViewHeight = ViewHeight;
	DynamicMaterials.Reset();
	DynamicMaterialBaseTints.Reset();
	DynamicMaterialBaseBrightness.Reset();
	LandscapeWrapWidths.Init(1.0f, 3);
	if (!DynamicSky) BuildSky(ViewWidth, ViewHeight);
	for (int32 Layer = 0; Layer < 3; ++Layer) BuildLandscape(Layer, ViewWidth, ViewHeight);
	if (!DynamicSky)
    {
        BuildClouds(ViewWidth, ViewHeight);
        BuildCelestial();
    }
	UpdateTransforms(ViewLocation, ViewRotation, ViewWidth, ViewHeight);
	UE_LOG(LogPWGEnvironmentV2, Display,
		TEXT("Built for view %.0fx%.0f: sky=%d close=%d mid=%d far=%d clouds=%d."),
		ViewWidth, ViewHeight,
		SkyMesh->GetNumSections(), LandscapeMeshes[0]->GetNumSections(),
		LandscapeMeshes[1]->GetNumSections(), LandscapeMeshes[2]->GetNumSections(),
		CloudMesh->GetNumSections());
}

void APWGEnvironmentV2::AddQuad(FMeshData& Data, const float X0, const float X1,
	const float Z0, const float Z1, const float Y, const FVector2D& UV0,
	const FVector2D& UV1, const bool bMirror) const
{
	const int32 Base = Data.Vertices.Num();
	Data.Vertices.Append({ FVector(X0,Y,Z0), FVector(X1,Y,Z0), FVector(X1,Y,Z1), FVector(X0,Y,Z1) });
	// The sidescroller camera sits on negative Y and looks towards positive Y.
	// Keep the visible/front face pointed at the camera (-Y), exactly like the
	// terrain and the original sprite-card helpers.  The previous reversed
	// winding pointed every Environment V2 quad away from the camera.
	Data.Triangles.Append({ Base, Base+1, Base+2, Base, Base+2, Base+3 });
	for (int32 I = 0; I < 4; ++I)
	{
		Data.Normals.Add(FVector(0.0f,-1.0f,0.0f));
		Data.Colors.Add(FColor::White);
		Data.Tangents.Add(FProcMeshTangent(1.0f,0.0f,0.0f));
	}
	const float U0 = bMirror ? UV1.X : UV0.X;
	const float U1 = bMirror ? UV0.X : UV1.X;
	Data.UVs.Append({ FVector2D(U0,UV1.Y), FVector2D(U1,UV1.Y), FVector2D(U1,UV0.Y), FVector2D(U0,UV0.Y) });
}

UMaterialInterface* APWGEnvironmentV2::GetMaterialFor(UTexture2D* Texture) const
{
	if (!Texture) return MaterialV2;
	return Texture->VirtualTextureStreaming && MaterialV2VT ? MaterialV2VT : MaterialV2;
}

void APWGEnvironmentV2::CommitSection(UProceduralMeshComponent* Mesh, const int32 Section,
	const FMeshData& Data, UTexture2D* Texture, const FLinearColor& Tint, const float Brightness,
	const bool bAffectByDayNight)
{
	if (!Mesh || !Texture || Data.Vertices.IsEmpty()) return;
	Mesh->CreateMeshSection(Section, Data.Vertices, Data.Triangles, Data.Normals,
		Data.UVs, Data.Colors, Data.Tangents, false);
	UMaterialInterface* Parent = GetMaterialFor(Texture);
	if (!Parent) return;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, this);
	MID->SetTextureParameterValue(TEXT("SpriteTexture"), Texture);
	MID->SetVectorParameterValue(TEXT("SpriteTint"), Tint);
	MID->SetScalarParameterValue(TEXT("SpriteBrightness"), Brightness);
	Mesh->SetMaterial(Section, MID);
	if (bAffectByDayNight)
	{
		DynamicMaterials.Add(MID);
		DynamicMaterialBaseTints.Add(Tint);
		DynamicMaterialBaseBrightness.Add(Brightness);
	}
}

void APWGEnvironmentV2::BuildSky(const float ViewWidth, const float ViewHeight)
{
	SkyMesh->ClearAllMeshSections();
	UTexture2D* Texture = SkyTexture.LoadSynchronous();
	if (!Texture) return;
	const float Aspect = static_cast<float>(Texture->GetSizeX()) / FMath::Max(1, Texture->GetSizeY());
	const float Coverage = FMath::Max(1.0f, SkyCoverageMultiplier);
	// The card must cover both the viewport and its signed vertical offset.
	const float OffsetWorld=FMath::Abs(SkyVerticalOffsetPixels)*ViewHeight/1080.0f;
	float Height = FMath::Max(ViewHeight * Coverage,ViewHeight+2*OffsetWorld+ViewHeight*0.1f);
	float Width = Height * Aspect;
	if (Width < ViewWidth * Coverage)
	{
		Width = ViewWidth * Coverage;
		Height = Width / FMath::Max(0.01f, Aspect);
	}
	FMeshData Data;
	AddQuad(Data, -Width*0.5f, Width*0.5f, -Height*0.5f, Height*0.5f, 0.0f);
	CommitSection(SkyMesh, 0, Data, Texture, FLinearColor::White, 1.0f);
}

const TArray<float>* APWGEnvironmentV2::GetSilhouette(UTexture2D* Texture, const float UVMinY,
	const float UVMaxY) const
{
	if (!Texture) return nullptr;
	static TMap<FString, TArray<float>> Cache;
	const float SafeMinV = FMath::Clamp(FMath::Min(UVMinY, UVMaxY), 0.0f, 1.0f);
	const float SafeMaxV = FMath::Clamp(FMath::Max(UVMinY, UVMaxY), SafeMinV, 1.0f);
	const FString CacheKey = FString::Printf(TEXT("%s|%.6f|%.6f"),
		*Texture->GetPathName(), SafeMinV, SafeMaxV);
	if (TArray<float>* Existing = Cache.Find(CacheKey)) return Existing;
	FTexturePlatformData* Data = Texture->GetPlatformData();
	if (!Data || Data->Mips.IsEmpty()) return nullptr;
	FTexture2DMipMap& Mip = Data->Mips[0];
	const int32 Width = Mip.SizeX;
	const int32 Height = Mip.SizeY;
	const int32 SliceFirstY = FMath::Clamp(FMath::FloorToInt(SafeMinV * Height), 0, Height - 1);
	const int32 SliceLastYExclusive = FMath::Clamp(FMath::CeilToInt(SafeMaxV * Height),
		SliceFirstY + 1, Height);
	const int32 SliceHeight = SliceLastYExclusive - SliceFirstY;
	const EPixelFormat Format = static_cast<EPixelFormat>(Data->PixelFormat);
	if ((Format != PF_B8G8R8A8 && Format != PF_R8G8B8A8) || Width <= 0 || Height <= 0
		|| Mip.BulkData.GetBulkDataSize() < static_cast<int64>(Width) * Height * 4)
	{
		UE_LOG(LogPWGEnvironmentV2, Warning,
			TEXT("Cannot read alpha silhouette of %s. Set Compression=UserInterface2D, Never Stream and Allow CPU Access."),
			*Texture->GetPathName());
		return nullptr;
	}
	const uint8* Pixels = static_cast<const uint8*>(Mip.BulkData.LockReadOnly());
	if (!Pixels) return nullptr;
	const uint8 Threshold = static_cast<uint8>(FMath::Clamp(LandscapeAlphaThreshold,0.0f,1.0f)*255.0f);
	TArray<float> Profile;
	Profile.SetNumUninitialized(Width);
	for (int32 X = 0; X < Width; ++X)
	{
		int32 First = SliceLastYExclusive - 1;
		for (int32 Y = SliceFirstY; Y < SliceLastYExclusive; ++Y)
		{
			if (Pixels[(static_cast<int64>(Y)*Width+X)*4+3] >= Threshold)
			{
				First = Y;
				break;
			}
		}
		Profile[X] = 1.0f - static_cast<float>(First - SliceFirstY)
			/ FMath::Max(1, SliceHeight - 1);
	}
	Mip.BulkData.Unlock();
	return &Cache.Add(CacheKey, MoveTemp(Profile));
}

void APWGEnvironmentV2::BuildLandscape(const int32 LayerIndex, const float ViewWidth, const float ViewHeight)
{
	if (!LandscapeMeshes.IsValidIndex(LayerIndex)) return;
	UProceduralMeshComponent* Mesh = LandscapeMeshes[LayerIndex];
	Mesh->ClearAllMeshSections();
	const FPWGEnvironmentLayerV2* LayerSettings[] = { &LandscapeClose, &LandscapeMid, &LandscapeFar };
	const FPWGEnvironmentLayerV2& Layer = *LayerSettings[LayerIndex];
	UTexture2D* Landscape = nullptr;
	bool bUsesMasterLandscape = false;
	if (LayerIndex < 3)
	{
		
	}
	if (!Landscape) Landscape = Layer.Texture.LoadSynchronous();
	if (!Landscape) return;
	const float MasterLandscapeMinV[] = {
		MasterLandscapeCloseMinV, MasterLandscapeMidMinV, MasterLandscapeFarMinV
	};
	const float MasterLandscapeMaxV[] = {
		MasterLandscapeCloseMaxV, MasterLandscapeMidMaxV, MasterLandscapeFarMaxV
	};
	const float LandscapeUVMinY = bUsesMasterLandscape ? MasterLandscapeMinV[LayerIndex] : 0.0f;
	const float LandscapeUVMaxY = bUsesMasterLandscape ? MasterLandscapeMaxV[LayerIndex] : 1.0f;
	const float Height = FMath::Min(ViewHeight * 0.25f, ViewHeight * Layer.ScreenHeightFraction);
	const float SourceSliceHeight = FMath::Max(1.0f,
		Landscape->GetSizeY() * (LandscapeUVMaxY - LandscapeUVMinY));
	const float Aspect = static_cast<float>(Landscape->GetSizeX()) / SourceSliceHeight;
	const float SegmentWidth = FMath::Max(1.0f, Height * Aspect);
	LandscapeWrapWidths[LayerIndex] = SegmentWidth;
	const float CoverageWidth = ViewWidth * 2.4f;
	const int32 Repeats = FMath::Max(1, FMath::CeilToInt(CoverageWidth / SegmentWidth) + 2);
	const float TotalWidth = Repeats * SegmentWidth;
	const float WorldPerPixel = ViewHeight / 1080.0f;
	const float Bottom = -ViewHeight*0.5f + Layer.VerticalOffsetPixels * WorldPerPixel;

	FMeshData LandscapeData;
	for (int32 Repeat = 0; Repeat < Repeats; ++Repeat)
	{
		const float X0 = -TotalWidth*0.5f + Repeat*SegmentWidth;
		AddQuad(LandscapeData, X0, X0+SegmentWidth, Bottom, Bottom+Height, 0.0f,
			FVector2D(0.0f, LandscapeUVMinY), FVector2D(1.0f, LandscapeUVMaxY));
	}
	CommitSection(Mesh, 0, LandscapeData, Landscape, Layer.Tint, Layer.Brightness);
	const bool bDeveloperSkipScatter = FParse::Param(FCommandLine::Get(), TEXT("PWGEnvironmentNoScatter"));
	if (bDeveloperSkipScatter) return;

	const TArray<float>* Profile = GetSilhouette(Landscape, LandscapeUVMinY, LandscapeUVMaxY);
	if ((!Profile || Profile->IsEmpty()) && bRequireLandscapeAlphaForScatter)
	{
		UE_LOG(LogPWGEnvironmentV2, Warning,
			TEXT("Scatter skipped for %s: its alpha silhouette is unavailable."), *Landscape->GetPathName());
		return;
	}
	TMap<UTexture2D*, FMeshData> SpriteBatches;
	int32 StableDepthOrder = 0;
	const float MaxSpriteHeight = FMath::Max(8.0f, ViewHeight * MaxScatterScreenHeightFraction);
	auto SurfaceAt = [&](const float U)
	{
		if (Profile && !Profile->IsEmpty())
		{
			const int32 PixelX = FMath::Clamp(FMath::FloorToInt(U * Profile->Num()), 0, Profile->Num() - 1);
			return Bottom + (*Profile)[PixelX] * Height;
		}
		return Bottom + Height * 0.35f;
	};
	auto Scatter = [&](const TArray<TSoftObjectPtr<UTexture2D>>& Pool, const int32 Count, const FVector2D HeightRange)
	{
		if (Pool.IsEmpty() || Count <= 0) return;
		// Generate one repeating pattern. Moving the panorama by one segment now
		// reveals the identical next pattern instead of teleporting random objects.
		const int32 PatternCount = FMath::Max(1, FMath::CeilToInt(static_cast<float>(Count) / Repeats));
		for (int32 I = 0; I < PatternCount; ++I)
		{
			UTexture2D* Sprite = Pool[Random.RandRange(0,Pool.Num()-1)].LoadSynchronous();
			if (!Sprite) continue;
			const float U = Random.FRandRange(0.03f, 0.97f);
			const float Surface = SurfaceAt(U);
			const float RequestedHeight = Random.FRandRange(FMath::Min(HeightRange.X,HeightRange.Y),
				FMath::Max(HeightRange.X,HeightRange.Y)) * Layer.ScatterScale;
			const float SpriteHeight = FMath::Clamp(RequestedHeight, 8.0f, MaxSpriteHeight);
			const float SpriteWidth = SpriteHeight * static_cast<float>(Sprite->GetSizeX()) / FMath::Max(1,Sprite->GetSizeY());
			const float Burial = Random.FRandRange(FMath::Min(RootBurialPixels.X,RootBurialPixels.Y),
				FMath::Max(RootBurialPixels.X,RootBurialPixels.Y))*Height/SourceSliceHeight;
			const bool bMirror = Random.FRand() < 0.5f;
			const float CardY = ScatterBehindLandscapeDepth + StableDepthOrder++ * ScatterDepthStep;
			for (int32 Repeat = 0; Repeat < Repeats; ++Repeat)
			{
				const float X = -TotalWidth * 0.5f + Repeat * SegmentWidth + U * SegmentWidth;
				AddQuad(SpriteBatches.FindOrAdd(Sprite), X-SpriteWidth*0.5f, X+SpriteWidth*0.5f,
					Surface-Burial, Surface-Burial+SpriteHeight, CardY, FVector2D::ZeroVector,
					FVector2D(1.0f,1.0f), bMirror);
			}
		}
	};
	if (bEnableTrees) Scatter(TreeSprites, Layer.TreeCount, Layer.TreeHeightRange);
	if (bEnableRocks) Scatter(RockSprites, Layer.RockCount, Layer.RockHeightRange);
	// Bushes belong to streamed PWG surface chunks and are generated as HISM cards.
	// They are intentionally never attached to a landscape panorama.
	int32 Section = 1;
	for (TPair<UTexture2D*,FMeshData>& Batch : SpriteBatches)
	{
		CommitSection(Mesh, Section++, Batch.Value, Batch.Key, Layer.Tint, Layer.Brightness);
	}
}

void APWGEnvironmentV2::BuildClouds(const float ViewWidth, const float ViewHeight)
{
	CloudMesh->ClearAllMeshSections();
	Clouds.Reset();
	if (!bEnableClouds) return;
	if (!CloudAtlas.LoadSynchronous()) return;
	const int32 SafeCloudCount = FMath::Clamp(CloudCount, 0, MaxRenderedClouds);
	const float MaxCloudHeight = FMath::Max(8.0f, ViewHeight * MaxCloudScreenHeightFraction);
	for (int32 I = 0; I < SafeCloudCount; ++I)
	{
		FCloud& Cloud = Clouds.AddDefaulted_GetRef();
		Cloud.X = Random.FRandRange(-ViewWidth,ViewWidth);
		Cloud.Z = Random.FRandRange(-ViewHeight*0.25f,ViewHeight*0.45f);
		Cloud.Height = FMath::Clamp(
			Random.FRandRange(FMath::Min(CloudHeightRange.X,CloudHeightRange.Y),FMath::Max(CloudHeightRange.X,CloudHeightRange.Y)),
			8.0f, MaxCloudHeight);
		Cloud.Width = Cloud.Height*2.0f;
		Cloud.Speed = Random.FRandRange(FMath::Min(CloudSpeedRange.X,CloudSpeedRange.Y),FMath::Max(CloudSpeedRange.X,CloudSpeedRange.Y));
		if (Random.FRand()<0.5f) Cloud.Speed *= -1.0f;
		Cloud.Frame = Random.RandRange(0,31);
	}
	UpdateCloudMesh(ViewWidth);
}

void APWGEnvironmentV2::UpdateCloudMesh(const float ViewWidth)
{
	UTexture2D* Atlas = CloudAtlas.Get();
	if (!Atlas) return;
	FMeshData Data;
	for (int32 CloudIndex = 0; CloudIndex < Clouds.Num(); ++CloudIndex)
	{
		const FCloud& Cloud = Clouds[CloudIndex];
		const int32 Column = Cloud.Frame%4;
		const int32 Row = Cloud.Frame/4;
		const FVector2D UV0 = bCloudUsesBiomeMasterAtlas
			? FVector2D(Column/static_cast<float>(MasterWideCellColumns),
				(8+Row)/static_cast<float>(MasterWideCellRows))
			: FVector2D(Column/4.0f,Row/8.0f);
		const FVector2D UV1 = bCloudUsesBiomeMasterAtlas
			? FVector2D((Column+1)/static_cast<float>(MasterWideCellColumns),
				(9+Row)/static_cast<float>(MasterWideCellRows))
			: FVector2D((Column+1)/4.0f,(Row+1)/8.0f);
		AddQuad(Data,Cloud.X-Cloud.Width*0.5f,Cloud.X+Cloud.Width*0.5f,
			Cloud.Z-Cloud.Height*0.5f,Cloud.Z+Cloud.Height*0.5f,
			CloudIndex * CloudDepthStep,UV0,UV1, false); // Never mirror clouds.
	}
	if (CloudMesh->GetProcMeshSection(0))
	{
		CloudMesh->UpdateMeshSection(0,Data.Vertices,Data.Normals,Data.UVs,Data.Colors,Data.Tangents);
	}
	else
	{
		CommitSection(CloudMesh,0,Data,Atlas,FLinearColor::White,1.0f);
	}
}

void APWGEnvironmentV2::BuildCelestial()
{
	SunMesh->ClearAllMeshSections();
	MoonMesh->ClearAllMeshSections();
	FMeshData Quad;
	AddQuad(Quad,-CelestialSpriteSize.X*0.5f,CelestialSpriteSize.X*0.5f,
		-CelestialSpriteSize.Y*0.5f,CelestialSpriteSize.Y*0.5f,0.0f);
	CommitSection(SunMesh,0,Quad,SunTexture.LoadSynchronous(),FLinearColor::White,1.0f,false);
	CommitSection(MoonMesh,0,Quad,MoonTexture.LoadSynchronous(),FLinearColor::White,1.0f,false);
	SunLight->SetLightColor(SunLightColor);
	MoonLight->SetLightColor(MoonLightColor);
}

void APWGEnvironmentV2::UpdateTransforms(const FVector& ViewLocation, const FRotator& ViewRotation,
	const float ViewWidth, const float ViewHeight)
{
	const FRotationMatrix CameraBasis(ViewRotation);
	const FVector Forward = CameraBasis.GetScaledAxis(EAxis::X);
	const FVector Right = CameraBasis.GetScaledAxis(EAxis::Y);
	const FVector Up = CameraBasis.GetScaledAxis(EAxis::Z);
	// Every exposed Depth value is measured behind the PWG terrain plane, not
	// from the camera. With a camera roughly 1840 cm in front of the terrain,
	// treating Close.Depth=1000 as camera distance placed it in the foreground.
	// The generator actor remains at the world origin while streamed chunks and the
	// camera travel along X. Projecting the full vector to that actor made the
	// background depth accumulate an X (and Z) component whenever the camera was
	// angled. Eventually Landscape Close crossed the terrain plane. The terrain's
	// depth axis is Y, so measure against that plane at the camera's current X/Z.
	const FVector TerrainPlanePoint = Generator
		? FVector(ViewLocation.X, Generator->GetActorLocation().Y, ViewLocation.Z)
		: ViewLocation;
	const float TerrainCameraDepth = FVector::DotProduct(TerrainPlanePoint-ViewLocation,Forward);
	if (StyledSky && StyleDomeRadius > 0)
    {
        const double Radius = ViewWidth * 0.9;
        StyledSky->SetActorScale3D(FVector(Radius / StyleDomeRadius));
        StyledSky->SetActorLocation(ViewLocation + Forward * (TerrainCameraDepth + SkyDepth + Radius) - Up * (ViewHeight * 0.35));
    }
    // The source cards face local -Y. The negative X scale preserves their original
	// left/right image orientation while local -Y continues to face the camera.
	const FRotator CardRotation = FRotationMatrix::MakeFromXZ(-Right, Up).Rotator();
	const FVector CardScale(-1.0f,1.0f,1.0f);
	// Sky is locked to the camera in camera space: translation and rotation cannot
	// reveal its edges or make the panorama slide across the screen.
	const float SkyVerticalOffsetWorld = SkyVerticalOffsetPixels * ViewHeight / 1080.0f;
	SkyMesh->SetWorldTransform(FTransform(CardRotation,
		ViewLocation+Forward*(TerrainCameraDepth+SkyDepth)+Up*SkyVerticalOffsetWorld,
        FVector(-ViewWidth/FMath::Max(1.f,BuiltViewWidth),1.f,ViewHeight/FMath::Max(1.f,BuiltViewHeight))));
	CloudMesh->SetWorldTransform(FTransform(CardRotation,
		ViewLocation+Forward*(TerrainCameraDepth+FMath::Max(6100.0f,SkyDepth-500.0f)),CardScale));
	const FPWGEnvironmentLayerV2* LayerSettings[] = { &LandscapeClose,&LandscapeMid,&LandscapeFar };
	for (int32 I=0;I<3;++I)
	{
		const float Parallax = FMath::Clamp(LayerSettings[I]->Parallax,0.0f,1.0f);
		const float CameraTravel = FVector::DotProduct(ViewLocation-InitialViewLocation, Right);
		const float RawScreenOffset = -CameraTravel*Parallax;
		const float Wrap = LandscapeWrapWidths.IsValidIndex(I) ? FMath::Max(1.0f,LandscapeWrapWidths[I]) : BuiltViewWidth;
		const float WrappedScreenOffset = FMath::Fmod(RawScreenOffset+Wrap*0.5f,Wrap);
		const float NormalizedOffset = (WrappedScreenOffset<0.0f ? WrappedScreenOffset+Wrap : WrappedScreenOffset)-Wrap*0.5f;
		const FVector Location = ViewLocation
			+ Forward*(TerrainCameraDepth+FMath::Max(1.0f,LayerSettings[I]->Depth))
			+ Right*NormalizedOffset;
		LandscapeMeshes[I]->SetWorldTransform(FTransform(CardRotation,Location,CardScale));
	}
}

void APWGEnvironmentV2::UpdateCelestial(const float DeltaSeconds, const FVector& ViewLocation,
	const FRotator& ViewRotation)
{
    float SunAlpha;
    if (DynamicSky)
    {
        SunAlpha = SyncStyledSky();
    }
    else
    {
    	DayClock = FMath::Fmod(DayClock+DeltaSeconds,FMath::Max(10.0f,DayDuration));
    	const float Angle = DayClock/FMath::Max(10.0f,DayDuration)*UE_TWO_PI;
    	const FVector Orbit(FMath::Cos(Angle)*OrbitRadius.X,0.0f,FMath::Sin(Angle)*OrbitRadius.Y);
    	const FRotationMatrix CameraBasis(ViewRotation);
    	const FVector Forward = CameraBasis.GetScaledAxis(EAxis::X);
    	const FVector Right = CameraBasis.GetScaledAxis(EAxis::Y);
    	const FVector Up = CameraBasis.GetScaledAxis(EAxis::Z);
    	const float TerrainCameraDepth = Generator
    		? FVector::DotProduct(Generator->GetActorLocation()-ViewLocation,Forward)
    		: 0.0f;
    	const FRotator CardRotation = FRotationMatrix::MakeFromXZ(-Right, Up).Rotator();
    	const FVector CardScale(-1.0f,1.0f,1.0f);
    	const FVector Centre = ViewLocation
    		+ Forward*(TerrainCameraDepth+FMath::Max(6200.0f,SkyDepth-350.0f));
    	const FVector ScreenOrbit = Right*Orbit.X + Up*Orbit.Z;
    	SunMesh->SetWorldTransform(FTransform(CardRotation,Centre+ScreenOrbit,CardScale));
    	MoonMesh->SetWorldTransform(FTransform(CardRotation,Centre-ScreenOrbit,CardScale));
        SunAlpha = FMath::Clamp(FMath::Sin(Angle)*4.0f+0.5f,0.0f,1.0f);
        const float MoonAlpha = 1.0f - SunAlpha;
    	SunMesh->SetVisibility(!bUndergroundView && SunAlpha>0.01f,true);
    	MoonMesh->SetVisibility(!bUndergroundView && MoonAlpha>0.01f,true);
    	SunLight->SetIntensity(bUndergroundView?0.0f:SunIntensity*SunAlpha);
    	MoonLight->SetIntensity(bUndergroundView?0.0f:MoonIntensity*MoonAlpha + DayFillLightIntensity);
    	FVector SunRayDirection(-Orbit.X, OrbitRadius.X * LightSideComponent, -FMath::Abs(Orbit.Z));
    	FVector MoonRayDirection(Orbit.X, OrbitRadius.X * LightSideComponent, -FMath::Abs(Orbit.Z));
    	const float MinimumDownward = FMath::Max(1.0f, OrbitRadius.X * MinimumLightDownwardComponent);
    	SunRayDirection.Z = FMath::Min(SunRayDirection.Z, -MinimumDownward);
    	MoonRayDirection.Z = FMath::Min(MoonRayDirection.Z, -MinimumDownward);
    	SunLight->SetWorldRotation(SunRayDirection.Rotation());
    	MoonLight->SetWorldRotation(MoonRayDirection.Rotation());
    }
    if (StyledSky)
    {
        const FRotationMatrix Basis(ViewRotation);
        const FVector Right = Basis.GetScaledAxis(EAxis::Y);
        const FVector Up = Basis.GetScaledAxis(EAxis::Z);
        const FVector Forward = Basis.GetScaledAxis(EAxis::X);
        const double Width = StyledSky->GetActorScale3D().X * StyleDomeRadius / 0.9;
        const double Height = Width * BuiltViewHeight / FMath::Max(BuiltViewWidth, 1.f);
        for (int32 Index = 0; Index < StyleDiscs.Num(); ++Index)
        {
            auto* Source = StyleDiscs[Index].Get();
            auto* Disc = CameraDiscs[Index].Get();
            const FVector Direction = (Source->GetComponentLocation() - StyledSky->GetActorLocation()).GetSafeNormal();
            const double Altitude = FVector::DotProduct(Direction, Up);
            const bool bMoon = Source->GetFName() == TEXT("MoonMesh");
            const double Opacity = FMath::SmoothStep(-0.05, 0.08, Altitude)
                * (1.0 - FMath::SmoothStep(0.35, 0.80, static_cast<double>(WeatherCloudAmount)));
            Disc->SetVisibility(!bUndergroundView && Opacity > 0.001);
            if (auto* Material = Cast<UMaterialInstanceDynamic>(Disc->GetMaterial(0)))
                Material->SetScalarParameterValue(bMoon ? TEXT("Moon Visibility") : TEXT("Sun Opacity"), Opacity);
            const FVector Center = StyledSky->GetActorLocation() + Up * Height * 0.35;
            const FVector Position = Center - Forward * Width * 1.05
                + Right * FVector::DotProduct(Direction, Right) * Width * 0.43
                + Up * (Altitude * 0.42 - 0.08) * Height;
            Disc->SetWorldTransform(FTransform(FRotationMatrix::MakeFromZX(-Forward, Right).Rotator(), Position, FVector(Width * (bMoon ? 0.115 : 0.09) / 100.0)));
        }
    }
    const float MoonAlpha = 1.0f - SunAlpha;
    UpdateDepthAtmosphere(SunAlpha);
	const float NightAmount = bEnableBackgroundDayNightTint && !bUndergroundView ? MoonAlpha : 0.0f;
	const FLinearColor NightMultiplier = FMath::Lerp(FLinearColor::White, NightBackgroundTint, NightAmount);
	const float NightBrightness = FMath::Lerp(1.0f, FMath::Clamp(NightBackgroundBrightness,0.0f,1.0f), NightAmount);
	for (int32 Index=0; Index<DynamicMaterials.Num(); ++Index)
	{
		UMaterialInstanceDynamic* MID=DynamicMaterials[Index];
		if (!MID || !DynamicMaterialBaseTints.IsValidIndex(Index)
			|| !DynamicMaterialBaseBrightness.IsValidIndex(Index)) continue;
		MID->SetVectorParameterValue(TEXT("SpriteTint"), DynamicMaterialBaseTints[Index]*NightMultiplier);
		MID->SetScalarParameterValue(TEXT("SpriteBrightness"), DynamicMaterialBaseBrightness[Index]*NightBrightness);
	}

	const float ActiveSun = SunIntensity*SunAlpha;
	const float ActiveMoon = MoonIntensity*MoonAlpha;
	const float ActiveTotal = ActiveSun+ActiveMoon;
	CameraFillLight->SetIntensity(bUndergroundView?2.5f:ActiveTotal*FMath::Max(0.0f,CameraLightFraction));
	CameraFillLight->SetLightColor(bUndergroundView ? FLinearColor::White : ActiveTotal > KINDA_SMALL_NUMBER
		? (SunLightColor*ActiveSun + MoonLightColor*ActiveMoon)/ActiveTotal
		: MoonLightColor);
	// PWG vegetation cards are authored in the generator's XZ plane with a -Y
	// normal. Do not derive the fill direction from ViewRotation: a controller or
	// camera transition may reverse that rotation even though the cards themselves
	// did not rotate, making all ordinary instances dark again.
	const FVector CardNormal = Generator
		? Generator->GetActorTransform().TransformVectorNoScale(-FVector::YAxisVector).GetSafeNormal()
		: -FVector::YAxisVector;
	CameraFillLight->SetWorldRotation(CardNormal.Rotation());

}

void APWGEnvironmentV2::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bInitialized) return;
	FVector ViewLocation;
	FRotator ViewRotation;
	float ViewWidth=0.0f;
	float ViewHeight=0.0f;
	if (!GetView(ViewLocation,ViewRotation,ViewWidth,ViewHeight)) return;
	if (BuiltViewWidth <= 0.0f || BuiltViewHeight <= 0.0f)
	{
		BuildAll();
		return;
	}
    // Zoom changes projection, not the generated environment. Rebuilding here recreated
    // all MIDs with daytime defaults and skipped UpdateCelestial every zoom frame.
	UpdateTransforms(ViewLocation,ViewRotation,ViewWidth,ViewHeight);
	for (FCloud& Cloud : Clouds)
	{
		Cloud.X += Cloud.Speed*DeltaSeconds*RegionWindMultiplier;
		if (Cloud.X>BuiltViewWidth*1.15f) Cloud.X=-BuiltViewWidth*1.15f;
		else if (Cloud.X<-BuiltViewWidth*1.15f) Cloud.X=BuiltViewWidth*1.15f;
	}
	if (!DynamicSky) UpdateCloudMesh(BuiltViewWidth);
	UpdateCelestial(DeltaSeconds,ViewLocation,ViewRotation);

#if !UE_BUILD_SHIPPING
    VerifySkySequence();
    VerifySemanticVisuals();
#endif
    // Opt-in developer verification only. It is inert in normal gameplay and
	// lets unattended rendering tests capture the settled orthographic camera.
	static int32 VerificationFrames = FParse::Param(FCommandLine::Get(), TEXT("PWGEnvironmentScreenshot")) ? 120 : -1;
	if (VerificationFrames > 0 && --VerificationFrames == 0)
	{
		FScreenshotRequest::RequestScreenshot(TEXT("PWGEnvironmentV2_Verification"), false, false);
	}
}

void APWGEnvironmentV2::DisableOldDirectionalLights()
{
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		if (ADirectionalLight* Light = *It)
		{
			Light->SetActorHiddenInGame(true);
			if (Light->GetLightComponent()) Light->GetLightComponent()->SetVisibility(false,true);
		}
	}
}

void APWGEnvironmentV2::SetUndergroundView(bool bUnderground)
{
    bUndergroundView=bUnderground;
    if (StyledSky) StyledSky->SetActorHiddenInGame(bUnderground);
    if (DynamicSky) DynamicSky->SetActorHiddenInGame(bUnderground);
    if (DynamicWeather) DynamicWeather->SetActorHiddenInGame(bUnderground);
    if (bUnderground)
        for (auto& Disc : CameraDiscs) Disc->SetVisibility(false);
    CameraFillLight->SetLightingChannels(bUnderground,true,false);
    SkyMesh->SetVisibility(!bUnderground,true); CloudMesh->SetVisibility(!bUnderground,true);
    for (UProceduralMeshComponent* Mesh:LandscapeMeshes) if (Mesh) Mesh->SetVisibility(!bUnderground,true);
    if (bUnderground) { SunMesh->SetVisibility(false,true); MoonMesh->SetVisibility(false,true); }
}
void APWGEnvironmentV2::SetRegionWeather(float Multiplier,float Wind)
{
    RegionWindMultiplier=FMath::Clamp(Wind,0.f,4.f);
    DepthWeather=FMath::Clamp(Multiplier,0.5f,2.f);
    const int32 Desired=FMath::Clamp(FMath::RoundToInt(Multiplier*12),0,64);
    if (!DynamicSky && Desired!=MaxRenderedClouds)
    {
        MaxRenderedClouds=Desired;
        BuildAll();
        SetUndergroundView(bUndergroundView);
    }
}



bool APWGEnvironmentV2::IsDepthAtmosphereActive() const
{
    const auto* WorldGenerator=Cast<APWGv2Generator>(Generator);
    return DepthMaterial && WorldGenerator && WorldGenerator->Settings.bDepthAtmosphere;
}

void APWGEnvironmentV2::RefreshVisualProfile()
{
    if (GetWorld() && IsValid(Generator)) UpdateDepthAtmosphere(LastDepthSunAmount);
}

FString APWGEnvironmentV2::GetVisualProfileStatus() const
{
    const bool bRequested=VisualProfile.Mode==EPWGVisualMode::Underground_Stylized;
    const bool bV2=StylizedDepthMaterial && ActiveDepthMaterial==StylizedDepthMaterial;
    const TCHAR* Effective=!IsDepthAtmosphereActive()?TEXT("Disabled"):bV2?TEXT("Underground_Stylized"):TEXT("Legacy");
    return FString::Printf(TEXT("Runtime: %s | requested: %s | %s | mix %.2f | stencil 201/202/203 | transient | Depth prototype asset: %s | Cave reference: %s"),
        Effective,bRequested?TEXT("Underground_Stylized"):TEXT("Legacy"),
        bUndergroundView?TEXT("underground"):TEXT("surface"),bV2?ProfileMix:0.f,bHasDepthPrototypeAsset?TEXT("loaded"):TEXT("not loaded"),bLastCaveReferenceActive?TEXT("active"):bHasIndependentCaveAsset?TEXT("available"):TEXT("not loaded"));
}

void APWGEnvironmentV2::UpdateDepthAtmosphere(float SunAmount)
{
    LastDepthSunAmount=SunAmount;
    auto* WorldGenerator=Cast<APWGv2Generator>(Generator);
    if (!DepthPostProcess) return;
    DepthPostProcess->BlendWeight=IsDepthAtmosphereActive()?1.f:0.f;
    if (!DepthMaterial || !WorldGenerator) return;
    const auto& S=WorldGenerator->Settings;

    const FPWGVisualProfile Profile=VisualProfile.Sanitized();
    const bool bWantsV2=Profile.ResolveMode(bUndergroundView && S.bDepthAtmosphere,true)==EPWGVisualMode::Underground_Stylized;
    if (bWantsV2 && !bTriedStylizedDepthAsset)
    {
        bTriedStylizedDepthAsset=true;
        if (UMaterialInterface* Parent=LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_DepthAtmosphere_CaveReference.M_PWGv2_DepthAtmosphere_CaveReference")))
        {
            StylizedDepthMaterial=UMaterialInstanceDynamic::Create(Parent,this);
            bHasIndependentCaveAsset=IsValid(StylizedDepthMaterial);
            bHasDepthPrototypeAsset=bHasIndependentCaveAsset;
        }
        if (!StylizedDepthMaterial)
        if (UMaterialInterface* Parent=LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_DepthAtmosphere_DepthLayersPrototype.M_PWGv2_DepthAtmosphere_DepthLayersPrototype")))
        {
            StylizedDepthMaterial=UMaterialInstanceDynamic::Create(Parent,this);
            bHasDepthPrototypeAsset=IsValid(StylizedDepthMaterial);
        }
        if (!StylizedDepthMaterial)
        {
            if (UMaterialInterface* Parent=StylizedDepthAsset.LoadSynchronous())
                StylizedDepthMaterial=UMaterialInstanceDynamic::Create(Parent,this);
            if (StylizedDepthMaterial)
                UE_LOG(LogPWGEnvironmentV2,Warning,TEXT("Depth Layers prototype asset unavailable; using Stage 1 SemanticPOI."));
        }
        if (!StylizedDepthMaterial)
            UE_LOG(LogPWGEnvironmentV2,Warning,TEXT("Visual Foundation V2 asset unavailable; using Legacy."));
    }
    const bool bV2=Profile.ResolveMode(bUndergroundView && S.bDepthAtmosphere,IsValid(StylizedDepthMaterial))
        ==EPWGVisualMode::Underground_Stylized;
#if !UE_BUILD_SHIPPING
    WorldGenerator->SetVisualMaskPassEnabled(bV2 && !bVisualProbeDisableMaskPass);
#else
    WorldGenerator->SetVisualMaskPassEnabled(bV2);
#endif
    FVector RefView=FVector::ZeroVector; FRotator RefRotation=FRotator::ZeroRotator; float RefW=0,RefH=0;
    const bool bReferenceView=GetView(RefView,RefRotation,RefW,RefH);
    const APawn* RefPawn=UGameplayStatics::GetPlayerPawn(this,0);
    WorldGenerator->UpdateCaveReference(Profile,bV2 && bHasIndependentCaveAsset && bReferenceView && RefPawn && !Profile.bCompareStage1,
        RefView,RefW,RefH,RefPawn?RefPawn->GetActorLocation():FVector::ZeroVector);
    WorldGenerator->SetDepthPrototypePresentation(bV2 && bHasDepthPrototypeAsset && Profile.bUseReliefCheckpoint && !Profile.bCaveReference && Profile.bEnableDepthLayers && !Profile.bCompareStage1,
        Profile.DepthFormationCount,Profile.bShowDepthNear,Profile.bShowDepthMid,Profile.bShowDepthBacking);
    UMaterialInstanceDynamic* Material=bV2?StylizedDepthMaterial.Get():DepthMaterial.Get();
    if (ActiveDepthMaterial.Get()!=Material)
    {
        // One existing component, one active material. Never stack two global passes.
        auto& Blendables=DepthPostProcess->Settings.WeightedBlendables.Array;
        Blendables.RemoveAll([this](const FWeightedBlendable& Entry)
        {
            return Entry.Object.Get()==DepthMaterial.Get() || Entry.Object.Get()==StylizedDepthMaterial.Get();
        });
        DepthPostProcess->AddOrUpdateBlendable(Material,1.f);
        ActiveDepthMaterial=Material;
        ProfileMix=0.f;
        bProfileUniformsInitialized=false;
    }

    const APawn* Pawn=UGameplayStatics::GetPlayerPawn(this,0);
    const FVector Origin=Pawn?Pawn->GetActorLocation():WorldGenerator->GetActorLocation();
    const FVector DepthAxis=WorldGenerator->GetActorTransform().TransformVectorNoScale(FVector::YAxisVector).GetSafeNormal();
    const float Ground=WorldGenerator->GetSurfaceHeightAt(Origin.X,Origin.Y);
    if (FMath::IsFinite(Ground) && Ground>-1.e8f)
    {
        DepthGroundZ=bDepthGroundInitialized?FMath::FInterpTo(DepthGroundZ,Ground,GetWorld()->GetDeltaSeconds(),2.f):Ground;
        bDepthGroundInitialized=true;
    }
    float FrontRange=S.ForegroundDepth;
    FVector ViewLocation; FRotator ViewRotation; float Width=0,Height=0;
    if (GetView(ViewLocation,ViewRotation,Width,Height))
        FrontRange=FMath::Min(FrontRange,float(FMath::Abs(FVector::DotProduct(Origin-ViewLocation,DepthAxis))));
    auto Scalar=[&](const TCHAR* Name,float V){Material->SetScalarParameterValue(Name,V);};
    Material->SetVectorParameterValue(TEXT("PlayerOrigin"),FLinearColor(Origin.X,Origin.Y,Origin.Z,0));
    Material->SetVectorParameterValue(TEXT("DepthAxis"),FLinearColor(DepthAxis.X,DepthAxis.Y,DepthAxis.Z,0));
    FLinearColor FogColor=FMath::Lerp(S.DepthNightFogColor,S.DepthDayFogColor,SunAmount);
    const float Dusk=4.f*SunAmount*(1.f-SunAmount)*0.25f;
    FogColor=FMath::Lerp(FogColor,FLinearColor(0.62f,0.42f,0.34f),Dusk);
    if (bUndergroundView) FogColor=FLinearColor(0.035f,0.045f,0.065f);
    Material->SetVectorParameterValue(TEXT("FogColor"),FogColor);
    Scalar(TEXT("NeutralWidth"),FMath::Max(50.f,S.DepthNeutralWidth));
    Scalar(TEXT("BackRange"),FMath::Max(200.f,S.BackgroundDepth));
    Scalar(TEXT("FrontRange"),FMath::Max(200.f,FrontRange));
    Scalar(TEXT("FogStrength"),FMath::Clamp(S.DepthFogStrength,0.f,1.f)*(bUndergroundView?0.3f:1.f));
    Scalar(TEXT("Desaturation"),FMath::Clamp(S.DepthDesaturation,0.f,1.f));
    Scalar(TEXT("ForegroundDarkness"),FMath::Clamp(S.DepthForegroundDarkness,0.f,1.f));
    Scalar(TEXT("GroundZ"),DepthGroundZ);
    Scalar(TEXT("MistHeight"),FMath::Max(20.f,S.DepthMistHeight));
    Scalar(TEXT("MistStrength"),bUndergroundView?0.f:FMath::Clamp(S.DepthGroundMist*DepthWeather,0.f,0.4f));
    Scalar(TEXT("PixelBands"),FMath::Clamp(S.DepthPixelBands,0.f,1.f));
    Scalar(TEXT("WindTime"),GetWorld()->GetTimeSeconds()*FMath::Clamp(RegionWindMultiplier,0.1f,2.f));
    Scalar(TEXT("SkyStart"),FMath::Max(S.BackgroundDepth+500.f,SkyDepth-800.f));
    Scalar(TEXT("SkyEnd"),SkyDepth-450.f);

    if (bV2)
    {
        const FVector ProfileOrigin=WorldGenerator->GetActorLocation();
        const float Neutral=Profile.ProtectedHalfWidth(S.LaneSpacing);
        if (!bProfileUniformsInitialized || !Profile.SameShaderValues(LastProfileUniforms)
            || !ProfileOrigin.Equals(LastProfileOrigin) || Neutral!=LastProfileNeutralWidth)
        {
            Material->SetVectorParameterValue(TEXT("ProfileOrigin"),FLinearColor(ProfileOrigin.X,ProfileOrigin.Y,ProfileOrigin.Z,0));
            Material->SetVectorParameterValue(TEXT("ProfileBackgroundTint"),Profile.BackgroundTint);
            Material->SetVectorParameterValue(TEXT("ProfileForegroundTint"),Profile.ForegroundTint);
            Scalar(TEXT("ProfileNeutralWidth"),Neutral);
            Scalar(TEXT("ProfileBackRange"),Profile.BackRange);
            Scalar(TEXT("ProfileFrontRange"),Profile.FrontRange);
            Scalar(TEXT("ProfileStrength"),Profile.Strength);
            Scalar(TEXT("ProfileFogStrength"),Profile.FogStrength);
            Scalar(TEXT("ProfileDesaturation"),Profile.Desaturation);
            Scalar(TEXT("ProfileForegroundDarkness"),Profile.ForegroundDarkness);
            Scalar(TEXT("ProfileBackgroundBrightness"),Profile.BackgroundBrightness);
            Scalar(TEXT("ProfileBackgroundContrast"),Profile.BackgroundContrast);
            Scalar(TEXT("ProfileBackgroundAmbient"),Profile.bDisableBackgroundAmbient?0.f:Profile.BackgroundAmbient);
            Material->SetVectorParameterValue(TEXT("ProfileAtmosphereColor"),Profile.AtmosphereColor);
            Scalar(TEXT("ProfileGameplayBrightness"),Profile.GameplayBrightness);
            Scalar(TEXT("ProfileGameplayContrast"),Profile.GameplayContrast);
            Material->SetVectorParameterValue(TEXT("ProfileGameplayTint"),Profile.GameplayTint);
            Scalar(TEXT("ProfileVisualizeMasks"),Profile.bVisualizeMasks?1.f:0.f);
            if (bHasDepthPrototypeAsset)
            {
                Scalar(TEXT("DepthGradeEnabled"),Profile.bUseReliefCheckpoint && Profile.bEnableDepthGrading && !Profile.bCompareStage1?1.f:0.f);
                Scalar(TEXT("DepthGradeStart"),Profile.DepthStart);
                Scalar(TEXT("DepthGradeRange"),Profile.DepthRange);
                Scalar(TEXT("DepthGradeBrightness"),Profile.DepthBrightness);
                Scalar(TEXT("DepthGradeContrast"),Profile.DepthContrast);
                Scalar(TEXT("DepthGradeDesaturation"),Profile.DepthDesaturation);
                Scalar(TEXT("DepthGradeTintStrength"),Profile.DepthTintStrength);
                Scalar(TEXT("DepthGradeFog"),Profile.DepthFog);
                Material->SetVectorParameterValue(TEXT("DepthGradeTint"),Profile.DepthTint);
                Scalar(TEXT("DepthLayerDebug"),Profile.bVisualizeDepthLayers && !Profile.bCompareStage1?1.f:0.f);
            }
            if (bHasIndependentCaveAsset)
                for(int32 I=0;I<4;++I)
                {
                    const auto& G=Profile.CaveGrade(I);
                    Material->SetVectorParameterValue(*FString::Printf(TEXT("CaveGrade%d"),I),FLinearColor(G.Brightness,G.Contrast,G.Saturation,G.Haze));
                    Material->SetVectorParameterValue(*FString::Printf(TEXT("CaveTint%d"),I),G.Tint);
                }
            LastProfileUniforms=Profile;
            LastProfileOrigin=ProfileOrigin;
            LastProfileNeutralWidth=Neutral;
        }
        if (bHasDepthPrototypeAsset)
        {
            // Matches the prototype's physical rear bounds, not Pawn or grading range.
            const float Span=FMath::Max(1.f,FMath::Min(S.BackgroundDepth,S.LaneSpacing+450.f)-260.f);
            const bool bActive=WorldGenerator->IsDepthPrototypeActive();
            if (!bProfileUniformsInitialized || Span!=LastDepthLayerSpan)
                Scalar(TEXT("DepthLayerSpan"),Span);
            if (!bProfileUniformsInitialized || bActive!=bLastDepthLayerActive)
                Scalar(TEXT("DepthLayerActive"),bActive?1.f:0.f);
            LastDepthLayerSpan=Span; bLastDepthLayerActive=bActive;
        }
        if (bHasIndependentCaveAsset)
        {
            const bool bRef=WorldGenerator->IsCaveReferenceActive();
            const float Start=WorldGenerator->GetCaveReferenceStart(), Depth=WorldGenerator->GetCaveReferenceDepth();
            if(!bProfileUniformsInitialized || bRef!=bLastCaveReferenceActive) Scalar(TEXT("CaveReferenceActive"),bRef?1.f:0.f);
            if(!bProfileUniformsInitialized || Start!=LastCaveStart) Scalar(TEXT("CaveReferenceStart"),Start);
            if(!bProfileUniformsInitialized || Depth!=LastCaveDepth) Scalar(TEXT("CaveReferenceDepth"),Depth);
            bLastCaveReferenceActive=bRef; LastCaveStart=Start; LastCaveDepth=Depth;
        }
        const float NextMix=Profile.BlendSeconds<=KINDA_SMALL_NUMBER?1.f:
            FMath::FInterpConstantTo(ProfileMix,1.f,GetWorld()->GetDeltaSeconds(),1.f/Profile.BlendSeconds);
        if (!bProfileUniformsInitialized || NextMix!=ProfileMix)
            Scalar(TEXT("ProfileMix"),NextMix);
        ProfileMix=NextMix;
        bProfileUniformsInitialized=true;
    }
}
