#include "World/DarkForestStory.h"

#include "Animation/AnimSequence.h"
#include "Animation/SkeletalMeshActor.h"
#include "Camera/CameraActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/DarkForestLog.h"
#include "Enemy/DarkForestGoblin.h"
#include "Enemy/GoblinKing.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileMediaSource.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "MediaTexture.h"
#include "Player/DarkForestHero.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "World/ShrineCheckpoint.h"

namespace
{
    const FVector VillageStart(1880, 550, 115);
    const FVector Mara(1850, 870, 25);
    const FVector Iven(2060, 775, 25);
    const FVector RaidStart(1330, 550, 115);
    const FVector TrailStart(950, 200, 115);
    const FVector Nest(-1640, -150, 150);
    constexpr float SceneDuration = 4.75f;
    constexpr int32 TutorialFrame = 3;
    constexpr int32 FinalFrame = 5;

    FVector ForestEnemyStart(int32 Index)
    {
        return FVector(-1100.f - 1350.f * Index, Index % 2 ? 260.f : -160.f, 115.f);
    }

    bool IsNear(const AActor* Actor, const FVector& Location)
    {
        return FVector::DistSquared2D(Actor->GetActorLocation(), Location) < FMath::Square(260.f) &&
            FMath::Abs(Actor->GetActorLocation().Z - Location.Z) < 160.f;
    }
}

ADarkForestStory::ADarkForestStory()
{
    PrimaryActorTick.bCanEverTick = false;
}

bool ADarkForestStory::Start(ADarkForestHero* Hero, UDemoFrontendState* Frontend)
{
    if (!IsValid(Hero) || !Frontend || !Frontend->Profile ||
        Frontend->Profile->StoryStage == EForestStoryStage::Inactive) return false;
    State = Frontend;

    TArray<ADarkForestGoblin*> Enemies;
    for (TActorIterator<ADarkForestGoblin> It(GetWorld()); It; ++It)
    {
        if (AGoblinKing* Found = Cast<AGoblinKing>(*It)) King = Found;
        else Enemies.Add(*It);
    }
    Enemies.Sort([](const ADarkForestGoblin& A, const ADarkForestGoblin& B)
    {
        return A.GetActorLocation().X > B.GetActorLocation().X;
    });
    if (Enemies.IsEmpty() || !King.IsValid())
    {
        UE_LOG(LogDarkForest, Error, TEXT("Story requires the placed raider and King."));
        State = nullptr;
        return false;
    }
    for (TActorIterator<AShrineCheckpoint> It(GetWorld()); It; ++It)
    {
        Shrine = *It;
        break;
    }
    if (!Shrine.IsValid())
    {
        UE_LOG(LogDarkForest, Error, TEXT("Story requires the placed forest shrine."));
        State = nullptr;
        return false;
    }

    Raider = Enemies[0];
    Raider->SetActorLocation(RaidStart, false, nullptr, ETeleportType::TeleportPhysics);
    for (int32 Index = 1; Index < Enemies.Num(); ++Index)
    {
        ForestEnemies.Add(Enemies[Index]);
        Enemies[Index]->SetActorLocation(ForestEnemyStart(Index - 1),
            false, nullptr, ETeleportType::TeleportPhysics);
    }
    King->SetActorLocation(Nest, false, nullptr, ETeleportType::TeleportPhysics);
    SpawnVillagers();
    ApplyEncounters();
    Respawn(Hero);
    if (GetStage() == EForestStoryStage::King) Shrine->Activate(Hero);
    if (GetStage() == EForestStoryStage::Introduction && !State->Profile->bWatchedStoryIntro)
        StartCinematic(0);
    else if (GetStage() == EForestStoryStage::Return)
        StartCinematic(4);
    return true;
}

EForestStoryStage ADarkForestStory::GetStage() const
{
    return State && State->Profile ? State->Profile->StoryStage : EForestStoryStage::Inactive;
}

