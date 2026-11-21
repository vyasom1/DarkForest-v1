#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "DarkForestHero.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
class UAnimMontage;
class UAnimSequence;
class USoundBase;
class UStaticMeshComponent;
class ADarkForestGoblin;
class AGoblinKing;
class UAttackComboComponent;
class UBossHealthWidget;
class UDamageNumberWidget;
class UGuardComponent;
class ULockOnComponent;
class UAranyaAttackData;
class UAranyaCombatTuning;

UCLASS()
class MYPROJECT_API ADarkForestHero : public ACharacter
{
    GENERATED_BODY()

public:
    ADarkForestHero();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UFUNCTION(BlueprintCallable, Category="Combat")
    void ReceiveMeleeHit(float HealthDamage, float PostureDamage, ADarkForestGoblin* Source,
        bool bUnblockable = false);
    UFUNCTION(BlueprintCallable, Category="Combat")
    void SetGodMode(bool bEnabled);
    UFUNCTION(BlueprintPure, Category="Combat")
    bool IsGodMode() const { return bGodMode; }
    UFUNCTION(BlueprintCallable, Category="Combat")
    void RestoreVitals();
    UFUNCTION(BlueprintCallable, Category="Checkpoint")
    void ReviveAt(const FTransform& RespawnTransform);
    UFUNCTION(BlueprintCallable, Category="Boss HUD")
    void BindBossWidget(AGoblinKing* Boss);
    UFUNCTION(BlueprintCallable, Category="Boss HUD")
    void HandleBossDefeated(AGoblinKing* Boss);
    UFUNCTION(BlueprintCallable, Category="Checkpoint")
    void HandleRespawned();
    UFUNCTION(BlueprintPure, Category="Combat")
    bool IsAlive() const { return Health > 0.f; }
    float GetDeathAnimationDuration() const;
    UFUNCTION(BlueprintPure, Category="Combat")
    float GetHealthFraction() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }
    UFUNCTION(BlueprintPure, Category="Combat")
    float GetPostureFraction() const { return MaxPosture > 0.f ? Posture / MaxPosture : 0.f; }
    UFUNCTION(BlueprintPure, Category="Combat")
    ADarkForestGoblin* GetLockedTarget() const;

    UFUNCTION(BlueprintImplementableEvent, Category="Combat")
    void OnSwordSwing(int32 ComboStep);
    UFUNCTION(BlueprintImplementableEvent, Category="Combat")
    void OnDeflectSuccess();
    UFUNCTION(BlueprintImplementableEvent, Category="Combat")
    void OnHeroDefeated();
    UFUNCTION(BlueprintImplementableEvent, Category="Combat")
    void OnBossVictory();
    UFUNCTION(BlueprintImplementableEvent, Category="Checkpoint")
    void OnRespawned();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Input")
    TObjectPtr<UInputMappingContext> InputContext;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Input")
    TObjectPtr<UInputAction> MoveAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Input")
    TObjectPtr<UInputAction> LookAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Input")
    TObjectPtr<UInputAction> JumpAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Input")
    TObjectPtr<UInputAction> AttackAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Input")
    TObjectPtr<UInputAction> GuardAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Input")
    TObjectPtr<UInputAction> DeflectAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Input")
    TObjectPtr<UInputAction> DeathblowAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Input")
    TObjectPtr<UInputAction> LockOnAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Assets")
    TArray<TObjectPtr<UAnimMontage>> LightComboMontages;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Assets")
    TObjectPtr<UAnimMontage> DeflectMontage;

    /** Optional sequence-based parry; legacy montage remains a fallback. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Guard")
    TObjectPtr<UAranyaAttackData> DeflectData;

    /**
     * Data-driven attack chain (index 0 = Hit1). When populated, attacks are
     * played from the referenced UAnimSequence via PlaySlotAnimationAsDynamicMontage.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Attack")
    TArray<TObjectPtr<UAranyaAttackData>> AttackDataChain;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Audio")
    TObjectPtr<USoundBase> SwordSwingSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Audio")
    TObjectPtr<USoundBase> SwordImpactSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Audio")
    TObjectPtr<USoundBase> DeflectSuccessSound;

    /** Optional combat tuning bundle (hit-stop, camera shake, VFX, dodge). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Tuning")
    TObjectPtr<UAranyaCombatTuning> CombatTuning;

    /** Optional WBP parented to UBossHealthWidget; canvas HUD is the fallback. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="UI")
    TSubclassOf<UBossHealthWidget> BossHealthWidgetClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI")
    TObjectPtr<UBossHealthWidget> BossHealthWidget;

    /** Optional WBP parented to UDamageNumberWidget; native C++ canvas is the fallback. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="UI")
    TSubclassOf<UDamageNumberWidget> DamageNumberWidgetClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI")
    TObjectPtr<UDamageNumberWidget> DamageNumberWidget;

    /** Owns the 3-hit chain state (step + reset timeout). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
    TObjectPtr<UAttackComboComponent> AttackComboComponent;

    /** Owns guard state and the deflect window. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
    TObjectPtr<UGuardComponent> GuardComponent;

    /** Owns lock-on target acquisition, validation, and clearing. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Components")
    TObjectPtr<ULockOnComponent> LockOnComponent;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Health", meta=(ClampMin="1"))
    float MaxHealth = 100.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Health", meta=(ClampMin="1"))
    float MaxPosture = 100.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Guard", meta=(ClampMin="0", ClampMax="1"))
    float GuardHealthDamageMultiplier = .18f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Guard", meta=(ClampMin="0"))
    float DeflectWindowSeconds = .25f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Guard", meta=(ClampMin="0"))
    float DeflectPostureDamage = 45.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Guard", meta=(ClampMin="0"))
    float PostureBrokenSeconds = 2.5f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Attack")
    TArray<float> LightComboDamage;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Attack", meta=(ClampMin="0"))
    float AttackPostureDamage = 22.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Attack", meta=(ClampMin="0"))
    float AttackRange = 210.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Attack", meta=(ClampMin="0"))
    float AttackHalfAngleDegrees = 55.f;
    /** Contact tolerance around the visible blade, in centimetres. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Weapon", meta=(ClampMin="0", ClampMax="15"))
    float SwordContactRadius = 5.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Weapon")
    TObjectPtr<UStaticMeshComponent> SwordBlade;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Attack", meta=(ClampMin="0"))
    float AttackHitDelaySeconds = .16f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Attack", meta=(ClampMin="0"))
    float AttackDurationSeconds = .5f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Attack", meta=(ClampMin="0"))
    float ComboResetSeconds = .95f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Death")
    TObjectPtr<UAnimSequence> DeathAnimSequence;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|Deathblow", meta=(ClampMin="0"))
    float DeathblowRange = 205.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|LockOn", meta=(ClampMin="0"))
    float LockRange = 1400.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat|LockOn", meta=(ClampMin="0"))
    float LockRotationSpeed = 7.f;
    /** Elevated combat framing; negative pitch looks down in Unreal. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Camera|LockOn", meta=(ClampMin="-65", ClampMax="-15"))
    float LockCameraPitch = -38.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Camera|LockOn", meta=(ClampMin="340", ClampMax="900"))
    float LockCameraArmLength = 520.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Camera|LockOn", meta=(ClampMin="0", ClampMax="200"))
    float LockCameraHeight = 90.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    float Health = 100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    float Posture = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    bool bGuarding = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    bool bPostureBroken = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    bool bGodMode = false;

private:
    friend class FDarkForestHeroGuardContinuity;
    friend class FDarkForestEnemyMeleeCover;
    friend class FDarkForestHeroAttackBuffer;
    void EnsureInputContext();
    void ActivateInputContext();
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void StartJump();
    void StopJumpingInput();
    void InputAttack();
    void BeginGuard();
    void EndGuard();
    void InputDeflect();
    void InputDeathblow();
    void ToggleLockOn();
    void BeginLockCamera();
    void ReleaseLockCamera(bool bImmediate);
    void StartAttack();
    void ResolveSwordHit();
    void BeginSwordHitWindow();
    void EndSwordHitWindow();
    void FinishAttack();
    void EndDeflect();
    void RecoverPosture();
    void Defeat();
    void EnsureBossWidget();
    void EnsureDamageNumberWidget();
    UAnimMontage* PlayAnimSequence(UAnimSequence* Sequence, float InPlayRate, int32 LoopCount, float BlendInTime, float BlendOutTime);
    void EndHitStop();
    AGoblinKing* FindBoss() const;
    ADarkForestGoblin* FindTarget(float Range, bool bRequireBroken) const;

    FTimerHandle HitTimer;
    FTimerHandle HitWindowEndTimer;
    TSet<TWeakObjectPtr<ADarkForestGoblin>> EnemiesHitThisSwing;
    FTimerHandle AttackEndTimer;
    FTimerHandle DeflectTimer;
    FTimerHandle StaggerTimer;
    FTimerHandle HitStopTimer;
    bool bDeflecting = false;
    bool bAttacking = false;
    bool bGuardInputHeld = false;
    FVector PreviousSwordBase = FVector::ZeroVector;
    FVector PreviousSwordTip = FVector::ZeroVector;
    bool bHasPreviousSwordSample = false;
    bool bHitStopActive = false;
    bool bLockCameraActive = false;
    bool bRestoringFreeCamera = false;
    bool bSavedFreeCamera = false;
    float SavedFreeCameraArm = 340.f;
    FVector SavedFreeCameraOffset = FVector::ZeroVector;
    float PreviousTimeDilation = 1.f;
};
