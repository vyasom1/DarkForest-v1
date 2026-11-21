#include "Combat/AttackComboComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"

UAttackComboComponent::UAttackComboComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    MaxComboSteps = FMath::Max(1, MaxComboSteps);
}

void UAttackComboComponent::AdvanceCombo()
{
    const double Now = FPlatformTime::Seconds();
    if (Now - LastAdvanceTime > ComboResetSeconds)
    {
        // Timeout without input: safely restart the chain at Hit1.
        CurrentStep = 0;
    }
    LastAdvanceTime = Now;

    // Exactly one step forward, wrapping after the final hit.
    CurrentStep = (CurrentStep % MaxComboSteps) + 1;
    OnComboStep(CurrentStep);
}

void UAttackComboComponent::ResetCombo()
{
    CurrentStep = 0;
    LastAdvanceTime = -1e9;
    ClearBufferedAttack();
}

void UAttackComboComponent::BufferAttackInput(double Now)
{
    bHasBufferedAttack = InputBufferDuration > 0.f && FMath::IsFinite(Now);
    BufferedAttackTime = Now;
}

bool UAttackComboComponent::ConsumeBufferedAttack(double Now)
{
    const double Age = Now - BufferedAttackTime;
    const bool bFresh = bHasBufferedAttack && InputBufferDuration > 0.f &&
        FMath::IsFinite(Age) && Age >= 0.0 && Age <= InputBufferDuration;
    ClearBufferedAttack();
    return bFresh;
}

void UAttackComboComponent::ClearBufferedAttack()
{
    bHasBufferedAttack = false;
    BufferedAttackTime = -1e9;
}

int32 UAttackComboComponent::GetCurrentStep() const
{
    return CurrentStep;
}

void UAttackComboComponent::SetComboChain(const TArray<UAranyaAttackData*>& Chain)
{
    ComboChain.Reset(Chain.Num());
    for (UAranyaAttackData* Data : Chain) ComboChain.Add(Data);
}

void UAttackComboComponent::PlayAttackMontage(const UAranyaAttackData* Data)
{
    if (!Data)
    {
        UE_LOG(LogAranyaCombat, Error, TEXT("[COMBAT] AttackData is null"));
        return;
    }
    if (!Data->AnimSequence)
    {
        UE_LOG(LogAranyaCombat, Error, TEXT("[COMBAT] AnimSequence null in %s"), *Data->GetName());
        return;
    }

    AActor* Owner = GetOwner();
    if (!Owner || !Owner->GetWorld())
    {
        UE_LOG(LogAranyaCombat, Error, TEXT("[COMBAT] No world for attack montage %s"), *Data->GetName());
        return;
    }

    USkeletalMeshComponent* Mesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
    if (!Mesh || !Mesh->GetAnimInstance())
    {
        UE_LOG(LogAranyaCombat, Error, TEXT("[COMBAT] No AnimInstance on mesh for attack %s"), *Data->GetName());
        return;
    }

    UAnimInstance* AnimInst = Mesh->GetAnimInstance();
    UAnimMontage* Montage = AnimInst->PlaySlotAnimationAsDynamicMontage(
        Data->AnimSequence,
        Data->SlotName,
        0.2f,  // BlendInTime
        0.2f,  // BlendOutTime
        Data->PlayRate,
        1,     // LoopCount: play the attack once
        -1.f,  // BlendOutTriggerTime: blend out at sequence end
        0.f);  // InTimeToStartMontageAt
    if (!Montage)
    {
        UE_LOG(LogAranyaCombat, Error, TEXT("[COMBAT] Failed to create dynamic montage for %s"), *Data->GetName());
        return;
    }

    UE_LOG(LogAranyaCombat, Log, TEXT("[COMBAT] Playing %s (Slot=%s, PlayRate=%.2f)"),
        *Data->AnimSequence->GetName(), *Data->SlotName.ToString(), Data->PlayRate);
}
