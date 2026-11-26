#pragma once
#include "CoreMinimal.h"
#include "CombatDefenseResolver.generated.h"

UENUM(BlueprintType)
enum class ECombatDefenseOutcome : uint8 { Pending, Miss, Block, PerfectBlock, Parry, BodyHit, Cancelled };

/** One committed strike/projectile. Its trajectory never follows the defender. */
USTRUCT(BlueprintType)
struct FCombatDefenseThreat
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FGuid Id;
    UPROPERTY(BlueprintReadOnly) ECombatDefenseOutcome Outcome=ECombatDefenseOutcome::Pending;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Attacker=nullptr;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Projectile=nullptr;
    UPROPERTY(BlueprintReadOnly) FGuid WeaponId;
    UPROPERTY(BlueprintReadOnly) int32 AttackId=0;
    UPROPERTY(BlueprintReadOnly) int32 StrikePhase=0;
    UPROPERTY() uint32 Epoch=0;
    UPROPERTY(BlueprintReadOnly) FVector Origin=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector Velocity=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector Acceleration=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) double StartTime=0;
    UPROPERTY(BlueprintReadOnly) double PredictedContactTime=0;
    UPROPERTY(BlueprintReadOnly) double ExpireTime=0;
    UPROPERTY(BlueprintReadOnly) bool bPierce=false;
    UPROPERTY(BlueprintReadOnly) bool bRanged=false;
    FVector Position(double Time) const;
    bool IsValid() const;
    bool IsFlashing(double Time) const;
};

USTRUCT(BlueprintType)
struct FCombatDefenseResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FGuid ThreatId;
    UPROPERTY(BlueprintReadOnly) ECombatDefenseOutcome Outcome=ECombatDefenseOutcome::Pending;
    UPROPERTY(BlueprintReadOnly) double Time=0;
    UPROPERTY(BlueprintReadOnly) FVector Point=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) float DamageScale=1;
    UPROPERTY(BlueprintReadOnly) float ImpulseScale=1;
    UPROPERTY(BlueprintReadOnly) float DurabilityCost=0;
    UPROPERTY(BlueprintReadOnly) bool bOmniblock=false;
    bool IsDefense() const;
};

/** All geometry is in the gameplay XZ plane, with an independent Y depth check. */
struct FCombatDefenseFrame
{
    FVector Centre=FVector::ZeroVector;
    FVector2D Paddle=FVector2D(1,0); // X/Z, direct input; no interpolation/rate limiter
    float Radius=90;
    float HalfDepth=35;
    float WeaponLength=100;
    float PerfectFraction=0.2f;
    bool bCanDefend=false;
};

/** Authority-owned per-defender ledger. Never applies damage itself.
 * Partition Advance at each physical hit time, finalize that hit, then advance the remainder.
 * Never advance predictions beyond a physical hit that has not yet been processed.
 * FinalizeBodyHit prevents a later prediction or parry from undoing real damage.
 * Persist weapon durability externally; SetWeapon only switches the equipped instance.
 */
class SIRROH_REIMAGINEG_V3_API FCombatDefenseResolver
{
public:
    bool AddThreat(const FCombatDefenseThreat& Threat);
    void SetWeapon(FGuid Id,float Durability);
    void Reset();
    void Cancel(FGuid ThreatId,double Time);
    bool TryParry(double Time,const FCombatDefenseFrame& Frame);
    void Advance(double From,double To,const FCombatDefenseFrame& Previous,const FCombatDefenseFrame& Current,TArray<FCombatDefenseResult>& Results);
    FCombatDefenseResult FinalizeBodyHit(FGuid ThreatId,double Time);
    const FCombatDefenseResult* FindResult(FGuid Id) const;
    const TArray<FCombatDefenseThreat>& GetThreats() const { return Threats; }
    float GetDurability() const { return Durability; }
    double GetOmniblockUntil() const { return OmniblockUntil; }
    static float PaddleHalfAngle(const FCombatDefenseFrame& Frame);
private:
    struct FEntry { FCombatDefenseResult Result; bool bBodyFinalized=false; double ParryAt=-DBL_MAX; double KeepUntil=0; };
    TArray<FCombatDefenseThreat> Threats;
    TMap<FGuid,FEntry> Ledger;
    FGuid WeaponId;
    float Durability=0;
    double OmniblockUntil=-DBL_MAX;
    double LastAdvance=-DBL_MAX;
};
