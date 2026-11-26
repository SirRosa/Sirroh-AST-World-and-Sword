#include "Combat/CombatReactionCatalog.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
const TArray<FCombatReactionEntry>& CombatReactions::Catalog(const FCombatStyleDefinition* Style)
{
    if (Style && !Style->CatalogProfile.IsNone())
    {
        struct FCached { TSharedPtr<const FResolvedCombatMotionProfile> Profile; TArray<FCombatReactionEntry> Entries; };
        static TMap<FString,FCached> Cache;
        const auto Profile=UCombatMotionCatalog::GetProfile(*Style);
        const FString Key=Style->MotionLibrary.ToSoftObjectPath().ToString()+TEXT("/")+Style->CatalogProfile.ToString();
        auto& Cached=Cache.FindOrAdd(Key);
        if (Cached.Profile!=Profile)
        {
            Cached.Profile=Profile; Cached.Entries.Reset();
            if (Profile) for (const auto& Category:Profile->Categories)
            {
                if (!Category.Name.ToString().StartsWith(TEXT("Received.")) || Category.Data.Mode==ECombatCatalogOverride::Disabled) continue;
                for (const auto& Slot:Category.Data.Slots) if (Slot.Mode==ECombatCatalogOverride::Override)
                    for (const auto& Set:Slot.Sets) for (const auto& V:Set.Variants)
                    {
                        if (!V.Motion.bEnabled || (V.Motion.Hands && V.Motion.Hands!=(Style->bTwoHanded?2:1))) continue;
                        FCombatReactionEntry E;
                        E.Id=V.Id; E.LegacyId=V.LegacyReactionId; E.Path=V.Motion.Animation.ToSoftObjectPath().ToString();
                        E.Action=V.Motion.Action; E.Region=Slot.Region; E.Incoming=V.Incoming; E.Hands=V.Motion.Hands;
                        E.bStrong=V.bStrong; E.bTurn=V.bTurn; E.bPierce=V.bPierce; E.bCut=V.bCut;
                        Cached.Entries.Add(MoveTemp(E));
                    }
            }
        }
        return Cached.Entries;
    }

    static const TArray<FCombatReactionEntry> Entries={
        {TEXT("Anim_GH_B_01"),TEXT("/Game/AdventureAnimset/Animations/GetHit/Anim_GH_B_01"),TEXT("Hit"),TEXT("Any"),FVector(-1,0,0),0,false,false,false,false},
        {TEXT("Anim_GH_F_01"),TEXT("/Game/AdventureAnimset/Animations/GetHit/Anim_GH_F_01"),TEXT("Hit"),TEXT("Any"),FVector(1,0,0),0,false,false,false,false},
        {TEXT("Anim_GH_F_L_01"),TEXT("/Game/AdventureAnimset/Animations/GetHit/Anim_GH_F_L_01"),TEXT("Hit"),TEXT("Any"),FVector(1,-1,0),0,false,false,false,false},
        {TEXT("Anim_GH_F_R_01"),TEXT("/Game/AdventureAnimset/Animations/GetHit/Anim_GH_F_R_01"),TEXT("Hit"),TEXT("Any"),FVector(1,1,0),0,false,false,false,false},
        {TEXT("Anim_GH_L_01"),TEXT("/Game/AdventureAnimset/Animations/GetHit/Anim_GH_L_01"),TEXT("Hit"),TEXT("Any"),FVector(0,-1,0),0,false,false,false,false},
        {TEXT("Anim_GH_R_01"),TEXT("/Game/AdventureAnimset/Animations/GetHit/Anim_GH_R_01"),TEXT("Hit"),TEXT("Any"),FVector(0,1,0),0,false,false,false,false},
        {TEXT("Anim_GR_struggle_loop"),TEXT("/Game/AdventureAnimset/Animations/GetHit/Anim_GR_struggle_loop"),TEXT("Air"),TEXT("Any"),FVector(1,0,0),0,false,false,false,false},
        {TEXT("Anim_dEAh_F_01"),TEXT("/Game/AdventureAnimset/Animations/Death/Anim_dEAh_F_01"),TEXT("Death"),TEXT("Neck"),FVector(1,0,0),0,false,false,false,true},
        {TEXT("Anim_dEAh_F_02"),TEXT("/Game/AdventureAnimset/Animations/Death/Anim_dEAh_F_02"),TEXT("Death"),TEXT("Torso"),FVector(0,-1,0),0,false,false,true,false},
        {TEXT("Anim_dEAh_F_03"),TEXT("/Game/AdventureAnimset/Animations/Death/Anim_dEAh_F_03"),TEXT("Death"),TEXT("Torso"),FVector(0,1,0),0,false,false,true,false},
        {TEXT("Anim_dEAh_F_04"),TEXT("/Game/AdventureAnimset/Animations/Death/Anim_dEAh_F_04"),TEXT("Death"),TEXT("Head"),FVector(1,0,0),0,false,false,false,false},
        {TEXT("Anim_DU_E_GR"),TEXT("/Game/AdventureAnimset/Animations/Death/Anim_DU_E_GR"),TEXT("Death"),TEXT("Any"),FVector(1,0,0),0,true,false,false,false},
        {TEXT("Anim_KD_F"),TEXT("/Game/AdventureAnimset/Animations/Death/Anim_KD_F"),TEXT("KO"),TEXT("Any"),FVector(1,0,0),0,false,false,false,false},
        {TEXT("Sword1h_Death_1"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Death_1"),TEXT("Death"),TEXT("Torso"),FVector(1,0,0),1,false,false,false,false},
        {TEXT("Sword1h_Death_2"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Death_2"),TEXT("Death"),TEXT("Torso"),FVector(1,0,0),1,false,false,false,false},
        {TEXT("Sword1h_Death_back"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Death_back"),TEXT("Death"),TEXT("Any"),FVector(-1,0,0),1,false,false,false,false},
        {TEXT("Sword1h_Death_Front"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Death_Front"),TEXT("Death"),TEXT("Any"),FVector(1,0,0),1,false,false,false,false},
        {TEXT("Sword1h_Death_L"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Death_L"),TEXT("Death"),TEXT("Any"),FVector(0,-1,0),1,false,false,false,false},
        {TEXT("Sword1h_Death_R"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Death_R"),TEXT("Death"),TEXT("Any"),FVector(0,1,0),1,false,false,false,false},
        {TEXT("Sword1h_Hit_Head_Front"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Hit_Head_Front"),TEXT("Hit"),TEXT("Head"),FVector(1,0,0),1,false,false,false,false},
        {TEXT("Sword1h_Hit_Head_Left"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Hit_Head_Left"),TEXT("Hit"),TEXT("Head"),FVector(0,-1,0),1,false,false,false,false},
        {TEXT("Sword1h_Hit_Head_LeftDown"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Hit_Head_LeftDown"),TEXT("Hit"),TEXT("Head"),FVector(0,-1,-1),1,false,false,false,false},
        {TEXT("Sword1h_Hit_Head_LeftUp"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Hit_Head_LeftUp"),TEXT("Hit"),TEXT("Head"),FVector(0,-1,1),1,false,false,false,false},
        {TEXT("Sword1h_Hit_Head_Right"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Hit_Head_Right"),TEXT("Hit"),TEXT("Head"),FVector(0,1,0),1,false,false,false,false},
        {TEXT("Sword1h_Hit_Head_RightDown"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Hit_Head_RightDown"),TEXT("Hit"),TEXT("Head"),FVector(0,1,-1),1,false,false,false,false},
        {TEXT("Sword1h_Hit_Head_RightUp"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Hit_Head_RightUp"),TEXT("Hit"),TEXT("Head"),FVector(0,1,1),1,false,false,false,false},
        {TEXT("Sword1h_Hit_Legs_Left"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Hit_Legs_Left"),TEXT("Hit"),TEXT("Legs"),FVector(0,-1,0),1,false,false,false,false},
        {TEXT("Sword1h_Hit_Legs_Right"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Hit_Legs_Right"),TEXT("Hit"),TEXT("Legs"),FVector(0,1,0),1,false,false,false,false},
        {TEXT("Sword1h_Hit_Torso_Front"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Hit_Torso_Front"),TEXT("Hit"),TEXT("Torso"),FVector(1,0,0),1,false,false,false,false},
        {TEXT("Sword1h_Hit_Torso_Left"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Hit_Torso_Left"),TEXT("Hit"),TEXT("Torso"),FVector(0,-1,0),1,false,false,false,false},
        {TEXT("Sword1h_Hit_Torso_Right"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Hit_Torso_Right"),TEXT("Hit"),TEXT("Torso"),FVector(0,1,0),1,false,false,false,false},
        {TEXT("Sword1h_Knockdown_Front"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Knockdown_Front"),TEXT("KO"),TEXT("Any"),FVector(1,0,0),1,false,false,false,false},
        {TEXT("Sword1h_Knockdown_Left"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Knockdown_Left"),TEXT("KO"),TEXT("Any"),FVector(0,-1,0),1,false,false,false,false},
        {TEXT("Sword1h_Knockdown_Right"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_1H_Anim/Animations/RootMotion/Sword1h_Knockdown_Right"),TEXT("KO"),TEXT("Any"),FVector(0,1,0),1,false,false,false,false},
        {TEXT("Longs_Death_L"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_2H_Anim/Animations/RootMotion/Longs_Death_L"),TEXT("Death"),TEXT("Any"),FVector(0,-1,0),2,false,false,false,false},
        {TEXT("Longs_Death_R"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_2H_Anim/Animations/RootMotion/Longs_Death_R"),TEXT("Death"),TEXT("Any"),FVector(0,1,0),2,false,false,false,false},
        {TEXT("Longs_Hit_Back"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_2H_Anim/Animations/RootMotion/Longs_Hit_Back"),TEXT("Hit"),TEXT("Any"),FVector(-1,0,0),2,false,false,false,false},
        {TEXT("Longs_Hit_BackTurn180"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_2H_Anim/Animations/RootMotion/Longs_Hit_BackTurn180"),TEXT("Hit"),TEXT("Any"),FVector(-1,0,0),2,false,true,false,false},
        {TEXT("Longs_Hit_D"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_2H_Anim/Animations/RootMotion/Longs_Hit_D"),TEXT("Hit"),TEXT("Any"),FVector(0,0,-1),2,false,false,false,false},
        {TEXT("Longs_Hit_L"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_2H_Anim/Animations/RootMotion/Longs_Hit_L"),TEXT("Hit"),TEXT("Any"),FVector(0,-1,0),2,false,false,false,false},
        {TEXT("Longs_Hit_L2"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_2H_Anim/Animations/RootMotion/Longs_Hit_L2"),TEXT("Hit"),TEXT("Any"),FVector(0,-1,0),2,true,false,false,false},
        {TEXT("Longs_Hit_R"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_2H_Anim/Animations/RootMotion/Longs_Hit_R"),TEXT("Hit"),TEXT("Any"),FVector(0,1,0),2,false,false,false,false},
        {TEXT("Longs_Hit_R2"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_2H_Anim/Animations/RootMotion/Longs_Hit_R2"),TEXT("Hit"),TEXT("Any"),FVector(0,1,0),2,true,false,false,false},
        {TEXT("Longs_Hit_U"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_2H_Anim/Animations/RootMotion/Longs_Hit_U"),TEXT("Hit"),TEXT("Any"),FVector(0,0,1),2,false,false,false,false},
        {TEXT("Longs_HitFront_p"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_2H_Anim/Animations/RootMotion/Longs_HitFront_p"),TEXT("Hit"),TEXT("Any"),FVector(1,0,0),2,false,false,false,false},
        {TEXT("Longs_HitLeft_p"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_2H_Anim/Animations/RootMotion/Longs_HitLeft_p"),TEXT("Hit"),TEXT("Any"),FVector(0,-1,0),2,false,false,false,false},
        {TEXT("Longs_HitRight_p"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_2H_Anim/Animations/RootMotion/Longs_HitRight_p"),TEXT("Hit"),TEXT("Any"),FVector(0,1,0),2,false,false,false,false},
        {TEXT("Longs_OnGround_Hit"),TEXT("/Game/Systems/Procedural_Combat/Animations/Combat_2H_Anim/Animations/RootMotion/Longs_OnGround_Hit"),TEXT("Ground"),TEXT("Any"),FVector(1,0,0),0,false,false,false,false},
    };
    return Entries;
}
const FCombatReactionEntry* CombatReactions::Find(FName Id,const FCombatStyleDefinition* Style)
{ return Catalog(Style).FindByPredicate([&](const auto& E){return E.Id==Id || (!E.LegacyId.IsNone() && E.LegacyId==Id);}); }
FName CombatReactions::Select(FName Action,FName Region,const FVector& Incoming,int32 Hands,float Strength,
    bool bPierce,bool bCut,uint32 Seed,FName Previous,bool bAllowTurn,const FCombatStyleDefinition* Style)
{
    if (Style && !Style->CatalogProfile.IsNone())
    {
        const auto Profile=UCombatMotionCatalog::GetProfile(*Style); if (!Profile) return NAME_None;
        FCombatCatalogQuery Q;
        Q.Category=Action==TEXT("Hit")?TEXT("Received.HitReaction"):Action==TEXT("KO")?TEXT("Received.Knockdown"):
            Action==TEXT("Death")?TEXT("Received.Death"):Action==TEXT("Air")?TEXT("Received.InAir"):
            Action==TEXT("Ground")?TEXT("Received.OnGround"):Action==TEXT("GetUp")?TEXT("Received.GetUp"):TEXT("");
        if (Q.Category.IsNone()) return NAME_None;
        Q.Direction=FMath::Abs(Incoming.Y)+FMath::Abs(Incoming.Z)>UE_SMALL_NUMBER?
            UCombatMotionCatalog::DirectionName(FVector2D(Incoming.Y,Incoming.Z)):Incoming.X>=0?TEXT("Front"):TEXT("Back");
        Q.Region=Region; Q.Seed=int32(Seed); Q.bRestrictVariants=true;
        for (const auto& E:Catalog(Style))
            if (!(E.bTurn && !bAllowTurn) && (!E.Hands || E.Hands==Hands) && (!E.bPierce || bPierce)
                && (!E.bCut || bCut) && (!E.bStrong || Strength>=0.65f)) Q.AllowedVariants.Add(E.Id);
        const auto Choice=UCombatMotionCatalog::ChooseForState(*Profile,Q);
        return Choice.bFound?Choice.Variant.Id:NAME_None;
    }
    double Best=DBL_MAX; FName Result;
    FRandomStream Random(Seed);
    for (const auto& E:Catalog(Style))
    {
        if ((E.bTurn && !bAllowTurn) || E.Action!=Action || (E.Hands && E.Hands!=Hands) || (E.bPierce&&!bPierce) || (E.bCut&&!bCut)) continue;
        if (E.Region!=TEXT("Any") && E.Region!=Region && !(Region==TEXT("Neck")&&E.Region==TEXT("Head"))) continue;
        if (E.bStrong && Strength<0.65f) continue;
        double Cost=2*(1-FVector::DotProduct(Incoming.GetSafeNormal(),E.Incoming.GetSafeNormal()));
        if (E.Region==TEXT("Any")) Cost+=0.3;
        if (!E.Hands) Cost+=0.15;
        if (Strength>=0.65f && !E.bStrong) Cost+=0.5;
        if (E.Id==Previous) Cost+=0.3;
        Cost+=Random.FRandRange(0.f,0.8f);
        if (Cost<Best) { Best=Cost; Result=E.Id; }
    }
    return Result;
}
bool CombatReactions::LoadRig(USkeletalMeshComponent& Mesh,const FCombatStyleDefinition& Source,FName Id,FCombatBodyRig& Out)
{
    const auto* E=Find(Id,&Source); if (!E || !Mesh.GetSkeletalMeshAsset()) return false;
    auto* Animation=LoadObject<UAnimSequence>(nullptr,*E->Path);
    if (!Animation || Animation->GetSkeleton()!=Mesh.GetSkeletalMeshAsset()->GetSkeleton()) return false;
    FCombatStyleDefinition Style=Source;
    Style.BasePose=Animation; Style.BasePoseTime=0; Style.bReadAnimationHints=false;
    Style.MotionLibrary.Reset(); Style.MotionSources.Reset(); Style.ThrustMotionSources.Reset();
    FCombatMotionSource Clip; Clip.Animation=Animation; Clip.Action=E->Action;
    Style.MotionSources.Add(Clip);
    FString Error;
    if (!CombatBodySolver::BindRig(Mesh,Style,Out,Error)) return false;
    return Out.Motions.Num()>0 && Out.Motions[0].IsValid();
}
