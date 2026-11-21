#include "UI/BossHealthWidget.h"
#include "Enemy/GoblinKing.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

TSharedRef<SWidget> UBossHealthWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BossRoot"));
        WidgetTree->RootWidget = Root;
        UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>();
        UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
        PanelSlot->SetAnchors(FAnchors(0.5f, 1.f));
        PanelSlot->SetAlignment(FVector2D(0.5f, 1.f));
        PanelSlot->SetPosition(FVector2D(0.f, -90.f));
        PanelSlot->SetSize(FVector2D(600.f, 110.f));

        BossNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BossNameText"));
        BossNameText->SetText(FText::FromString(TEXT("GOBLIN KING")));
        BossNameText->SetJustification(ETextJustify::Center);
        Panel->AddChildToVerticalBox(BossNameText);

        UHorizontalBox* HealthRow = WidgetTree->ConstructWidget<UHorizontalBox>();
        Panel->AddChildToVerticalBox(HealthRow)->SetPadding(FMargin(0.f, 6.f));
        SegmentBar1 = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("SegmentBar1"));
        SegmentBar2 = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("SegmentBar2"));
        SegmentBar3 = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("SegmentBar3"));
        for (UProgressBar* Bar : {SegmentBar1.Get(), SegmentBar2.Get(), SegmentBar3.Get()})
        {
            Bar->SetFillColorAndOpacity(FLinearColor(0.65f, 0.08f, 0.06f));
            UHorizontalBoxSlot* HealthSlot = HealthRow->AddChildToHorizontalBox(Bar);
            HealthSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            HealthSlot->SetPadding(FMargin(3.f, 0.f));
        }
        PostureBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("PostureBar"));
        PostureBar->SetFillColorAndOpacity(FLinearColor(0.9f, 0.6f, 0.15f));
        Panel->AddChildToVerticalBox(PostureBar)->SetPadding(FMargin(3.f, 4.f));
        PhaseText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PhaseText"));
        PhaseText->SetJustification(ETextJustify::Center);
        Panel->AddChildToVerticalBox(PhaseText);
    }
    return Super::RebuildWidget();
}
void UBossHealthWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Collapsed);
}

void UBossHealthWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!IsValid(Boss))
    {
        return;
    }

    RefreshAccumulator += InDeltaTime;
    if (RefreshAccumulator >= 0.1f)
    {
        RefreshAccumulator = 0.f;
        RefreshFromBoss();
    }
}

void UBossHealthWidget::BindToBoss(AGoblinKing* InBoss)
{
    Boss = InBoss;
    bVictoryShown = false;
    bDefeat = false;
    if (IsValid(Boss))
    {
        SetVisibility(ESlateVisibility::Visible);
        RefreshFromBoss();
    }
    else
    {
        SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UBossHealthWidget::RefreshFromBoss()
{
    if (!IsValid(Boss))
    {
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    const int32 Segments = Boss->GetSegmentsRemaining();
    const float SegmentFraction = Boss->GetSegmentHealthFraction();
    const float PostureFraction = Boss->GetPostureFraction();
    const int32 Phase = Boss->GetPhaseNumber();

    UProgressBar* Bars[3] = { SegmentBar1.Get(), SegmentBar2.Get(), SegmentBar3.Get() };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        if (!Bars[Index])
        {
            continue;
        }
        const bool bSegmentExists = Index < Segments;
        const float Percent = !bSegmentExists ? 0.f : (Index == 0 ? SegmentFraction : 1.f);
        Bars[Index]->SetPercent(Percent);
    }

    if (PostureBar)
    {
        PostureBar->SetPercent(PostureFraction);
    }

    if (PhaseText)
    {
        FString Label = FString::Printf(TEXT("PHASE %d / 3   SEGMENTS %d"), Phase, Segments);
        if (Boss->IsTelegraphing())
        {
            Label += FString::Printf(TEXT("   %s%s%s"),
                Boss->IsCurrentAttackUnblockable() ? TEXT("[UNBLOCKABLE] ") : TEXT(""),
                *Boss->GetCurrentAttackName().ToString(),
                Boss->IsCurrentAttackJumpable() ? TEXT(" [JUMP]") : TEXT(""));
        }
        else if (Boss->IsPunishWindowOpen())
        {
            Label += TEXT("   PUNISH!");
        }
        PhaseText->SetText(FText::FromString(Label));
    }

    if (BossNameText)
    {
        BossNameText->SetText(FText::FromString(TEXT("GOBLIN KING")));
    }

    OnBossUpdated(Segments, SegmentFraction, PostureFraction, Phase);

    if (!Boss->IsAlive() && !bVictoryShown)
    {
        ShowVictoryState();
    }
}

void UBossHealthWidget::ShowDeathState()
{
    bDefeat = true;
    bVictoryShown = false;
    SetVisibility(ESlateVisibility::Visible);
    OnEncounterEnded(false);
}

void UBossHealthWidget::ShowVictoryState()
{
    bVictoryShown = true;
    bDefeat = false;
    SetVisibility(ESlateVisibility::Visible);
    OnEncounterEnded(true);
}

void UBossHealthWidget::ShowAliveState()
{
    bVictoryShown = false;
    bDefeat = false;
    if (IsValid(Boss))
    {
        SetVisibility(ESlateVisibility::Visible);
        RefreshFromBoss();
    }
}
