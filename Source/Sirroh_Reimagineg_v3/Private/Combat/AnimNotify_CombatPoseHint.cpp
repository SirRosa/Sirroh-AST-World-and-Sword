#include "Combat/AnimNotify_CombatPoseHint.h"
FString UAnimNotify_CombatPoseHint::GetNotifyName_Implementation() const
{
    return StaticEnum<ECombatPoseHintMarker>()->GetNameStringByValue(static_cast<int64>(Marker));
}
