#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthPostureComponent.generated.h"

/** Vitals payload: current health and posture after clamping. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDarkForestVitalsChanged, float, Health, float, Posture);

/** Fires when posture reaches zero (posture break). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDarkForestPostureBroken);

/** Fires when health reaches zero (defeat). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDarkForestDefeated);

/**
 * Canonical health and posture vitals.
 *
 * Health clamps to [0, MaxHealth] and fires the defeat event at zero.
 * Posture clamps to [0, MaxPosture], breaks (delegate) when it hits zero,
 * and recovers toward max after PostureRecoveryDelaySeconds without guarding.
 * Vitals are evaluated clock-based so the component behaves identically in
 * automation tests and in the ticking game world.
 *
 * Bind UI to the BlueprintAssignable OnVitalsUpdated delegate and read
 * GetHealth()/GetPosture() once at bind time to initialize the HUD from the
 * current values.
 */
UCLASS(Blueprintable, meta=(BlueprintSpawnableComponent))
class MYPROJECT_API UHealthPostureComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHealthPostureComponent();

    /** Apply signed health damage (positive values remove health). */
    UFUNCTION(BlueprintCallable, Category="Dark Forest")
    void ApplyDamage(float Amount);

    /** Apply signed posture damage (positive values drain posture toward the break). */
    UFUNCTION(BlueprintCallable, Category="Dark Forest")
    void ApplyPosture(float Amount);

    /** Pause or resume posture recovery; owners mirror their guard state here. */
    UFUNCTION(BlueprintCallable, Category="Dark Forest")
    void SetGuarding(bool bEnabled);

    /** Current health, clamped. */
    UFUNCTION(BlueprintPure, Category="Dark Forest")
    float GetHealth() const;

    /** Current posture, clamped. Recovery is evaluated on read. */
    UFUNCTION(BlueprintPure, Category="Dark Forest")
    float GetPosture();

    /** True while health is above zero. */
    UFUNCTION(BlueprintPure, Category="Dark Forest")
    bool IsAlive() const;

    /** Fired after every clamped change, with the new values. */
    UFUNCTION(BlueprintImplementableEvent, Category="Dark Forest")
    void OnVitalsChanged(float Health, float Posture);

    /** BlueprintAssignable vitals delegate; broadcast after applying and clamping. */
    UPROPERTY(BlueprintAssignable, Category="Dark Forest")
    FDarkForestVitalsChanged OnVitalsUpdated;

    /** BlueprintAssignable; fires once when posture reaches zero. */
    UPROPERTY(BlueprintAssignable, Category="Dark Forest")
    FDarkForestPostureBroken OnPostureBroken;

    /** BlueprintAssignable; fires once when health reaches zero. */
    UPROPERTY(BlueprintAssignable, Category="Dark Forest")
    FDarkForestDefeated OnDefeated;

    /** Seconds after a posture hit before recovery starts; guarding blocks recovery. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Posture", meta=(ClampMin="0"))
    float PostureRecoveryDelaySeconds = 3.f;

    /** Posture points recovered per second once recovery is active. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Posture", meta=(ClampMin="0"))
    float PostureRecoveryRate = 25.f;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Health", meta=(ClampMin="1"))
    float MaxHealth = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Health", meta=(ClampMin="1"))
    float MaxPosture = 100.f;

private:
    void BroadcastVitals();
    void UpdatePostureRecovery();

    float CurrentHealth = 100.f;
    float CurrentPosture = 100.f;
    bool bGuarding = false;
    bool bPostureBroken = false;
    bool bDefeated = false;
    double LastPostureHitTime = -1e9;
    double LastRecoveryEvalTime = 0.0;
};
