#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DemoFrontendState.generated.h"
class AShrineCheckpoint;
class ADarkForestHero;

UENUM(BlueprintType)
enum class EForestStoryStage : uint8
{
    Inactive, Introduction, Raid, Forest, Shrine, King, Return, Complete
};

UCLASS()
class MYPROJECT_API UDarkForestDemoSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) bool bHasCheckpoint = false;
    UPROPERTY(SaveGame) FString Map;
    UPROPERTY(SaveGame) FName ShrineName;
    UPROPERTY(SaveGame) FTransform Checkpoint = FTransform::Identity;
    UPROPERTY(SaveGame) float MasterVolume = 1.f;
    UPROPERTY(SaveGame) float MouseSensitivity = 1.f;
    UPROPERTY(SaveGame) EForestStoryStage StoryStage = EForestStoryStage::Inactive;
    UPROPERTY(SaveGame) bool bSpokeToMara = false;
    UPROPERTY(SaveGame) bool bWatchedStoryIntro = false;
    UPROPERTY(SaveGame) TArray<FName> DefeatedStoryEnemies;
};

/** Frontend travel intent and a checkpoint/settings save; no Blueprint dependency. */
UCLASS()
class MYPROJECT_API UDemoFrontendState : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    bool Save();
    bool StartNewStory(const FString& MapName);
    void RecordCheckpoint(AShrineCheckpoint* Shrine);
    void ApplyPendingResume(ADarkForestHero* Hero);
    UPROPERTY() TObjectPtr<UDarkForestDemoSave> Profile;
    bool bEnterGameplayOnNextMap = false;
    bool bResumeOnNextMap = false;
private:
    FString SaveSlot;
};
