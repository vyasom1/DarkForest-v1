#include "AI/GoblinAIController.h"
#include "GameFramework/Pawn.h"

bool AGoblinAIController::RequestPursuit(AActor* Target)
{
    if (!IsTargetValid(Target))
    {
        return false;
    }

    PursuedTarget = Target;

    if (IsValid(BehaviorTreeAsset))
    {
        if (!RunBehaviorTree(BehaviorTreeAsset))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("GoblinAIController: RunBehaviorTree failed; falling back to simple chase."));
            MoveToActor(Target);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("GoblinAIController: no Behavior Tree configured; falling back to simple chase."));
        MoveToActor(Target);
    }

    OnPursuitStarted(Target);
    return true;
}

void AGoblinAIController::ClearPursuit()
{
    if (!PursuedTarget)
    {
        return;
    }
    PursuedTarget = nullptr;
    OnPursuitStarted(nullptr);
}

AActor* AGoblinAIController::GetPursuedTarget() const
{
    return PursuedTarget.Get();
}

bool AGoblinAIController::IsTargetValid(const AActor* Target) const
{
    if (!Target || Target == GetPawn())
    {
        return false;
    }
    if (!Target->IsValidLowLevel() || Target->IsPendingKillPending())
    {
        return false;
    }

    // Player/goblin must still be alive when they carry vitals.
    if (const UHealthPostureComponent* Vitals = Target->FindComponentByClass<UHealthPostureComponent>())
    {
        if (!Vitals->IsAlive())
        {
            return false;
        }
    }

    // Within perception range of the possessed pawn, when one exists.
    if (const APawn* PossessedPawn = GetPawn())
    {
        if (FVector::DistSquared(PossessedPawn->GetActorLocation(), Target->GetActorLocation())
            > FMath::Square(PerceptionRange))
        {
            return false;
        }
    }
    return true;
}
