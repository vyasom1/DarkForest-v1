#include "DarkForestEditor.h"

#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "LevelEditor.h"

#include "LightingSetupTool.h"

#define LOCTEXT_NAMESPACE "DarkForestEditor"

void FDarkForestEditorModule::StartupModule()
{
    MenuExtender = MakeShareable(new FExtender());
    MenuExtender->AddMenuExtension(
        "Tools",
        EExtensionHook::After,
        TSharedPtr<FUICommandList>(),
        FMenuExtensionDelegate::CreateStatic(&FDarkForestEditorModule::AddToolsMenuExtension));

    FLevelEditorModule& LevelEditorModule =
        FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
    LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
}

void FDarkForestEditorModule::ShutdownModule()
{
    if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
    {
        FLevelEditorModule& LevelEditorModule =
            FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
        LevelEditorModule.GetMenuExtensibilityManager()->RemoveExtender(MenuExtender);
    }
    MenuExtender.Reset();
}

void FDarkForestEditorModule::AddToolsMenuExtension(FMenuBuilder& Builder)
{
    Builder.AddSubMenu(
        LOCTEXT("DarkForestSubmenuLabel", "DarkForest"),
        LOCTEXT("DarkForestSubmenuTooltip", "Dark Forest editor tools"),
        FNewMenuDelegate::CreateStatic(&FDarkForestEditorModule::BuildDarkForestSubmenu));
}

void FDarkForestEditorModule::BuildDarkForestSubmenu(FMenuBuilder& Builder)
{
    Builder.AddMenuEntry(
        LOCTEXT("ApplyLightingLabel", "Apply Lighting Setup"),
        LOCTEXT("ApplyLightingTooltip", "Applies the Dark Forest lighting setup to the level"),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateStatic(&FDarkForestEditorModule::ExecuteApplyLighting)));
}

void FDarkForestEditorModule::ExecuteApplyLighting()
{
    UDarkForestLightingTool::ApplyDarkForestLighting();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDarkForestEditorModule, DarkForestEditor)
