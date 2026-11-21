#include "Combat/LockOnComponent.h"
#include "Enemy/DarkForestGoblin.h"

bool ULockOnComponent::SetTarget(AActor* Target)
{
    if (!IsTargetValid(Target))
    {
        return false;
    }

    CurrentTarget = Target;
    OnTargetChanged(Target);
    return true;
}

void ULockOnComponent::ClearTarget()
{
    if (!CurrentTarget)
    {
        return;
    }
    CurrentTarget = nullptr;
    OnTargetChanged(nullptr);
}

AActor* ULockOnComponent::GetCurrentTarget()
{
    if (CurrentTarget && !IsTargetValid(CurrentTarget))
    {
        // Target was destroyed or died: drop it and notify listeners.
        ClearTarget();
    }
    return CurrentTarget.Get();
}

bool ULockOnComponent::IsTargetLocked() const
{
    return CurrentTarget != nullptr;
}

bool ULockOnComponent::IsTargetValid(const AActor* Target) const
{
    if (!Target || Target == GetOwner())
    {
        return false;
    }
    if (!Target->IsValidLowLevel() || Target->IsPendingKillPending())
    {
        return false;
    }

    // These enemies own native vitals, not a UHealthPostureComponent.
    if (const ADarkForestGoblin* Goblin = Cast<ADarkForestGoblin>(Target))
        if (!Goblin->IsAlive() || Goblin->IsHidden()) return false;

    // Preserve support for other actors using the generic vitals component.
    if (const UHealthPostureComponent* Vitals = Target->FindComponentByClass<UHealthPostureComponent>())
    {
        if (!Vitals->IsAlive())
        {
            return false;
        }
    }

    if (const AActor* Owner = GetOwner())
    {
        if (const UWorld* World = Owner->GetWorld())
        {
            const FVector OwnerLocation = Owner->GetActorLocation();
            const FVector TargetLocation = Target->GetActorLocation();
            if (FVector::DistSquared2D(OwnerLocation, TargetLocation) > FMath::Square(LockOnRange))
            {
                return false;
            }
            const FVector Direction = (TargetLocation - OwnerLocation).GetSafeNormal2D();
            const float DotThreshold = FMath::Cos(FMath::DegreesToRadians(LockOnAngleDegrees));
            if (Owner->GetActorForwardVector().GetSafeNormal2D().Dot(Direction) < DotThreshold)
            {
                return false;
            }
        }
    }
    return true;
}
