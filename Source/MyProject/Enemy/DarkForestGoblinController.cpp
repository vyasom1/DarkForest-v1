#include "Enemy/DarkForestGoblinController.h"
#include "AI/GoblinAIController.h"
#include "Enemy/DarkForestGoblin.h"
#include "Player/DarkForestHero.h"
#include "Core/DarkForestLog.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"

ADarkForestGoblinController::ADarkForestGoblinController()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ADarkForestGoblinController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    Goblin = Cast<ADarkForestGoblin>(InPawn);
    if (!IsValid(Goblin) || !GetWorld()) return;
    PatrolOrigin = Goblin->GetActorLocation();
    NextMoveRequestTime = 0.0;
    GetWorldTimerManager().SetTimer(DecisionTimer, this, &ADarkForestGoblinController::Decide,
        DecisionIntervalSeconds, true);
    Decide();
}

void ADarkForestGoblinController::OnUnPossess()
{
    GetWorldTimerManager().ClearTimer(DecisionTimer);
    Goblin = nullptr;
    ClearPursuit();
    Super::OnUnPossess();
}

void ADarkForestGoblinController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(DecisionTimer);
    Super::EndPlay(EndPlayReason);
}

void ADarkForestGoblinController::Decide()
{
    if (!GetWorld() || !IsValid(Goblin) || !Goblin->IsAlive()) return;
    if (Goblin->IsHidden() || Goblin->IsPostureBroken() || Goblin->IsTelegraphing() || Goblin->IsRecovering())
    {
        StopMovement();
        return;
    }
    ADarkForestHero* Player = Cast<ADarkForestHero>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
    if (IsTargetValid(Player) && Player->IsAlive())
    {
        const float Distance = FVector::Dist2D(Goblin->GetActorLocation(), Player->GetActorLocation());
        if (Distance <= DetectionRadius)
        {
            if (Distance <= Goblin->GetMeleeRange() && Goblin->TryBeginMeleeAttack(Player)) return;
            if (GetMoveStatus() == EPathFollowingStatus::Moving ||
                GetWorld()->GetTimeSeconds() < NextMoveRequestTime) return;

            NextMoveRequestTime = GetWorld()->GetTimeSeconds() + 1.0;
            if (Distance <= StrafeDistance)
            {
                const FVector ToPlayer = (Player->GetActorLocation() - Goblin->GetActorLocation()).GetSafeNormal2D();
                const FVector Side = FVector::CrossProduct(ToPlayer, FVector::UpVector) *
                    (bStrafeLeft ? -StrafeOffset : StrafeOffset);
                bStrafeLeft = !bStrafeLeft;
                const FVector Desired = Player->GetActorLocation() - ToPlayer * StoppingDistance + Side;
                if (MoveToLocation(Desired, 35.f, false, true, true, false, nullptr, false)
                    != EPathFollowingRequestResult::Failed) return;
            }
            if (BehaviorTreeAsset)
            {
                if (GetPursuedTarget() != Player) RequestPursuit(Player);
            }
            else MoveToActor(Player, StoppingDistance, false, true, false, nullptr, false);
            return;
        }
    }
    if (GetPursuedTarget()) ClearPursuit();
    if (GetMoveStatus() == EPathFollowingStatus::Moving ||
        GetWorld()->GetTimeSeconds() < NextMoveRequestTime) return;
    NextMoveRequestTime = GetWorld()->GetTimeSeconds() + 2.0;
    UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!Nav)
    {
        return;
    }
    FNavLocation PatrolPoint;
    if (Nav->GetRandomReachablePointInRadius(PatrolOrigin, PatrolRadius, PatrolPoint))
    {
        MoveToLocation(PatrolPoint.Location, 55.f);
    }
}
