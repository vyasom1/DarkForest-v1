#include "Enemy/GoblinKingPhaseComponent.h"

namespace
{
    FGoblinKingAttack MakeAttack(
        const TCHAR* Id,
        const TCHAR* Name,
        float Damage,
        float PostureDamage,
        float Windup,
        float Range,
        const FLinearColor& TellColor,
        int32 ChainHits = 1,
        float ChainInterval = 0.5f,
        bool bUnblockable = false,
        bool bJumpable = false)
    {
        FGoblinKingAttack Attack;
        Attack.AttackId = FName(Id);
        Attack.DisplayName = FText::FromString(Name);
        Attack.Damage = Damage;
        Attack.PostureDamage = PostureDamage;
        Attack.WindupSeconds = Windup;
        Attack.Range = Range;
        Attack.TellColor = TellColor;
        Attack.ChainHits = ChainHits;
        Attack.ChainIntervalSeconds = ChainInterval;
        Attack.bUnblockable = bUnblockable;
        Attack.bJumpable = bJumpable;
        return Attack;
    }

    TArray<FGoblinKingPhase> MakeDefaultPhases()
    {
        const FLinearColor Orange(1.f, 0.5f, 0.1f, 1.f);
        const FLinearColor Amber(1.f, 0.72f, 0.15f, 1.f);
        const FLinearColor Crimson(1.f, 0.15f, 0.12f, 1.f);
        TArray<FGoblinKingPhase> Phases;
        // Keep the same four answers across phases. Later phases shorten the openings.
        for (int32 PhaseNumber = 1; PhaseNumber <= 3; ++PhaseNumber)
        {
            FGoblinKingPhase& Phase = Phases.AddDefaulted_GetRef();
            Phase.PhaseNumber = PhaseNumber;
            Phase.RecoverySeconds = 1.35f - .15f * (PhaseNumber - 1);
            Phase.PunishWindowSeconds = Phase.RecoverySeconds;
            Phase.WindupMultiplier = 1.f - .07f * (PhaseNumber - 1);
            Phase.Attacks.Add(MakeAttack(TEXT("King.Slash"), TEXT("Slash"), 18.f, 20.f, .65f, 300.f, Orange));
            Phase.Attacks.Add(MakeAttack(TEXT("King.DelayedHeavy"), TEXT("Delayed heavy"), 26.f, 29.f, 1.12f, 275.f, Amber));
            Phase.Attacks.Add(MakeAttack(TEXT("King.DoubleSlash"), TEXT("Double slash"), 12.f, 13.f, .65f, 295.f, Orange, 2, .62f));
            Phase.Attacks.Add(MakeAttack(TEXT("King.LowSweep"), TEXT("Low sweep"), 25.f, 24.f, 1.05f, 330.f, Crimson, 1, .5f, true, true));
        }

        return Phases;
    }
}

UGoblinKingPhaseComponent::UGoblinKingPhaseComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    Phases = MakeDefaultPhases();
    ResetSegmentState();
}

void UGoblinKingPhaseComponent::EnsureDefaultPhases()
{
    if (Phases.Num() == 0)
    {
        Phases = MakeDefaultPhases();
    }
    ResetEncounter();
}

void UGoblinKingPhaseComponent::SetPhases(const TArray<FGoblinKingPhase>& InPhases)
{
    Phases = InPhases;
    if (Phases.Num() == 0)
    {
        Phases = MakeDefaultPhases();
    }
    TotalSegments = FMath::Max(1, TotalSegments);
    ResetEncounter();
}

void UGoblinKingPhaseComponent::ResetEncounter()
{
    Phase = 1;
    SegmentsRemaining = TotalSegments;
    bDefeated = false;
    LastAttackIndex = INDEX_NONE;
    RepeatCount = 0;
    ConsecutiveAttacks = 0;
    NextAttackCursor = 0;
    NextAttackAllowedTime = 0.0;
    PunishWindowEndTime = 0.0;
    ResetSegmentState();
}

