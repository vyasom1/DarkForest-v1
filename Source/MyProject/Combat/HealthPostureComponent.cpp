#include "Combat/HealthPostureComponent.h"

UHealthPostureComponent::UHealthPostureComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    CurrentHealth = MaxHealth;
    CurrentPosture = MaxPosture;
}

void UHealthPostureComponent::ApplyDamage(float Amount)
{
    if (bDefeated || Amount <= 0.f)
    {
        return;
    }

    CurrentHealth = FMath::Clamp(CurrentHealth - Amount, 0.f, MaxHealth);
    BroadcastVitals();

    if (CurrentHealth <= 0.f)
    {
        bDefeated = true;
        OnDefeated.Broadcast();
    }
}

void UHealthPostureComponent::ApplyPosture(float Amount)
{
    if (bDefeated)
    {
        return;
    }

    const float PreviousPosture = CurrentPosture;
    CurrentPosture = FMath::Clamp(CurrentPosture - FMath::Max(0.f, Amount), 0.f, MaxPosture);

    if (Amount > 0.f)
    {
        LastPostureHitTime = FPlatformTime::Seconds();
    }

    if (CurrentPosture <= 0.f && PreviousPosture > 0.f && !bPostureBroken)
    {
        bPostureBroken = true;
        OnPostureBroken.Broadcast();
    }
    else if (CurrentPosture > 0.f)
    {
        bPostureBroken = false;
    }

    BroadcastVitals();
}

void UHealthPostureComponent::SetGuarding(bool bEnabled)
{
    bGuarding = bEnabled;
    UpdatePostureRecovery();
}

float UHealthPostureComponent::GetHealth() const
{
    return CurrentHealth;
}

float UHealthPostureComponent::GetPosture()
{
    // Lazy evaluation keeps recovery correct in tests that never tick.
    UpdatePostureRecovery();
    return CurrentPosture;
}

bool UHealthPostureComponent::IsAlive() const
{
    return CurrentHealth > 0.f;
}

void UHealthPostureComponent::BroadcastVitals()
{
    OnVitalsChanged(CurrentHealth, CurrentPosture);
    OnVitalsUpdated.Broadcast(CurrentHealth, CurrentPosture);
}

void UHealthPostureComponent::UpdatePostureRecovery()
{
    if (bGuarding || bDefeated || CurrentPosture >= MaxPosture || PostureRecoveryRate <= 0.f)
    {
        return;
    }

    const double Now = FPlatformTime::Seconds();
    if (Now - LastPostureHitTime < PostureRecoveryDelaySeconds)
    {
        LastRecoveryEvalTime = Now;
        return;
    }

    const double DeltaTime = Now - LastRecoveryEvalTime;
    LastRecoveryEvalTime = Now;
    if (DeltaTime <= 0.0)
    {
        return;
    }

    const float PreviousPosture = CurrentPosture;
    CurrentPosture = FMath::Clamp(
        CurrentPosture + PostureRecoveryRate * static_cast<float>(DeltaTime),
        0.f, MaxPosture);

    if (CurrentPosture > 0.f && PreviousPosture <= 0.f)
    {
        bPostureBroken = false;
    }
    BroadcastVitals();
}
