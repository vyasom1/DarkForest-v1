#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/DamageNumbers.h"
#include "Data/AranyaCombatTuning.h"
#include "DamageNumberWidget.generated.h"

class UTextBlock;
class UCanvasPanel;
class UCanvasPanelSlot;

/** Per-entry presentation state for the floating damage number pool. */
USTRUCT()
struct FDamageNumberEntry
{
    GENERATED_BODY()

    UPROPERTY()
    TObjectPtr<UTextBlock> Text = nullptr;

    UPROPERTY()
    TObjectPtr<UCanvasPanelSlot> Slot = nullptr;

    FVector WorldLocation = FVector::ZeroVector;
    float Age = 0.f;
    float Lifetime = 0.8f;
    float HorizontalOffset = 0.f;
    bool bActive = false;
    EDamageNumberType Type = EDamageNumberType::Health;
    float Amount = 0.f;
};

/**
 * Dedicated overlay for floating damage numbers. Built entirely in C++ so it is
 * independent of the boss HUD and its visibility.
 */
UCLASS(Blueprintable)
class MYPROJECT_API UDamageNumberWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UDamageNumberWidget(const FObjectInitializer& ObjectInitializer);

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /** Bind to a tuning asset (or null for safe native defaults). */
    UFUNCTION(BlueprintCallable, Category="Damage Numbers")
    void SetTuning(UAranyaCombatTuning* InTuning);

    /** Number of pooled entries (for tests/inspection). */
    UFUNCTION(BlueprintPure, Category="Damage Numbers")
    int32 GetPoolSize() const { return Pool.Num(); }

    /** Number of currently active entries (for tests/inspection). */
    UFUNCTION(BlueprintPure, Category="Damage Numbers")
    int32 GetActiveCount() const
    {
        int32 Count = 0;
        for (const FDamageNumberEntry& Entry : Pool) if (Entry.bActive) ++Count;
        return Count;
    }

    /** Entry point invoked by the world router. */
    UFUNCTION()
    void HandleDamageNumber(float Amount, FVector WorldLocation, EDamageNumberType Type);

    void SpawnTestNumber(float Amount, FVector WorldLocation, float LifetimeOverride);

    friend class FDarkForestDamageNumberPoolLifecycle;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

    int32 AcquireEntry();
    void ResetEntry(FDamageNumberEntry& Entry);
    void DeactivateEntry(FDamageNumberEntry& Entry);

    // Tuning accessors with safe native defaults.
    bool IsEffectEnabled() const;
    float GetLifetime() const;
    float GetRise() const;
    float GetPopScale() const;
    float GetPopDuration() const;
    float GetFadeFraction() const;
    FLinearColor GetColor(EDamageNumberType Type) const;
    float GetNormalFontSize() const;
    float GetDeathblowFontSize() const;
    int32 GetMaxEntries() const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage Numbers")
    int32 DefaultMaxEntries = 24;

    UPROPERTY()
    TObjectPtr<UCanvasPanel> RootCanvas = nullptr;

    UPROPERTY()
    TArray<FDamageNumberEntry> Pool;

    UPROPERTY()
    TObjectPtr<UAranyaCombatTuning> Tuning = nullptr;

    bool bBound = false;
};
