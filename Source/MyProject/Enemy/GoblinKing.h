#pragma once

#include "CoreMinimal.h"
#include "Enemy/DarkForestGoblin.h"
#include "GoblinKing.generated.h"

class UGoblinKingAttackData;
class UGoblinKingPhaseComponent;
struct FGoblinKingAttack;

/**
 * Three-segment boss derived from the ordinary goblin.
 *
 * Normal damage drains the current segment's health and posture but never
 * removes a segment; a deathblow during a posture break removes exactly one
 * segment. Phase advances 1 -> 2 -> 3 at 100/66/33% remaining health, and the
 * encounter ends only when the final segment is deathblown.
 *
 * Attack data comes from a UGoblinKingAttackData asset when assigned; otherwise
 * the built-in three-phase kit is used so the fight is playable immediately.
 */
UCLASS()
class MYPROJECT_API AGoblinKing : public ADarkForestGoblin
{
    GENERATED_BODY()

public:
    AGoblinKing();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual void ReceiveSwordHit(float HealthDamage, float PostureDamage, ADarkForestHero* Source) override;
    virtual void ReceiveDeflect(float PostureDamage) override;
    virtual void ApplyDeathblow(ADarkForestHero* Source) override;
    virtual bool TryBeginMeleeAttack(ADarkForestHero* Target) override;
    virtual bool IsAlive() const override;
    virtual bool IsPostureBroken() const override;
    virtual bool IsTelegraphing() const override;
    virtual bool IsRecovering() const override;
    virtual float GetHealthFraction() const override;
    virtual float GetPostureFraction() const override;

    /** Put the boss back to full three-segment state (used on shrine respawn). */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void ResetEncounter(bool bReturnToSpawn = true);

    /** Debug/authoring helper; also used by Aranya.SetPhase. */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void SetPhase(int32 NewPhase);

    UFUNCTION(BlueprintPure, Category="Boss")
    int32 GetPhaseNumber() const;

    UFUNCTION(BlueprintPure, Category="Boss")
    int32 GetSegmentsRemaining() const;

    UFUNCTION(BlueprintPure, Category="Boss")
    float GetSegmentHealthFraction() const;

    UFUNCTION(BlueprintPure, Category="Boss")
    FText GetCurrentAttackName() const;

    UFUNCTION(BlueprintPure, Category="Boss")
    bool IsCurrentAttackUnblockable() const;

    UFUNCTION(BlueprintPure, Category="Boss")
    bool IsCurrentAttackJumpable() const;

    UFUNCTION(BlueprintPure, Category="Boss")
    bool IsPunishWindowOpen() const;

    UFUNCTION(BlueprintPure, Category="Boss")
    FLinearColor GetCurrentTellColor() const;

    /** Fired at wind-up entry for every hit, including chain follow-ups. */
    UFUNCTION(BlueprintImplementableEvent, Category="Boss")
    void OnAttackTell(FName AttackId, const FText& DisplayName, bool bUnblockable, bool bJumpable, FLinearColor TellColor);

    /** Fired when a deathblow removes a segment (not on the final one). */
    UFUNCTION(BlueprintImplementableEvent, Category="Boss")
    void OnSegmentRemoved(int32 SegmentsRemaining, int32 NewPhase);

    UFUNCTION(BlueprintImplementableEvent, Category="Boss")
    void OnBossDefeated();

    UFUNCTION(BlueprintImplementableEvent, Category="Boss")
    void OnBossRecovered();

    UFUNCTION(BlueprintImplementableEvent, Category="Boss")
    void OnPunishWindowOpened();

    /** Optional authored kit; defaults are used when this is null. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Boss|Data")
    TObjectPtr<UGoblinKingAttackData> AttackData;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss")
    TObjectPtr<UGoblinKingPhaseComponent> PhaseComponent;

protected:
    virtual void Die() override;

    void ResolveChainHit();
    void FinishAttackSequence();
    void EndForcedRecovery();
    void EnterStagger();
    void RecoverFromStagger();
    void ClearKingTimers();
    void ApplyAttackData();
    void PlayKingAttackVisual(const FGoblinKingAttack& Attack, float Windup);

    UPROPERTY()
    TObjectPtr<UAnimSequence> HeavySwing;

    TWeakObjectPtr<ADarkForestHero> KingTarget;
    FTimerHandle KingTellTimer;
    FTimerHandle KingStaggerTimer;
    FTimerHandle KingRecoveryTimer;
    int32 CurrentAttackIndex = INDEX_NONE;
    int32 CurrentChainHit = 0;
    bool bKingAttacking = false;
    bool bTellActive = false;
    bool bStaggered = false;
    FVector SpawnLocation = FVector::ZeroVector;
    FRotator SpawnRotation = FRotator::ZeroRotator;
};
