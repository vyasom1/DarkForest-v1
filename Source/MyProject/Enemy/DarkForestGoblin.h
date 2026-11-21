#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DarkForestGoblin.generated.h"

class ADarkForestHero;
class UAnimMontage;
class UAnimSequence;
class USoundBase;

UCLASS()
class MYPROJECT_API ADarkForestGoblin : public ACharacter
{
    GENERATED_BODY()
public:
    ADarkForestGoblin();
    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, Category="Combat")
    virtual void ReceiveSwordHit(float HealthDamage, float PostureDamage, ADarkForestHero* Source);
    UFUNCTION(BlueprintCallable, Category="Combat")
    virtual void ReceiveDeflect(float PostureDamage);
    UFUNCTION(BlueprintCallable, Category="Combat")
    virtual void ApplyDeathblow(ADarkForestHero* Source);
    UFUNCTION(BlueprintCallable, Category="Combat")
    virtual bool TryBeginMeleeAttack(ADarkForestHero* Target);
    UFUNCTION(BlueprintPure, Category="Combat")
    virtual bool IsAlive() const { return Health > 0.f; }
    UFUNCTION(BlueprintPure, Category="Combat")
    virtual bool IsPostureBroken() const { return bPostureBroken; }
    UFUNCTION(BlueprintPure, Category="Combat")
    virtual bool IsTelegraphing() const { return bAttacking; }
    virtual bool IsRecovering() const;
    void ResetForStoryRespawn(const FVector& Location);
    UFUNCTION(BlueprintPure, Category="Combat")
    virtual float GetHealth() const { return Health; }
    UFUNCTION(BlueprintPure, Category="Combat")
    virtual float GetPosture() const { return Posture; }
    UFUNCTION(BlueprintPure, Category="Combat")
    virtual float GetHealthFraction() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }
    UFUNCTION(BlueprintPure, Category="Combat")
    virtual float GetPostureFraction() const { return MaxPosture > 0.f ? Posture / MaxPosture : 0.f; }
    UFUNCTION(BlueprintPure, Category="Combat")
    virtual float GetMeleeRange() const { return MeleeRange; }

    UFUNCTION(BlueprintImplementableEvent, Category="Combat")
    void OnAttackTelegraphed();
    UFUNCTION(BlueprintImplementableEvent, Category="Combat")
    void OnPostureBroken();
    UFUNCTION(BlueprintImplementableEvent, Category="Combat")
    void OnGoblinDefeated();

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Assets")
    TObjectPtr<UAnimMontage> AttackMontage;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Assets")
    TObjectPtr<UAnimMontage> StaggerMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Audio")
    TObjectPtr<USoundBase> AttackTellSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Audio")
    TObjectPtr<USoundBase> HurtVoiceSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Health", meta=(ClampMin="1"))
    float MaxHealth = 90.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Health", meta=(ClampMin="1"))
    float MaxPosture = 100.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Attack", meta=(ClampMin="0"))
    float MeleeDamage = 20.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Attack", meta=(ClampMin="0"))
    float MeleePostureDamage = 23.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Attack", meta=(ClampMin="0"))
    float MeleeRange = 175.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Attack", meta=(ClampMin="0"))
    float TelegraphSeconds = .7f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Attack", meta=(ClampMin="0"))
    float AttackCooldownSeconds = 1.65f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Posture", meta=(ClampMin="0"))
    float PostureBrokenSeconds = 2.5f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Death")
    float DeadActorLifetimeSeconds = 4.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    float Health = 90.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    float Posture = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    bool bPostureBroken = false;

protected:
    void PlayAttackVisual(UAnimMontage* Montage, float Windup, UAnimSequence* Alternate = nullptr);
    bool CanCommitMelee(const ADarkForestHero* Target) const;
    bool IsInCommittedArc(const ADarkForestHero* Target, float Reach) const;
    void FaceMeleeTarget(const ADarkForestHero* Target);
    void CancelAttackVisual();
    virtual void ResolveAttack();
    virtual void RecoverPosture();
    virtual void Die();
    virtual void AddPosture(float Amount);

private:
    UPROPERTY()
    TObjectPtr<UAnimSequence> GoblinWalk;
    UPROPERTY()
    TObjectPtr<UAnimSequence> GoblinSwing;
    TWeakObjectPtr<ADarkForestHero> AttackTarget;
    FTimerHandle TelegraphTimer;
    FTimerHandle StaggerTimer;
    bool bAttacking = false;
    float NextAttackTime = 0.f;
};
