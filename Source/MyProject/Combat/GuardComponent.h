#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/DarkForestLog.h"
#include "Data/AranyaAttackData.h"
#include "GuardComponent.generated.h"

/**
 * Guard state plus a configurable deflect window.
 *
 * SetGuarding(true) opens the deflect window for DeflectWindowSeconds; the
 * window closes by itself after that time (clock-based, so it also works in
 * automation tests without a ticking world). TryDeflect succeeds only while
 * the window is open and fires the OnDeflect Blueprint event.
 *
 * When a deflect data asset is provided, the deflect animation is played
 * through UAnimInstance::PlaySlotAnimationAsDynamicMontage on the owner.
 */
UCLASS(Blueprintable, meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UGuardComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /** Enter or leave guard. Entering guard opens the deflect window. */
    UFUNCTION(BlueprintCallable, Category="Dark Forest")
    void SetGuarding(bool bEnabled);

    /** True while the owner is holding guard. */
    UFUNCTION(BlueprintPure, Category="Dark Forest")
    bool IsGuarding() const;

    /** True only while the deflect window is still open after guarding started. */
    UFUNCTION(BlueprintPure, Category="Dark Forest")
    bool IsDeflectWindowOpen() const;

    /** Attempt a deflect; succeeds only inside the window and fires OnDeflect. */
    UFUNCTION(BlueprintCallable, Category="Dark Forest")
    bool TryDeflect();

    /** Fired when a deflect attempt succeeds inside the window. */
    UFUNCTION(BlueprintImplementableEvent, Category="Dark Forest")
    void OnDeflect();

    /**
     * Play the deflect animation described by a data asset through the
     * owner's anim slot as a dynamic montage.
     */
    UFUNCTION(BlueprintCallable, Category="Combat|Guard")
    void PlayDeflectAnimation(const UAranyaAttackData* Data);

    /** Seconds the deflect window stays open after guard starts. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Guard", meta=(ClampMin="0"))
    float DeflectWindowSeconds = 0.4f;

private:
    bool bGuarding = false;
    double WindowOpenTime = 0.0;
};
