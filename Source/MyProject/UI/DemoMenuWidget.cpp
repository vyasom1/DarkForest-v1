#include "UI/DemoMenuWidget.h"
#include "UI/DemoFrontendState.h"
#include "UI/DemoAssetCredits.inl"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/ScrollBox.h"
#include "Core/DarkForestGameMode.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "World/DarkForestStory.h"

namespace
{
    void UpdateCinematicPause(UWorld* World, bool bPaused)
    {
        if (auto* Mode = World ? World->GetAuthGameMode<ADarkForestGameMode>() : nullptr)
            if (auto* Story = Mode->GetStory()) Story->SetCinematicPaused(bPaused);
    }
}

UDemoFrontendState* UDemoMenuWidget::State() const
{
    return GetGameInstance() ? GetGameInstance()->GetSubsystem<UDemoFrontendState>() : nullptr;
}

TSharedRef<SWidget> UDemoMenuWidget::RebuildWidget()
{
    if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this);
    SetIsFocusable(true);
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Root;
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(.008f,.014f,.022f,.96f));
    UCanvasPanelSlot* Fill = Root->AddChildToCanvas(Background);
    Fill->SetAnchors(FAnchors(0,0,1,1)); Fill->SetOffsets(FMargin(0));
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    UCanvasPanelSlot* Centre = Root->AddChildToCanvas(Scroll);
    Centre->SetAnchors(FAnchors(.5f,.1f,.5f,.9f));
    Centre->SetAlignment(FVector2D(.5f,0)); Centre->SetOffsets(FMargin(0,0,520,0));
    Panel = WidgetTree->ConstructWidget<UVerticalBox>();
    Scroll->AddChild(Panel);
    return Super::RebuildWidget();
}

void UDemoMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    VolumeMix = NewObject<USoundMix>(this);
    UGameplayStatics::PushSoundMixModifier(this,VolumeMix);
    ApplyAudio();
    SetVisibility(ESlateVisibility::Collapsed);
}

void UDemoMenuWidget::NativeDestruct()
{
    if (VolumeMix) UGameplayStatics::PopSoundMixModifier(this,VolumeMix);
    Super::NativeDestruct();
}

void UDemoMenuWidget::ApplyAudio()
{
    const auto* S = State();
    USoundClass* Master = LoadObject<USoundClass>(nullptr,TEXT("/Engine/EngineSounds/Master.Master"));
    if (Master && VolumeMix && S && S->Profile)
        UGameplayStatics::SetSoundMixClassOverride(this,VolumeMix,Master,FMath::Clamp(S->Profile->MasterVolume,0.f,1.f),1.f,.1f,true);
}

void UDemoMenuWidget::ShowMenu(bool bTitle)
{
    if (!Panel) return;
    bTitleContext = bTitle;
    if (!bOpen) bOwnsPause = !UGameplayStatics::IsGamePaused(this);
    bOpen = true;
    UGameplayStatics::SetGamePaused(this,true);
    UpdateCinematicPause(GetWorld(), true);
    SetVisibility(ESlateVisibility::Visible);
    BuildMain();
    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->bShowMouseCursor = true;
        FInputModeGameAndUI Mode;
        Mode.SetWidgetToFocus(TakeWidget()); Mode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(Mode);
        SetKeyboardFocus();
    }
}

void UDemoMenuWidget::CloseMenu()
{
    bOpen = false;
    SetVisibility(ESlateVisibility::Collapsed);
    if (bOwnsPause) UGameplayStatics::SetGamePaused(this,false);
    UpdateCinematicPause(GetWorld(), UGameplayStatics::IsGamePaused(this));
    bOwnsPause = false;
    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly());
    }
}

void UDemoMenuWidget::Heading(const FString& Value)
{
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(FText::FromString(Value));
    FSlateFontInfo Font = Label->GetFont(); Font.Size = 34; Label->SetFont(Font);
    Label->SetColorAndOpacity(FSlateColor(FLinearColor(.86f,.76f,.49f)));
    Panel->AddChildToVerticalBox(Label)->SetPadding(FMargin(8,18));
}

void UDemoMenuWidget::Text(const FString& Value)
{
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(FText::FromString(Value)); Label->SetAutoWrapText(true);
    Panel->AddChildToVerticalBox(Label)->SetPadding(FMargin(8,8));
}