void ADarkForestStory::ApplyEncounters()
{
    const EForestStoryStage Stage = GetStage();
    const auto SetActive = [this](ADarkForestGoblin* Enemy, bool bActive)
    {
        if (!IsValid(Enemy) || !Enemy->IsAlive()) return;
        bActive &= !State->Profile->DefeatedStoryEnemies.Contains(Enemy->GetFName());
        Enemy->SetActorEnableCollision(bActive);
        Enemy->SetActorHiddenInGame(!bActive);
    };
    SetActive(Raider.Get(), Stage == EForestStoryStage::Raid);
    for (const auto& Enemy : ForestEnemies)
        SetActive(Enemy.Get(), Stage == EForestStoryStage::Forest);
    SetActive(King.Get(), Stage == EForestStoryStage::King);
    if (auto* PC = GetWorld()->GetFirstPlayerController())
        if (auto* Hero = Cast<ADarkForestHero>(PC->GetPawn()))
            Hero->BindBossWidget(Stage == EForestStoryStage::King ? King.Get() : nullptr);
}

void ADarkForestStory::EnterStage(EForestStoryStage Stage)
{
    State->Profile->StoryStage = Stage;
    State->Save();
    ApplyEncounters();
    UE_LOG(LogDarkForest, Log, TEXT("Story milestone: %s"), *UEnum::GetValueAsString(Stage));
}

bool ADarkForestStory::AllowsShrine(const AShrineCheckpoint* Candidate) const
{
    return !State || (Candidate == Shrine.Get() &&
        (GetStage() == EForestStoryStage::Shrine || GetStage() == EForestStoryStage::King));
}

void ADarkForestStory::ShrineActivated(AShrineCheckpoint* Activated)
{
    if (Activated == Shrine.Get() && GetStage() == EForestStoryStage::Shrine)
        EnterStage(EForestStoryStage::King);
}

void ADarkForestStory::EnemyDefeated(ADarkForestGoblin* Enemy)
{
    const EForestStoryStage Stage = GetStage();
    const bool bRaid = Stage == EForestStoryStage::Raid && Enemy == Raider.Get();
    const bool bForest = Stage == EForestStoryStage::Forest &&
        ForestEnemies.ContainsByPredicate([Enemy](const auto& Entry) { return Entry.Get() == Enemy; });
    if (!bRaid && !bForest) return;
    State->Profile->DefeatedStoryEnemies.AddUnique(Enemy->GetFName());
    if (bRaid)
    {
        EnterStage(ForestEnemies.IsEmpty() ? EForestStoryStage::Shrine : EForestStoryStage::Forest);
        return;
    }
    for (const auto& Remaining : ForestEnemies)
    {
        if (Remaining.IsValid() && Remaining->IsAlive() &&
            !State->Profile->DefeatedStoryEnemies.Contains(Remaining->GetFName()))
        {
            State->Save();
            return;
        }
    }
    EnterStage(EForestStoryStage::Shrine);
}

void ADarkForestStory::KingDefeated(AGoblinKing* Defeated)
{
    if (Defeated != King.Get() || GetStage() != EForestStoryStage::King) return;
    EnterStage(EForestStoryStage::Return);
    StartCinematic(4);
}

void ADarkForestStory::Respawn(ADarkForestHero* Hero)
{
    FVector Safe = VillageStart;
    float Yaw = 85.f;
    switch (GetStage())
    {
        case EForestStoryStage::Raid:
            if (Raider.IsValid()) Raider->ResetForStoryRespawn(RaidStart);
            Yaw = 180.f;
            break;
        case EForestStoryStage::Forest:
            for (int32 Index = 0; Index < ForestEnemies.Num(); ++Index)
                if (ForestEnemies[Index].IsValid()) ForestEnemies[Index]->ResetForStoryRespawn(ForestEnemyStart(Index));
            Safe = TrailStart;
            Yaw = 180.f;
            break;
        case EForestStoryStage::Shrine: Safe = FVector(-2060, 0, 115); Yaw = 180.f; break;
        case EForestStoryStage::King:
            if (King.IsValid()) King->SetActorLocation(Nest, false, nullptr, ETeleportType::TeleportPhysics);
            [[fallthrough]];
        case EForestStoryStage::Return:
            if (State->Profile->ShrineName != NAME_None) Safe = State->Profile->Checkpoint.GetLocation();
            else if (Shrine.IsValid()) Safe = Shrine->GetRespawnTransform().GetLocation();
            Yaw = 0.f;
            break;
        default: break;
    }
    Hero->ReviveAt(FTransform(FRotator(0, Yaw, 0), Safe));
    Hero->HandleRespawned();
}

