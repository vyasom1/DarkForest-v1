#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Combat/DamageNumbers.h"
#include "CombatAutomationTests.generated.h"

/** UFUNCTION-based receiver so dynamic delegates can be observed in tests. */
UCLASS()
class UDarkForestTestVitalsReceiver : public UObject
{
    GENERATED_BODY()

public:
    int32 VitalsCount = 0;
    int32 DefeatCount = 0;
    int32 BreakCount = 0;
    float LastHealth = -1.f;
    float LastPosture = -1.f;

    UFUNCTION()
    void HandleVitals(float Health, float Posture)
    {
        ++VitalsCount;
        LastHealth = Health;
        LastPosture = Posture;
    }

    UFUNCTION()
    void HandleDefeated() { ++DefeatCount; }

    UFUNCTION()
    void HandleBroken() { ++BreakCount; }
};

/** UFUNCTION-based receiver for damage number delegate tests. */
UCLASS()
class UDarkForestTestDamageNumberReceiver : public UObject
{
    GENERATED_BODY()
public:
    int32 Count = 0;
    UFUNCTION()
    void Handle(float Amount, FVector WorldLocation, EDamageNumberType Type)
    {
        ++Count;
    }
};
