#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DemoMenuWidget.generated.h"
class UVerticalBox;
class UButton;
class UComboBoxString;
class USlider;
class UTextBlock;
class USoundMix;
class UDemoFrontendState;

/** Code-built title/pause/settings UI. No widget Blueprint graph edits. */
UCLASS()
class MYPROJECT_API UDemoMenuWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void ShowMenu(bool bTitle);
    void CloseMenu();
    bool IsMenuOpen() const { return bOpen; }
    bool IsTitleMenu() const { return bOpen && bTitleContext; }
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry&,const FKeyEvent&) override;
private:
    UDemoFrontendState* State() const;
    void BuildMain();
    void Heading(const FString& Text);
    UButton* Button(const FString& Text);
    void Text(const FString& Value);
    void ApplyAudio();
    UFUNCTION() void NewGame();
    UFUNCTION() void ContinueGame();
    UFUNCTION() void Resume();
    UFUNCTION() void RestartCheckpoint();
    UFUNCTION() void Settings();
    UFUNCTION() void ApplySettings();
    UFUNCTION() void Credits();
    UFUNCTION() void Back();
    UFUNCTION() void ReturnToTitle();
    UFUNCTION() void Quit();
    UPROPERTY() TObjectPtr<UVerticalBox> Panel;
    UPROPERTY() TObjectPtr<UComboBoxString> Quality;
    UPROPERTY() TObjectPtr<UComboBoxString> Resolution;
    UPROPERTY() TObjectPtr<UComboBoxString> Display;
    UPROPERTY() TObjectPtr<USlider> Volume;
    UPROPERTY() TObjectPtr<USlider> Sensitivity;
    UPROPERTY() TObjectPtr<UTextBlock> Status;
    UPROPERTY() TObjectPtr<USoundMix> VolumeMix;
    bool bOpen = false;
    bool bTitleContext = true;
    bool bOwnsPause = false;
    bool bConfirmNewGame = false;
};
