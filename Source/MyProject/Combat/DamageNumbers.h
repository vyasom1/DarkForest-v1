#pragma once

#include "CoreMinimal.h"
#include "DamageNumbers.generated.h"

/** Categories of floating damage numbers emitted by the combat system. */
UENUM(BlueprintType, Category="Combat|DamageNumbers")
enum class EDamageNumberType : uint8
{
    Health UMETA(DisplayName="Health"),
    Posture UMETA(DisplayName="Posture"),
    Deflect UMETA(DisplayName="Deflect"),
    Deathblow UMETA(DisplayName="Deathblow")
};

/** Native multicast delegate used by the world-scoped damage number router. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FDamageNumberEvent, float, Amount, FVector, WorldLocation, EDamageNumberType, DamageType);