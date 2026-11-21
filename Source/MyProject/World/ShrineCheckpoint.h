#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShrineCheckpoint.generated.h"

class ADarkForestHero;
class USoundBase;
class USphereComponent;
class UPrimitiveComponent;

/**
 * Shrine checkpoint. The first overlap activates the shrine, plays the bell
 * once, registers it with the game mode, and stores the respawn transform.
 * Touching it again only restores vitals; it never spawns anything.
 */
UCLASS()
class MYPROJECT_API AShrineCheckpoint : public AActor
{
    GENERATED_BODY()

public:
    AShrineCheckpoint();
    virtual void BeginPlay() override;

    /** Activate (first time) or heal (later touches). Returns false for an invalid hero. */
    UFUNCTION(BlueprintCallable, Category="Checkpoint")
    bool Activate(ADarkForestHero* Hero);

    UFUNCTION(BlueprintPure, Category="Checkpoint")
    bool IsActivated() const { return bActivated; }

    UFUNCTION(BlueprintPure, Category="Checkpoint")
    FTransform GetRespawnTransform() const;

    UFUNCTION(BlueprintImplementableEvent, Category="Checkpoint")
    void OnShrineActivated(ADarkForestHero* Hero);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Checkpoint")
    TObjectPtr<USphereComponent> ActivationSphere;

    /** Editable bell slot; defaults to the imported placeholder. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint|Audio")
    TObjectPtr<USoundBase> ShrineBellSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint", meta=(ClampMin="0"))
    float ActivationRadius = 260.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
    bool bActivateOnOverlap = true;

    /** Restore the hero's health/posture on every touch (first touch always does). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
    bool bHealOnTouch = true;

protected:
    UFUNCTION()
    void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

private:
    bool bActivated = false;
    FTransform RespawnTransform;
};