void UGoblinKingPhaseComponent::SetPhase(int32 NewPhase)
{
    if (Phases.Num() == 0)
    {
        EnsureDefaultPhases();
    }
    if (Phases.Num() == 0)
    {
        return;
    }

    Phase = FMath::Clamp(NewPhase, 1, Phases.Num());
    SegmentsRemaining = FMath::Clamp(TotalSegments - Phase + 1, 1, TotalSegments);
    bDefeated = false;
    LastAttackIndex = INDEX_NONE;
    RepeatCount = 0;
    ConsecutiveAttacks = 0;
    NextAttackCursor = 0;
    NextAttackAllowedTime = 0.0;
    PunishWindowEndTime = 0.0;
    ResetSegmentState();
    OnPhaseChanged.Broadcast(Phase);
}

void UGoblinKingPhaseComponent::ApplySegmentDamage(float HealthDamage, float PostureDamage)
{
    if (bDefeated)
    {
        return;
    }

    if (HealthDamage > 0.f)
    {
        SegmentHealth = FMath::Clamp(SegmentHealth - HealthDamage, 0.f, SegmentMaxHealth);
    }

    if (PostureDamage > 0.f && !bPostureBroken)
    {
        CurrentPosture = FMath::Clamp(CurrentPosture - PostureDamage, 0.f, SegmentMaxPosture);
    }

    // A fully depleted segment can only be finished with a deathblow; make
    // sure that deathblow is available by forcing the posture break.
    const bool bShouldBreak = (CurrentPosture <= 0.f) || (SegmentHealth <= 0.f);
    if (bShouldBreak && !bPostureBroken)
    {
        bPostureBroken = true;
        CurrentPosture = 0.f;
        OnPostureBroken.Broadcast();
    }
}

bool UGoblinKingPhaseComponent::TryDeathblow()
{
    if (bDefeated || !bPostureBroken || SegmentsRemaining <= 0)
    {
        return false;
    }

    --SegmentsRemaining;
    if (SegmentsRemaining <= 0)
    {
        SegmentsRemaining = 0;
        bPostureBroken = false;
        bDefeated = true;
        PunishWindowEndTime = 0.0;
        OnDefeated.Broadcast();
        return true;
    }

    ResetSegmentState();
    Phase = FMath::Clamp(TotalSegments - SegmentsRemaining + 1, 1, FMath::Max(1, Phases.Num()));
    LastAttackIndex = INDEX_NONE;
    RepeatCount = 0;
    ConsecutiveAttacks = 0;
    NextAttackAllowedTime = 0.0;
    PunishWindowEndTime = 0.0;
    BroadcastSegmentRemoved();
    OnPhaseChanged.Broadcast(Phase);
    return true;
}

void UGoblinKingPhaseComponent::RecoverPosture()
{
    if (bDefeated)
    {
        return;
    }
    bPostureBroken = false;
    CurrentPosture = FMath::Clamp(SegmentMaxPosture * PostureRecoveryFraction, 0.f, SegmentMaxPosture);
    OnPostureRecovered.Broadcast();
}

int32 UGoblinKingPhaseComponent::ChooseNextAttackIndex()
{
    const FGoblinKingPhase* Current = GetCurrentPhase();
    if (!Current || Current->Attacks.Num() == 0)
    {
        return INDEX_NONE;
    }

    const int32 Count = Current->Attacks.Num();
    if (Count == 1)
    {
        return 0;
    }

    int32 Candidate = NextAttackCursor % Count;
    if (Candidate < 0)
    {
        Candidate = 0;
    }

    // Safety net for authored/extended kits: never allow a third consecutive
    // repeat of the same attack.
    if (Candidate == LastAttackIndex && !CanRepeatAttack(Candidate))
    {
        Candidate = (Candidate + 1) % Count;
    }

    NextAttackCursor = (Candidate + 1) % Count;
    return Candidate;
}

bool UGoblinKingPhaseComponent::CanStartAttack() const
{
    return !bDefeated && !bPostureBroken && FPlatformTime::Seconds() >= NextAttackAllowedTime;
}

