#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Player/DarkForestHero.h"
#include "Enemy/GoblinKing.h"
#include "Core/DarkForestGameMode.h"
#include "Core/DarkForestLog.h"
#include "UI/DamageNumberWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"

namespace
{
    void AranyaTestDamageNumber(const TArray<FString>& Args, UWorld* World)
    {
        APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
        if (!PC || Args.IsEmpty()) return;
        FVector Location;
        FRotator Rotation;
        PC->GetPlayerViewPoint(Location, Rotation);
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UDamageNumberWidget::StaticClass(), true);
        for (UUserWidget* Widget : Widgets)
        {
            if (Widget && Widget->GetOwningPlayer() == PC)
            {
                CastChecked<UDamageNumberWidget>(Widget)->SpawnTestNumber(FCString::Atof(*Args[0]),
                    Location + Rotation.Vector() * 250.f, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.f);
                UE_LOG(LogAranyaCombat, Log, TEXT("TestDamageNumber amount=%s"), *Args[0]);
                return;
            }
        }
        UE_LOG(LogAranyaCombat, Warning, TEXT("TestDamageNumber: no player overlay"));
    }
    ADarkForestHero* FindHero(UWorld* World)
    {
        return World ? Cast<ADarkForestHero>(UGameplayStatics::GetPlayerCharacter(World, 0)) : nullptr;
    }

    AGoblinKing* FindKing(UWorld* World)
    {
        if (!World)
        {
            return nullptr;
        }
        for (TActorIterator<AGoblinKing> It(World); It; ++It)
        {
            if (IsValid(*It))
            {
                return *It;
            }
        }
        return nullptr;
    }

    void AranyaGodCommand(const TArray<FString>& Args, UWorld* World)
    {
        ADarkForestHero* Hero = FindHero(World);
        if (!Hero)
        {
            UE_LOG(LogDarkForest, Warning, TEXT("Aranya.God: no hero."));
            return;
        }
        const bool bEnable = Args.Num() == 0 ? !Hero->IsGodMode() : Args[0].ToBool();
        Hero->SetGodMode(bEnable);
    }

    void AranyaTimeScaleCommand(const TArray<FString>& Args, UWorld* World)
    {
        if (!World || Args.Num() == 0)
        {
            UE_LOG(LogDarkForest, Warning, TEXT("Aranya.TimeScale <0.1-5.0>"));
            return;
        }
        const float Scale = FMath::Clamp(FCString::Atof(*Args[0]), 0.1f, 5.f);
        if (AWorldSettings* Settings = World->GetWorldSettings())
        {
            Settings->TimeDilation = Scale;
        }
        UE_LOG(LogDarkForest, Log, TEXT("Aranya.TimeScale = %.2f"), Scale);
    }

    void AranyaSpawnBossCommand(const TArray<FString>& Args, UWorld* World)
    {
        if (!World)
        {
            return;
        }
        if (AGoblinKing* Existing = FindKing(World))
        {
            UE_LOG(LogDarkForest, Warning,
                TEXT("Aranya.SpawnBoss: a Goblin King already exists (%s); not spawning a duplicate."),
                *Existing->GetName());
            return;
        }

        ADarkForestHero* Hero = FindHero(World);
        const FVector Origin = Hero ? Hero->GetActorLocation() : FVector::ZeroVector;
        const FRotator Facing = Hero ? Hero->GetActorRotation() : FRotator::ZeroRotator;
        const FVector Location = Origin + Facing.Vector() * 500.f + FVector(0.f, 0.f, 100.f);

        // Prefer the project's configured boss class so dev-spawned kings carry
        // the same mesh/AnimBP as the placed encounter. Fall back to the native
        // class so the command still works in a bare map.
        const ADarkForestGameMode* GameMode = Cast<ADarkForestGameMode>(UGameplayStatics::GetGameMode(World));
        UClass* ClassToSpawn = GameMode && GameMode->BossClass ? GameMode->BossClass.Get() : AGoblinKing::StaticClass();

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AGoblinKing* Spawned = World->SpawnActor<AGoblinKing>(
            ClassToSpawn, Location, (Origin - Location).Rotation(), Params);
        UE_LOG(LogDarkForest, Log, TEXT("Aranya.SpawnBoss: %s"),
            Spawned ? *Spawned->GetName() : TEXT("spawn failed"));
    }

    void AranyaSetPhaseCommand(const TArray<FString>& Args, UWorld* World)
    {
        AGoblinKing* King = FindKing(World);
        if (!King || Args.Num() == 0)
        {
            UE_LOG(LogDarkForest, Warning, TEXT("Aranya.SetPhase <1-3>: no boss or missing phase."));
            return;
        }
        King->SetPhase(FCString::Atoi(*Args[0]));
        UE_LOG(LogDarkForest, Log, TEXT("Aranya.SetPhase -> %d"), King->GetPhaseNumber());
    }
}

static FAutoConsoleCommandWithWorldAndArgs GAranyaTestNumber(
    TEXT("Aranya.TestDamageNumber"), TEXT("<Amount> [LifetimeOverride]"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AranyaTestDamageNumber));

static FAutoConsoleCommandWithWorldAndArgs GAranyaGod(
    TEXT("Aranya.God"),
    TEXT("Toggle hero invulnerability (optional: 0/1)."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AranyaGodCommand));

static FAutoConsoleCommandWithWorldAndArgs GAranyaTimeScale(
    TEXT("Aranya.TimeScale"),
    TEXT("Set world time dilation: Aranya.TimeScale 0.5."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AranyaTimeScaleCommand));

static FAutoConsoleCommandWithWorldAndArgs GAranyaSpawnBoss(
    TEXT("Aranya.SpawnBoss"),
    TEXT("Spawn one Goblin King in front of the hero if none exists."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AranyaSpawnBossCommand));

static FAutoConsoleCommandWithWorldAndArgs GAranyaSetPhase(
    TEXT("Aranya.SetPhase"),
    TEXT("Jump the boss to phase 1-3: Aranya.SetPhase 2."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AranyaSetPhaseCommand));
