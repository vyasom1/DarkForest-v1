#include "Tests/CombatAutomationTests.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

#include "Combat/DamageNumberWorldSubsystem.h"
#include "UI/DamageNumberWidget.h"
#include "Data/AranyaCombatTuning.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestDamageNumberPoolLifecycle,
    "DarkForest.DamageNumberPoolLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestDamageNumberPoolLifecycle::RunTest(const FString& Parameters)
{
    if (!GWorld) { AddError(TEXT("World required")); return false; }
    UWorld* World = GWorld;
    UDamageNumberWidget* Widget = CreateWidget<UDamageNumberWidget>(World);
    if (!Widget) { AddError(TEXT("Widget creation failed")); return false; }
    const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
    if (!TestEqual(TEXT("pool constructed"), Widget->GetPoolSize(), 24)) return false;
    for (int32 i = 0; i < 24; ++i)
    {
        Widget->HandleDamageNumber(float(i + 1), FVector::ZeroVector, EDamageNumberType::Health);
        Widget->Pool[i].Age = float(24 - i) * 0.01f;
    }
    for (int32 i = 0; i < 5; ++i)
        Widget->HandleDamageNumber(float(100 + i), FVector::ZeroVector, EDamageNumberType::Health);
    TestEqual(TEXT("bounded pool after overflow"), Widget->GetActiveCount(), 24);
    for (int32 i = 0; i < 5; ++i)
        TestEqual(TEXT("oldest entries recycled in order"), Widget->Pool[i].Amount, float(100 + i));
    Widget->NativeTick(FGeometry(), 1.f);
    TestEqual(TEXT("all expired without requiring a controller"), Widget->GetActiveCount(), 0);
    for (const FDamageNumberEntry& Entry : Widget->Pool)
    {
        TestNotNull(TEXT("text exists"), Entry.Text.Get());
        if (Entry.Text) TestTrue(TEXT("expired text collapsed"), Entry.Text->GetVisibility() == ESlateVisibility::Collapsed);
    }
    Widget->NativeDestruct();
    return true;
}

// ---------------------------------------------------------------------------
// Subsystem broadcasts to bound receivers.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestDamageNumberSubsystemBroadcasts,
    "DarkForest.DamageNumberSubsystemBroadcasts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestDamageNumberSubsystemBroadcasts::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    if (!World)
    {
        AddError(TEXT("World required for damage number subsystem check."));
        return false;
    }

    UDamageNumberWorldSubsystem* Subsystem = UDamageNumberWorldSubsystem::Get(World);
    if (!Subsystem)
    {
        AddError(TEXT("Damage number subsystem unavailable."));
        return false;
    }

    UDarkForestTestDamageNumberReceiver* Receiver = NewObject<UDarkForestTestDamageNumberReceiver>(GetTransientPackage());
    Subsystem->OnDamageNumber.AddDynamic(Receiver, &UDarkForestTestDamageNumberReceiver::Handle);

    Subsystem->BroadcastDamageNumber(12.f, FVector::ZeroVector, EDamageNumberType::Health);
    TestEqual("broadcast reaches the bound receiver", Receiver->Count, 1);

    Subsystem->OnDamageNumber.RemoveDynamic(Receiver, &UDarkForestTestDamageNumberReceiver::Handle);
    return true;
}

// ---------------------------------------------------------------------------
// Zero-damage health/posture emissions are suppressed.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestDamageNumberZeroSuppressed,
    "DarkForest.DamageNumberZeroSuppressed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestDamageNumberZeroSuppressed::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    if (!World)
    {
        AddError(TEXT("World required for zero-suppression check."));
        return false;
    }

    UDamageNumberWorldSubsystem* Subsystem = UDamageNumberWorldSubsystem::Get(World);
    if (!Subsystem)
    {
        AddError(TEXT("Damage number subsystem unavailable."));
        return false;
    }

    UDarkForestTestDamageNumberReceiver* Receiver = NewObject<UDarkForestTestDamageNumberReceiver>(GetTransientPackage());
    Subsystem->OnDamageNumber.AddDynamic(Receiver, &UDarkForestTestDamageNumberReceiver::Handle);

    const int32 Before = Receiver->Count;
    Subsystem->BroadcastDamageNumber(0.f, FVector::ZeroVector, EDamageNumberType::Health);
    Subsystem->BroadcastDamageNumber(0.f, FVector::ZeroVector, EDamageNumberType::Posture);
    TestEqual("zero health/posture emissions are suppressed", Receiver->Count, Before);

    // Deflect is not suppressed by amount.
    Subsystem->BroadcastDamageNumber(0.f, FVector::ZeroVector, EDamageNumberType::Deflect);
    TestEqual("deflect emission is not amount-suppressed", Receiver->Count, Before + 1);

    Subsystem->OnDamageNumber.RemoveDynamic(Receiver, &UDarkForestTestDamageNumberReceiver::Handle);
    return true;
}

// ---------------------------------------------------------------------------
// Pool defaults to 24 entries.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestDamageNumberPoolDefaultSize,
    "DarkForest.DamageNumberPoolDefaultSize",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestDamageNumberPoolDefaultSize::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    if (!World)
    {
        AddError(TEXT("World required for pool size check."));
        return false;
    }

    UDamageNumberWidget* Widget = CreateWidget<UDamageNumberWidget>(World, UDamageNumberWidget::StaticClass());
    if (!Widget)
    {
        AddError(TEXT("Could not create damage number widget."));
        return false;
    }

    const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
    TestEqual("default pool preallocates 24 entries", Widget->GetPoolSize(), 24);

    Widget->RemoveFromParent();
    return true;
}

// ---------------------------------------------------------------------------
// Disabled tuning suppresses new numbers.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkForestDamageNumberDisabledSetting,
    "DarkForest.DamageNumberDisabledSetting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkForestDamageNumberDisabledSetting::RunTest(const FString& Parameters)
{
    UWorld* World = GWorld;
    if (!World)
    {
        AddError(TEXT("World required for disabled-setting check."));
        return false;
    }

    UDamageNumberWidget* Widget = CreateWidget<UDamageNumberWidget>(World, UDamageNumberWidget::StaticClass());
    if (!Widget)
    {
        AddError(TEXT("Could not create damage number widget."));
        return false;
    }

    const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();

    UAranyaCombatTuning* Tuning = NewObject<UAranyaCombatTuning>(GetTransientPackage());
    Tuning->bDamageNumbersEnabled = false;
    Widget->SetTuning(Tuning);

    const int32 Before = Widget->GetActiveCount();
    Widget->HandleDamageNumber(10.f, FVector::ZeroVector, EDamageNumberType::Health);
    TestEqual("disabled setting suppresses new numbers", Widget->GetActiveCount(), Before);

    Widget->RemoveFromParent();
    return true;
}
