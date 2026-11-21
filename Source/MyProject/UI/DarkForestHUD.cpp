#include "UI/DarkForestHUD.h"
#include "UI/DemoMenuWidget.h"
#include "UI/DemoFrontendState.h"
#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "Player/DarkForestHero.h"
#include "Enemy/DarkForestGoblin.h"
#include "Enemy/GoblinKing.h"
#include "Core/DarkForestGameMode.h"
#include "World/DarkForestStory.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "MediaTexture.h"
#include "UObject/ConstructorHelpers.h"

ADarkForestHUD::ADarkForestHUD()
{
    PrimaryActorTick.bTickEvenWhenPaused = true;
    static ConstructorHelpers::FObjectFinderOptional<UTexture2D> Illustrations(
        TEXT("/Game/AutoAssets/Story/T_DarkForest_Cinematics.T_DarkForest_Cinematics"));
    StoryIllustrations = Illustrations.Get();
}

void ADarkForestHUD::BeginPlay()
{
    Super::BeginPlay();
    if (!PlayerOwner || !PlayerOwner->IsLocalController()) return;
    EnableInput(PlayerOwner);
    if (InputComponent)
    {
        InputComponent->BindKey(EKeys::P,IE_Pressed,this,&ADarkForestHUD::TogglePauseMenu).bExecuteWhenPaused = true;
        InputComponent->BindKey(EKeys::V, IE_Pressed, this, &ADarkForestHUD::SkipStoryDialogue);
    }
    Menu = CreateWidget<UDemoMenuWidget>(PlayerOwner.Get());
    if (!Menu) return;
    Menu->AddToViewport(1000);
    UDemoFrontendState* State = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDemoFrontendState>() : nullptr;
    if (State && State->bEnterGameplayOnNextMap)
    {
        State->bEnterGameplayOnNextMap = false;
        GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this]()
        {
            auto* S = GetGameInstance()->GetSubsystem<UDemoFrontendState>();
            auto* Hero = Cast<ADarkForestHero>(PlayerOwner->GetPawn());
            auto* Mode = GetWorld()->GetAuthGameMode<ADarkForestGameMode>();
            if (Mode && Mode->BeginStory(Hero, S)) S->bResumeOnNextMap = false;
            else if (S) S->ApplyPendingResume(Hero);
        }));
    }
    else Menu->ShowMenu(true);
}

void ADarkForestHUD::SkipStoryDialogue()
{
    if (Menu && Menu->IsMenuOpen()) return;
    if (auto* Mode = GetWorld()->GetAuthGameMode<ADarkForestGameMode>())
        if (auto* Story = Mode->GetStory()) Story->SkipDialogue();
}

void ADarkForestHUD::TogglePauseMenu()
{
    if (!Menu) return;
    if (Menu->IsTitleMenu()) return;
    if (Menu->IsMenuOpen()) Menu->CloseMenu(); else Menu->ShowMenu(false);
}

void ADarkForestHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (Menu)
    {
        if (Menu->IsMenuOpen()) Menu->CloseMenu();
        Menu->RemoveFromParent();
        Menu = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}

