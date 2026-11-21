/**
 * Dark Forest automation tests (combat, AI, and navigation).
 *
 * Component logic tests are world-less: the components are clock-based, so
 * they run without a ticking world. Navigation and actor-based lock-on tests
 * use GWorld when a level is available (editor Test Runner / Automation
 * commandlet) and skip gracefully with a warning when no world exists.
 */
#include "Tests/CombatAutomationTests.h"
#include "Misc/AutomationTest.h"
#include "UObject/Object.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Info.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "BehaviorTree/BehaviorTree.h"

#include "Combat/AttackComboComponent.h"
#include "Combat/GuardComponent.h"
#include "Combat/HealthPostureComponent.h"
#include "Combat/LockOnComponent.h"
#include "AI/GoblinAIController.h"

// ---------------------------------------------------------------------------
// ComboTransitionsCorrectly: Hit1 -> Hit2 -> Hit3 -> reset, no invalid jumps.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestComboTransitionsCorrectly,
    "DarkForest.ComboTransitionsCorrectly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestComboTransitionsCorrectly::RunTest(const FString& Parameters)
{
    UAttackComboComponent* Combo = NewObject<UAttackComboComponent>(GetTransientPackage());
    Combo->ComboResetSeconds = 60.f; // keep the chain alive for the walk test

    TestEqual("chain starts idle at step 0", Combo->GetCurrentStep(), 0);

    Combo->AdvanceCombo();
    TestEqual("first advance lands on Hit1", Combo->GetCurrentStep(), 1);
    Combo->AdvanceCombo();
    TestEqual("second advance lands on Hit2", Combo->GetCurrentStep(), 2);
    Combo->AdvanceCombo();
    TestEqual("third advance lands on Hit3", Combo->GetCurrentStep(), 3);
    Combo->AdvanceCombo();
    TestEqual("fourth advance wraps back to Hit1", Combo->GetCurrentStep(), 1);

    // Timeout reset: a fresh advance long after the last input restarts at Hit1.
    Combo->ComboResetSeconds = 0.05f;
    FPlatformProcess::Sleep(0.1f);
    Combo->AdvanceCombo();
    TestEqual("timeout resets the chain back to Hit1", Combo->GetCurrentStep(), 1);

    Combo->ResetCombo();
    TestEqual("ResetCombo returns to idle step 0", Combo->GetCurrentStep(), 0);
    return true;
}

// ---------------------------------------------------------------------------
// DeflectWindowClosesAfterWindow: guard opens the window for 0.4s, then closes.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestDeflectWindowClosesAfterWindow,
    "DarkForest.DeflectWindowClosesAfterWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestDeflectWindowClosesAfterWindow::RunTest(const FString& Parameters)
{
    UGuardComponent* Guard = NewObject<UGuardComponent>(GetTransientPackage());
    Guard->DeflectWindowSeconds = 0.4f;

    TestFalse("no deflect before guarding", Guard->IsDeflectWindowOpen());

    Guard->SetGuarding(true);
    TestTrue("guarding", Guard->IsGuarding());
    TestTrue("deflect window is open right after guard starts", Guard->IsDeflectWindowOpen());
    TestTrue("deflect succeeds inside the window", Guard->TryDeflect());

    FPlatformProcess::Sleep(0.45f);
    TestFalse("deflect window closed after 0.4s", Guard->IsDeflectWindowOpen());
    TestFalse("deflect fails outside the window", Guard->TryDeflect());
    TestTrue("still guarding after the window closed", Guard->IsGuarding());

    Guard->SetGuarding(false);
    TestFalse("guard released", Guard->IsGuarding());
    return true;
}

// ---------------------------------------------------------------------------
// HealthDamageClampsAndBroadcasts: clamp at 0, delegate fires with values.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestHealthDamageClampsAndBroadcasts,
    "DarkForest.HealthDamageClampsAndBroadcasts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestHealthDamageClampsAndBroadcasts::RunTest(const FString& Parameters)
{
    UHealthPostureComponent* Vitals = NewObject<UHealthPostureComponent>(GetTransientPackage());
    UDarkForestTestVitalsReceiver* Receiver = NewObject<UDarkForestTestVitalsReceiver>(GetTransientPackage());
    Vitals->OnVitalsUpdated.AddDynamic(Receiver, &UDarkForestTestVitalsReceiver::HandleVitals);
    Vitals->OnDefeated.AddDynamic(Receiver, &UDarkForestTestVitalsReceiver::HandleDefeated);

    Vitals->ApplyDamage(30.f);
    TestEqual("delegate fired once", Receiver->VitalsCount, 1);
    TestTrue("health reduced", FMath::IsNearlyEqual(Receiver->LastHealth, 70.f));
    TestTrue("posture untouched by health damage", FMath::IsNearlyEqual(Receiver->LastPosture, 100.f));

    // Overkill damage clamps at zero and fires defeat.
    Vitals->ApplyDamage(150.f);
    TestTrue("health clamped at zero", FMath::IsNearlyEqual(Vitals->GetHealth(), 0.f));
    TestTrue("delegate broadcast with clamped values", FMath::IsNearlyEqual(Receiver->LastHealth, 0.f));
    TestEqual("defeat fired exactly once", Receiver->DefeatCount, 1);
    TestFalse("defeated actor is not alive", Vitals->IsAlive());

    // Negative damage is ignored.
    const int32 Before = Receiver->VitalsCount;
    Vitals->ApplyDamage(-10.f);
    TestEqual("negative damage ignored", Receiver->VitalsCount, Before);
    return true;
}

