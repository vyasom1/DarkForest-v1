#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * DarkForest editor module. Registers the Tools -> DarkForest menu entry
 * that drives the lighting setup tool.
 */
class FDarkForestEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    static void AddToolsMenuExtension(class FMenuBuilder& Builder);
    static void BuildDarkForestSubmenu(class FMenuBuilder& Builder);
    static void ExecuteApplyLighting();

    TSharedPtr<class FExtender> MenuExtender;
};