UButton* UDemoMenuWidget::Button(const FString& Value)
{
    UButton* Result = WidgetTree->ConstructWidget<UButton>();
    UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(FText::FromString(Value));
    Result->SetContent(Label);
    Result->SetBackgroundColor(FLinearColor(.08f,.12f,.15f));
    Panel->AddChildToVerticalBox(Result)->SetPadding(FMargin(8,6));
    return Result;
}

void UDemoMenuWidget::BuildMain()
{
    bConfirmNewGame = false;
    Panel->ClearChildren();
    Heading(bTitleContext ? TEXT("DARKFOREST") : TEXT("PAUSED"));
    if (bTitleContext)
    {
        Button(TEXT("New Game"))->OnClicked.AddDynamic(this,&UDemoMenuWidget::NewGame);
        UButton* Continue = Button(TEXT("Continue"));
        Continue->SetIsEnabled(State() && State()->Profile && State()->Profile->bHasCheckpoint);
        Continue->OnClicked.AddDynamic(this,&UDemoMenuWidget::ContinueGame);
    }
    else
    {
        Button(TEXT("Resume"))->OnClicked.AddDynamic(this,&UDemoMenuWidget::Resume);
        Button(TEXT("Restart checkpoint"))->OnClicked.AddDynamic(this,&UDemoMenuWidget::RestartCheckpoint);
        Button(TEXT("Restart level"))->OnClicked.AddDynamic(this,&UDemoMenuWidget::NewGame);
    }
    Button(TEXT("Settings"))->OnClicked.AddDynamic(this,&UDemoMenuWidget::Settings);
    Button(TEXT("Credits"))->OnClicked.AddDynamic(this,&UDemoMenuWidget::Credits);
    if (!bTitleContext) Button(TEXT("Title screen"))->OnClicked.AddDynamic(this,&UDemoMenuWidget::ReturnToTitle);
    Button(TEXT("Quit"))->OnClicked.AddDynamic(this,&UDemoMenuWidget::Quit);
}

void UDemoMenuWidget::NewGame()
{
    UDemoFrontendState* S = State(); if (!S || !S->Profile) return;
    if (S->Profile->bHasCheckpoint && !bConfirmNewGame)
    {
        bConfirmNewGame = true;
        Panel->ClearChildren(); Heading(TEXT("START NEW GAME?"));
        Text(TEXT("This replaces your saved checkpoint."));
        Button(TEXT("Start new game"))->OnClicked.AddDynamic(this,&UDemoMenuWidget::NewGame);
        Button(TEXT("Back"))->OnClicked.AddDynamic(this,&UDemoMenuWidget::Back);
        return;
    }
    const FString MapName = UGameplayStatics::GetCurrentLevelName(this,true);
    if (!S->StartNewStory(MapName))
    {
        Panel->ClearChildren(); Heading(TEXT("COULD NOT SAVE"));
        Text(TEXT("Your previous checkpoint has been kept. Try again when saving is available."));
        Button(TEXT("Back"))->OnClicked.AddDynamic(this,&UDemoMenuWidget::Back);
        return;
    }
    S->bEnterGameplayOnNextMap = true; S->bResumeOnNextMap = false;
    const FName Map(*MapName);
    CloseMenu();
    UGameplayStatics::OpenLevel(this,Map);
}

void UDemoMenuWidget::ContinueGame()
{
    UDemoFrontendState* S = State();
    if (!S || !S->Profile || !S->Profile->bHasCheckpoint || S->Profile->Map.IsEmpty()) return;
    const FName Map(*S->Profile->Map);
    S->bEnterGameplayOnNextMap = true; S->bResumeOnNextMap = true;
    CloseMenu(); UGameplayStatics::OpenLevel(this,Map);
}

void UDemoMenuWidget::Resume() { CloseMenu(); }
void UDemoMenuWidget::RestartCheckpoint()
{
    if (State() && State()->Profile && State()->Profile->bHasCheckpoint) ContinueGame();
    else NewGame();
}
void UDemoMenuWidget::Back() { BuildMain(); }
void UDemoMenuWidget::ReturnToTitle() { ShowMenu(true); }
void UDemoMenuWidget::Quit()
{
    CloseMenu();
    UKismetSystemLibrary::QuitGame(this,GetOwningPlayer(),EQuitPreference::Quit,false);
}

