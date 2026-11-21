#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Combat/HealthPostureComponent.h"
#include "LockOnComponent.generated.h"

/**
 * Soft-lock targeting: acquire the nearest valid target within range and
 * angle, keep the reference self-validating, and clear it automatically when
 * the target is destroyed or dies.
 *
 * A target is valid when it is a live actor, is not the owner itself, is
 * alive (via UHealthPostureComponent when present), and sits within
 * LockOnRange and LockOnAngleDegrees of the owner's forward vector. Range and
 * angle checks are skipped when the component has no owner or world, which
 * keeps the component usable in automation tests.
 */
UCLASS(Blueprintable, meta=(BlueprintSpawnableComponent))
class MYPROJECT_API ULockOnComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /** Try to lock onto Target. Returns false and keeps the current target for invalid targets. */
    UFUNCTION(BlueprintCallable, Category="Dark Forest")
    bool SetTarget(AActor* Target);

    /** Drop the current target and broadcast OnTargetChanged with nullptr. */
    UFUNCTION(BlueprintCallable, Category="Dark Forest")
    void ClearTarget();

    /** Current target, or nullptr after the target died, was destroyed, or left range. */
    UFUNCTION(BlueprintPure, Category="Dark Forest")
    AActor* GetCurrentTarget();

    /** True when a target is currently held. */
    UFUNCTION(BlueprintPure, Category="Dark Forest")
    bool IsTargetLocked() const;

    /** Fired with the new target whenever the lock changes, including clears (nullptr). */
    UFUNCTION(BlueprintImplementableEvent, Category="Dark Forest")
    void OnTargetChanged(AActor* Target);

public:
    /** Maximum distance at which a target can be acquired. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|LockOn", meta=(ClampMin="0"))
    float LockOnRange = 1400.f;

    /** Maximum angle off the owner's forward vector for acquisition. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|LockOn", meta=(ClampMin="0", ClampMax="360"))
    float LockOnAngleDegrees = 90.f;

protected:
    bool IsTargetValid(const AActor* Target) const;

private:
    TObjectPtr<AActor> CurrentTarget;
};
