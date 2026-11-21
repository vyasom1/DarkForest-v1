#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/DemoFrontendState.h"
#include "DarkForestStory.generated.h"

class ADarkForestGoblin;
class ADarkForestHero;
class AGoblinKing;
class AShrineCheckpoint;
class ACameraActor;
class ASkeletalMeshActor;
class UFileMediaSource;
class UMediaPlayer;
class UMediaSoundComponent;
class UMediaTexture;

/** Quest scripting for the existing DarkForest_Main landmarks. */
UCLASS()
class MYPROJECT_API ADarkForestStory : public AActor
{
    GENERATED_BODY()

public:
    ADarkForestStory();
    bool Start(ADarkForestHero* Hero, UDemoFrontendState* Frontend);
    bool Interact(ADarkForestHero* Hero);
    void SkipDialogue();
    void EnemyDefeated(ADarkForestGoblin* Enemy);
    void ShrineActivated(AShrineCheckpoint* Activated);
    void KingDefeated(AGoblinKing* Defeated);
    void Respawn(ADarkForestHero* Hero);
    bool AllowsShrine(const AShrineCheckpoint* Candidate) const;
    bool GetWaypoint(const FVector& From, FVector& Out) const;
    const FString* GetSubtitle() const;
    UFUNCTION(BlueprintPure, Category="Story")
    int32 GetCinematicFrame() const { return CinematicFrame; }
    float GetCinematicProgress() const;
    FString GetCinematicCaption() const;
    UMediaTexture* GetCinematicMovie() const;
    void SetCinematicPaused(bool bPaused);
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    UFUNCTION(BlueprintPure, Category="Story")
    EForestStoryStage GetStage() const;
    UFUNCTION(BlueprintPure, Category="Story")
    FString GetObjective() const;

private:
    void EnterStage(EForestStoryStage Stage);
    void ApplyEncounters();
    void StartDialogue(ADarkForestHero* Hero, const FVector& Focus, TArray<FString> Lines);
    void FinishDialogue();
    void CutTo(ADarkForestHero* Hero, const FVector& Focus);
    void RestoreCamera();
    void SpawnVillagers();
    void StartCinematic(int32 FirstFrame);
    void PlayCinematicScene();
    void AdvanceCinematic();
    void FinishCinematic();
    void ReleaseCinematicInput();
    UFUNCTION() void MovieOpened(FString OpenedUrl);
    UFUNCTION() void MovieFailed(FString FailedUrl);
    UFUNCTION() void MovieEnded();
    void MovieTimedOut();
    void CloseMovie();

    UPROPERTY() TObjectPtr<UDemoFrontendState> State;
    UPROPERTY() TWeakObjectPtr<ADarkForestGoblin> Raider;
    UPROPERTY() TArray<TWeakObjectPtr<ADarkForestGoblin>> ForestEnemies;
    UPROPERTY() TWeakObjectPtr<AGoblinKing> King;
    UPROPERTY() TWeakObjectPtr<AShrineCheckpoint> Shrine;
    UPROPERTY() TWeakObjectPtr<ACameraActor> StoryCamera;
    UPROPERTY() TWeakObjectPtr<APlayerController> CameraOwner;
    UPROPERTY() TArray<TObjectPtr<ASkeletalMeshActor>> Villagers;
    UPROPERTY() TObjectPtr<UFileMediaSource> MovieSource;
    UPROPERTY() TObjectPtr<UMediaPlayer> MoviePlayer;
    UPROPERTY() TObjectPtr<UMediaTexture> MovieTexture;
    UPROPERTY() TObjectPtr<UMediaSoundComponent> MovieSound;
    FTimerHandle CameraTimer;
    FTimerHandle CinematicTimer;
    TWeakObjectPtr<APlayerController> CinematicOwner;
    TArray<FString> Dialogue;
    int32 SubtitleIndex = 0;
    int32 CinematicFrame = INDEX_NONE;
    bool bMovieActive = false;
    bool bCinematicPaused = false;
};