// ---------------------------------------------------------------------------
// PostureRecoversAfterDelay: posture recovers after the delay once not guarding.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestPostureRecoversAfterDelay,
    "DarkForest.PostureRecoversAfterDelay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestPostureRecoversAfterDelay::RunTest(const FString& Parameters)
{
    UHealthPostureComponent* Vitals = NewObject<UHealthPostureComponent>(GetTransientPackage());
    Vitals->PostureRecoveryDelaySeconds = 0.05f;
    Vitals->PostureRecoveryRate = 1000.f; // recover fully within one test sleep

    Vitals->ApplyPosture(40.f);
    TestTrue("posture damaged toward the break", FMath::IsNearlyEqual(Vitals->GetPosture(), 60.f));

    // Guarding pauses recovery.
    Vitals->SetGuarding(true);
    FPlatformProcess::Sleep(0.1f);
    TestTrue("guarding pauses posture recovery", FMath::IsNearlyEqual(Vitals->GetPosture(), 60.f));

    // Releasing guard lets the delayed recovery run.
    Vitals->SetGuarding(false);
    FPlatformProcess::Sleep(0.1f);
    TestTrue("posture fully recovered after the delay", FMath::IsNearlyEqual(Vitals->GetPosture(), 100.f));

    // Clamping on the damage side: draining beyond zero clamps and breaks.
    UHealthPostureComponent* Breaker = NewObject<UHealthPostureComponent>(GetTransientPackage());
    UDarkForestTestVitalsReceiver* Receiver = NewObject<UDarkForestTestVitalsReceiver>(GetTransientPackage());
    Breaker->OnPostureBroken.AddDynamic(Receiver, &UDarkForestTestVitalsReceiver::HandleBroken);
    Breaker->ApplyPosture(150.f);
    TestTrue("posture clamped at zero", FMath::IsNearlyEqual(Breaker->GetPosture(), 0.f));
    TestEqual("posture break fired exactly once", Receiver->BreakCount, 1);
    return true;
}

// ---------------------------------------------------------------------------
// LockOnRejectsInvalidTargets: null, self, dead, and out-of-range targets.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestLockOnRejectsInvalidTargets,
    "DarkForest.LockOnRejectsInvalidTargets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestLockOnRejectsInvalidTargets::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    if (!World)
    {
        AddWarning(TEXT("No world available; skipping actor-based lock-on checks."));
        return true;
    }

    AActor* Owner = World->SpawnActor<AActor>();
    USceneComponent* OwnerRoot = NewObject<USceneComponent>(Owner, TEXT("Root"));
    Owner->SetRootComponent(OwnerRoot);
    OwnerRoot->RegisterComponent();
    ULockOnComponent* Lock = NewObject<ULockOnComponent>(Owner);
    TestTrue("component owner resolved", Lock->GetOwner() == Owner);

    TestFalse("null target rejected", Lock->SetTarget(nullptr));
    TestFalse("self target rejected", Lock->SetTarget(Owner));
    TestFalse("nothing locked after invalid requests", Lock->IsTargetLocked());

    // Dead target: carry vitals with zero health.
    AActor* DeadTarget = World->SpawnActor<AActor>();
    UHealthPostureComponent* DeadVitals = NewObject<UHealthPostureComponent>(DeadTarget);
    DeadTarget->AddInstanceComponent(DeadVitals);
    DeadVitals->ApplyDamage(1000.f);
    TestFalse("dead target rejected", Lock->SetTarget(DeadTarget));

    // Out-of-range target: 200cm away with a 100cm range.
    Lock->LockOnRange = 100.f;
    AActor* FarTarget = World->SpawnActor<AActor>();
    USceneComponent* FarRoot = NewObject<USceneComponent>(FarTarget, TEXT("Root"));
    FarTarget->SetRootComponent(FarRoot);
    FarRoot->RegisterComponent();
    FarTarget->SetActorLocation(FVector(200.f, 0.f, 0.f));
    TestFalse("out-of-range target rejected", Lock->SetTarget(FarTarget));

    // In-range, alive, non-self target is accepted.
    AActor* NearTarget = World->SpawnActor<AActor>();
    USceneComponent* NearRoot = NewObject<USceneComponent>(NearTarget, TEXT("Root"));
    NearTarget->SetRootComponent(NearRoot);
    NearRoot->RegisterComponent();
    NearTarget->SetActorLocation(FVector(60.f, 0.f, 0.f));
    TestTrue("valid in-range target accepted", Lock->SetTarget(NearTarget));
    TestTrue("current target is the accepted actor", Lock->GetCurrentTarget() == NearTarget);

    FarTarget->Destroy();
    NearTarget->Destroy();
    DeadTarget->Destroy();
    Owner->Destroy();
    return true;
}

