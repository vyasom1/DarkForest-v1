#include "UI/DamageNumberWidget.h"

#include "Combat/DamageNumberWorldSubsystem.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UDamageNumberWidget::UDamageNumberWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

TSharedRef<SWidget> UDamageNumberWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DamageNumberRoot"));
        WidgetTree->RootWidget = RootCanvas;
    }
    return Super::RebuildWidget();
}

void UDamageNumberWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Hit-test-invisible overlay independent of the boss HUD.
    SetVisibility(ESlateVisibility::HitTestInvisible);

    const int32 Count = FMath::Clamp(GetMaxEntries(), 1, 256);
    Pool.SetNum(Count);

    if (RootCanvas)
    {
        RootCanvas->ClearChildren();
        for (int32 Index = 0; Index < Pool.Num(); ++Index)
        {
            FDamageNumberEntry& Entry = Pool[Index];
            ResetEntry(Entry);
            UTextBlock* Text = WidgetTree ? WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
                *FString::Printf(TEXT("DamageNumber_%d"), Index)) : nullptr;
            Entry.Text = Text;
            if (Text && RootCanvas)
            {
                Text->SetVisibility(ESlateVisibility::Collapsed);
                UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Text);
                Entry.Slot = PanelSlot;
                if (PanelSlot)
                {
                    PanelSlot->SetAnchors(FAnchors(0.f, 0.f));
                    PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
                    PanelSlot->SetAutoSize(true);
                    PanelSlot->SetPosition(FVector2D(0.f, 0.f));
                    PanelSlot->SetSize(FVector2D(200.f, 40.f));
                }
            }
        }
    }

    // Bind to the world router once.
    if (!bBound)
    {
        if (APlayerController* PC = GetOwningPlayer<APlayerController>())
        {
            if (UWorld* World = PC->GetWorld())
            {
                if (UDamageNumberWorldSubsystem* Subsystem = UDamageNumberWorldSubsystem::Get(World))
                {
                    Subsystem->OnDamageNumber.AddDynamic(this, &UDamageNumberWidget::HandleDamageNumber);
                    bBound = true;
                }
            }
        }
    }
}

void UDamageNumberWidget::NativeDestruct()
{
    if (bBound)
    {
        if (APlayerController* PC = GetOwningPlayer<APlayerController>())
        {
            if (UWorld* World = PC->GetWorld())
            {
                if (UDamageNumberWorldSubsystem* Subsystem = UDamageNumberWorldSubsystem::Get(World))
                {
                    Subsystem->OnDamageNumber.RemoveDynamic(this, &UDamageNumberWidget::HandleDamageNumber);
                }
            }
        }
        bBound = false;
    }

    Super::NativeDestruct();
}

void UDamageNumberWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    APlayerController* PC = GetOwningPlayer<APlayerController>();

    for (FDamageNumberEntry& Entry : Pool)
    {
        if (!Entry.bActive)
        {
            continue;
        }

        Entry.Age += InDeltaTime;
        if (Entry.Age >= Entry.Lifetime)
        {
            DeactivateEntry(Entry);
            continue;
        }

        FVector2D ScreenPos = FVector2D::ZeroVector;
        const bool bProjected = PC && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Entry.WorldLocation, ScreenPos, true);
        if (!bProjected)
        {
            if (Entry.Text)
            {
                Entry.Text->SetVisibility(ESlateVisibility::Collapsed);
            }
            continue;
        }

        if (Entry.Text)
        {
            Entry.Text->SetVisibility(ESlateVisibility::Visible);
        }

        const float SafeLifetime = FMath::Max(0.01f, Entry.Lifetime);
        const float t = FMath::Clamp(Entry.Age / SafeLifetime, 0.f, 1.f);

        FVector2D Pos = ScreenPos;
        Pos.Y -= GetRise() * t;
        Pos.X += Entry.HorizontalOffset;

        float Scale = 1.f;
        const float PopDuration = GetPopDuration();
        if (PopDuration > 0.f && Entry.Age < PopDuration)
        {
            const float pt = Entry.Age / PopDuration;
            Scale = FMath::Lerp(GetPopScale(), 1.f, pt);
        }

        float Opacity = 1.f;
        const float FadeStart = GetFadeFraction();
        if (t >= FadeStart)
        {
            const float Denom = FMath::Max(0.001f, 1.f - FadeStart);
            const float fadeT = FMath::Clamp((t - FadeStart) / Denom, 0.f, 1.f);
            Opacity = FMath::Lerp(1.f, 0.f, fadeT);
        }

        if (Entry.Text)
        {
            Entry.Text->SetRenderOpacity(Opacity);
            FWidgetTransform Transform;
            Transform.Scale = FVector2D(Scale, Scale);
            Entry.Text->SetRenderTransform(Transform);
        }
        if (Entry.Slot)
        {
            Entry.Slot->SetPosition(Pos);
        }
    }
}

