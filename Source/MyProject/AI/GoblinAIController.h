#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Combat/HealthPostureComponent.h"
#include "GoblinAIController.generated.h"

/**
 * Goblin AI controller: validated pursuit targets, navigation pursuit, and
 * Behavior Tree integration ONLY when a valid BT asset is configured. Without
 * one it logs a warning and falls back to a simple chase, so the controller
 * initializes safely with and without a BT asset.
 */
UCLASS(Blueprintable)
class MYPROJECT_API AGoblinAIController : public AAIController
{
    GENERATED_BODY()

public:
    /**
     * Validate the target (alive, within perception range, not self) and start
     * pursuit via the configured Behavior Tree, or a simple chase when no BT
     * asset is set. Returns false for invalid targets.
     */
    UFUNCTION(BlueprintCallable, Category="Dark Forest")
    bool RequestPursuit(AActor* Target);

    /** Drop the pursued target and broadcast OnPursuitStarted with nullptr. */
    UFUNCTION(BlueprintCallable, Category="Dark Forest")
    void ClearPursuit();

    /** Actor currently being pursued, or nullptr. */
    UFUNCTION(BlueprintPure, Category="Dark Forest")
    AActor* GetPursuedTarget() const;

    /** Fired when pursuit starts; fires with nullptr when pursuit is cleared. */
    UFUNCTION(BlueprintImplementableEvent, Category="Dark Forest")
    void OnPursuitStarted(AActor* Target);

    /** Behavior Tree run on pursuit when configured; simple chase fallback when unset. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

    /** Maximum distance from the possessed pawn at which a target may be pursued. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Detection", meta=(ClampMin="0"))
    float PerceptionRange = 1500.f;

protected:
    bool IsTargetValid(const AActor* Target) const;

private:
    TObjectPtr<AActor> PursuedTarget;
};
