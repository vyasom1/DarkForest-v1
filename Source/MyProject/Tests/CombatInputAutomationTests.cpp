#include "Misc/AutomationTest.h"
#include "Combat/AttackComboComponent.h"
#include "Combat/GuardComponent.h"
#include "Player/DarkForestHero.h"
#include "Enemy/DarkForestGoblin.h"
#include "Enemy/GoblinKing.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestEnemyAttackTurns,
    "DarkForest.EnemyAttackTurns",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestEnemyAttackTurns::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    if (!TestNotNull(TEXT("World required"), World)) return false;
    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FVector Origin(0, 0, 5000);
    ADarkForestHero* Hero = World->SpawnActor<ADarkForestHero>(Origin, FRotator::ZeroRotator, Params);
    ADarkForestGoblin* Goblin = World->SpawnActor<ADarkForestGoblin>(Origin + FVector(150, 0, 0), FRotator::ZeroRotator, Params);
    AGoblinKing* King = World->SpawnActor<AGoblinKing>(Origin + FVector(0, 200, 0), FRotator::ZeroRotator, Params);
    const bool bSpawned = Hero && Goblin && King;
    if (TestTrue(TEXT("Combat fixtures spawned"), bSpawned))
    {
        TestTrue(TEXT("Goblin starts a clear wind-up"), Goblin->TryBeginMeleeAttack(Hero));
        TestFalse(TEXT("King waits while the goblin attacks"), King->TryBeginMeleeAttack(Hero));
        Goblin->ReceiveDeflect(0.f);
        TestTrue(TEXT("A deflected goblin must recover"), Goblin->IsRecovering());
        TestTrue(TEXT("King can now take a turn"), King->TryBeginMeleeAttack(Hero));
        King->ReceiveDeflect(0.f);
        TestFalse(TEXT("Deflect cancels the King's tell"), King->IsTelegraphing());
        TestTrue(TEXT("King exposes a punish beat after the deflect"), King->IsPunishWindowOpen());
        TestFalse(TEXT("King cannot restart immediately"), King->TryBeginMeleeAttack(Hero));
    }
    if (King) King->Destroy();
    if (Goblin) Goblin->Destroy();
    if (Hero) Hero->Destroy();
    return bSpawned;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestEnemyMeleeCover,
    "DarkForest.EnemyMeleeCover",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestEnemyMeleeCover::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    if (!TestNotNull(TEXT("World required"), World)) return false;

    const FVector Origin(0, 0, 5000);
    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ADarkForestHero* Hero = World->SpawnActor<ADarkForestHero>(Origin, FRotator::ZeroRotator, Params);
    ADarkForestGoblin* Goblin = World->SpawnActor<ADarkForestGoblin>(Origin + FVector(150, 0, 0), FRotator::ZeroRotator, Params);
    AGoblinKing* King = World->SpawnActor<AGoblinKing>(Origin + FVector(200, 0, 0), FRotator::ZeroRotator, Params);
    AActor* Cover = World->SpawnActor<AActor>(Origin, FRotator::ZeroRotator, Params);
    const bool bSpawned = Hero && Goblin && King && Cover;
    if (TestTrue(TEXT("Combat fixtures spawned"), bSpawned))
    {
        UBoxComponent* Box = NewObject<UBoxComponent>(Cover);
        Cover->SetRootComponent(Box);
        Box->SetBoxExtent(FVector(5, 100, 100));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionResponseToAllChannels(ECR_Ignore);
        Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Box->SetWorldLocation(Origin + FVector(75, 0, 0));
        Box->RegisterComponent();

        Hero->InputDeflect();
        Hero->ReceiveMeleeHit(10.f, 20.f, Goblin);
        TestEqual(TEXT("Cover prevents a false deflect"), Goblin->GetPosture(), 0.f);
        Goblin->SetActorEnableCollision(false);
        Hero->ReceiveMeleeHit(10.f, 20.f, King, true);
        TestEqual(TEXT("Cover blocks even an unblockable attack"), Hero->GetHealthFraction(), 1.f);
        TestEqual(TEXT("Covered attacks leave hero posture unchanged"), Hero->GetPostureFraction(), 0.f);

        Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Hero->ReceiveMeleeHit(10.f, 20.f, King, true);
        TestTrue(TEXT("Clear attack damages health"), Hero->GetHealthFraction() < 1.f);
        TestTrue(TEXT("Clear attack damages posture"), Hero->GetPostureFraction() > 0.f);
    }
    if (Cover) Cover->Destroy();
    if (King) King->Destroy();
    if (Goblin) Goblin->Destroy();
    if (Hero) Hero->Destroy();
    return bSpawned;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestAttackBufferExpiry,
    "DarkForest.AttackBufferExpiry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestAttackBufferExpiry::RunTest(const FString& Parameters)
{
    UAttackComboComponent* Combo = NewObject<UAttackComboComponent>();
    Combo->InputBufferDuration = .15f;
    TestFalse("no input means no follow-up", Combo->ConsumeBufferedAttack(10.0));
    Combo->BufferAttackInput(10.0);
    TestFalse("early input expires before recovery", Combo->ConsumeBufferedAttack(10.3));
    Combo->BufferAttackInput(11.0);
    TestTrue("recent input is accepted", Combo->ConsumeBufferedAttack(11.1));
    TestFalse("one input is consumed exactly once", Combo->ConsumeBufferedAttack(11.1));
    Combo->BufferAttackInput(12.0);
    Combo->BufferAttackInput(12.1);
    TestTrue("new click refreshes one pending input", Combo->ConsumeBufferedAttack(12.2));
    TestFalse("multiple clicks do not queue multiple attacks", Combo->ConsumeBufferedAttack(12.2));
    Combo->BufferAttackInput(13.0);
    Combo->ResetCombo();
    TestFalse("reset clears pending input", Combo->ConsumeBufferedAttack(13.05));
    Combo->InputBufferDuration = 0.f;
    Combo->BufferAttackInput(14.0);
    TestFalse("zero duration disables buffering", Combo->ConsumeBufferedAttack(14.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestHeroAttackBuffer,
    "DarkForest.HeroAttackBuffer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestHeroAttackBuffer::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    if (!World) { AddError(TEXT("World required for hero input regression")); return false; }
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ADarkForestHero* Hero = World->SpawnActor<ADarkForestHero>(FVector(0,0,5000),FRotator::ZeroRotator,Params);
    if (!Hero) { AddError(TEXT("Hero spawn failed")); return false; }
    Hero->AttackComboComponent->ComboResetSeconds = 60.f;
    Hero->InputAttack();
    TestEqual("one click starts step one",Hero->AttackComboComponent->GetCurrentStep(),1);
    Hero->FinishAttack();
    TestFalse("single click recovers to idle",Hero->bAttacking);
    TestEqual("single click does not start a full combo",Hero->AttackComboComponent->GetCurrentStep(),1);
    Hero->InputAttack();
    Hero->AttackComboComponent->BufferAttackInput(World->GetTimeSeconds()-1.0);
    Hero->FinishAttack();
    TestFalse("expired click does not start another strike",Hero->bAttacking);
    Hero->InputAttack();
    Hero->InputAttack();
    Hero->FinishAttack();
    TestTrue("fresh second click chains at recovery",Hero->bAttacking);
    const int32 ChainedStep = Hero->AttackComboComponent->GetCurrentStep();
    Hero->FinishAttack();
    TestFalse("buffered click cannot trigger a second follow-up",Hero->bAttacking);
    TestEqual("chain stays at its last deliberate step",Hero->AttackComboComponent->GetCurrentStep(),ChainedStep);
    Hero->InputAttack();
    Hero->InputAttack();
    Hero->BeginGuard();
    TestFalse("guard does not cancel an active attack",Hero->GuardComponent->IsGuarding());
    Hero->FinishAttack();
    TestTrue("held guard begins when attack recovers",Hero->GuardComponent->IsGuarding());
    TestFalse("held defense wins over buffered attack",Hero->bAttacking);
    Hero->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestHeroGuardContinuity,
    "DarkForest.HeroGuardContinuity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestHeroGuardContinuity::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    if (!World) { AddError(TEXT("World required for guard regression")); return false; }
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ADarkForestHero* Hero = World->SpawnActor<ADarkForestHero>(FVector(0,0,5000),FRotator::ZeroRotator,Params);
    ADarkForestGoblin* Goblin = World->SpawnActor<ADarkForestGoblin>(FVector(100,0,5000),FRotator::ZeroRotator,Params);
    if (!Hero || !Goblin)
    {
        if (Hero) Hero->Destroy();
        if (Goblin) Goblin->Destroy();
        AddError(TEXT("Combat actor spawn failed"));
        return false;
    }
    Hero->BeginGuard();
    Hero->InputDeflect();
    Hero->ReceiveMeleeHit(10.f,10.f,Goblin);
    TestEqual("successful deflect preserves health",Hero->GetHealthFraction(),1.f);
    TestTrue("successful deflect increases attacker posture",Goblin->GetPosture()>0.f);
    TestTrue("held RMB still guards after successful deflect",Hero->GuardComponent->IsGuarding());
    Hero->EndGuard();
    TestFalse("releasing held guard clears guard",Hero->GuardComponent->IsGuarding());
    Hero->InputDeflect();
    Hero->EndGuard();
    TestTrue("RMB release does not cancel F window",Hero->bDeflecting);
    TestTrue("F window still guards during RMB release",Hero->GuardComponent->IsGuarding());
    Hero->EndDeflect();
    TestFalse("standalone deflect expires to unguarded",Hero->GuardComponent->IsGuarding());
    Hero->ReceiveMeleeHit(10.f,0.f,Goblin);
    TestTrue("late hit outside deflect damages hero",Hero->GetHealthFraction()<1.f);
    Hero->BeginGuard();
    Hero->ReceiveMeleeHit(0.f,Hero->MaxPosture,Goblin);
    TestTrue("posture breaks",Hero->bPostureBroken);
    TestFalse("posture break clears component guard",Hero->GuardComponent->IsGuarding());
    TestFalse("posture break clears held guard",Hero->bGuardInputHeld);
    Hero->Destroy();
    Goblin->Destroy();
    return true;
}
