#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/DamageNumbers.h"
#include "DamageNumberWorldSubsystem.generated.h"

/**
 * World-scoped router for floating damage numbers.
 *
 * Combat code broadcasts through this subsystem so that ordinary goblins, the
 * boss and runtime-spawned enemies are covered without actor scanning. The
 * player's damage number overlay binds to OnDamageNumber once.
 */
UCLASS()
class MYPROJECT_API UDamageNumberWorldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    UDamageNumberWorldSubsystem();

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Broadcast a damage number event to every bound overlay. */
    UFUNCTION(BlueprintCallable, Category="Damage Numbers")
    void BroadcastDamageNumber(float Amount, FVector WorldLocation, EDamageNumberType DamageType);

    /** Native multicast delegate consumed by the player overlay. */
    UPROPERTY(BlueprintAssignable, Category="Damage Numbers")
    FDamageNumberEvent OnDamageNumber;

    /** Convenience accessor. */
    static UDamageNumberWorldSubsystem* Get(const UWorld* World);

    /** Convenience broadcast helper usable from non-subsystem call sites. */
    static void Broadcast(UWorld* World, float Amount, FVector WorldLocation, EDamageNumberType DamageType);
};