bool ADarkForestStory::Interact(ADarkForestHero* Hero)
{
    if (CinematicFrame != INDEX_NONE)
    {
        AdvanceCinematic();
        return true;
    }
    if (Dialogue.IsValidIndex(SubtitleIndex))
    {
        if (++SubtitleIndex >= Dialogue.Num()) FinishDialogue();
        return true;
    }
    switch (GetStage())
    {
        case EForestStoryStage::Introduction:
            if (!State->Profile->bSpokeToMara && IsNear(Hero, Mara))
            {
                StartDialogue(Hero, Mara + FVector(0, 0, 130), {
                    TEXT("Mara: The survivors made it here. I wish Lio had been among them."),
                    TEXT("Mara: You can carry that grief without letting it choose every swing."),
                    TEXT("Mara: Left click to strike. Hold right click to guard. Iven will teach you the timing.")
                });
                return true;
            }
            if (State->Profile->bSpokeToMara && IsNear(Hero, Iven))
            {
                StartDialogue(Hero, Iven + FVector(0, 0, 130), {
                    TEXT("Iven: Tap F just before impact to deflect. Wait for the swing, then answer it."),
                    TEXT("Iven: The trail leads past the old shrine. Free the village, then come home."),
                    TEXT("Iven: A lookout is coming. Try what we taught you.")
                });
                return true;
            }
            break;
        case EForestStoryStage::Shrine:
            if (Shrine.IsValid() && IsNear(Hero, Shrine->GetActorLocation()))
                return Shrine->Activate(Hero);
            break;
        case EForestStoryStage::Complete:
            if (IsNear(Hero, Mara))
            {
                StartDialogue(Hero, Mara + FVector(0, 0, 130), {
                    TEXT("Mara: Listen to the bell. The village will remember what you did.")
                });
                return true;
            }
            break;
        default: break;
    }
    return false;
}

void ADarkForestStory::StartDialogue(ADarkForestHero* Hero, const FVector& Focus, TArray<FString> Lines)
{
    Dialogue = MoveTemp(Lines);
    SubtitleIndex = 0;
    CutTo(Hero, Focus);
}

void ADarkForestStory::SkipDialogue()
{
    if (CinematicFrame != INDEX_NONE) FinishCinematic();
    else if (!Dialogue.IsEmpty()) FinishDialogue();
}

void ADarkForestStory::FinishDialogue()
{
    Dialogue.Reset();
    SubtitleIndex = 0;
    RestoreCamera();
    if (GetStage() == EForestStoryStage::Introduction)
    {
        if (!State->Profile->bSpokeToMara)
        {
            State->Profile->bSpokeToMara = true;
            State->Save();
        }
        else EnterStage(EForestStoryStage::Raid);
    }
}

void ADarkForestStory::StartCinematic(int32 FirstFrame)
{
    CinematicFrame = FirstFrame;
    CinematicOwner = GetWorld()->GetFirstPlayerController();
    if (CinematicOwner.IsValid())
    {
        CinematicOwner->SetIgnoreMoveInput(true);
        CinematicOwner->SetIgnoreLookInput(true);
    }
    PlayCinematicScene();
}

