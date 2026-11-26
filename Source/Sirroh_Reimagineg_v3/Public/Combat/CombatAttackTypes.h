#pragma once

#include "CoreMinimal.h"
#include "CombatAttackTypes.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EDirectionalAttack : uint8 { Light, Heavy, Pierce };

UENUM(BlueprintType)
enum class ECombatGuard : uint8 { Up, Front, Low };

UENUM(BlueprintType)
enum class ECombatIntensity : uint8 { Weak, Medium, Strong };

/** Camera at -Y looks toward +Y: screen right=-X, screen up=+Z.
 * Facing changes anatomical forward, never the sign of camera depth. */
namespace CombatSpace
{
    inline FVector Forward(float Facing) { return FVector(Facing>=0?1:-1,0,0); }
    inline FVector Local(const FVector& Value,float Facing) { return FVector(Value.X*Facing,Value.Y,Value.Z); }
    inline FQuat Orientation(FVector Blade)
    {
        Blade=Blade.GetSafeNormal(FVector::FReal(0.0001),FVector::XAxisVector);
        const FVector Normal=FMath::Abs(Blade.Y)>0.95?FVector::ZAxisVector:FVector::YAxisVector;
        return FRotationMatrix::MakeFromXY(Blade,Normal).ToQuat();
    }
    inline FVector InLane(FVector Position,float LaneY) { Position.Y-=LaneY; return Position; }
}

USTRUCT(BlueprintType)
struct FCombatChargeSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float LightStaggerSeconds=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float LightMaximumSeconds=2;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float HeavyDisarmSeconds=3;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float LightImpact=3;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float StaggerImpact=10;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float DisarmImpact=18;
};

UENUM(BlueprintType)
enum class EDirectionalCombatPhase : uint8 { Combat, Aiming, Attacking, Recovery, Blocking, Staggered, Disarmed };

UENUM(BlueprintType)
enum class ECombatContactResult : uint8 { World, Hit, Rebound, Shake, Stagger, Disarm, Breakthrough, Weapon };

/** Fixed tuning for this prototype, independent of HP and any statistics provider. */
USTRUCT(BlueprintType)
struct FCombatContactSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float BladeRadius=3;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float Strength=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float Endurance=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float Weight=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float Power=1;
    /** cm/s represented by one unit in the impact formula. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float SpeedUnit=100;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float ImpactScale=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float StaggerSeconds=1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) float KnockbackSpeed=220;
};

/** Shared gameplay geometry. X is the blade direction, independent of camera/card orientation. */
USTRUCT()
struct FCombatWeaponFrame
{
    GENERATED_BODY()
    UPROPERTY() FVector Grip=FVector::ZeroVector;
    UPROPERTY() float Angle=0;
    UPROPERTY() FQuat Rotation=FQuat::Identity;
    FVector Direction() const { return Rotation.GetAxisX(); }
    FVector Base(float PixelScale) const { return Grip-Direction()*16*PixelScale; }
    FVector Tip(float PixelScale) const { return Grip+Direction()*368*PixelScale; }
};

USTRUCT()
struct FCombatRig
{
    GENERATED_BODY()
    UPROPERTY() FVector Shoulder=FVector(0,0,35);
    UPROPERTY() float ArmLength=60;
    UPROPERTY() float PixelScale=0.3f;
    UPROPERTY() float HeadHeight=75;
    UPROPERTY() float PelvisHeight=0;
};

USTRUCT(BlueprintType)
struct FCombatContactEvent
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 AttackId=0;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Attacker=nullptr;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Target=nullptr;
    UPROPERTY(BlueprintReadOnly) ECombatContactResult Result=ECombatContactResult::World;
    UPROPERTY(BlueprintReadOnly) FVector Point=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector Direction=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) float Impact=0;
    UPROPERTY(BlueprintReadOnly) double Time=0;
    UPROPERTY(BlueprintReadOnly) ECombatIntensity Intensity=ECombatIntensity::Weak;
    UPROPERTY(BlueprintReadOnly) bool bSwordCollision=false;
    /** 255 infers from target; explicit surface survives non-replicated world actors. */
    UPROPERTY(BlueprintReadOnly) uint8 EffectSurface=255;
};

/** Replicated interruption, including the owning client and late observers. */
USTRUCT()
struct FCombatReaction
{
    GENERATED_BODY()
    UPROPERTY() uint32 Serial=0;
    UPROPERTY() uint32 InterruptedAttackId=0;
    UPROPERTY() ECombatContactResult Result=ECombatContactResult::Shake;
    UPROPERTY() double StartTime=0;
    UPROPERTY() double EndTime=0;
    UPROPERTY() FCombatWeaponFrame Start;
    UPROPERTY() FCombatWeaponFrame End;
    UPROPERTY() bool bInterrupt=false;
    UPROPERTY() bool bDisarmed=false;
    UPROPERTY() float PoseWeight=0;
};

USTRUCT(BlueprintType)
struct FDirectionalAttackSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0.08")) float Duration=0.28f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0.08")) float Recovery=0.25f;
    /** Additional follow-through beyond the direction selected on release. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="5",ClampMax="150")) float FollowThroughDegrees=65;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0.2",ClampMax="0.95")) float ReachFraction=0.88f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0")) float PoseTime=0;
};

/** Compact attack description, evaluated from shared time rather than replicated bone transforms.
 * Offsets use world axes relative to the moving pawn origin, not its changing facing rotation. */
USTRUCT()
struct FDirectionalAttackMotion
{
    GENERATED_BODY()
    UPROPERTY() uint32 Id=0;
    UPROPERTY() bool bPrepared=false;
    UPROPERTY() bool bReleased=false;
    UPROPERTY() EDirectionalAttack Type=EDirectionalAttack::Light;
    UPROPERTY() double StartTime=0;
    UPROPERTY() float Facing=1;
    UPROPERTY() FVector StartGrip=FVector::ZeroVector;
    UPROPERTY() FVector EndGrip=FVector::ZeroVector;
    UPROPERTY() FVector ControlGrip=FVector::ZeroVector;
    UPROPERTY() FVector LockedTarget=FVector::ZeroVector;
    UPROPERTY() float StartAngle=0;
    UPROPERTY() float SweepAngle=0;
    UPROPERTY() float Duration=0.28f;
    UPROPERTY() float Recovery=0.25f;
    UPROPERTY() float PreparationAlpha=1;
    UPROPERTY() ECombatGuard Guard=ECombatGuard::Front;
    UPROPERTY() double PreparedAt=0;
    UPROPERTY() float ChargeSeconds=0;
    UPROPERTY() float ImpactScore=3;
    UPROPERTY() bool bPush=false;
    UPROPERTY() ECombatIntensity Intensity=ECombatIntensity::Weak;
    UPROPERTY() FQuat StartRotation=FQuat::Identity;
    UPROPERTY() FQuat ContactRotation=FQuat::Identity;
    UPROPERTY() FQuat EndRotation=FQuat::Identity;
    UPROPERTY() FVector ReadyGrip=FVector::ZeroVector;
    UPROPERTY() FQuat ReadyRotation=FQuat::Identity;
};