void UDamageNumberWidget::SetTuning(UAranyaCombatTuning* InTuning)
{
    Tuning = InTuning;
}

void UDamageNumberWidget::HandleDamageNumber(float Amount, FVector WorldLocation, EDamageNumberType Type)
{
    if (!IsEffectEnabled())
    {
        return;
    }

    const int32 Index = AcquireEntry();
    if (Index < 0 || !Pool.IsValidIndex(Index))
    {
        return;
    }

    FDamageNumberEntry& Entry = Pool[Index];
    ResetEntry(Entry);
    Entry.bActive = true;
    Entry.WorldLocation = WorldLocation;
    Entry.Type = Type;
    Entry.Amount = Amount;
    Entry.Age = 0.f;
    Entry.Lifetime = GetLifetime();
    Entry.HorizontalOffset = FMath::FRandRange(-18.f, 18.f);

    if (Entry.Text)
    {
        FString Str;
        switch (Type)
        {
            case EDamageNumberType::Health:
                Str = FString::Printf(TEXT("%d"), FMath::RoundToInt(Amount));
                break;
            case EDamageNumberType::Posture:
                Str = FString::Printf(TEXT("%d"), FMath::RoundToInt(Amount));
                break;
            case EDamageNumberType::Deflect:
                Str = TEXT("DEFLECT");
                break;
            case EDamageNumberType::Deathblow:
                Str = TEXT("DEATHBLOW");
                break;
            default:
                Str = FString::Printf(TEXT("%d"), FMath::RoundToInt(Amount));
                break;
        }
        Entry.Text->SetText(FText::FromString(Str));
        Entry.Text->SetColorAndOpacity(FSlateColor(GetColor(Type)));

        FSlateFontInfo Font = Entry.Text->GetFont();
        Font.Size = (Type == EDamageNumberType::Deathblow) ? GetDeathblowFontSize() : GetNormalFontSize();
        Font.OutlineSettings.OutlineSize = 2;
        Font.OutlineSettings.OutlineColor = FLinearColor::Black;
        Entry.Text->SetFont(Font);

        Entry.Text->SetRenderOpacity(1.f);
        FWidgetTransform Identity;
        Entry.Text->SetRenderTransform(Identity);
        Entry.Text->SetVisibility(ESlateVisibility::Visible);
    }
}

void UDamageNumberWidget::SpawnTestNumber(float Amount, FVector WorldLocation, float LifetimeOverride)
{
    if (!IsEffectEnabled()) return;
    const int32 Index = AcquireEntry();
    HandleDamageNumber(Amount, WorldLocation, EDamageNumberType::Health);
    if (Pool.IsValidIndex(Index) && LifetimeOverride > 0.f)
        Pool[Index].Lifetime = FMath::Clamp(LifetimeOverride, 0.1f, 30.f);
}

int32 UDamageNumberWidget::AcquireEntry()
{
    for (int32 Index = 0; Index < Pool.Num(); ++Index)
    {
        if (!Pool[Index].bActive)
        {
            return Index;
        }
    }

    int32 OldestIndex = -1;
    float OldestAge = -1.f;
    for (int32 Index = 0; Index < Pool.Num(); ++Index)
    {
        if (Pool[Index].bActive && Pool[Index].Age > OldestAge)
        {
            OldestAge = Pool[Index].Age;
            OldestIndex = Index;
        }
    }

    if (OldestIndex >= 0)
    {
        ResetEntry(Pool[OldestIndex]);
    }
    return OldestIndex;
}

