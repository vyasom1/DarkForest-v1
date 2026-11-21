#pragma once

#include "Animation/AnimInstance.h"
#include "GoblinCombatAnimInstance.generated.h"

class UAnimSequence;

UCLASS()
class UGoblinCombatAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> Walk;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> Swing;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> Death;

    float AttackStart = -100.f;
    float AttackDuration = 0.f;
    float AttackCancelledAt = -1.f;

protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
