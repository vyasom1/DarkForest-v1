#include "UI/DemoFrontendState.h"
#include "World/ShrineCheckpoint.h"
#include "Player/DarkForestHero.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Core/DarkForestLog.h"

namespace { const FString ProfileSlot = TEXT("DarkForestDemoProfile"); }

void UDemoFrontendState::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    const FWorldContext* Context = GetGameInstance()->GetWorldContext();
    SaveSlot = Context && Context->WorldType == EWorldType::PIE ? ProfileSlot + TEXT("_PIE") : ProfileSlot;
    const FString& ReadSlot = UGameplayStatics::DoesSaveGameExist(SaveSlot, 0) ? SaveSlot : ProfileSlot;
    if (UGameplayStatics::DoesSaveGameExist(ReadSlot,0))
        Profile = Cast<UDarkForestDemoSave>(UGameplayStatics::LoadGameFromSlot(ReadSlot,0));
    if (!Profile) Profile = Cast<UDarkForestDemoSave>(UGameplayStatics::CreateSaveGameObject(UDarkForestDemoSave::StaticClass()));
}

bool UDemoFrontendState::Save()
{
    const bool bSaved = Profile && UGameplayStatics::SaveGameToSlot(Profile,SaveSlot,0);
    if (!bSaved) UE_LOG(LogDarkForest,Warning,TEXT("Could not save demo checkpoint/settings."));
    return bSaved;
}

bool UDemoFrontendState::StartNewStory(const FString& MapName)
{
    if (!Profile) return false;
    UDarkForestDemoSave* Previous = Profile;
    Profile = DuplicateObject<UDarkForestDemoSave>(Previous, this);
    Profile->bHasCheckpoint = true;
    Profile->Map = MapName;
    Profile->ShrineName = NAME_None;
    Profile->Checkpoint = FTransform::Identity;
    Profile->StoryStage = EForestStoryStage::Introduction;
    Profile->bSpokeToMara = false;
    Profile->bWatchedStoryIntro = false;
    Profile->DefeatedStoryEnemies.Reset();
    if (Save()) return true;
    Profile = Previous;
    return false;
}

void UDemoFrontendState::RecordCheckpoint(AShrineCheckpoint* Shrine)
{
    if (!Profile || !IsValid(Shrine)) return;
    Profile->bHasCheckpoint = true;
    Profile->Map = UGameplayStatics::GetCurrentLevelName(Shrine,true);
    Profile->ShrineName = Shrine->GetFName();
    Profile->Checkpoint = Shrine->GetRespawnTransform();
    Save();
}

void UDemoFrontendState::ApplyPendingResume(ADarkForestHero* Hero)
{
    if (!bResumeOnNextMap || !Profile || !IsValid(Hero)) return;
    bResumeOnNextMap = false;
    for (TActorIterator<AShrineCheckpoint> It(Hero->GetWorld()); It; ++It)
    {
        if (It->GetFName() != Profile->ShrineName) continue;
        // Activate first so the existing game mode also owns a valid respawn shrine.
        if (It->Activate(Hero)) Hero->ReviveAt(Profile->Checkpoint);
        return;
    }
    UE_LOG(LogDarkForest,Warning,TEXT("Saved shrine is missing; keeping safe player-start spawn."));
}