void UDamageNumberWidget::ResetEntry(FDamageNumberEntry& Entry)
{
    Entry.Age = 0.f;
    Entry.WorldLocation = FVector::ZeroVector;
    Entry.HorizontalOffset = 0.f;
    Entry.Amount = 0.f;
    Entry.bActive = false;
    Entry.Lifetime = GetLifetime();

    if (Entry.Text)
    {
        Entry.Text->SetRenderOpacity(1.f);
        FWidgetTransform Identity;
        Entry.Text->SetRenderTransform(Identity);
        Entry.Text->SetVisibility(ESlateVisibility::Collapsed);
        Entry.Text->SetText(FText::GetEmpty());
    }
}

void UDamageNumberWidget::DeactivateEntry(FDamageNumberEntry& Entry)
{
    Entry.bActive = false;
    if (Entry.Text)
    {
        Entry.Text->SetVisibility(ESlateVisibility::Collapsed);
        Entry.Text->SetRenderOpacity(0.f);
    }
}

bool UDamageNumberWidget::IsEffectEnabled() const
{
    if (Tuning)
    {
        return Tuning->bDamageNumbersEnabled;
    }
    return true;
}

float UDamageNumberWidget::GetLifetime() const
{
    float Value = 0.8f;
    if (Tuning)
    {
        Value = Tuning->DamageNumberLifetime;
    }
    return FMath::Clamp(Value, 0.1f, 5.f);
}

float UDamageNumberWidget::GetRise() const
{
    float Value = 100.f;
    if (Tuning)
    {
        Value = Tuning->DamageNumberRise;
    }
    return FMath::Max(0.f, Value);
}

float UDamageNumberWidget::GetPopScale() const
{
    float Value = 1.3f;
    if (Tuning)
    {
        Value = Tuning->DamageNumberPopScale;
    }
    return FMath::Clamp(Value, 1.f, 3.f);
}

float UDamageNumberWidget::GetPopDuration() const
{
    float Value = 0.1f;
    if (Tuning)
    {
        Value = Tuning->DamageNumberPopDuration;
    }
    return FMath::Clamp(Value, 0.f, 1.f);
}

float UDamageNumberWidget::GetFadeFraction() const
{
    float Value = 0.7f;
    if (Tuning)
    {
        Value = Tuning->DamageNumberFadeFraction;
    }
    return FMath::Clamp(Value, 0.f, 1.f);
}

FLinearColor UDamageNumberWidget::GetColor(EDamageNumberType Type) const
{
    if (Tuning)
    {
        switch (Type)
        {
            case EDamageNumberType::Health: return Tuning->DamageNumberHealthColor;
            case EDamageNumberType::Posture: return Tuning->DamageNumberPostureColor;
            case EDamageNumberType::Deflect: return Tuning->DamageNumberDeflectColor;
            case EDamageNumberType::Deathblow: return Tuning->DamageNumberDeathblowColor;
            default: break;
        }
    }
    switch (Type)
    {
        case EDamageNumberType::Health: return FLinearColor::White;
        case EDamageNumberType::Posture: return FLinearColor(1.f, 0.78f, 0.2f, 1.f);
        case EDamageNumberType::Deflect: return FLinearColor(0.2f, 0.85f, 1.f, 1.f);
        case EDamageNumberType::Deathblow: return FLinearColor(1.f, 0.2f, 0.15f, 1.f);
        default: return FLinearColor::White;
    }
}

float UDamageNumberWidget::GetNormalFontSize() const
{
    float Value = 24.f;
    if (Tuning)
    {
        Value = Tuning->DamageNumberFontSize;
    }
    return FMath::Clamp(Value, 4.f, 96.f);
}

float UDamageNumberWidget::GetDeathblowFontSize() const
{
    float Value = 36.f;
    if (Tuning)
    {
        Value = Tuning->DamageNumberDeathblowFontSize;
    }
    return FMath::Clamp(Value, 4.f, 96.f);
}

int32 UDamageNumberWidget::GetMaxEntries() const
{
    int32 Value = DefaultMaxEntries;
    if (Tuning)
    {
        Value = Tuning->MaxDamageNumbersActive;
    }
    return FMath::Clamp(Value, 1, 256);
}
