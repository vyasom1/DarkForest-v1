#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DarkForestGameMode.generated.h"

class ADarkForestHero;
class AGoblinKing;
class AShrineCheckpoint;
class ADarkForestStory;
class UDemoFrontendState;

UCLASS()
class MYPROJECT_API ADarkForestGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ADarkForestGameMode();
    virtual void BeginPlay() override;
    bool BeginStory(ADarkForestHero* Hero, UDemoFrontendState* State);

    UFUNCTION(BlueprintPure, Category="Story")
    ADarkForestStory* GetStory() const { return Story; }

    /** First shrine to activate owns the respawn point. Later shrines are ignored. */
    UFUNCTION(BlueprintCallable, Category="Checkpoint")
    void RegisterShrine(AShrineCheckpoint* Shrine);

    UFUNCTION(BlueprintPure, Category="Checkpoint")
    AShrineCheckpoint* GetActiveShrine() const { return ActiveShrine; }

    /** Tracks the single active boss; duplicate bosses are removed. */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void RegisterBoss(AGoblinKing* Boss);

    UFUNCTION(BlueprintPure, Category="Boss")
    AGoblinKing* GetActiveBoss() const { return ActiveBoss; }

    UFUNCTION(BlueprintCallable, Category="Boss")
    void NotifyBossDefeated(AGoblinKing* Boss);

    UFUNCTION(BlueprintPure, Category="Boss")
    bool IsBossDefeated() const { return bBossDefeated; }

    /** Restore every King in the level to full three-segment state. */
    UFUNCTION(BlueprintCallable, Category="Boss")
    void ResetBossEncounter();

    /** Schedule a shrine respawn; no-op (with a warning) when no shrine is active. */
    UFUNCTION(BlueprintCallable, Category="Checkpoint")
    void HandleHeroDefeated(ADarkForestHero* Hero);

    UFUNCTION(BlueprintCallable, Category="Checkpoint")
    void RespawnHero();

    /** Seconds between defeating the hero and the shrine respawn. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Checkpoint", meta=(ClampMin="0"))
    float RespawnDelaySeconds = 2.f;

    /** Optional class used by tools/commands to spawn the boss. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Boss")
    TSubclassOf<AGoblinKing> BossClass;

private:
    UPROPERTY()
    TObjectPtr<ADarkForestStory> Story;
    UPROPERTY()
    TObjectPtr<AShrineCheckpoint> ActiveShrine;

    UPROPERTY()
    TObjectPtr<AGoblinKing> ActiveBoss;

    FTimerHandle RespawnTimer;
    bool bBossDefeated = false;
};
