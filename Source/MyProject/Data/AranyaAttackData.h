#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AranyaAttackData.generated.h"

/**
 * Data-driven melee attack definition for the Sekiro-style sword.
 *
 * Replaces hand-built UAnimMontage attack entries: the attack is played by
 * feeding an UAnimSequence + slot into UAnimInstance::PlaySlotAnimationAsDynamicMontage
 * so timing, damage, posture damage, audio, and lunge are all data-driven.
 */
class UAnimSequence;
class USoundBase;

UCLASS(BlueprintType)
class MYPROJECT_API UAranyaAttackData : public UDataAsset
{
    GENERATED_BODY()

public:
    /** Animation sequence played on the configured slot node. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation")
    TObjectPtr<UAnimSequence> AnimSequence;

    /** AnimGraph slot node name that receives the sequence. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation")
    FName SlotName = "DefaultSlot";

    /** Playback rate multiplier for the sequence. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation")
    float PlayRate = 1.0f;

    /** Hit trace start offset (seconds from montage start). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing")
    float HitWindowStart = 0.0f;

    /** Hit trace end offset (seconds from montage start). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing")
    float HitWindowEnd = 0.0f;

    /** Input buffer window during which the next combo hit may be accepted. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing")
    float ComboCancelWindow = 0.0f;

    /** Forward lunge distance on attack start. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing")
    float LungeDistance = 0.0f;

    /** Damage dealt on hit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
    float Damage = 10.0f;

    /** Posture damage dealt on hit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
    float PostureDamage = 10.0f;

    /** Swing sound played at attack start. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    TObjectPtr<USoundBase> SwingSFX;

    /** Impact sound played at hit window. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    TObjectPtr<USoundBase> HitSFX;

    /** Deflect ping sound. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    TObjectPtr<USoundBase> DeflectPingSFX;

    /** Unblockable attacks must be dodged. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flags")
    bool bIsDangerous = false;

    /** Whether the attack can be deflected. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flags")
    bool bCanBeDeflected = true;

    /** Whether the attack launches the target on hit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flags")
    bool bLaunchesOnHit = false;
};
