#pragma once

#include "CoreMinimal.h"
#include "AI/GoblinAIController.h"
#include "DarkForestGoblinController.generated.h"

class ADarkForestGoblin;
class ADarkForestHero;

UCLASS()
class MYPROJECT_API ADarkForestGoblinController : public AGoblinAIController
{
    GENERATED_BODY()
public:
    ADarkForestGoblinController();
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="AI|Detection", meta=(ClampMin="0"))
    float DetectionRadius = 1500.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="AI|Patrol", meta=(ClampMin="0"))
    float PatrolRadius = 650.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="AI|Patrol", meta=(ClampMin="0"))
    float DecisionIntervalSeconds = .4f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="AI|Combat", meta=(ClampMin="0"))
    float StoppingDistance = 130.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="AI|Combat", meta=(ClampMin="0"))
    float StrafeDistance = 230.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="AI|Combat", meta=(ClampMin="0"))
    float StrafeOffset = 145.f;

private:
    void Decide();
    UPROPERTY()
    TObjectPtr<ADarkForestGoblin> Goblin;
    FVector PatrolOrigin = FVector::ZeroVector;
    FTimerHandle DecisionTimer;
    bool bStrafeLeft = false;
    double NextMoveRequestTime = 0.0;
};