void ADarkForestStory::PlayCinematicScene()
{
    const TCHAR* Path = nullptr;
    if (CinematicFrame == 0) Path = TEXT("./Movies/DarkForest_Opening.mp4");
    if (CinematicFrame == FinalFrame) Path = TEXT("./Movies/DarkForest_Finale.mp4");
    if (!Path)
    {
        GetWorldTimerManager().SetTimer(CinematicTimer, this, &ADarkForestStory::AdvanceCinematic, SceneDuration, false);
        return;
    }

    if (!MoviePlayer)
    {
        MoviePlayer = NewObject<UMediaPlayer>(this);
        MoviePlayer->PlayOnOpen = false;
        MoviePlayer->OnMediaOpened.AddDynamic(this, &ADarkForestStory::MovieOpened);
        MoviePlayer->OnMediaOpenFailed.AddDynamic(this, &ADarkForestStory::MovieFailed);
        MoviePlayer->OnEndReached.AddDynamic(this, &ADarkForestStory::MovieEnded);

        MovieSource = NewObject<UFileMediaSource>(this);
        MovieTexture = NewObject<UMediaTexture>(this);
        MovieTexture->NewStyleOutput = true;
        MovieTexture->SetMediaPlayer(MoviePlayer);
        MovieTexture->UpdateResource();

        MovieSound = NewObject<UMediaSoundComponent>(this);
        MovieSound->SetMediaPlayer(MoviePlayer);
        MovieSound->RegisterComponent();
    }
    MovieSource->SetFilePath(Path);
    bMovieActive = true;
    GetWorldTimerManager().SetTimer(CinematicTimer, this, &ADarkForestStory::MovieTimedOut, 5.f, false);
    if (!MoviePlayer->OpenSource(MovieSource)) MovieFailed(Path);
}

void ADarkForestStory::MovieOpened(FString OpenedUrl)
{
    if (!bMovieActive) return;
    MoviePlayer->SetLooping(false);
    const float Duration = static_cast<float>(MoviePlayer->GetDuration().GetTotalSeconds());
    GetWorldTimerManager().SetTimer(CinematicTimer, this, &ADarkForestStory::MovieTimedOut, Duration + 5.f, false);
    if (!bCinematicPaused && !MoviePlayer->Play())
    {
        MovieFailed(OpenedUrl);
        return;
    }
    UE_LOG(LogDarkForest, Display, TEXT("Story movie opened (%0.1fs): %s"), Duration, *OpenedUrl);
}

void ADarkForestStory::MovieFailed(FString FailedUrl)
{
    if (!bMovieActive) return;
    UE_LOG(LogDarkForest, Warning, TEXT("Story movie unavailable, using illustrations: %s"), *FailedUrl);
    CloseMovie();
    GetWorldTimerManager().SetTimer(CinematicTimer, this, &ADarkForestStory::AdvanceCinematic, SceneDuration, false);
}

void ADarkForestStory::MovieTimedOut()
{
    if (MovieTexture && MovieTexture->GetWidth() > 2) MovieEnded();
    else if (MovieSource) MovieFailed(MovieSource->GetUrl());
}

void ADarkForestStory::MovieEnded()
{
    if (!bMovieActive) return;
    UE_LOG(LogDarkForest, Display, TEXT("Story movie finished at frame %d."), CinematicFrame);
    AdvanceCinematic();
}

void ADarkForestStory::CloseMovie()
{
    if (!bMovieActive) return;
    bMovieActive = false;
    MoviePlayer->Close();
}

UMediaTexture* ADarkForestStory::GetCinematicMovie() const
{
    return bMovieActive && MovieTexture && MovieTexture->GetWidth() > 2 && MovieTexture->GetHeight() > 2
        ? MovieTexture.Get() : nullptr;
}

void ADarkForestStory::SetCinematicPaused(bool bPaused)
{
    bCinematicPaused = bPaused;
    if (!bMovieActive || !MoviePlayer->IsReady()) return;
    if (bPaused) MoviePlayer->Pause();
    else MoviePlayer->Play();
}

