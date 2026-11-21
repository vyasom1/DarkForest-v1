#include "Combat/GuardComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"

void UGuardComponent::SetGuarding(bool bEnabled)
{
    bGuarding = bEnabled;
    if (bEnabled)
    {
        WindowOpenTime = FPlatformTime::Seconds();
    }
}

bool UGuardComponent::IsGuarding() const
{
    return bGuarding;
}

bool UGuardComponent::IsDeflectWindowOpen() const
{
    if (!bGuarding)
    {
        return false;
    }
    return (FPlatformTime::Seconds() - WindowOpenTime) <= DeflectWindowSeconds;
}

bool UGuardComponent::TryDeflect()
{
    if (!IsDeflectWindowOpen())
    {
        return false;
    }
    OnDeflect();
    return true;
}

void UGuardComponent::PlayDeflectAnimation(const UAranyaAttackData* Data)
{
    if (!Data)
    {
        UE_LOG(LogAranyaCombat, Error, TEXT("[COMBAT] DeflectData is null"));
        return;
    }
    if (!Data->AnimSequence)
    {
        UE_LOG(LogAranyaCombat, Error, TEXT("[COMBAT] Deflect AnimSequence null"));
        return;
    }

    AActor* Owner = GetOwner();
    if (!Owner || !Owner->GetWorld())
    {
        return;
    }

    USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
    if (!Mesh || !Mesh->GetAnimInstance())
    {
        return;
    }

    Mesh->GetAnimInstance()->PlaySlotAnimationAsDynamicMontage(
        Data->AnimSequence,
        Data->SlotName,
        0.06f, // Defensive pose must arrive within the existing 0.25s window.
        0.10f, // Short recovery blend; playtest against locomotion next session.
        Data->PlayRate,
        1,     // LoopCount
        -1.f,  // BlendOutTriggerTime
        0.f);  // InTimeToStartMontageAt
}