void UGoblinKingPhaseComponent::RegisterAttackStarted(int32 AttackIndex)
{
    if (AttackIndex == LastAttackIndex)
    {
        ++RepeatCount;
    }
    else
    {
        RepeatCount = 1;
    }
    LastAttackIndex = AttackIndex;
    ++ConsecutiveAttacks;

    const FGoblinKingPhase* Current = GetCurrentPhase();
    if (Current && Current->Attacks.Num() > 0)
    {
        NextAttackCursor = (FMath::Max(0, AttackIndex) + 1) % Current->Attacks.Num();
    }
}

bool UGoblinKingPhaseComponent::RegisterAttackFinished()
{
    const FGoblinKingPhase* Current = GetCurrentPhase();
    const float Recovery = Current ? FMath::Max(0.f, Current->RecoverySeconds) : 1.f;

    const bool bForcedRecovery = ConsecutiveAttacks >= MaxConsecutiveAttacks;
    if (bForcedRecovery)
    {
        // The third consecutive sequence is followed by a longer, readable
        // recovery instead of immediate pressure.
        NextAttackAllowedTime = FPlatformTime::Seconds() + Recovery * 1.5f;
        ConsecutiveAttacks = 0;
        RepeatCount = 0;
    }
    else
    {
        NextAttackAllowedTime = FPlatformTime::Seconds() + Recovery;
    }
    if (Current && Current->PunishWindowSeconds > 0.f)
        OpenPunishWindow(FMath::Min(Current->PunishWindowSeconds, Recovery * (bForcedRecovery ? 1.5f : 1.f)));
    return bForcedRecovery;
}

void UGoblinKingPhaseComponent::RegisterRecoveryFinished()
{
    ConsecutiveAttacks = 0;
    RepeatCount = 0;
    NextAttackAllowedTime = 0.0;
}

void UGoblinKingPhaseComponent::OpenPunishWindow(float Seconds)
{
    if (Seconds <= 0.f || bDefeated)
    {
        return;
    }
    PunishWindowEndTime = FPlatformTime::Seconds() + Seconds;
}

bool UGoblinKingPhaseComponent::IsPunishWindowOpen() const
{
    return !bDefeated && FPlatformTime::Seconds() < PunishWindowEndTime;
}

float UGoblinKingPhaseComponent::GetIncomingPostureMultiplier() const
{
    return IsPunishWindowOpen() ? 1.5f : 1.f;
}

float UGoblinKingPhaseComponent::GetEffectiveWindup(int32 AttackIndex) const
{
    const FGoblinKingAttack* Attack = GetAttack(AttackIndex);
    if (!Attack)
    {
        return MinWindupSeconds;
    }
    const FGoblinKingPhase* Current = GetCurrentPhase();
    const float Multiplier = Current ? FMath::Max(0.1f, Current->WindupMultiplier) : 1.f;
    return FMath::Max(FMath::Max(0.35f, MinWindupSeconds), Attack->WindupSeconds * Multiplier);
}

const FGoblinKingAttack* UGoblinKingPhaseComponent::GetAttack(int32 AttackIndex) const
{
    const FGoblinKingPhase* Current = GetCurrentPhase();
    if (!Current || !Current->Attacks.IsValidIndex(AttackIndex))
    {
        return nullptr;
    }
    return &Current->Attacks[AttackIndex];
}

bool UGoblinKingPhaseComponent::CanRepeatAttack(int32 AttackIndex) const
{
    return AttackIndex != LastAttackIndex || RepeatCount < MaxAttackRepeats;
}

void UGoblinKingPhaseComponent::ResetSegmentState()
{
    SegmentHealth = SegmentMaxHealth;
    CurrentPosture = SegmentMaxPosture;
    bPostureBroken = false;
}

void UGoblinKingPhaseComponent::BroadcastSegmentRemoved()
{
    OnSegmentRemoved.Broadcast(SegmentsRemaining);
}

const FGoblinKingPhase* UGoblinKingPhaseComponent::GetCurrentPhase() const
{
    const int32 Index = Phase - 1;
    return Phases.IsValidIndex(Index) ? &Phases[Index] : nullptr;
}
