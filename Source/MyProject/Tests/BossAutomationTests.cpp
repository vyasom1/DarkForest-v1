/**
 * Goblin King and checkpoint automation tests.
 *
 * The segment/phase brain is actor-free and clock-based, so phase, deathblow,
 * wind-up, repeat, punish, and reset tests run without a world. Shrine and
 * hero-god tests use GWorld when available and skip gracefully otherwise.
 */
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Enemy/GoblinKingPhaseComponent.h"
#include "Enemy/GoblinKing.h"
#include "Enemy/GoblinKingTypes.h"
#include "World/ShrineCheckpoint.h"
#include "Player/DarkForestHero.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestBossActorSurvivesNonFinalDeathblows,
    "DarkForest.BossActorSurvivesNonFinalDeathblows",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestBossActorSurvivesNonFinalDeathblows::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    if (!TestNotNull(TEXT("World required"), World)) return false;
    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AGoblinKing* King = World->SpawnActor<AGoblinKing>(FVector(0, 0, 5000), FRotator::ZeroRotator, Params);
    if (!TestNotNull(TEXT("King fixture"), King)) return false;
    for (int32 Remaining = 2; Remaining >= 0; --Remaining)
    {
        King->ReceiveSwordHit(0.f, 10000.f, nullptr);
        King->ApplyDeathblow(nullptr);
        TestEqual(TEXT("One segment is removed"), King->GetSegmentsRemaining(), Remaining);
        TestEqual(TEXT("King dies only after the final segment"), King->IsAlive(), Remaining > 0);
    }
    King->Destroy();
    return true;
}

namespace
{
    UGoblinKingPhaseComponent* MakePhaseComponent()
    {
        return NewObject<UGoblinKingPhaseComponent>(GetTransientPackage());
    }

    void BreakPosture(UGoblinKingPhaseComponent* Component)
    {
        Component->ApplySegmentDamage(0.f, 10000.f);
    }
}

// ---------------------------------------------------------------------------
// Phase transitions: 3 -> 2 -> 1 segments gives phase 1 -> 2 -> 3 -> defeated.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestBossPhaseTransitions,
    "DarkForest.BossPhaseTransitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestBossPhaseTransitions::RunTest(const FString& Parameters)
{
    UGoblinKingPhaseComponent* Boss = MakePhaseComponent();
    TestEqual("starts with three segments", Boss->GetSegmentsRemaining(), 3);
    TestEqual("starts in phase 1", Boss->GetPhase(), 1);
    TestFalse("starts alive", Boss->IsDefeated());

    BreakPosture(Boss);
    TestTrue("first deathblow lands", Boss->TryDeathblow());
    TestEqual("one segment removed", Boss->GetSegmentsRemaining(), 2);
    TestEqual("phase advances to 2", Boss->GetPhase(), 2);

    BreakPosture(Boss);
    TestTrue("second deathblow lands", Boss->TryDeathblow());
    TestEqual("two segments removed", Boss->GetSegmentsRemaining(), 1);
    TestEqual("phase advances to 3", Boss->GetPhase(), 3);

    BreakPosture(Boss);
    TestTrue("final deathblow lands", Boss->TryDeathblow());
    TestEqual("no segments remain", Boss->GetSegmentsRemaining(), 0);
    TestTrue("encounter defeated on the final segment", Boss->IsDefeated());
    return true;
}

// ---------------------------------------------------------------------------
// Deathblow segment removal: only posture breaks allow it; one segment each.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestBossDeathblowSegmentRemoval,
    "DarkForest.BossDeathblowSegmentRemoval",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestBossDeathblowSegmentRemoval::RunTest(const FString& Parameters)
{
    UGoblinKingPhaseComponent* Boss = MakePhaseComponent();

    TestFalse("deathblow rejected while posture is intact", Boss->TryDeathblow());
    TestEqual("rejected deathblow removes nothing", Boss->GetSegmentsRemaining(), 3);

    // Depleting health without a deathblow must not remove a segment; it makes
    // the deathblow available by forcing the posture break.
    Boss->ApplySegmentDamage(99999.f, 0.f);
    TestEqual("health depletion alone keeps the segment", Boss->GetSegmentsRemaining(), 3);
    TestTrue("depleted segment exposes a posture break", Boss->IsPostureBroken());
    TestTrue("deathblow now lands", Boss->TryDeathblow());
    TestEqual("exactly one segment removed per deathblow", Boss->GetSegmentsRemaining(), 2);
    TestFalse("segment state reset after removal", Boss->IsPostureBroken());
    TestTrue("new segment is at full health",
        FMath::IsNearlyEqual(Boss->GetSegmentHealthFraction(), 1.f));
    return true;
}

