#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/DarkForestLog.h"
#include "Data/AranyaAttackData.h"
#include "Animation/AnimMontage.h"
#include "AttackComboComponent.generated.h"

/**
 * Three-hit light attack chain (Hit1 -> Hit2 -> Hit3 -> Hit1).
 *
 * AdvanceCombo walks the chain exactly one step per call and restarts at
 * Hit1 when no input arrives within ComboResetSeconds, so the state machine
 * only ever makes safe single-step transitions. The step is clock-based so
 * the component works with or without a ticking world (automation tests).
 *
 * When a data-driven ComboChain is populated, attacks play through
 * UAnimInstance::PlaySlotAnimationAsDynamicMontage from the referenced
 * UAnimSequence + slot, instead of hand-built UAnimMontage assets.
 */
UCLASS(Blueprintable, meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UAttackComboComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAttackComboComponent();

    /** Advance the chain one step and fire OnComboStep with the new 1-based step. */
    UFUNCTION(BlueprintCallable, Category="Dark Forest")
    void AdvanceCombo();

    /** Restart the chain at Hit1 without firing OnComboStep. */
    UFUNCTION(BlueprintCallable, Category="Dark Forest")
    void ResetCombo();

    /** Store at most one follow-up input, using the caller's gameplay clock. */
    void BufferAttackInput(double Now);
    /** Consume a fresh input exactly once; stale inputs are discarded. */
    bool ConsumeBufferedAttack(double Now);
    void ClearBufferedAttack();

    /** Current chain step, 1-based; 0 before the first attack. */
    UFUNCTION(BlueprintPure, Category="Dark Forest")
    int32 GetCurrentStep() const;

    /** Fired after AdvanceCombo with the new 1-based step. */
    UFUNCTION(BlueprintImplementableEvent, Category="Dark Forest")
    void OnComboStep(int32 Step);

    /**
     * Play the attack described by a data asset through the owner's anim slot
     * as a dynamic montage. Plays nothing when the data, sequence, or anim
     * instance is invalid (errors are logged).
     */
    UFUNCTION(BlueprintCallable, Category="Combat|Attack")
    void PlayAttackMontage(const UAranyaAttackData* Data);

    /** Replace the data-driven combo chain; element 0 is Hit1. */
    UFUNCTION(BlueprintCallable, Category="Combat|Attack")
    void SetComboChain(const TArray<UAranyaAttackData*>& Chain);

public:
    /** Number of hits in the chain before it wraps back to Hit1. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Attack", meta=(ClampMin="1"))
    int32 MaxComboSteps = 3;

    /** Seconds without input after which the chain restarts at Hit1. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Attack", meta=(ClampMin="0"))
    float ComboResetSeconds = 2.f;

    /** Data-driven combo chain; empty means the legacy montage array is authoritative. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Attack")
    TArray<TObjectPtr<UAranyaAttackData>> ComboChain;

    /** Input buffer window in seconds for the next combo hit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Attack", meta=(ClampMin="0"))
    float InputBufferDuration = 0.15f;

    /** Seconds before the combo chain resets to Hit1 after the last hit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Attack", meta=(ClampMin="0"))
    float ComboResetDelay = 1.5f;

protected:
    int32 CurrentStep = 0;
    double LastAdvanceTime = -1e9;
    double BufferedAttackTime = -1e9;
    bool bHasBufferedAttack = false;
};
