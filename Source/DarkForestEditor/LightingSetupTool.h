#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LightingSetupTool.generated.h"

/**
 * Editor-only lighting setup entry point.
 *
 * ApplyDarkForestLighting is exposed with CallInEditor so it can also be
 * bound from Editor Utility Blueprints; the menu entry in
 * Tools -> DarkForest -> Apply Lighting Setup calls the same function.
 */
UCLASS()
class DARKFORESTEDITOR_API UDarkForestLightingTool : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(CallInEditor, BlueprintCallable, Category = "DarkForest|Lighting")
    static bool ApplyDarkForestLighting();
};
