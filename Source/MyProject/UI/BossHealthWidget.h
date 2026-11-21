#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BossHealthWidget.generated.h"

class AGoblinKing;
class UProgressBar;
class UTextBlock;

/** Native boss HUD with optional named controls supplied by a Blueprint override. */
UCLASS(Blueprintable)
class MYPROJECT_API UBossHealthWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Boss HUD")
    void BindToBoss(AGoblinKing* InBoss);

    UFUNCTION(BlueprintCallable, Category="Boss HUD")
    void RefreshFromBoss();

    UFUNCTION(BlueprintCallable, Category="Boss HUD")
    void ShowDeathState();

    UFUNCTION(BlueprintCallable, Category="Boss HUD")
    void ShowVictoryState();

    UFUNCTION(BlueprintCallable, Category="Boss HUD")
    void ShowAliveState();

    UFUNCTION(BlueprintPure, Category="Boss HUD")
    AGoblinKing* GetBoss() const { return Boss; }

    /** Hook for authored visual polish; C++ already sets the bound widgets. */
    UFUNCTION(BlueprintImplementableEvent, Category="Boss HUD")
    void OnBossUpdated(int32 SegmentsRemaining, float SegmentFraction, float PostureFraction, int32 Phase);

    UFUNCTION(BlueprintImplementableEvent, Category="Boss HUD")
    void OnEncounterEnded(bool bVictory);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UProgressBar> SegmentBar1;

    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UProgressBar> SegmentBar2;

    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UProgressBar> SegmentBar3;

    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UProgressBar> PostureBar;

    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> PhaseText;

    UPROPERTY(meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> BossNameText;

    UPROPERTY(BlueprintReadOnly, Category="Boss HUD")
    TObjectPtr<AGoblinKing> Boss;

private:
    bool bVictoryShown = false;
    bool bDefeat = false;
    float RefreshAccumulator = 0.f;
};