void ADarkForestStory::AdvanceCinematic()
{
    if (CinematicFrame == INDEX_NONE) return;
    if (CinematicFrame == TutorialFrame || CinematicFrame == FinalFrame)
    {
        FinishCinematic();
        return;
    }
    const bool bCompletedOpeningMovie = bMovieActive && CinematicFrame == 0;
    CloseMovie();
    CinematicFrame = bCompletedOpeningMovie ? TutorialFrame : CinematicFrame + 1;
    PlayCinematicScene();
}

float ADarkForestStory::GetCinematicProgress() const
{
    return FMath::Clamp(GetWorldTimerManager().GetTimerElapsed(CinematicTimer) / SceneDuration, 0.f, 1.f);
}

FString ADarkForestStory::GetCinematicCaption() const
{
    static const TCHAR* Captions[] = {
        TEXT("That night, Lio made me promise we'd dance at every festival."),
        TEXT("Then the goblins came. The warning bell never rang."),
        TEXT("By morning, all I could find of Lio was this ribbon."),
        TEXT("We fled to the ridge. I wanted revenge. Mara and Iven taught me how to fight."),
        TEXT("The King fell. We carried the bell home and raised the village again."),
        TEXT("When the lanterns went up, I remembered our dance. And I smiled.")
    };
    return CinematicFrame >= 0 && CinematicFrame < UE_ARRAY_COUNT(Captions) ? Captions[CinematicFrame] : TEXT("");
}

void ADarkForestStory::ReleaseCinematicInput()
{
    if (CinematicOwner.IsValid())
    {
        CinematicOwner->SetIgnoreMoveInput(false);
        CinematicOwner->SetIgnoreLookInput(false);
        CinematicOwner.Reset();
    }
}

void ADarkForestStory::FinishCinematic()
{
    const bool bEnding = CinematicFrame >= 4;
    CinematicFrame = INDEX_NONE;
    GetWorldTimerManager().ClearTimer(CinematicTimer);
    CloseMovie();
    ReleaseCinematicInput();
    if (bEnding)
    {
        EnterStage(EForestStoryStage::Complete);
        if (auto* PC = GetWorld()->GetFirstPlayerController())
            if (auto* Hero = Cast<ADarkForestHero>(PC->GetPawn())) Respawn(Hero);
        if (USoundBase* Bell = LoadObject<USoundBase>(nullptr,
            TEXT("/Game/AutoAssets/Audio/Placeholder/PLACEHOLDER_ShrineBell.PLACEHOLDER_ShrineBell")))
            UGameplayStatics::PlaySoundAtLocation(this, Bell, Mara);
    }
    else
    {
        State->Profile->bWatchedStoryIntro = true;
        State->Save();
    }
}

const FString* ADarkForestStory::GetSubtitle() const
{
    return Dialogue.IsValidIndex(SubtitleIndex) ? &Dialogue[SubtitleIndex] : nullptr;
}

FString ADarkForestStory::GetObjective() const
{
    switch (GetStage())
    {
        case EForestStoryStage::Introduction: return State->Profile->bSpokeToMara ? TEXT("Speak to Iven at the lookout [E]") : TEXT("Speak to Mara at the cabin [E]");
        case EForestStoryStage::Raid: return TEXT("Repel the raider. LMB: strike | RMB: guard | F: timed deflect");
        case EForestStoryStage::Forest: return TEXT("Follow the trail and defeat the remaining raiders");
        case EForestStoryStage::Shrine: return TEXT("Visit the old shrine to save your progress [E]");
        case EForestStoryStage::King: return TEXT("Defeat the Goblin King. Break posture, then press E for a deathblow");
        case EForestStoryStage::Return: return TEXT("Return the warning bell to Mara [E]");
        case EForestStoryStage::Complete: return TEXT("DARKFOREST COMPLETE   |   The village is safe");
        default: return FString();
    }
}

