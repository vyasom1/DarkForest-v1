#include "Combat/DamageNumberWorldSubsystem.h"

#include "Engine/World.h"

UDamageNumberWorldSubsystem::UDamageNumberWorldSubsystem()
{
}

void UDamageNumberWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

void UDamageNumberWorldSubsystem::Deinitialize()
{
    OnDamageNumber.Clear();
    Super::Deinitialize();
}

void UDamageNumberWorldSubsystem::BroadcastDamageNumber(float Amount, FVector WorldLocation, EDamageNumberType DamageType)
{
    // Suppress zero-damage health/posture emissions; deflect and deathblow carry
    // their own semantics and may legitimately carry a zero amount.
    if ((DamageType == EDamageNumberType::Health || DamageType == EDamageNumberType::Posture) && Amount <= 0.f)
    {
        return;
    }

    OnDamageNumber.Broadcast(Amount, WorldLocation, DamageType);
}

UDamageNumberWorldSubsystem* UDamageNumberWorldSubsystem::Get(const UWorld* World)
{
    return World ? World->GetSubsystem<UDamageNumberWorldSubsystem>() : nullptr;
}

void UDamageNumberWorldSubsystem::Broadcast(UWorld* World, float Amount, FVector WorldLocation, EDamageNumberType DamageType)
{
    if (UDamageNumberWorldSubsystem* Subsystem = Get(World))
    {
        Subsystem->BroadcastDamageNumber(Amount, WorldLocation, DamageType);
    }
}