void ADarkForestHUD::DrawHUD()
{
    Super::DrawHUD();
    if (Menu && Menu->IsMenuOpen()) return;
    if (!Canvas || !GetWorld() || !GEngine) return;
    ADarkForestHero* Hero = Cast<ADarkForestHero>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
    if (!IsValid(Hero)) return;
    UFont* Font = GEngine->GetSmallFont();
    const float Width = static_cast<float>(Canvas->SizeX);
    const float Height = static_cast<float>(Canvas->SizeY);
    const FLinearColor Pale(0.85f, .9f, .83f);
    const FLinearColor Dark(.02f, .04f, .03f, .8f);
    const float BarWidth = 220.f;
    const auto* Mode = GetWorld()->GetAuthGameMode<ADarkForestGameMode>();
    const auto* Story = Mode ? Mode->GetStory() : nullptr;
    if (Story)
    {
        const int32 Frame = Story->GetCinematicFrame();
        if (Frame != INDEX_NONE)
        {
            DrawRect(FLinearColor(.013f, .018f, .028f), 0.f, 0.f, Width, Height);
            if (UMediaTexture* Movie = Story->GetCinematicMovie())
            {
                const float Aspect = Movie->GetAspectRatio();
                const float DisplayHeight = FMath::Min(Height, Width / Aspect);
                const float DisplayWidth = DisplayHeight * Aspect;
                DrawTexture(Movie, (Width - DisplayWidth) * .5f, (Height - DisplayHeight) * .5f,
                    DisplayWidth, DisplayHeight, 0.f, 0.f, 1.f, 1.f, FLinearColor::White, BLEND_Opaque);
                DrawRect(FLinearColor(0.f, 0.f, 0.f, .8f), Width * .025f, Height * .92f, 250.f, 29.f);
                const TCHAR* Prompt = Frame == 0 ? TEXT("E  Go to tutorial     V  Skip intro") : TEXT("E / V  Skip ending");
                DrawText(Prompt, Pale, Width * .035f, Height * .93f, Font, 1.15f);
                return;
            }
            const float Progress = Story->GetCinematicProgress();
            const float Fade = FMath::Clamp(FMath::Min(Progress, 1.f - Progress) * 15.f, 0.f, 1.f);
            if (StoryIllustrations)
            {
                const float Aspect = (2.f * StoryIllustrations->GetSizeX()) / (3.f * StoryIllustrations->GetSizeY());
                const float DisplayHeight = FMath::Min(Height * .72f, Width * .88f / Aspect);
                const float DisplayWidth = DisplayHeight * Aspect;
                const float Inset = .007f + .036f * Progress;
                const float U = (Frame % 3 + Inset) / 3.f;
                const float V = (Frame / 3 + Inset) / 2.f;
                DrawTexture(StoryIllustrations, (Width - DisplayWidth) * .5f, Height * .075f,
                    DisplayWidth, DisplayHeight, U, V, (1.f - 2.f * Inset) / 3.f,
                    (1.f - 2.f * Inset) / 2.f, FLinearColor(Fade, Fade, Fade, 1.f), BLEND_Opaque);
            }
            const FString Caption = Story->GetCinematicCaption();
            float CaptionWidth, CaptionHeight;
            GetTextSize(Caption, CaptionWidth, CaptionHeight, Font);
            const float CaptionScale = FMath::Min(1.8f, Width * .92f / FMath::Max(CaptionWidth, 1.f));
            DrawText(Caption, Pale, (Width - CaptionWidth * CaptionScale) * .5f,
                Height * .83f, Font, CaptionScale);
            DrawText(TEXT("E  Next scene     V  Skip cinematic"), FLinearColor(.85f, .72f, .43f),
                Width * .04f, Height * .94f, Font, 1.2f);
            return;
        }
        if (const FString* Subtitle = Story->GetSubtitle())
        {
            float TextWidth, TextHeight;
            GetTextSize(*Subtitle, TextWidth, TextHeight, Font);
            const float SubtitleScale = FMath::Min(1.65f, (Width * .8f - 30.f) / FMath::Max(TextWidth, 1.f));
            DrawRect(FLinearColor(.015f, .025f, .028f, .93f), Width * .1f, Height * .71f, Width * .8f, 67.f);
            DrawText(*Subtitle, Pale, Width * .1f + 14.f, Height * .71f + 10.f, Font, SubtitleScale);
            DrawText(TEXT("E  Next     V  Skip dialogue"), FLinearColor(.85f, .72f, .43f),
                Width * .1f + 14.f, Height * .71f + 39.f, Font, 1.25f);
            return;
        }
        DrawText(Story->GetObjective(), FLinearColor(1.f, .83f, .48f), 24.f, 134.f, Font, 1.4f);
        FVector Goal;
        if (Story->GetWaypoint(Hero->GetActorLocation(), Goal))
        {
            const FVector Delta = Goal - Hero->GetActorLocation();
            const FVector Screen = Project(Goal, false);
            const int32 Metres = FMath::RoundToInt(Delta.Size2D() / 100.0);
            if (Screen.Z > 0.f && Screen.X > 36.f && Screen.X < Width - 68.f &&
                Screen.Y > 45.f && Screen.Y < Height - 68.f)
            {
                DrawLine(Screen.X, Screen.Y, Screen.X, Screen.Y + 12.f, FLinearColor(1.f, .83f, .48f), 2.f);
                DrawText(FString::Printf(TEXT("%d m"), Metres), Pale, Screen.X - 14.f, Screen.Y - 18.f, Font, 1.f);
            }
            else
            {
                const float Bearing = FMath::FindDeltaAngleDegrees(PlayerOwner->GetControlRotation().Yaw, Delta.Rotation().Yaw);
                DrawText(FString::Printf(TEXT("%d m  |  %s"), Metres,
                    FMath::Abs(Bearing) < 25.f ? TEXT("Ahead") : Bearing > 0.f ? TEXT("Turn right") : TEXT("Turn left")),
                    Pale, 24.f, 156.f, Font, 1.f);
            }
        }
    }

    DrawText(TEXT("HEALTH"), Pale, 24.f, 20.f, Font, 1.f);
    DrawRect(Dark, 24.f, 42.f, BarWidth, 13.f);
    DrawRect(FLinearColor(.73f, .11f, .09f), 24.f, 42.f, BarWidth * Hero->GetHealthFraction(), 13.f);
    DrawText(TEXT("POSTURE"), Pale, 24.f, 60.f, Font, 1.f);
    DrawRect(Dark, 24.f, 82.f, BarWidth, 10.f);
    DrawRect(FLinearColor(.9f, .62f, .16f), 24.f, 82.f, BarWidth * Hero->GetPostureFraction(), 10.f);

    int32 AliveEnemies = 0;
    for (TActorIterator<ADarkForestGoblin> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It) && It->IsAlive() && !It->IsHidden()) ++AliveEnemies;
    }
    if (!Story || AliveEnemies > 0)
        DrawText(FString::Printf(TEXT("Goblins remaining: %d"), AliveEnemies), Pale, 24.f, 108.f, Font, 1.f);

    ADarkForestGoblin* Enemy = Hero->GetLockedTarget();
    if (IsValid(Enemy) && Enemy->IsAlive())
    {
        const float EnemyX = (Width - BarWidth) * .5f;
        DrawText(TEXT("LOCKED TARGET"), Pale, EnemyX, 20.f, Font, 1.f);
        DrawRect(Dark, EnemyX, 42.f, BarWidth, 13.f);
        DrawRect(FLinearColor(.6f, .08f, .06f), EnemyX, 42.f,
            BarWidth * Enemy->GetHealthFraction(), 13.f);
        DrawRect(Dark, EnemyX, 61.f, BarWidth, 9.f);
        DrawRect(FLinearColor(.95f, .68f, .2f), EnemyX, 61.f,
            BarWidth * Enemy->GetPostureFraction(), 9.f);
        if (AGoblinKing* King = Cast<AGoblinKing>(Enemy))
        {
            const int32 Segments = King->GetSegmentsRemaining();
            const float Gap = 6.f;
            const float SegmentWidth = (BarWidth - Gap * 2.f) / 3.f;
            for (int32 Index = 0; Index < 3; ++Index)
            {
                const float X = EnemyX + Index * (SegmentWidth + Gap);
                DrawRect(Dark, X, 34.f, SegmentWidth, 6.f);
                const float Fill = Index < Segments
                    ? (Index == 0 ? King->GetSegmentHealthFraction() : 1.f) : 0.f;
                DrawRect(FLinearColor(.72f, .12f, .1f), X, 34.f, SegmentWidth * Fill, 6.f);
            }
            DrawText(FString::Printf(TEXT("GOBLIN KING  PHASE %d / 3"), King->GetPhaseNumber()),
                Pale, EnemyX, 4.f, Font, 1.f);
            if (Enemy->IsTelegraphing())
            {
                DrawText(FString::Printf(TEXT("%s%s%s"),
                    King->IsCurrentAttackUnblockable() ? TEXT("[UNBLOCKABLE] ") : TEXT(""),
                    *King->GetCurrentAttackName().ToString(),
                    King->IsCurrentAttackJumpable() ? TEXT(" [JUMP]") : TEXT("")),
                    FLinearColor(1.f, .3f, .12f), EnemyX, 92.f, Font, 1.f);
            }
            else if (King->IsPunishWindowOpen())
            {
                DrawText(TEXT("PUNISH WINDOW"), FLinearColor(.6f, 1.f, .5f), EnemyX, 92.f, Font, 1.f);
            }
        }
        if (Enemy->IsPostureBroken()) DrawText(TEXT("PRESS E: DEATHBLOW"), Pale, EnemyX, 77.f, Font, 1.1f);
        else if (Enemy->IsTelegraphing() && !Cast<AGoblinKing>(Enemy)) DrawText(TEXT("INCOMING ATTACK - DEFLECT"), FLinearColor(1.f, .3f, .12f),
            EnemyX, 77.f, Font, 1.1f);
    }
    DrawText(TEXT("WASD move   Mouse look   Left click combo   Right click guard   F deflect   E deathblow   Q lock on   P pause"),
        Pale, 24.f, Height - 28.f, Font, .85f);
    if (ADarkForestGameMode* GameMode = Cast<ADarkForestGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
    {
        if (!Story && GameMode->IsBossDefeated())
        {
            DrawText(TEXT("GOBLIN KING DEFEATED"), FLinearColor(.7f, 1.f, .55f), Width * .36f,
                Height * .4f, Font, 1.8f);
        }
    }
    if (!Hero->IsAlive())
    {
        DrawText(TEXT("DEFEATED"), FLinearColor(.9f, .15f, .1f), Width * .43f,
            Height * .4f, Font, 2.f);
    }
    else if (!Story && AliveEnemies == 0)
    {
        DrawText(TEXT("CLEARING SECURED"), Pale, Width * .4f, Height * .4f, Font, 1.5f);
    }
}