bool ADarkForestStory::GetWaypoint(const FVector& From, FVector& Out) const
{
    switch (GetStage())
    {
        case EForestStoryStage::Introduction: Out = State->Profile->bSpokeToMara ? Iven : Mara; break;
        case EForestStoryStage::Raid:
            if (!Raider.IsValid()) return false;
            Out = Raider->GetActorLocation();
            break;
        case EForestStoryStage::Forest:
        {
            const ADarkForestGoblin* Best = nullptr;
            double Distance = TNumericLimits<double>::Max();
            for (const auto& Enemy : ForestEnemies)
            {
                if (!Enemy.IsValid() || Enemy->IsHidden() || !Enemy->IsAlive()) continue;
                const double Candidate = FVector::DistSquared2D(From, Enemy->GetActorLocation());
                if (Candidate < Distance) { Distance = Candidate; Best = Enemy.Get(); }
            }
            if (!Best) return false;
            Out = Best->GetActorLocation();
            break;
        }
        case EForestStoryStage::Shrine:
            if (!Shrine.IsValid()) return false;
            Out = Shrine->GetActorLocation();
            break;
        case EForestStoryStage::King: Out = Nest; break;
        case EForestStoryStage::Return: Out = Mara; break;
        default: return false;
    }
    Out.Z += 140.f;
    return true;
}

void ADarkForestStory::SpawnVillagers()
{
    USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny"));
    UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Animations/Manny/MM_Idle.MM_Idle"));
    if (!Mesh) return;
    UMaterialInterface* Gold = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/StarterContent/Materials/M_Metal_Gold.M_Metal_Gold"));
    for (const FVector& Position : { Mara, Iven })
    {
        ASkeletalMeshActor* Villager = GetWorld()->SpawnActor<ASkeletalMeshActor>(Position, FRotator(0, Position == Mara ? -175.f : -125.f, 0));
        if (!Villager) continue;
        Villagers.Add(Villager);
        USkeletalMeshComponent* Body = Villager->GetSkeletalMeshComponent();
        Body->SetSkeletalMesh(Mesh);
        Villager->SetActorEnableCollision(false);
        if (Idle) Body->PlayAnimation(Idle, true);
        if (Position == Mara && Gold) Body->SetMaterial(0, Gold);
    }
}

void ADarkForestStory::CutTo(ADarkForestHero* Hero, const FVector& Focus)
{
    auto* PC = Cast<APlayerController>(Hero->GetController());
    if (!PC) return;
    const FVector Eye = Focus + FVector(-325.f, -430.f, 90.f);
    if (!StoryCamera.IsValid())
        StoryCamera = GetWorld()->SpawnActor<ACameraActor>(Eye, (Focus - Eye).Rotation());
    if (!StoryCamera.IsValid()) return;
    StoryCamera->SetActorLocationAndRotation(Eye, (Focus - Eye).Rotation());
    CameraOwner = PC;
    PC->SetIgnoreMoveInput(true);
    PC->SetIgnoreLookInput(true);
    PC->SetViewTargetWithBlend(StoryCamera.Get(), .3f);
    GetWorldTimerManager().SetTimer(CameraTimer, this, &ADarkForestStory::RestoreCamera, 2.0f, false);
}

void ADarkForestStory::RestoreCamera()
{
    GetWorldTimerManager().ClearTimer(CameraTimer);
    if (CameraOwner.IsValid())
    {
        CameraOwner->SetIgnoreMoveInput(false);
        CameraOwner->SetIgnoreLookInput(false);
        if (APawn* Pawn = CameraOwner->GetPawn()) CameraOwner->SetViewTargetWithBlend(Pawn, .25f);
        CameraOwner.Reset();
    }
}

void ADarkForestStory::EndPlay(const EEndPlayReason::Type Reason)
{
    CloseMovie();
    ReleaseCinematicInput();
    RestoreCamera();
    Super::EndPlay(Reason);
}
