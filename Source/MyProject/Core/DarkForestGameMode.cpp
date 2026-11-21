#include "Core/DarkForestGameMode.h"
#include "Core/DarkForestLog.h"
#include "Player/DarkForestHero.h"
#include "Enemy/GoblinKing.h"
#include "World/ShrineCheckpoint.h"
#include "World/DarkForestStory.h"
#include "UI/DarkForestHUD.h"
#include "UI/DemoFrontendState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

ADarkForestGameMode::ADarkForestGameMode()
{
    DefaultPawnClass = ADarkForestHero::StaticClass();
    HUDClass = ADarkForestHUD::StaticClass();
}

bool ADarkForestGameMode::BeginStory(ADarkForestHero* Hero, UDemoFrontendState* State)
{
    if (Story) return true;
    if (!State || !State->Profile || State->Profile->StoryStage == EForestStoryStage::Inactive ||
        UGameplayStatics::GetCurrentLevelName(this, true) != TEXT("DarkForest_Main")) return false;
    Story = GetWorld()->SpawnActor<ADarkForestStory>();
    if (Story && Story->Start(Hero, State)) return true;
    if (Story) Story->Destroy();
    Story = nullptr;
    return false;
}

void ADarkForestGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Pick up a King that was placed in the level before the game mode ran.
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AGoblinKing> It(World); It; ++It)
        {
            if (IsValid(*It))
            {
                RegisterBoss(*It);
                break;
            }
        }
    }
}

void ADarkForestGameMode::RegisterShrine(AShrineCheckpoint* Shrine)
{
    if (!IsValid(Shrine))
    {
        return;
    }
    if (IsValid(ActiveShrine) && ActiveShrine != Shrine)
    {
        UE_LOG(LogDarkForest, Log, TEXT("Shrine already active; ignoring %s."), *Shrine->GetName());
        return;
    }
    ActiveShrine = Shrine;
    if (GetGameInstance())
        if (auto* Frontend = GetGameInstance()->GetSubsystem<UDemoFrontendState>()) Frontend->RecordCheckpoint(Shrine);
    if (Story) Story->ShrineActivated(Shrine);
    UE_LOG(LogDarkForest, Log, TEXT("Respawn shrine registered: %s"), *Shrine->GetName());
}

void ADarkForestGameMode::RegisterBoss(AGoblinKing* Boss)
{
    if (!IsValid(Boss))
    {
        return;
    }

    if (IsValid(ActiveBoss) && ActiveBoss != Boss)
    {
        UE_LOG(LogDarkForest, Warning,
            TEXT("Duplicate Goblin King %s removed; %s is already active."),
            *Boss->GetName(), *ActiveBoss->GetName());
        Boss->Destroy();
        return;
    }

    ActiveBoss = Boss;
    bBossDefeated = false;

    if (UWorld* World = GetWorld())
    {
        if (ADarkForestHero* Hero = Cast<ADarkForestHero>(UGameplayStatics::GetPlayerCharacter(World, 0)))
        {
            Hero->BindBossWidget(Boss);
        }
    }
}

void ADarkForestGameMode::NotifyBossDefeated(AGoblinKing* Boss)
{
    if (IsValid(Boss) && IsValid(ActiveBoss) && Boss != ActiveBoss)
    {
        return;
    }

    bBossDefeated = true;
    if (UWorld* World = GetWorld())
    {
        if (ADarkForestHero* Hero = Cast<ADarkForestHero>(UGameplayStatics::GetPlayerCharacter(World, 0)))
        {
            Hero->HandleBossDefeated(ActiveBoss);
        }
    }
    if (Story) Story->KingDefeated(Boss);
}

void ADarkForestGameMode::ResetBossEncounter()
{
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AGoblinKing> It(World); It; ++It)
        {
            if (IsValid(*It))
            {
                It->ResetEncounter(true);
                ActiveBoss = *It;
            }
        }
    }
    bBossDefeated = false;

    if (!IsValid(ActiveBoss))
    {
        UE_LOG(LogDarkForest, Warning, TEXT("Boss reset requested but no Goblin King exists."));
    }
}

void ADarkForestGameMode::HandleHeroDefeated(ADarkForestHero* Hero)
{
    if (!GetWorld())
    {
        return;
    }

    (void)Hero;
    if (!Story && !IsValid(ActiveShrine))
    {
        UE_LOG(LogDarkForest, Warning,
            TEXT("Hero defeated with no active shrine; respawn skipped."));
        return;
    }

    GetWorldTimerManager().SetTimer(RespawnTimer, this, &ADarkForestGameMode::RespawnHero,
        FMath::Max(RespawnDelaySeconds, Hero->GetDeathAnimationDuration() + .2f), false);
}

void ADarkForestGameMode::RespawnHero()
{
    if (!GetWorld())
    {
        return;
    }

    ADarkForestHero* Hero = Cast<ADarkForestHero>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
    if (Story && IsValid(Hero))
    {
        if (Story->GetStage() == EForestStoryStage::King && IsValid(ActiveBoss))
            ActiveBoss->ResetEncounter(false);
        Story->Respawn(Hero);
        return;
    }
    if (!IsValid(Hero) || !IsValid(ActiveShrine))
    {
        UE_LOG(LogDarkForest, Warning, TEXT("Respawn aborted: hero or shrine missing."));
        return;
    }

    ResetBossEncounter();
    Hero->ReviveAt(ActiveShrine->GetRespawnTransform());
    Hero->HandleRespawned();
    bBossDefeated = false;

    UE_LOG(LogDarkForest, Log, TEXT("Hero respawned at shrine %s."), *ActiveShrine->GetName());
}
