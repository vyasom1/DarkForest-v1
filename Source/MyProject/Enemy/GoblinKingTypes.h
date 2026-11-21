#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GoblinKingTypes.generated.h"

class UAnimMontage;
class USoundBase;

/**
 * One Goblin King attack. Values are authored on editable defaults or on a
 * UGoblinKingAttackData asset; the phase component enforces the 0.35 s
 * wind-up floor and the chain/repeat limits at runtime.
 */
USTRUCT(BlueprintType)
struct FGoblinKingAttack
{
    GENERATED_BODY()

    /** Stable identifier used by tells, HUD text, and animation notifies. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
    FName AttackId = NAME_None;

    /** Player-facing name for the HUD / debug output. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="0"))
    float Damage = 20.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="0"))
    float PostureDamage = 22.f;

    /** Authored wind-up; the runtime clamps this up to MinWindupSeconds. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="0"))
    float WindupSeconds = 0.6f;

    /** Reach of this specific attack, in centimetres. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="0"))
    float Range = 300.f;

    /** Total hits in the chain (1 = single attack). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="1"))
    int32 ChainHits = 1;

    /** Delay between chain hits, also floored to MinWindupSeconds. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="0"))
    float ChainIntervalSeconds = 0.5f;

    /** Passes through guard and cannot be deflected; dodge or move out of range. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
    bool bUnblockable = false;

    /** Jumping avoids the hit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
    bool bJumpable = false;

    /** Colour of the visual tell (mesh flash / HUD accent). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack")
    FLinearColor TellColor = FLinearColor(1.f, 0.55f, 0.1f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|Assets")
    TObjectPtr<UAnimMontage> Montage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack|Audio")
    TObjectPtr<USoundBase> TellSound;
};

/** Attacks available in one health-segment phase. */
USTRUCT(BlueprintType)
struct FGoblinKingPhase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Phase", meta=(ClampMin="1"))
    int32 PhaseNumber = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Phase")
    TArray<FGoblinKingAttack> Attacks;

    /** Cooldown between attack sequences in this phase. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Phase", meta=(ClampMin="0"))
    float RecoverySeconds = 1.1f;

    /** Window after a sequence during which the boss takes extra posture damage. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Phase", meta=(ClampMin="0"))
    float PunishWindowSeconds = 0.f;

    /** Scales authored wind-ups; below 1 is faster. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Phase", meta=(ClampMin="0.1"))
    float WindupMultiplier = 1.f;
};

/**
 * Optional Data Asset for designers. When not assigned the King uses the
 * built-in default kit so the fight works before any asset authoring.
 */
UCLASS(BlueprintType)
class MYPROJECT_API UGoblinKingAttackData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss")
    TArray<FGoblinKingPhase> Phases;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss", meta=(ClampMin="0.35"))
    float MinWindupSeconds = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss", meta=(ClampMin="1"))
    int32 MaxConsecutiveAttacks = 3;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss", meta=(ClampMin="0"))
    int32 MaxAttackRepeats = 2;
};
