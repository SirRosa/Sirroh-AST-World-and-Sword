#include "Debug/SirrohDebugSubsystem.h"
#include "Procedural_World_Generator/PWGv2Generator.h"
#include "PWG/PWGPlayerController.h"
#include "PWG/PWGEnvironmentV2.h"
#include "PWG/PWGSidescrollerInputComponent.h"
#include "Combat/DirectionalCombatComponent.h"
#include "Combat/CombatMotionCatalog.h"
#include "Combat/CombatDragonBodyAdapter.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "Placeholder/PlaceholderHandsComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Components/ActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "MoverComponent.h"
#include "MoveLibrary/ConstrainedMoveUtils.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "UObject/UnrealType.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

namespace SirrohDebug
{
    using FAddress=TFunction<void*()>;
    struct FTarget
    {
        FString Name;
        const UStruct* Type=nullptr;
        TWeakObjectPtr<UObject> Owner;
        FAddress Address;
        TFunction<void()> Changed;
        bool bWorld=false;
        TSet<FString> IncludedFields;
    };
    struct FEdit
    {
        TSharedPtr<FTarget> Target;
        FProperty* Property=nullptr;
        FAddress Address;
        FString Path, Before, After;
    };
    FText Text(const FString& S) { return FText::FromString(S); }
    FString Read(FProperty* P,void* Address)
    {
        FString S;
        if (Address) P->ExportTextItem_Direct(S,Address,nullptr,nullptr,PPF_None);
        return S;
    }
}

class SSirrohDebugPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSirrohDebugPanel) {} SLATE_END_ARGS()
    void Construct(const FArguments&,USirrohDebugSubsystem* InSystem,APlayerController* InPC)
    {
        System=InSystem; PC=InPC;
        for (TActorIterator<APWGv2Generator> It(InPC->GetWorld());It;++It) { Generator=*It; break; }
        for (const TCHAR* Name:{TEXT("Powierzchnia"),TEXT("Podziemia"),TEXT("Jaskinie"),TEXT("Lochy"),TEXT("Katakumby")}) Layers.Add(MakeShared<FString>(Name));
        SelectedLayer=Layers[0];
        for(const TCHAR* Name:{TEXT("Wszystko"),TEXT("Swiat / Regiony"),TEXT("Biomy / Roslinnosc"),TEXT("Podziemia"),TEXT("Combat / Solvery"),TEXT("Animacje"),TEXT("Stawy PhysicsAsset"),TEXT("Pozostale")}) Categories.Add(MakeShared<FString>(Name));
        SelectedCategory=Categories[0];
        ChildSlot
        [ SNew(SOverlay)
          +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(12)
          [ SNew(SBorder).Padding(12).BorderBackgroundColor(FLinearColor(0.015f,0.02f,0.025f,0.95f))
            [ SNew(STextBlock).Text_Lambda([this]{
                APawn* P=PC.IsValid()?PC->GetPawn().Get():nullptr;
                return SirrohDebug::Text(P && Generator.IsValid()?Generator->DebugLocation(P->GetActorLocation()):TEXT("Brak postaci / generatora"));
            }) ] ]
          +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Fill).Padding(10)
          [ SAssignNew(MenuBox,SBox).WidthOverride(520)
            [ SNew(SBorder).Padding(12).BorderBackgroundColor(FLinearColor(0.025f,0.035f,0.045f,0.98f))
              [ SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
                [ SNew(STextBlock).Text(SirrohDebug::Text(TEXT("SIRROH | LIVE DEBUG   [F7 / Esc zamknij]"))) ]
                +SVerticalBox::Slot().AutoHeight()
                [ SNew(STextBlock).AutoWrapText(true).Text(SirrohDebug::Text(TEXT("Ruch i walka dzialaja przy otwartym menu. Edycja: Standalone.\nSwiat: sandbox + Przebuduj. Combat: LIVE, przeliczenie po zatwierdzeniu."))) ]
                +SVerticalBox::Slot().AutoHeight().Padding(0,8)
                [ SNew(SHorizontalBox)
                  +SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("Parametry"),[this]{bTeleport=false; RebuildFields();})]
                  +SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("Teleport To"),[this]{bTeleport=true; RebuildFields();})]
                  +SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("Odswiez cele"),[this]{GatherTargets(); RebuildFields();})] ]
                +SVerticalBox::Slot().AutoHeight()
                [SNew(SExpandableArea).InitiallyCollapsed(false)
                  .HeaderContent()[SNew(STextBlock).Text(SirrohDebug::Text(TEXT("Systemy / wybierz galaz")))]
                  .BodyContent()[SNew(SBox).MaxDesiredHeight(190)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(Navigation,SVerticalBox)]]]]
                +SVerticalBox::Slot().AutoHeight().Padding(0,6)
                [ SNew(SSearchBox).HintText(SirrohDebug::Text(TEXT("Szukaj parametru: grass, strength, anatomy, motion...")))
                  .OnTextChanged_Lambda([this](const FText& T){Search=T.ToString(); RebuildFields();}) ]
                +SVerticalBox::Slot().FillHeight(1)
                [SAssignNew(Scroll,SScrollBox)+SScrollBox::Slot()[SAssignNew(Fields,SVerticalBox)]]
                +SVerticalBox::Slot().AutoHeight().Padding(0,6)
                [SNew(SHorizontalBox)
                  +SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("Cofnij"),[this]{Undo();})]
                  +SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("Reset celu"),[this]{ResetSelected();})]
                  +SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("Eksport zmian JSON"),[this]{Export();})]
                  +SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("Zamknij F7"),[this]{if(System.IsValid()) System->Close();})]]
                +SVerticalBox::Slot().AutoHeight()
                [SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.5f,0.85f,1)).Text_Lambda([this]{return SirrohDebug::Text(Status);})]
              ] ] ]
        ];
        GatherTargets(); RebuildFields();
    }
    virtual void Tick(const FGeometry& G,double Time,float Delta) override { SCompoundWidget::Tick(G,Time,Delta); if(bRefreshTeleport){bRefreshTeleport=false;RebuildFields();} }
    bool PointerOverMenu() const { return MenuBox.IsValid() && MenuBox->IsHovered(); }
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnPreviewKeyDown(const FGeometry&,const FKeyEvent& Event) override
    {
        if (Event.GetKey()==EKeys::F7 || Event.GetKey()==EKeys::Escape)
        { if(System.IsValid()) System->Close(); return FReply::Handled(); }
        return FReply::Unhandled();
    }