// ---------------------------------------------------------------------------
// LockOnClearsDestroyedTargets: lock clears when the target is destroyed.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestLockOnClearsDestroyedTargets,
    "DarkForest.LockOnClearsDestroyedTargets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestLockOnClearsDestroyedTargets::RunTest(const FString& Parameters)
{
    ULockOnComponent* Lock = NewObject<ULockOnComponent>(GetTransientPackage());
    AActor* Target = NewObject<AActor>(GetTransientPackage());

    TestTrue("fresh actor accepted as target", Lock->SetTarget(Target));
    TestTrue("target locked", Lock->IsTargetLocked());

    // OnTargetChanged is a BlueprintImplementableEvent; observe the clear via
    // the self-validating getter and the locked flag instead. The object is
    // not rooted, so MarkPendingKill is legal and GC may collect it any time
    // after this call — every assertion below holds in both cases.
    Target->MarkPendingKill();
    TestTrue("destroyed target no longer returned", Lock->GetCurrentTarget() == nullptr);
    TestFalse("lock cleared after target destruction", Lock->IsTargetLocked());

    TestFalse("re-locking the destroyed target is rejected", Lock->SetTarget(Target));
    return true;
}

// ---------------------------------------------------------------------------
// GoblinControllerInitializesSafely: with AND without a BT asset configured.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestGoblinControllerInitializesSafely,
    "DarkForest.GoblinControllerInitializesSafely",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestGoblinControllerInitializesSafely::RunTest(const FString& Parameters)
{
    // Without a Behavior Tree asset: must construct and reject invalid targets safely.
    AGoblinAIController* Controller = NewObject<AGoblinAIController>(GetTransientPackage());
    TestFalse("null target rejected without BT", Controller->RequestPursuit(nullptr));
    TestTrue("controller alive after invalid request", IsValid(Controller));

    // With a Behavior Tree asset set: same safe rejection, no BT run.
    UBehaviorTree* Tree = NewObject<UBehaviorTree>(GetTransientPackage());
    Controller->BehaviorTreeAsset = Tree;
    TestFalse("null target rejected with BT configured", Controller->RequestPursuit(nullptr));
    TestTrue("controller alive after invalid request with BT", IsValid(Controller));
    TestTrue("no pursuit started for invalid targets", Controller->GetPursuedTarget() == nullptr);
    return true;
}

// ---------------------------------------------------------------------------
// PlayerStartProjectsToNavigation: PlayerStart projects onto the navmesh.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestPlayerStartProjectsToNavigation,
    "DarkForest.PlayerStartProjectsToNavigation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestPlayerStartProjectsToNavigation::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    if (!World)
    {
        AddWarning(TEXT("No world available; skipping nav projection check."));
        return true;
    }

    APlayerStart* Start = nullptr;
    TArray<AActor*> Starts;
    UGameplayStatics::GetAllActorsOfClass(World, APlayerStart::StaticClass(), Starts);
    if (Starts.Num() > 0)
    {
        Start = Cast<APlayerStart>(Starts[0]);
    }
    else
    {
        Start = World->SpawnActor<APlayerStart>();
    }
    if (!Start)
    {
        AddError(TEXT("No PlayerStart available for the navigation projection test."));
        return false;
    }

    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
    if (!NavSys)
    {
        AddError(TEXT("No navigation system in the current world."));
        return false;
    }

    const FVector Original = Start->GetActorLocation();
    FNavLocation Projected;
    const bool bProjected = NavSys->ProjectPointToNavigation(
        Original, Projected, FVector(500.f, 500.f, 1000.f));

    TestTrue("PlayerStart projected onto navigation", bProjected);
    TestTrue("projection stays near the PlayerStart",
        FVector::Dist(FVector(Projected), Original) < 1000.f);
    return true;
}
