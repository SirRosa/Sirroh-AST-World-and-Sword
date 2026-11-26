#include "Combat/DirectionalCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Combat/CombatMotionCatalog.h"
#include "Engine/DataTable.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    TArray<FName> StyleRows(const UDataTable* Table)
    {
        if (!Table) return {};
        if (Table->GetRowStruct()==FCombatMotionProfile::StaticStruct())
        {
            TArray<FName> Rows;
            for (FName Name:Table->GetRowNames())
                if (Table->FindRow<FCombatMotionProfile>(Name,TEXT("Selectable profiles"))->bSelectable) Rows.Add(Name);
            Rows.Sort(FNameLexicalLess()); return Rows;
        }
        if (Table->GetRowStruct()!=FCombatStyleDefinition::StaticStruct()) return {};
        TArray<FName> Rows=Table->GetRowNames();
        Rows.Sort(FNameLexicalLess()); // Stable order, identical after save/cook and on every peer.
        return Rows;
    }
    bool ReadStyle(const UDataTable* Table,FName Name,FCombatStyleDefinition& Out)
    {
        if (!Table) return false;
        if (Table->GetRowStruct()==FCombatMotionProfile::StaticStruct())
        { FString Error; return UCombatMotionCatalog::StyleForProfile(const_cast<UDataTable*>(Table),Name,Out,Error); }
        if (Table->GetRowStruct()!=FCombatStyleDefinition::StaticStruct()) return false;
        const auto* Row=Table->FindRow<FCombatStyleDefinition>(Name,TEXT("Read combat style"));
        if (!Row) return false;
        Out=*Row; Out.StyleId=Name; return true;
    }
}
FText UDirectionalCombatComponent::GetCombatStyleName() const
{
    return MotionCoreStyle.DisplayName.IsEmpty()?FText::FromName(MotionCoreStyle.StyleId):MotionCoreStyle.DisplayName;
}
void UDirectionalCombatComponent::InitializeCombatStyles()
{
    if (!GetOwner()->HasAuthority()) return;
    if (UCombatMotionCatalog::IsEnabled())
    {
        auto* Catalog=LoadObject<UDataTable>(nullptr,TEXT("/Game/Systems/Procedural_Combat/DT_Combat_Motion_v3.DT_Combat_Motion_v3"));
        if (Catalog && Catalog->GetRowStruct()==FCombatMotionProfile::StaticStruct()) CombatStyles=Catalog;
        else UE_LOG(LogTemp,Warning,TEXT("New motion catalog unavailable; retaining legacy styles."));
    }
    const auto Rows=StyleRows(CombatStyles.LoadSynchronous());
    if (Rows.IsEmpty()) return; // Existing explicitly configured style remains usable without a table.
    if (Rows.Contains(MotionCoreStyle.StyleId) && ApplyCombatStyle(MotionCoreStyle.StyleId)) return;
    for (FName Name:Rows) if (ApplyCombatStyle(Name)) return;
}
bool UDirectionalCombatComponent::ApplyCombatStyle(FName RowName)
{
    const auto* Table=CombatStyles.LoadSynchronous();
    FCombatBodyRig Validated; FString Error;
    FCombatStyleDefinition Candidate;
    if (!ReadStyle(Table,RowName,Candidate) || !Mesh || !Mesh->GetSkeletalMeshAsset()) return false;
    if (GripMode.Serial>0)
    {
        FCombatStyleDefinition Gripped;
        if (!UCombatMotionCatalog::StyleForGrip(Candidate,GripMode.bTwoHanded,Gripped,Error)) return false;
        Candidate=MoveTemp(Gripped);
    }
    if (!CombatBodySolver::BindRig(*Mesh,Candidate,Validated,Error))
    {
        UE_LOG(LogTemp,Warning,TEXT("Combat style %s rejected: %s"),*RowName.ToString(),*Error);
        return false;
    }
    MotionCoreStyle=MoveTemp(Candidate);
    OnRep_MotionCoreConfiguration();
    MotionRig=MoveTemp(Validated); MotionRigMesh=Mesh->GetSkeletalMeshAsset();
    MotionRigRelativeTransform=Mesh->GetRelativeTransform();
    bMotionRigAttempted=true; bMotionRigReady=true;
    GetOwner()->ForceNetUpdate();
    return true;
}
void UDirectionalCombatComponent::CycleCombatStyle(int32 Direction)
{
    if (bUseMotionCoreV3 && IsLocal() && (Direction==-1 || Direction==1)) ServerCycleCombatStyle(Direction);
}
void UDirectionalCombatComponent::ServerCycleCombatStyle_Implementation(int32 Direction)
{
    if (!bUseMotionCoreV3 || (Direction!=-1 && Direction!=1)) return;
    const auto* Table=CombatStyles.LoadSynchronous();
    const auto Rows=StyleRows(Table);
    if (Rows.IsEmpty() || !Mesh || !Mesh->GetSkeletalMeshAsset()) return;
    const FName Current=PendingCombatStyle.IsNone()?(MotionCoreStyle.CatalogProfile.IsNone()?MotionCoreStyle.StyleId:MotionCoreStyle.CatalogProfile):PendingCombatStyle;
    int32 Index=Rows.IndexOfByKey(Current);
    if (Index==INDEX_NONE && !MotionCoreStyle.CatalogProfile.IsNone())
        for (int32 I=0;I<Rows.Num();++I)
        { FCombatStyleDefinition Entry; if (ReadStyle(Table,Rows[I],Entry) && Entry.StyleId==MotionCoreStyle.StyleId) { Index=I; break; } }
    for (int32 Offset=1; Offset<=Rows.Num(); ++Offset)
    {
        const int32 Start=Index==INDEX_NONE?(Direction>0?-1:0):Index;
        const FName Name=Rows[(Start+Direction*Offset+Rows.Num()*2)%Rows.Num()];
        FCombatBodyRig Bound; FString Error;
        FCombatStyleDefinition Candidate,Gripped;
        if (!ReadStyle(Table,Name,Candidate)) continue;
        if (!UCombatMotionCatalog::StyleForGrip(Candidate,MotionCoreStyle.bTwoHanded,Gripped,Error)) continue;
        if (CombatBodySolver::BindRig(*Mesh,Gripped,Bound,Error)) { PendingCombatStyle=Name; break; }
        UE_LOG(LogTemp,Warning,TEXT("Skipping combat style %s: %s"),*Name.ToString(),*Error);
    }
}
bool UDirectionalCombatComponent::IsStyleControlsHovered() const
{
    return StyleControls.IsValid() && StyleControls->IsHovered();
}
void UDirectionalCombatComponent::UpdateCombatStyles()
{
    UpdateGripMode();
    if (GetOwner()->HasAuthority() && !PendingCombatStyle.IsNone())
    {
        const auto Phase=GetCombatPhase();
        // Do not use style changes to escape a hit or interrupt physical follow-through.
        if ((Phase==EDirectionalCombatPhase::Combat || Phase==EDirectionalCombatPhase::Recovery)
            && CombatTime()>=MotionReaction.GuardAt+0.55)
        {
            const FName Next=PendingCombatStyle; PendingCombatStyle=NAME_None;
            ApplyCombatStyle(Next);
        }
    }
    if (bAIControlled || !IsLocal() || GetNetMode()==NM_DedicatedServer) return;
    auto* Viewport=GetWorld()->GetGameViewport();
    if (!Viewport || StyleControlsLayer.IsValid()) return;
    const TWeakObjectPtr<UDirectionalCombatComponent> Weak(this);
    StyleControls=SNew(SBorder).Padding(FMargin(10,5))
        [SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth()
        [SNew(SButton).IsFocusable(false).Text(FText::FromString(TEXT("<")))
            .OnClicked_Lambda([Weak]() { if (Weak.IsValid()) Weak->CycleCombatStyle(-1); return FReply::Handled(); })]
        +SHorizontalBox::Slot().Padding(12,0).VAlign(VAlign_Center)
        [SNew(STextBlock).Text_Lambda([Weak]() {return Weak.IsValid()?Weak->GetCombatStyleName():FText::GetEmpty();})]
        +SHorizontalBox::Slot().AutoWidth()
        [SNew(SButton).IsFocusable(false).Text(FText::FromString(TEXT(">")))
            .OnClicked_Lambda([Weak]() { if (Weak.IsValid()) Weak->CycleCombatStyle(1); return FReply::Handled(); })]];
    StyleControlsLayer=SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0,20)
        [StyleControls.ToSharedRef()];
    Viewport->AddViewportWidgetContent(StyleControlsLayer.ToSharedRef(),15);
}
void UDirectionalCombatComponent::RemoveStyleControls()
{
    if (StyleControlsLayer.IsValid() && GetWorld() && GetWorld()->GetGameViewport())
        GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(StyleControlsLayer.ToSharedRef());
    StyleControls.Reset(); StyleControlsLayer.Reset();
}
