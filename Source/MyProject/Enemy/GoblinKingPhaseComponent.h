#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Enemy/GoblinKingTypes.h"
#include "GoblinKingPhaseComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGoblinKingPhaseChanged, int32, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGoblinKingSegmentRemoved, int32, SegmentsRemaining);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGoblinKingPostureBrokenEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGoblinKingPostureRecoveredEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGoblinKingDefeatedEvent);

/**
 * Segment / phase / attack-cadence brain for the Goblin King.
 *
 * Kept actor-free and clock-based so automation tests can exercise phase
 * transitions, deathblow segment removal, wind-up floors, repeat limits,
 * punish cadence, and reset behaviour without a world.
 *
 * Posture is a deplete gauge: it starts full and breaks at zero. A deathblow
 * is only valid while broken and removes exactly one of three segments; the
 * final segment ends the encounter.
 */
UCLASS(Blueprintable, meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UGoblinKingPhaseComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGoblinKingPhaseComponent();

    /** Populate the built-in three-phase kit when no Data Asset is assigned. */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void EnsureDefaultPhases();

    /** Replace the phase kit and reset all per-segment and cadence state. */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void SetPhases(const TArray<FGoblinKingPhase>& InPhases);

    const TArray<FGoblinKingPhase>& GetPhases() const { return Phases; }

    UFUNCTION(BlueprintPure, Category="Boss")
    int32 GetPhaseCount() const { return Phases.Num(); }

    /** Current phase, 1-based (100/66/33% = 3/2/1 segments). */
    UFUNCTION(BlueprintPure, Category="Boss")
    int32 GetPhase() const { return Phase; }

    UFUNCTION(BlueprintPure, Category="Boss")
    int32 GetSegmentsRemaining() const { return SegmentsRemaining; }

    UFUNCTION(BlueprintPure, Category="Boss")
    bool IsDefeated() const { return bDefeated; }

    UFUNCTION(BlueprintPure, Category="Boss")
    bool IsPostureBroken() const { return bPostureBroken; }

    UFUNCTION(BlueprintPure, Category="Boss")
    float GetSegmentHealth() const { return SegmentHealth; }

    UFUNCTION(BlueprintPure, Category="Boss")
    float GetSegmentMaxHealth() const { return SegmentMaxHealth; }

    UFUNCTION(BlueprintPure, Category="Boss")
    float GetSegmentPosture() const { return CurrentPosture; }

    UFUNCTION(BlueprintPure, Category="Boss")
    float GetSegmentMaxPosture() const { return SegmentMaxPosture; }

    UFUNCTION(BlueprintPure, Category="Boss")
    float GetSegmentHealthFraction() const
    {
        return SegmentMaxHealth > 0.f ? SegmentHealth / SegmentMaxHealth : 0.f;
    }

    UFUNCTION(BlueprintPure, Category="Boss")
    float GetPostureFraction() const
    {
        return SegmentMaxPosture > 0.f ? CurrentPosture / SegmentMaxPosture : 0.f;
    }

    /** Apply damage to the current segment. Health at zero forces a posture break. */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void ApplySegmentDamage(float HealthDamage, float PostureDamage);

    /**
     * Remove one segment when posture is broken. Returns false when the
     * posture is intact or the encounter is already over.
     */
    UFUNCTION(BlueprintCallable, Category="Boss")
    bool TryDeathblow();

    /** Full encounter reset: three segments, phase 1, full vitals, no timers. */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void ResetEncounter();

    /** Debug/authoring helper: jump to a phase and reset that segment's state. */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void SetPhase(int32 NewPhase);

    /** Recover from a stagger: posture returns to PostureRecoveryFraction of max. */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void RecoverPosture();

    /** Next attack in the phase kit, respecting the two-repeat ceiling. */
    UFUNCTION(BlueprintCallable, Category="Boss")
    int32 ChooseNextAttackIndex();

    /** True when the cadence allows another sequence right now. */
    UFUNCTION(BlueprintPure, Category="Boss")
    bool CanStartAttack() const;

    /** Record that a sequence started (updates consecutive and repeat counters). */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void RegisterAttackStarted(int32 AttackIndex);

    /**
     * Record that a sequence finished. Returns true when the three-attack
     * ceiling forced an extra recovery and opened the punish window.
     */
    UFUNCTION(BlueprintCallable, Category="Boss")
    bool RegisterAttackFinished();

    /** Clear the forced-recovery counters. */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void RegisterRecoveryFinished();

    UFUNCTION(BlueprintCallable, Category="Boss")
    void OpenPunishWindow(float Seconds);

    UFUNCTION(BlueprintPure, Category="Boss")
    bool IsPunishWindowOpen() const;

    /** Extra posture damage taken during a punish opening (1.5x by default). */
    UFUNCTION(BlueprintPure, Category="Boss")
    float GetIncomingPostureMultiplier() const;

    /** Authored wind-up after the phase speed multiplier and the 0.35 s floor. */
    UFUNCTION(BlueprintPure, Category="Boss")
    float GetEffectiveWindup(int32 AttackIndex) const;

    const FGoblinKingAttack* GetAttack(int32 AttackIndex) const;

    UFUNCTION(BlueprintPure, Category="Boss")
    bool CanRepeatAttack(int32 AttackIndex) const;

    UFUNCTION(BlueprintPure, Category="Boss")
    int32 GetLastAttackIndex() const { return LastAttackIndex; }

    UFUNCTION(BlueprintPure, Category="Boss")
    int32 GetRepeatCount() const { return RepeatCount; }

    UFUNCTION(BlueprintPure, Category="Boss")
    int32 GetConsecutiveAttacks() const { return ConsecutiveAttacks; }

    UPROPERTY(BlueprintAssignable, Category="Boss")
    FGoblinKingPhaseChanged OnPhaseChanged;

    UPROPERTY(BlueprintAssignable, Category="Boss")
    FGoblinKingSegmentRemoved OnSegmentRemoved;

    UPROPERTY(BlueprintAssignable, Category="Boss")
    FGoblinKingPostureBrokenEvent OnPostureBroken;

    UPROPERTY(BlueprintAssignable, Category="Boss")
    FGoblinKingPostureRecoveredEvent OnPostureRecovered;

    UPROPERTY(BlueprintAssignable, Category="Boss")
    FGoblinKingDefeatedEvent OnDefeated;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Segments", meta=(ClampMin="1"))
    float SegmentMaxHealth = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Segments", meta=(ClampMin="1"))
    float SegmentMaxPosture = 100.f;

    /** Fraction of posture restored after the stagger window times out. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Segments", meta=(ClampMin="0", ClampMax="1"))
    float PostureRecoveryFraction = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Cadence", meta=(ClampMin="0.35"))
    float MinWindupSeconds = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Cadence", meta=(ClampMin="1"))
    int32 MaxConsecutiveAttacks = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Cadence", meta=(ClampMin="0"))
    int32 MaxAttackRepeats = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Cadence", meta=(ClampMin="1"))
    int32 TotalSegments = 3;

private:
    void ResetSegmentState();
    void BroadcastSegmentRemoved();
    const FGoblinKingPhase* GetCurrentPhase() const;

    TArray<FGoblinKingPhase> Phases;
    int32 Phase = 1;
    int32 SegmentsRemaining = 3;
    int32 LastAttackIndex = INDEX_NONE;
    int32 RepeatCount = 0;
    int32 ConsecutiveAttacks = 0;
    int32 NextAttackCursor = 0;
    float SegmentHealth = 120.f;
    float CurrentPosture = 100.f;
    bool bPostureBroken = false;
    bool bDefeated = false;
    double NextAttackAllowedTime = 0.0;
    double PunishWindowEndTime = 0.0;
};