// ---------------------------------------------------------------------------
// Wind-up floor: every attack, including zero-authored wind-ups, is >= 0.35 s.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestBossWindupFloor,
    "DarkForest.BossWindupFloor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestBossWindupFloor::RunTest(const FString& Parameters)
{
    UGoblinKingPhaseComponent* Boss = MakePhaseComponent();

    TArray<FGoblinKingPhase> Phases = Boss->GetPhases();
    TestTrue("default kit has three phases", Phases.Num() == 3);
    for (FGoblinKingPhase& Phase : Phases)
    {
        Phase.WindupMultiplier = 0.5f; // deliberately too fast
        for (FGoblinKingAttack& Attack : Phase.Attacks)
        {
            Attack.WindupSeconds = 0.f;
        }
    }
    Boss->SetPhases(Phases);

    for (int32 PhaseIndex = 0; PhaseIndex < Boss->GetPhaseCount(); ++PhaseIndex)
    {
        Boss->SetPhase(PhaseIndex + 1);
        const FGoblinKingPhase& Phase = Boss->GetPhases()[PhaseIndex];
        TestTrue("phase has attacks", Phase.Attacks.Num() > 0);
        for (int32 AttackIndex = 0; AttackIndex < Phase.Attacks.Num(); ++AttackIndex)
        {
            TestTrue("authored 0 s wind-up is floored to 0.35 s",
                Boss->GetEffectiveWindup(AttackIndex) >= 0.35f);
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Repeat limit: at most two consecutive repeats, and recovery after three.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestBossAttackRepeatLimit,
    "DarkForest.BossAttackRepeatLimit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestBossAttackRepeatLimit::RunTest(const FString& Parameters)
{
    UGoblinKingPhaseComponent* Boss = MakePhaseComponent();

    Boss->RegisterAttackStarted(0);
    Boss->RegisterAttackStarted(0);
    TestEqual("two repeats recorded", Boss->GetRepeatCount(), 2);
    TestFalse("a third consecutive repeat is disallowed", Boss->CanRepeatAttack(0));
    TestTrue("choosing next avoids the capped attack", Boss->ChooseNextAttackIndex() != 0);

    // Three consecutive sequences force a longer recovery.
    UGoblinKingPhaseComponent* Cadence = MakePhaseComponent();
    bool bForced = false;
    for (int32 Sequence = 0; Sequence < 3; ++Sequence)
    {
        Cadence->RegisterAttackStarted(Sequence % Cadence->GetPhases()[0].Attacks.Num());
        bForced = Cadence->RegisterAttackFinished();
        if (Sequence < 2)
        {
            TestFalse("no forced recovery before the third sequence", bForced);
        }
    }
    TestTrue("third sequence forces recovery", bForced);
    TestFalse("no attack starts during forced recovery", Cadence->CanStartAttack());
    TestEqual("consecutive counter reset after recovery", Cadence->GetConsecutiveAttacks(), 0);
    return true;
}

// ---------------------------------------------------------------------------
// Punish cadence: the phase-3 recovery opens a readable punish window.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestBossPunishCadence,
    "DarkForest.BossPunishCadence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestBossPunishCadence::RunTest(const FString& Parameters)
{
    UGoblinKingPhaseComponent* Boss = MakePhaseComponent();
    Boss->SetPhase(3);
    TestEqual("debug set-phase lands in phase 3", Boss->GetPhase(), 3);

    Boss->RegisterAttackStarted(0);
    Boss->RegisterAttackFinished();
    Boss->RegisterAttackStarted(1);
    Boss->RegisterAttackFinished();
    Boss->RegisterAttackStarted(2);
    TestTrue("third sequence forces a punishable recovery", Boss->RegisterAttackFinished());
    TestTrue("punish window opens", Boss->IsPunishWindowOpen());
    TestTrue("punish window increases posture damage", Boss->GetIncomingPostureMultiplier() > 1.f);

    // A short authored window closes on its own.
    UGoblinKingPhaseComponent* ShortWindow = MakePhaseComponent();
    TArray<FGoblinKingPhase> Phases = ShortWindow->GetPhases();
    Phases[2].PunishWindowSeconds = 0.02f;
    ShortWindow->SetPhases(Phases);
    ShortWindow->SetPhase(3);
    for (int32 Sequence = 0; Sequence < 3; ++Sequence)
    {
        ShortWindow->RegisterAttackStarted(Sequence % 2);
        ShortWindow->RegisterAttackFinished();
    }
    TestTrue("short punish window initially open", ShortWindow->IsPunishWindowOpen());
    FPlatformProcess::Sleep(0.05f);
    TestFalse("punish window closes after its duration", ShortWindow->IsPunishWindowOpen());
    TestTrue("posture multiplier returns to 1", FMath::IsNearlyEqual(ShortWindow->GetIncomingPostureMultiplier(), 1.f));
    return true;
}

// ---------------------------------------------------------------------------
// Reset behaviour: full vitals, phase 1, counters and punish state cleared.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestBossResetBehavior,
    "DarkForest.BossResetBehavior",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestBossResetBehavior::RunTest(const FString& Parameters)
{
    UGoblinKingPhaseComponent* Boss = MakePhaseComponent();

    BreakPosture(Boss);
    Boss->TryDeathblow();
    Boss->ApplySegmentDamage(20.f, 20.f);
    Boss->RegisterAttackStarted(0);
    Boss->RegisterAttackFinished();

    Boss->ResetEncounter();

    TestEqual("reset restores three segments", Boss->GetSegmentsRemaining(), 3);
    TestEqual("reset returns to phase 1", Boss->GetPhase(), 1);
    TestFalse("reset clears defeat", Boss->IsDefeated());
    TestFalse("reset clears posture break", Boss->IsPostureBroken());
    TestTrue("reset restores segment health",
        FMath::IsNearlyEqual(Boss->GetSegmentHealthFraction(), 1.f));
    TestTrue("reset restores posture", FMath::IsNearlyEqual(Boss->GetPostureFraction(), 1.f));
    TestEqual("reset clears the repeat counter", Boss->GetRepeatCount(), 0);
    TestEqual("reset clears consecutive attacks", Boss->GetConsecutiveAttacks(), 0);
    TestFalse("reset closes the punish window", Boss->IsPunishWindowOpen());
    TestTrue("reset allows an immediate attack", Boss->CanStartAttack());

    Boss->SetPhase(2);
    TestEqual("debug phase 2 has two segments", Boss->GetSegmentsRemaining(), 2);
    Boss->SetPhase(3);
    TestEqual("debug phase 3 has one segment", Boss->GetSegmentsRemaining(), 1);
    return true;
}

// ---------------------------------------------------------------------------
// Shrine checkpoint: activation is idempotent and captures the respawn point.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestShrineCheckpointActivatesOnce,
    "DarkForest.ShrineCheckpointActivatesOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestShrineCheckpointActivatesOnce::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    if (!World)
    {
        AddWarning(TEXT("No world available; skipping shrine checkpoint check."));
        return true;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AShrineCheckpoint* Shrine = World->SpawnActor<AShrineCheckpoint>(AShrineCheckpoint::StaticClass(),
        FVector(100.f, 100.f, 0.f), FRotator::ZeroRotator, SpawnParams);
    ADarkForestHero* Hero = World->SpawnActor<ADarkForestHero>(ADarkForestHero::StaticClass(),
        FVector(0.f, 0.f, 120.f), FRotator::ZeroRotator, SpawnParams);
    if (!Shrine || !Hero)
    {
        AddError(TEXT("Could not spawn shrine or hero for the checkpoint test."));
        return false;
    }

    TestFalse("shrine starts inactive", Shrine->IsActivated());
    TestTrue("first activation succeeds", Shrine->Activate(Hero));
    TestTrue("shrine is active after the first touch", Shrine->IsActivated());

    const FTransform Captured = Shrine->GetRespawnTransform();
    Shrine->SetActorLocation(FVector(900.f, 900.f, 0.f));
    TestTrue("second activation is idempotent", Shrine->Activate(Hero));
    TestTrue("respawn point stays at the first activation",
        Shrine->GetRespawnTransform().GetLocation().Equals(Captured.GetLocation(), 1.f));

    Shrine->Destroy();
    Hero->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// God mode: Aranya.God ignores damage and normal damage still defeats.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestHeroGodModeIgnoresDamage,
    "DarkForest.HeroGodModeIgnoresDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestHeroGodModeIgnoresDamage::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    if (!World)
    {
        AddWarning(TEXT("No world available; skipping god-mode check."));
        return true;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ADarkForestHero* Hero = World->SpawnActor<ADarkForestHero>(ADarkForestHero::StaticClass(),
        FVector(0.f, 0.f, 120.f), FRotator::ZeroRotator, SpawnParams);
    if (!Hero)
    {
        AddError(TEXT("Could not spawn the hero for the god-mode test."));
        return false;
    }

    Hero->SetGodMode(true);
    Hero->ReceiveMeleeHit(500.f, 100.f, nullptr);
    TestTrue("god mode ignores lethal damage", Hero->IsAlive());
    TestTrue("god mode leaves health full", FMath::IsNearlyEqual(Hero->GetHealthFraction(), 1.f));

    Hero->SetGodMode(false);
    Hero->ReceiveMeleeHit(500.f, 100.f, nullptr, true);
    TestFalse("normal unblockable damage defeats the hero", Hero->IsAlive());
    TestTrue("defeat leaves zero health", FMath::IsNearlyEqual(Hero->GetHealthFraction(), 0.f));

    Hero->Destroy();
    return true;
}
