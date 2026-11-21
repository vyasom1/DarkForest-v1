#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DarkForestHUD.generated.h"
class UDemoMenuWidget;
class UTexture2D;

UCLASS()
class MYPROJECT_API ADarkForestHUD : public AHUD
{
    GENERATED_BODY()
public:
    ADarkForestHUD();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void DrawHUD() override;
private:
    void TogglePauseMenu();
    void SkipStoryDialogue();
    UPROPERTY() TObjectPtr<UDemoMenuWidget> Menu;
    UPROPERTY() TObjectPtr<UTexture2D> StoryIllustrations;
};
