#pragma once
#include "CoreMinimal.h"
#include "Combat/CombatBodySolver.h"
struct FCombatReactionEntry
{
    FName Id;
    FString Path;
    FName Action,Region;
    FVector Incoming=FVector::ForwardVector;
    int32 Hands=0;
    bool bStrong=false,bTurn=false,bPierce=false,bCut=false;
    FName LegacyId;
};
namespace CombatReactions
{
    const TArray<FCombatReactionEntry>& Catalog(const FCombatStyleDefinition* Style=nullptr);
    const FCombatReactionEntry* Find(FName Id,const FCombatStyleDefinition* Style=nullptr);
    FName Select(FName Action,FName Region,const FVector& Incoming,int32 Hands,float Strength,
        bool bPierce,bool bCut,uint32 Seed,FName Previous=NAME_None,bool bAllowTurn=true,const FCombatStyleDefinition* Style=nullptr);
    bool LoadRig(USkeletalMeshComponent& Mesh,const FCombatStyleDefinition& Style,FName Id,FCombatBodyRig& Out);
}