private:
    TWeakObjectPtr<USirrohDebugSubsystem> System;
    TWeakObjectPtr<APlayerController> PC;
    TWeakObjectPtr<APWGv2Generator> Generator;
    TArray<TSharedPtr<SirrohDebug::FTarget>> Targets,VisibleTargets;
    TArray<TSharedPtr<FString>> Categories;
    TSharedPtr<FString> SelectedCategory;
    TSharedPtr<SirrohDebug::FTarget> Selected;
    TSharedPtr<SComboBox<TSharedPtr<SirrohDebug::FTarget>>> TargetCombo;
    TSharedPtr<SVerticalBox> Fields,Navigation;
    TSharedPtr<SBox> MenuBox;
    TSharedPtr<SScrollBox> Scroll;
    TArray<SirrohDebug::FEdit> History;
    TArray<TSharedPtr<FString>> Regions,Biomes,Layers;
    TSharedPtr<FString> SelectedRegion,SelectedBiome,SelectedLayer;
    FString Search,Status=TEXT("Zmiany sa tymczasowe. Eksport zachowuje liste zmian w Saved/DebugMenu.");
    bool bTeleport=false,bRefreshTeleport=false;
    TArray<int32> LayerIds;
    int32 FieldCount=0;
    bool CanEdit() const { return PC.IsValid() && PC->GetWorld() && PC->GetWorld()->GetNetMode()==NM_Standalone; }
    TSharedRef<SWidget> Button(const FString& Label,TFunction<void()> Action)
    { return SNew(SButton).IsFocusable(false).Text(SirrohDebug::Text(Label)).OnClicked_Lambda([Action]{Action(); return FReply::Handled();}); }
    void Label(const FString& Value)
    { Fields->AddSlot().AutoHeight().Padding(2,5)[SNew(STextBlock).AutoWrapText(true).Text(SirrohDebug::Text(Value))]; }
    void AddTarget(FString Name,UObject* Object,const UStruct* Type,SirrohDebug::FAddress Address,TFunction<void()> Changed,bool World=false)
    {
        if (!Object || !Type) return;
        auto T=MakeShared<SirrohDebug::FTarget>(); T->Name=Name; T->Owner=Object; T->Type=Type;
        T->Address=MoveTemp(Address); T->Changed=MoveTemp(Changed); T->bWorld=World; Targets.Add(T);
    }
    void AddObject(FString Name,UObject* Object,TFunction<void()> Changed={})
    {
        if (!Object) return;
        TWeakObjectPtr<UObject> Weak=Object;
        AddTarget(Name,Object,Object->GetClass(),[Weak]()->void*{return Weak.Get();},MoveTemp(Changed));
    }
    struct FBranch
    {
        FString Name;
        TArray<TSharedPtr<FBranch>> Children;
        TSharedPtr<SirrohDebug::FTarget> Target;
    };
    TSharedRef<SWidget> BranchWidget(const TSharedPtr<FBranch>& B)
    {
        if(B->Target) return Button(B->Name,[this,B]{Selected=B->Target;bTeleport=false;Search.Reset();RebuildFields();});
        auto Box=SNew(SVerticalBox);
        B->Children.Sort([](const auto& A,const auto& C){return A->Name<C->Name;});
        for(const auto& C:B->Children) Box->AddSlot().AutoHeight().Padding(8,1)[BranchWidget(C)];
        return SNew(SExpandableArea).InitiallyCollapsed(true)
            .HeaderContent()[SNew(STextBlock).Text(SirrohDebug::Text(B->Name))].BodyContent()[Box];
    }
    void FilterTargets()
    {
        if(!Navigation)return;
        Navigation->ClearChildren();
        auto Root=MakeShared<FBranch>();
        for(const auto& T:Targets)
        {
            TArray<FString> Parts;T->Name.ParseIntoArray(Parts,TEXT(" / "),true);
            if(Parts.IsEmpty())continue;
            if(Parts[0]==TEXT("REGION"))Parts[0]=TEXT("Regiony");
            else if(Parts[0]==TEXT("BIOM"))Parts[0]=TEXT("Biomy");
            else if(Parts[0]==TEXT("PODZIEMIA"))Parts[0]=TEXT("Podziemia");
            else if(Parts[0]==TEXT("GRACZ") || Parts[0]==TEXT("NPC"))Parts.Insert(TEXT("Solvery"),0);
            else if(Parts[0].StartsWith(TEXT("KATALOG")))Parts.Insert(TEXT("Katalogi"),0);
            TSharedPtr<FBranch> Parent=Root;
            for(const auto& Part:Parts)
            {
                auto* Found=Parent->Children.FindByPredicate([&](const auto& C){return C->Name==Part;});
                auto Child=Found?*Found:MakeShared<FBranch>();
                if(!Found){Child->Name=Part;Parent->Children.Add(Child);}
                Parent=Child;
            }
            Parent->Target=T;
        }
        Root->Children.Sort([](const auto& A,const auto& B){return A->Name<B->Name;});
        for(const auto& B:Root->Children)Navigation->AddSlot().AutoHeight()[BranchWidget(B)];
    }
    void GatherTargets()
    {
        const FString Old=Selected?Selected->Name:FString();
        Targets.Reset();
        if (Generator.IsValid() && CanEdit())
        {
            TWeakObjectPtr<APWGv2Generator> G=Generator;
            if (auto* Data=G->BeginDebugSession())
            {
                auto Changed=[this]{Status=TEXT("Zmiana swiata oczekuje na Przebuduj. Zapis oryginalnej wyspy jest zablokowany.");};
                AddTarget(TEXT("SWIAT / Ustawienia"),G.Get(),FPWGv2WorldRow::StaticStruct(),[G]()->void*{return G.IsValid() && G->GetDebugEditWorld()?&G->GetDebugEditWorld()->Settings:nullptr;},Changed,true);
                for(int32 I=0;I<Data->Regions.Num();++I)
                    AddTarget(TEXT("REGION / ")+Data->Regions[I].Id.ToString(),G.Get(),FPWGv2RegionRow::StaticStruct(),[G,I]()->void*{auto* D=G.IsValid()?G->GetDebugEditWorld():nullptr; return D && D->Regions.IsValidIndex(I)?&D->Regions[I].Definition:nullptr;},Changed,true);
                for(int32 I=0;I<Data->Biomes.Num();++I)
                    AddTarget(FString::Printf(TEXT("BIOM / %s [%d]"),*Data->Biomes[I].Id.ToString(),I),G.Get(),FPWGv2BiomeRow::StaticStruct(),[G,I]()->void*{auto* D=G.IsValid()?G->GetDebugEditWorld():nullptr; return D && D->Biomes.IsValidIndex(I)?&D->Biomes[I].Definition:nullptr;},Changed,true);
                for(int32 I=0;I<Data->Layers.Num();++I)
                    AddTarget(FString(TEXT("PODZIEMIA / "))+TArray<FString>{TEXT("Podziemia"),TEXT("Jaskinie"),TEXT("Lochy"),TEXT("Katakumby")}[I],G.Get(),FPWGv2UndergroundRow::StaticStruct(),[G,I]()->void*{auto* D=G.IsValid()?G->GetDebugEditWorld():nullptr; return D && D->Layers.IsValidIndex(I)?&D->Layers[I]:nullptr;},Changed,true);
                for(int32 I=0;I<Data->TreeDefinitions.Num();++I)
                    AddTarget(FString::Printf(TEXT("DRZEWA / Gatunek %d"),I),G.Get(),FPWGv2TreeRow::StaticStruct(),[G,I]()->void*{auto* D=G.IsValid()?G->GetDebugEditWorld():nullptr; return D && D->TreeDefinitions.IsValidIndex(I)?&D->TreeDefinitions[I]:nullptr;},Changed,true);
            }
        }
        if(Generator.IsValid() && CanEdit())
        {
            const TArray<TPair<FString,UDataTable*>> Tables={
                {TEXT("KATALOG BIOMOW / "),Generator->BiomesTable},
                {TEXT("KATALOG DRZEW / "),Generator->TreesTable},
                {TEXT("KATALOG PODZIEMI / "),Generator->UndergroundTable},
                {TEXT("TEKSTURY TERENU / "),Generator->TerrainTexturesTable}};
            for(const auto& Entry:Tables) if(Entry.Value)
            {
                TWeakObjectPtr<UDataTable> W=Entry.Value;
                auto Names=Entry.Value->GetRowNames();Names.Sort(FNameLexicalLess());
                for(FName Row:Names)
                    AddTarget(Entry.Key+Row.ToString(),Entry.Value,Entry.Value->GetRowStruct(),[W,Row]()->void*{return W.IsValid()?W->FindRowUnchecked(Row):nullptr;},[this]{Status=TEXT("Katalog zmieniony w RAM. Przelosuj biomy/wejscia, aby zastosowac definicje katalogu.");},true);
            }
        }
        if (PC.IsValid())
        {
            if (Generator.IsValid())
                for (TActorIterator<APWGEnvironmentV2> It(PC->GetWorld());It;++It)
                {
                    if (It->GetOwner()!=Generator.Get()) continue;
                    TWeakObjectPtr<APWGEnvironmentV2> E=*It;
                    AddTarget(TEXT("WIZUALIA / Visual Profiles"),E.Get(),FPWGVisualProfile::StaticStruct(),
                        [E]()->void*{return E.IsValid()?&E->VisualProfile:nullptr;},
                        [this,E]{if(E.IsValid()){E->RefreshVisualProfile();Status=E->GetVisualProfileStatus();}});
                    break;
                }
            for (TActorIterator<AActor> It(PC->GetWorld());It;++It)
            {
                auto* Combat=It->FindComponentByClass<UDirectionalCombatComponent>();
                if (!Combat) continue;
                TWeakObjectPtr<UDirectionalCombatComponent> C=Combat;
                const FString Name=(It.operator*()==PC->GetPawn()?TEXT("GRACZ / "):TEXT("NPC / "))+It->GetName();
                AddObject(Name+TEXT(" / Combat / Ustawienia ogolne"),Combat,[C]{if(C.IsValid()) C->DebugRefreshConfiguration();});
                auto StageGroup=[&](const FString& Group,TSet<FString> Members)
                {
                    AddTarget(Name+TEXT(" / ")+Group,Combat,FCombatDebugStages::StaticStruct(),[C]()->void*{return C.IsValid()?&C->MotionCoreStyle.DebugStages:nullptr;},[C]{if(C.IsValid())C->DebugRefreshSolvers();});
                    Targets.Last()->IncludedFields=MoveTemp(Members);
                };
                StageGroup(TEXT("Combat / Etapy wewnetrzne"),{});
                StageGroup(TEXT("Combat / Wind up"),{TEXT("WindUp"),TEXT("EntryPoseBlend")});
                StageGroup(TEXT("Combat / Light"),{TEXT("Light")});
                StageGroup(TEXT("Combat / Heavy"),{TEXT("Heavy")});
                StageGroup(TEXT("Combat / Thrust"),{TEXT("Thrust")});
                StageGroup(TEXT("Aim"),{TEXT("ShoulderAim"),TEXT("ElbowFlex"),TEXT("ChestClamp"),TEXT("Wrist"),TEXT("MotionFit")});
                StageGroup(TEXT("Combat / Styles"),{TEXT("BasePose"),TEXT("AuthoredPose"),TEXT("AnimationHints"),TEXT("GuardLibrary"),TEXT("ActionLibrary"),TEXT("Guidance")});
                AddObject(Name+TEXT(" / Look at"),Combat,[C]{if(C.IsValid())C->DebugRefreshSolvers();});
                Targets.Last()->IncludedFields={TEXT("bDebugLookSolver"),TEXT("DebugLookWeight")};
                if(auto* Adapter=It->FindComponentByClass<UCombatDragonBodyAdapter>())
                {
                    if(auto* Physics=Adapter->DebugPhysicsMesh())
                    {
                        TWeakObjectPtr<USkeletalMeshComponent> P=Physics;
                        for(auto* Constraint:Physics->Constraints)
                        {
                            if(!Constraint)continue;
                            const FName Joint=Constraint->JointName;
                            AddTarget(Name+TEXT(" / STAW / ")+Joint.ToString(),Physics,FConstraintProfileProperties::StaticStruct(),[P,Joint]()->void*{auto* C=P.IsValid()?P->FindConstraintInstance(Joint):nullptr;return C?&C->ProfileInstance:nullptr;},[P,Joint]{if(auto* C=P.IsValid()?P->FindConstraintInstance(Joint):nullptr){const auto Profile=C->ProfileInstance;C->CopyProfilePropertiesFrom(Profile);}});
                        }
                    }
                }
                // Copy the library only once per actor. Original DataTables are never edited.
                if (CanEdit() && System.IsValid())
                {
                    auto* Table=Combat->MotionCoreStyle.MotionLibrary.LoadSynchronous();
                    if (Table && !Table->HasAnyFlags(RF_Transient))
                    {
                        Table=Cast<UDataTable>(System->KeepCopy(Table));
                        Combat->MotionCoreStyle.MotionLibrary=Table;
                        Combat->DebugRefreshConfiguration();
                    }
                    if(Table)
                    {
                        TWeakObjectPtr<UDataTable> W=Table;
                        auto Names=Table->GetRowNames(); Names.Sort(FNameLexicalLess());
                        for(FName Row:Names)
                            AddTarget(Name+TEXT(" / Animacje / ")+Row.ToString(),Table,Table->GetRowStruct(),[W,Row]()->void*{return W.IsValid()?W->FindRowUnchecked(Row):nullptr;},[C,W]{if(W.IsValid()) W->HandleDataTableChanged(); if(C.IsValid()) C->DebugRefreshConfiguration();});
                    }
                }
            }
            if (APawn* Pawn=PC->GetPawn())
            {
                TInlineComponentArray<UActorComponent*> Components(Pawn);
                for(auto* C:Components)
                    if(C && !C->IsA<UDirectionalCombatComponent>() && C->GetClass()->GetOutermost()->GetName().Contains(TEXT("Sirroh")))
                        AddObject(TEXT("KOMPONENT / ")+C->GetName(),C);
                if(auto* Mover=Pawn->FindComponentByClass<UMoverComponent>())
                    AddObject(TEXT("RUCH / Mover"),Mover->FindSharedSettings_Mutable<UCommonLegacyMovementSettings>());
            }
            AddObject(TEXT("KAMERA / Aktywny aktor"),PC->GetViewTarget());
        }
        Selected=nullptr;
        for(const auto& T:Targets) if(T->Name==Old) {Selected=T;break;}
        if(!Selected && !Targets.IsEmpty()) Selected=Targets[0];
        FilterTargets();
    }
    void Notify(const TSharedPtr<SirrohDebug::FTarget>& Target,const FString& Path)
    {
        if(auto* Combat=Cast<UDirectionalCombatComponent>(Target->Owner.Get());Combat && (Path.StartsWith(TEXT("Debug")) || Path.StartsWith(TEXT("bDebug")) || Path.StartsWith(TEXT("MotionCoreStyle.DebugStages"))))
            Combat->DebugRefreshSolvers();
        else if(Target->Changed) Target->Changed();
    }
    void Write(const TSharedPtr<SirrohDebug::FTarget>& Target,FProperty* P,const SirrohDebug::FAddress& Address,const FString& Path,const FString& Value)
    {
        void* Data=Target->Owner.IsValid()?Address():nullptr;
        if(!CanEdit() || !Data || Value.Len()>4096) {Status=TEXT("Brak celu lub edycja niedostepna poza Standalone.");return;}
        const FString Before=SirrohDebug::Read(P,Data);
        // Import into temporary storage first; malformed text cannot partially mutate live state.
        void* Temp=FMemory::Malloc(P->GetSize(),P->GetMinAlignment()); P->InitializeValue(Temp); P->CopyCompleteValue(Temp,Data);
        const TCHAR* End=P->ImportText_Direct(*Value,Temp,Target->Owner.Get(),PPF_None);
        const bool Valid=End && FString(End).TrimStartAndEnd().IsEmpty();
        if(Valid) P->CopyCompleteValue(Data,Temp);
        P->DestroyValue(Temp); FMemory::Free(Temp);
        if(!Valid) {Status=TEXT("Niepoprawna wartosc. Zmiana odrzucona."); return;}
        const FString After=SirrohDebug::Read(P,Data);
        if(Before==After) { FSlateApplication::Get().SetAllUserFocusToGameViewport(); return; }
        History.Add({Target,P,Address,Path,Before,After});
        if(History.Num()>4096) History.RemoveAt(0);
        Status=TEXT("LIVE: ")+Path+TEXT(" = ")+After;
        Notify(Target,Path);
        FSlateApplication::Get().SetAllUserFocusToGameViewport();
    }
    void Undo()
    {
        if(!CanEdit() || History.IsEmpty()) return;
        auto E=History.Pop();
        void* Data=E.Target->Owner.IsValid()?E.Address():nullptr;
        if(Data) {E.Property->ImportText_Direct(*E.Before,Data,E.Target->Owner.Get(),PPF_None); Notify(E.Target,E.Path);}
        Status=Data?TEXT("Cofnieto: ")+E.Path:TEXT("Cel juz nie istnieje.");
        RebuildFields();
    }
    void ResetSelected()
    {
        if(!CanEdit() || !Selected)return;
        bool Changed=false;
        for(int32 I=History.Num()-1;I>=0;--I)
        {
            const auto& E=History[I];if(E.Target->Name!=Selected->Name)continue;
            if(void* V=E.Target->Owner.IsValid()?E.Address():nullptr)
            {E.Property->ImportText_Direct(*E.Before,V,E.Target->Owner.Get(),PPF_None);Changed=true;}
            History.RemoveAt(I);
        }
        if(Changed && Selected->Changed)Selected->Changed();
        Status=Selected->Type==FPWGVisualProfile::StaticStruct()
            ?TEXT("Przywrocono profil sprzed edycji, bez przebudowy swiata.")
            :TEXT("Przywrocono wartosci celu sprzed edycji. Dla swiata nacisnij Przebuduj.");
        RebuildFields();
    }
    void Export()
    {
        auto Root=MakeShared<FJsonObject>(); Root->SetStringField(TEXT("format"),TEXT("SirrohDebugChanges-v1"));
        const bool bVisual=Selected && Selected->Type==FPWGVisualProfile::StaticStruct();
        if (bVisual)
        {
            Root->SetStringField(TEXT("format"),TEXT("SirrohVisualProfile-v1"));
            auto Profile=MakeShared<FJsonObject>();
            if (Selected->Owner.IsValid())
                if (void* Data=Selected->Address())
                    for(TFieldIterator<FProperty> It(FPWGVisualProfile::StaticStruct());It;++It)
                        Profile->SetStringField(It->GetName(),SirrohDebug::Read(*It,It->ContainerPtrToValuePtr<void>(Data)));
            Root->SetObjectField(TEXT("profile"),Profile);
            if (auto* Environment=Cast<APWGEnvironmentV2>(Selected->Owner.Get()))
                Root->SetStringField(TEXT("runtime"),Environment->GetVisualProfileStatus());
        }
        TArray<TSharedPtr<FJsonValue>> Values;
        for(const auto& E:History)
        {
            if (bVisual && E.Target->Type!=FPWGVisualProfile::StaticStruct()) continue;
            auto O=MakeShared<FJsonObject>(); O->SetStringField(TEXT("target"),E.Target->Name); O->SetStringField(TEXT("property"),E.Path);
            O->SetStringField(TEXT("before"),E.Before); O->SetStringField(TEXT("after"),E.After); Values.Add(MakeShared<FJsonValueObject>(O));
        }
        Root->SetArrayField(TEXT("changes"),Values);
        FString Json; FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));
        const FString File=FPaths::ProjectSavedDir()/TEXT("DebugMenu")/(FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))+TEXT(".json"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true);
        Status=FFileHelper::SaveStringToFile(Json,*File)?TEXT("Zapisano: ")+File:TEXT("Blad zapisu JSON.");
    }
    void RebuildFields()
    {
        if(!Fields) return;
        Fields->ClearChildren(); FieldCount=0;
        if(bTeleport) {BuildTeleport();return;}
        if(!Selected) {Label(TEXT("Brak celu. Uruchom rozgrywke i odswiez liste."));return;}
        if(Selected->bWorld)
        {
            Label(TEXT("ZMIANY OCZEKUJACE | Przebudowa zachowuje rozmiar wyspy i granice regionow/biomow. Moze chwilowo zatrzymac klatke."));
            Fields->AddSlot().AutoHeight()[Button(TEXT("Przelosuj biomy / wejscia z regul regionow (reset ich nadpisan)"),[this]{RebuildWorld(false,true);})];
            Fields->AddSlot().AutoHeight()[SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("Przebuduj teren i roslinnosc"),[this]{RebuildWorld(false);})]
                +SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("Przywroc wyspe"),[this]{RebuildWorld(true);})]];
        }
        Label(Selected->Name);
        if (Selected->Type==FPWGVisualProfile::StaticStruct())
        {
            TWeakObjectPtr<APWGEnvironmentV2> E=Cast<APWGEnvironmentV2>(Selected->Owner.Get());
            Fields->AddSlot().AutoHeight()[SNew(STextBlock).AutoWrapText(true)
                .Text_Lambda([E]{return SirrohDebug::Text(E.IsValid()?E->GetVisualProfileStatus():TEXT("Brak srodowiska."));})];
            Label(TEXT("Maski: cyjan = tlo, zolty = powierzchnie rozgrywki, magenta = przekroj, zielony = postac/przedmioty/pozostale. Zmiany LIVE."));
            const TCHAR* Names[]={TEXT("Dark Cave"),TEXT("Atmospheric Cave"),TEXT("Warm Ruins")};
            for(int32 I=0;I<3;++I)
                Fields->AddSlot().AutoHeight().Padding(0,2)[Button(Names[I],[this,E,I]{
                    if(E.IsValid() && CanEdit()){E->ApplyVisualPreset(I);Status=E->GetVisualProfileStatus();RebuildFields();}
                })];
            auto Group=[this](const TCHAR* GroupName,std::initializer_list<const TCHAR*> Properties)
            {
                auto Parent=Fields;
                auto Children=SNew(SVerticalBox);
                Fields=Children;
                const int32 Before=FieldCount;
                for(const TCHAR* PropertyName:Properties)
                    if(FProperty* P=FindFProperty<FProperty>(FPWGVisualProfile::StaticStruct(),FName(PropertyName)))
                    {
                        auto Address=Selected->Address;
                        SirrohDebug::FAddress A=[Address,P]()->void*{void* V=Address();return V?P->ContainerPtrToValuePtr<void>(V):nullptr;};
                        WalkProperty(Selected,P,A,P->GetName(),0);
                    }
                Fields=Parent;
                if(FieldCount>Before) Fields->AddSlot().AutoHeight().Padding(2,4)
                    [SNew(SExpandableArea).InitiallyCollapsed(false).HeaderContent()[SNew(STextBlock).Text(SirrohDebug::Text(GroupName))].BodyContent()[Children]];
            };
            Group(TEXT("Profil"),{TEXT("Mode"),TEXT("Strength"),TEXT("BlendSeconds")});
            Group(TEXT("Tlo"),{TEXT("BackgroundBrightness"),TEXT("BackgroundContrast"),TEXT("BackgroundTint"),TEXT("BackgroundAmbient"),TEXT("bDisableBackgroundAmbient"),TEXT("Desaturation")});
            Group(TEXT("Powierzchnie rozgrywki"),{TEXT("GameplayBrightness"),TEXT("GameplayContrast"),TEXT("GameplayTint")});
            Group(TEXT("Przekroj"),{TEXT("ForegroundDarkness"),TEXT("ForegroundTint")});
            Group(TEXT("Atmosfera"),{TEXT("AtmosphereColor"),TEXT("FogStrength"),TEXT("BackRange")});
            Group(TEXT("Procedural Depth Layers"),{TEXT("bCompareStage1"),TEXT("bEnableDepthGrading"),TEXT("DepthStart"),TEXT("DepthRange"),TEXT("DepthBrightness"),
                TEXT("DepthContrast"),TEXT("DepthDesaturation"),TEXT("DepthTintStrength"),TEXT("DepthTint"),TEXT("DepthFog"),
                TEXT("bEnableDepthLayers"),TEXT("DepthFormationCount"),TEXT("bShowDepthNear"),TEXT("bShowDepthMid"),TEXT("bShowDepthBacking"),TEXT("bVisualizeDepthLayers")});
            Label(TEXT("Stage 1 A/B: przelacz bCompareStage1. Prototyp: stale odleglosci i gestosc, widocznosc LIVE."));
            Group(TEXT("Diagnostyka"),{TEXT("bVisualizeMasks")});
            Label(TEXT("NeutralPadding / FrontRange: pola zgodnosci, nieaktywne w shaderze semantycznym."));
        }
        else Walk(Selected,Selected->Type,Selected->Address,TEXT(""),0);
        Label(FString::Printf(TEXT("%d pol. Wartosci zapisujesz Enterem. Referencje assetow tylko do odczytu."),FieldCount));
    }
    void Walk(TSharedPtr<SirrohDebug::FTarget> T,const UStruct* Type,SirrohDebug::FAddress Parent,FString Prefix,int32 Depth)
    {
        if(Depth>14 || FieldCount>1800) return;
        for(TFieldIterator<FProperty> It(Type);It;++It)
        {
            FProperty* P=*It;
            if(Prefix.IsEmpty() && !T->IncludedFields.IsEmpty() && !T->IncludedFields.Contains(P->GetName()))continue;
            if(!P->HasAnyPropertyFlags(CPF_Edit) || P->HasAnyPropertyFlags(CPF_EditConst|CPF_Deprecated)) continue;
            if(Type->IsA<UClass>() && P->GetOwnerStruct()->GetOutermost()->GetName()==TEXT("/Script/Engine")) continue;
            const FString Path=Prefix.IsEmpty()?P->GetName():Prefix+TEXT(".")+P->GetName();
            SirrohDebug::FAddress A=[Parent,P]()->void*{void* V=Parent();return V?P->ContainerPtrToValuePtr<void>(V):nullptr;};
            WalkProperty(T,P,A,Path,Depth);
        }
    }
    void WalkProperty(TSharedPtr<SirrohDebug::FTarget> T,FProperty* P,SirrohDebug::FAddress A,FString Path,int32 Depth)
    {
        if(auto* Color=CastField<FStructProperty>(P);
            T->Type==FPWGVisualProfile::StaticStruct() && Color && Color->Struct==TBaseStructure<FLinearColor>::Get())
        {
            if(!Search.IsEmpty() && !Path.Contains(Search)) return;
            ++FieldCount;
            Fields->AddSlot().AutoHeight().Padding(2,3)[SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(0.38f)[SNew(STextBlock).Text(SirrohDebug::Text(P->GetName()))]
                +SHorizontalBox::Slot().FillWidth(0.62f)[SNew(SEditableTextBox).IsReadOnly(!CanEdit())
                    .Text_Lambda([T,P,A]{return SirrohDebug::Text(T->Owner.IsValid()?SirrohDebug::Read(P,A()):TEXT("--"));})
                    .OnTextCommitted_Lambda([this,T,P,A,Path](const FText& V,ETextCommit::Type How)
                    {if(How==ETextCommit::OnEnter) Write(T,P,A,Path,V.ToString());})]];
            return;
        }
        if(auto* S=CastField<FStructProperty>(P))
        {
            auto Parent=Fields; auto Children=SNew(SVerticalBox); Fields=Children;
            const int32 Before=FieldCount;Walk(T,S->Struct,A,Path,Depth+1);Fields=Parent;
            if(FieldCount>Before)Fields->AddSlot().AutoHeight().Padding(4,2)
                [SNew(SExpandableArea).InitiallyCollapsed(Search.IsEmpty()).HeaderContent()[SNew(STextBlock).Text(SirrohDebug::Text(P->GetName()==TEXT("DebugStages")?TEXT("Combat - etapy obliczen (0 = OFF)"):P->GetName()==TEXT("MotionCoreStyle")?TEXT("Styles / Combat"):P->GetName()))].BodyContent()[Children]];
            return;
        }
        if(auto* Arr=CastField<FArrayProperty>(P);Arr && !CastField<FNameProperty>(Arr->Inner))
        {
            void* V=A(); if(!V) return;
            FScriptArrayHelper Helper(Arr,V);
            for(int32 I=0;I<FMath::Min(Helper.Num(),256);++I)
            {
                SirrohDebug::FAddress Item=[A,Arr,I]()->void*{void* V=A();if(!V)return nullptr;FScriptArrayHelper H(Arr,V);return H.IsValidIndex(I)?H.GetRawPtr(I):nullptr;};
                WalkProperty(T,Arr->Inner,Item,Path+FString::Printf(TEXT("[%d]"),I),Depth+1);
            }
            return;
        }
        if(!Search.IsEmpty() && !Path.Contains(Search)) return;
        ++FieldCount;
        bool Locked=!CanEdit();
        if(T->bWorld)
        {
            for(const TCHAR* Name:{TEXT("SampleSpacing"),TEXT("BiomeLengthMetres"),TEXT("OceanLengthMetres"),TEXT("LengthMetres"),TEXT("MinBiomes"),TEXT("MaxBiomes"),TEXT("Layer")})
                if(Path==Name) Locked=true;
        }
        if(Path==TEXT("bUseMotionCoreV3") || Path.EndsWith(TEXT("CatalogProfile")) || Path.EndsWith(TEXT("bTwoHanded"))) Locked=true;
        TSharedPtr<SWidget> Editor;
        if(Path==TEXT("AllowedBiomes") && Generator.IsValid() && Generator->BiomesTable)
        {
            auto List=SNew(SVerticalBox);
            List->AddSlot().AutoHeight()[SNew(STextBlock).AutoWrapText(true).Text(SirrohDebug::Text(TEXT("Pusta lista = wszystkie. Zastosuj: Przelosuj biomy / wejscia.")))];
            auto Names=Generator->BiomesTable->GetRowNames();Names.Sort(FNameLexicalLess());
            for(FName Name:Names)
                List->AddSlot().AutoHeight()[SNew(SCheckBox).IsFocusable(false).IsEnabled(!Locked)
                    .IsChecked_Lambda([T,A,Name]{const auto* V=T->Owner.IsValid()?static_cast<TArray<FName>*>(A()):nullptr;return V && V->Contains(Name)?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                    .OnCheckStateChanged_Lambda([this,T,P,A,Path,Name](ECheckBoxState State){if(!T->Owner.IsValid() || !A())return;auto Names=*static_cast<TArray<FName>*>(A());if(State==ECheckBoxState::Checked)Names.AddUnique(Name);else Names.Remove(Name);Write(T,P,A,Path,SirrohDebug::Read(P,&Names));})
                    [SNew(STextBlock).Text(FText::FromName(Name))]];
            Editor=SNew(SExpandableArea).InitiallyCollapsed(true).HeaderContent()[SNew(STextBlock).Text(SirrohDebug::Text(TEXT("Wybierz dozwolone biomy")))].BodyContent()[List];
        }
        else if(auto* B=CastField<FBoolProperty>(P))
        {
            Editor=SNew(SCheckBox).IsFocusable(false).IsEnabled(!Locked).IsChecked_Lambda([T,A,B]{void* V=T->Owner.IsValid()?A():nullptr;return V && B->GetPropertyValue(V)?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                .OnCheckStateChanged_Lambda([this,T,P,A,Path](ECheckBoxState State){Write(T,P,A,Path,State==ECheckBoxState::Checked?TEXT("True"):TEXT("False"));});
        }
        else if(auto* N=CastField<FNumericProperty>(P);N && !N->IsEnum())
        {
            double Min=-1000000,Max=1000000;
            if(Path.EndsWith(TEXT(".Columns")) || Path.EndsWith(TEXT(".Rows"))) {Min=1;Max=64;}
            if(Path.EndsWith(TEXT("LimitDegrees"))) {Min=0;Max=180;}
            if(Path.Contains(TEXT("Strength")) || Path.Contains(TEXT("Damping")) || Path.Contains(TEXT("Stiffness"))) Min=0;

            if(P->HasMetaData(TEXT("ClampMin"))) Min=FCString::Atod(*P->GetMetaData(TEXT("ClampMin")));
            if(P->HasMetaData(TEXT("ClampMax"))) Max=FCString::Atod(*P->GetMetaData(TEXT("ClampMax")));
            Editor=SNew(SNumericEntryBox<double>).IsEnabled(!Locked).AllowSpin(false).MinValue(Min).MaxValue(Max)
                .Value_Lambda([T,A,N]()->TOptional<double>{void* V=T->Owner.IsValid()?A():nullptr;if(!V)return {};return N->IsFloatingPoint()?N->GetFloatingPointPropertyValue(V):double(N->GetSignedIntPropertyValue(V));})
                .OnValueCommitted_Lambda([this,T,P,A,Path,N,Min,Max](double V,ETextCommit::Type){if(!FMath::IsFinite(V)){Status=TEXT("Odrzucono NaN/Infinity.");return;} V=FMath::Clamp(V,Min,Max);Write(T,P,A,Path,N->IsInteger()?FString::Printf(TEXT("%lld"),int64(V)):FString::Printf(TEXT("%.9g"),V));});
        }
        else if(auto* E=CastField<FEnumProperty>(P); E || (CastField<FByteProperty>(P) && CastField<FByteProperty>(P)->Enum))
        {
            UEnum* Enum=E?E->GetEnum():CastField<FByteProperty>(P)->Enum.Get();
            Editor=SNew(SButton).IsFocusable(false).IsEnabled(!Locked).Text_Lambda([T,P,A]{return SirrohDebug::Text(T->Owner.IsValid()?SirrohDebug::Read(P,A()):TEXT("--"));})
                .OnClicked_Lambda([this,T,P,A,Path,Enum]{
                    const FString Current=SirrohDebug::Read(P,A());int32 Index=Enum->GetIndexByNameString(Current); if(Index<0) Index=0;
                    Index=(Index+1)%FMath::Max(1,Enum->NumEnums()-1);Write(T,P,A,Path,Enum->GetNameStringByIndex(Index));return FReply::Handled();});
        }
        else
        {
            const bool Writable=CastField<FNameProperty>(P) || CastField<FStrProperty>(P) || CastField<FArrayProperty>(P);
            Editor=SNew(SEditableTextBox).IsReadOnly(Locked || !Writable)
                .Text_Lambda([T,P,A]{return SirrohDebug::Text(T->Owner.IsValid()?SirrohDebug::Read(P,A()):TEXT("--"));})
                .OnTextCommitted_Lambda([this,T,P,A,Path,Locked,Writable](const FText& V,ETextCommit::Type How){if(!Locked && Writable && How==ETextCommit::OnEnter) Write(T,P,A,Path,V.ToString());});
        }
        if(Path.Contains(TEXT("DebugStages.")) || T->Type==FCombatDebugStages::StaticStruct())
        {
            auto* N=CastField<FNumericProperty>(P);
            if(N)
            {
                const bool Gate=Path.EndsWith(TEXT("GripPair")) || Path.EndsWith(TEXT("GripTransition")) || Path.EndsWith(TEXT("AnatomyPass")) || Path.EndsWith(TEXT("FootRestore")) || Path.EndsWith(TEXT("CoreLimits")) || Path.EndsWith(TEXT("ContinuityFallback")) || Path.EndsWith(TEXT("Inertia")) || Path.EndsWith(TEXT("GuardLibrary")) || Path.EndsWith(TEXT("ActionLibrary")) || Path.EndsWith(TEXT("Guidance")) || Path.EndsWith(TEXT("HitAnimation")) || Path.EndsWith(TEXT("MotionFit"));
                Editor=SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().AutoWidth()[SNew(SCheckBox).IsFocusable(false).IsEnabled(!Locked)
                        .IsChecked_Lambda([T,A,N]{return T->Owner.IsValid() && A() && N->GetFloatingPointPropertyValue(A())>0?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
                        .OnCheckStateChanged_Lambda([this,T,P,A,Path](ECheckBoxState V){Write(T,P,A,Path,V==ECheckBoxState::Checked?TEXT("1"):TEXT("0"));})]
                    +SHorizontalBox::Slot().FillWidth(1)[SNew(SBox).Visibility(Gate?EVisibility::Collapsed:EVisibility::Visible)[Editor.ToSharedRef()]];
            }
        }
        FString Tip=P->GetMetaData(TEXT("ToolTip"));
        if(Locked) Tip+=TEXT("\nTylko odczyt: topologia / konfiguracja inicjalizacji / tryb sieciowy.");
        Fields->AddSlot().AutoHeight().Padding(2,3)
        [ SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(0.66f).VAlign(VAlign_Center)
            [SNew(STextBlock).AutoWrapText(true).Text(SirrohDebug::Text(P->GetName())).ToolTipText(SirrohDebug::Text(Tip))]
            +SHorizontalBox::Slot().FillWidth(0.34f)[Editor.ToSharedRef()] ];
    }
    void BuildTeleport()
    {
        const FString OldRegion=SelectedRegion?*SelectedRegion:FString(),OldBiome=SelectedBiome?*SelectedBiome:FString();
        const FString OldLayer=SelectedLayer?*SelectedLayer:FString();
        Regions.Reset();Biomes.Reset();Layers.Reset();LayerIds.Reset();
        const auto* D=Generator.IsValid()?Generator->GetDebugWorld():nullptr;
        if(!D || D->Regions.IsEmpty()){Label(TEXT("Brak wygenerowanej wyspy."));return;}
        TSet<FString> R;
        for(const auto& Region:D->Regions) R.Add(Region.Id.ToString());
        auto RN=R.Array();RN.Sort();for(const auto& N:RN)Regions.Add(MakeShared<FString>(N));
        auto Select=[](const auto& Options,const FString& Old){for(const auto& O:Options)if(*O==Old)return O;return Options.IsEmpty()?TSharedPtr<FString>():Options[0];};
        SelectedRegion=Select(Regions,OldRegion);
        TSet<FString> B;
        for(const auto& Region:D->Regions)if(Region.Id.ToString()==*SelectedRegion)
            for(const auto& Biome:D->Biomes)if(FMath::Max(Region.StartX,Biome.StartX)<FMath::Min(Region.EndX,Biome.EndX))B.Add(Biome.Id.ToString());
        auto BN=B.Array();BN.Sort();for(const auto& N:BN)Biomes.Add(MakeShared<FString>(N));
        SelectedBiome=Select(Biomes,OldBiome);
        if(!SelectedBiome){Label(TEXT("Region nie ma wygenerowanych biomow."));return;}
        const TArray<FString> Names={TEXT("Powierzchnia"),TEXT("Podziemia"),TEXT("Jaskinie"),TEXT("Lochy"),TEXT("Katakumby")};
        for(int32 L:Generator->DebugAvailableLayers(FName(**SelectedRegion),FName(**SelectedBiome),PC.IsValid()?PC->GetPawn().Get():nullptr))
        {Layers.Add(MakeShared<FString>(Names[L]));LayerIds.Add(L);}
        SelectedLayer=Select(Layers,OldLayer);
        Label(TEXT("Region -> biomy obecne w regionie -> istniejace warstwy. Kolizja jest dodatkowo sprawdzana przed ruchem."));
        auto Combo=[this](const FString& Name,TArray<TSharedPtr<FString>>& Options,TSharedPtr<FString>& Choice,bool Dependent)
        {
            Label(Name);auto* Ref=&Choice;
            Fields->AddSlot().AutoHeight().Padding(0,4)[SNew(SComboBox<TSharedPtr<FString>>).OptionsSource(&Options).InitiallySelectedItem(Choice)
                .OnGenerateWidget_Lambda([](TSharedPtr<FString> V){return SNew(STextBlock).Text(SirrohDebug::Text(*V));})
                .OnSelectionChanged_Lambda([this,Ref,Dependent](TSharedPtr<FString> V,ESelectInfo::Type How){*Ref=V;if(Dependent && How!=ESelectInfo::Direct){bRefreshTeleport=true;}})
                [SNew(STextBlock).Text_Lambda([Ref]{return SirrohDebug::Text(Ref->IsValid()?**Ref:TEXT("Brak dostepnych warstw"));})]];
        };
        Combo(TEXT("Region"),Regions,SelectedRegion,true);Combo(TEXT("Biom w regionie"),Biomes,SelectedBiome,true);Combo(TEXT("Warstwa"),Layers,SelectedLayer,false);
        if(SelectedLayer)Fields->AddSlot().AutoHeight().Padding(0,12)[Button(TEXT("Teleportuj"),[this]{
            if(!CanEdit() || !Generator.IsValid() || !PC->GetPawn() || !SelectedLayer)return;
            if(auto* Input=PC->GetPawn()->FindComponentByClass<UPWGSidescrollerInputComponent>();Input && Input->IsTraversalActive()){Status=TEXT("Poczekaj na zakonczenie traversal.");return;}
            const int32 Index=Layers.IndexOfByKey(SelectedLayer);if(!LayerIds.IsValidIndex(Index))return;
            FVector Target;
            if(Generator->DebugFindDestination(FName(**SelectedRegion),FName(**SelectedBiome),LayerIds[Index],PC->GetPawn(),Target,Status))Teleport(Target);
        })];
    }
    void Teleport(const FVector& Target)
    {
        APawn* Pawn=PC.IsValid()?PC->GetPawn().Get():nullptr;if(!Pawn)return;
        if(auto* Input=Pawn->FindComponentByClass<UPWGSidescrollerInputComponent>()) Input->CancelLaneMove();
        if(auto* Combat=Pawn->FindComponentByClass<UDirectionalCombatComponent>()) {Combat->CancelAttack();Combat->DebugRefreshConfiguration();}
        if(auto* Mover=Pawn->FindComponentByClass<UMoverComponent>())
        {
            auto Constraint=Mover->GetPlanarConstraint();Constraint.bConstrainToPlane=true;Constraint.PlaneConstraintNormal=FVector::YAxisVector;Constraint.PlaneConstraintOrigin=FVector(0,Target.Y,0);Mover->SetPlanarConstraint(Constraint);
            auto Effect=MakeShared<FTeleportEffect>();Effect->TargetLocation=Target;Effect->bUseActorRotation=true;Mover->QueueInstantMovementEffect(Effect);
            auto Stop=MakeShared<FApplyVelocityEffect>();Stop->VelocityToApply=FVector::ZeroVector;Stop->bAdditiveVelocity=false;Mover->QueueInstantMovementEffect(Stop);
        }
        else if(!Pawn->TeleportTo(Target,Pawn->GetActorRotation())) {Status=TEXT("Teleport odrzucony przez kolizje.");return;}
        if(auto* Controller=Cast<APWGPlayerController>(PC.Get())) Controller->CurrentTerrainLane=EPWGTerrainLane::Middle;
        Status=TEXT("Zweryfikowano kolizje; teleport przekazany do ruchu postaci.");
    }
    void RebuildWorld(bool Restore,bool Replan=false)
    {
        if(!CanEdit() || !Generator.IsValid() || !PC->GetPawn())return;
        if(auto* Input=PC->GetPawn()->FindComponentByClass<UPWGSidescrollerInputComponent>();Input && Input->IsTraversalActive()) {Status=TEXT("Poczekaj na zakonczenie traversal.");return;}
        if(!Generator->DebugRebuild(Restore,Status,Replan))return;
        FVector Destination;
        if(Generator->DebugFindDestination(NAME_None,NAME_None,0,PC->GetPawn(),Destination,Status)) Teleport(Destination);
        else
        {
            FString Recovery;
            Restore=true;
            Generator->DebugRebuild(true,Recovery);
            Generator->EnsureLocationLoaded(PC->GetPawn()->GetActorLocation());
            if(Generator->DebugFindDestination(NAME_None,NAME_None,0,PC->GetPawn(),Destination,Recovery)) Teleport(Destination);
            Status=TEXT("Brak bezpiecznego miejsca po przebudowie - automatycznie przywrocono oryginalna wyspe.");
        }
        if(Restore || Replan) History.RemoveAll([](const auto& E){return E.Target->bWorld;});
        GatherTargets();RebuildFields();
    }
};

void USirrohDebugSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    MapHandle=FCoreUObjectDelegates::PreLoadMap.AddLambda([this](const FString&){Close();Panel.Reset();Controller.Reset();Copies.Reset();});
}
UObject* USirrohDebugSubsystem::KeepCopy(UObject* Source)
{
    if(!Source)return nullptr;
    UObject* Copy=DuplicateObject(Source,this);Copy->SetFlags(RF_Transient);Copies.Add(Copy);return Copy;
}
void USirrohDebugSubsystem::Toggle(APlayerController* InPC)
{
#if !UE_BUILD_SHIPPING
    if(bOpen) {Close();return;}
    if(!InPC || !InPC->IsLocalController() || !InPC->GetWorld() || !InPC->GetWorld()->GetGameViewport())return;
    if(Controller!=InPC) {Panel.Reset();Controller=InPC;}
    if(!Panel) Panel=SNew(SSirrohDebugPanel,this,InPC);
    Viewport=InPC->GetWorld()->GetGameViewport();
    Viewport->AddViewportWidgetContent(Panel.ToSharedRef(),10000);
    bOpen=true;
    FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);InPC->SetInputMode(Mode);InPC->bShowMouseCursor=true;
    FSlateApplication::Get().SetAllUserFocusToGameViewport();
#endif
}
bool USirrohDebugSubsystem::IsPointerOverMenu() const { return bOpen && Panel && Panel->PointerOverMenu(); }
bool USirrohDebugSubsystem::IsEditingText() const
{
    if(!bOpen || !FSlateApplication::IsInitialized()) return false;
    const auto Focus=FSlateApplication::Get().GetKeyboardFocusedWidget();
    return Focus && Focus->GetTypeAsString().Contains(TEXT("EditableText"));
}
void USirrohDebugSubsystem::Close()
{
    if(!bOpen)return;
    bOpen=false;
    if(Viewport.IsValid() && Panel)Viewport->RemoveViewportWidgetContent(Panel.ToSharedRef());
    if(Controller.IsValid())
    {
        Controller->FlushPressedKeys();FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockOnCapture);Controller->SetInputMode(Mode);
        FSlateApplication::Get().SetAllUserFocusToGameViewport();
    }
}
void USirrohDebugSubsystem::Deinitialize()
{
    FCoreUObjectDelegates::PreLoadMap.Remove(MapHandle);
    Close();Panel.Reset();Copies.Reset();Super::Deinitialize();
}