void UDemoMenuWidget::Settings()
{
    Panel->ClearChildren(); Heading(TEXT("SETTINGS"));
    UGameUserSettings* User = GEngine ? GEngine->GetGameUserSettings() : nullptr;
    Text(TEXT("Graphics preset")); Quality = WidgetTree->ConstructWidget<UComboBoxString>();
    for (const TCHAR* Option : {TEXT("Low"),TEXT("Medium"),TEXT("High")}) Quality->AddOption(Option);
    Quality->SetSelectedIndex(User ? FMath::Clamp(User->GetOverallScalabilityLevel(),0,2) : 1);
    Panel->AddChildToVerticalBox(Quality);
    Text(TEXT("Resolution")); Resolution = WidgetTree->ConstructWidget<UComboBoxString>();
    for (const TCHAR* Option : {TEXT("1280 x 720"),TEXT("1600 x 900"),TEXT("1920 x 1080")}) Resolution->AddOption(Option);
    const FIntPoint Current = User ? User->GetScreenResolution() : FIntPoint(1920,1080);
    Resolution->SetSelectedIndex(Current.X <= 1280 ? 0 : Current.X <= 1600 ? 1 : 2);
    Panel->AddChildToVerticalBox(Resolution);
    Text(TEXT("Display")); Display = WidgetTree->ConstructWidget<UComboBoxString>();
    Display->AddOption(TEXT("Windowed")); Display->AddOption(TEXT("Borderless"));
    Display->SetSelectedIndex(User && User->GetFullscreenMode()==EWindowMode::Windowed ? 0 : 1);
    Panel->AddChildToVerticalBox(Display);
    Text(TEXT("Master volume")); Volume = WidgetTree->ConstructWidget<USlider>();
    Volume->SetValue(State() && State()->Profile ? State()->Profile->MasterVolume : 1.f);
    Panel->AddChildToVerticalBox(Volume);
    Text(TEXT("Mouse sensitivity")); Sensitivity = WidgetTree->ConstructWidget<USlider>();
    Sensitivity->SetMinValue(.25f); Sensitivity->SetMaxValue(2.5f);
    Sensitivity->SetValue(State() && State()->Profile ? State()->Profile->MouseSensitivity : 1.f);
    Panel->AddChildToVerticalBox(Sensitivity);
    Button(TEXT("Apply"))->OnClicked.AddDynamic(this,&UDemoMenuWidget::ApplySettings);
    Status = WidgetTree->ConstructWidget<UTextBlock>(); Panel->AddChildToVerticalBox(Status);
    Button(TEXT("Back"))->OnClicked.AddDynamic(this,&UDemoMenuWidget::Back);
}

void UDemoMenuWidget::ApplySettings()
{
    if (!Quality || !Resolution || !Display || !Volume || !Sensitivity) return;
    if (UGameUserSettings* User = GEngine ? GEngine->GetGameUserSettings() : nullptr)
    {
        const FIntPoint Sizes[] = {FIntPoint(1280,720),FIntPoint(1600,900),FIntPoint(1920,1080)};
        User->SetOverallScalabilityLevel(FMath::Clamp(Quality->GetSelectedIndex(),0,2));
        User->SetResolutionScaleValueEx(75.f);
        User->SetScreenResolution(Sizes[FMath::Clamp(Resolution->GetSelectedIndex(),0,2)]);
        User->SetFullscreenMode(Display->GetSelectedIndex()==0 ? EWindowMode::Windowed : EWindowMode::WindowedFullscreen);
        User->ApplySettings(false);
    }
    bool bSaved = false;
    if (UDemoFrontendState* S = State()) if (S->Profile)
    {
        S->Profile->MasterVolume = Volume->GetValue();
        S->Profile->MouseSensitivity = Sensitivity->GetValue();
        bSaved = S->Save(); ApplyAudio();
    }
    if (Status) Status->SetText(FText::FromString(bSaved ? TEXT("Settings saved") : TEXT("Could not save settings")));
}

void UDemoMenuWidget::Credits()
{
    Panel->ClearChildren(); Heading(TEXT("CREDITS"));
    Text(DarkForestAssetCreditText());
    Button(TEXT("Back"))->OnClicked.AddDynamic(this,&UDemoMenuWidget::Back);
}

FReply UDemoMenuWidget::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if (Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::P)
    {
        if (bTitleContext) BuildMain(); else CloseMenu();
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(Geometry,Event);
}
