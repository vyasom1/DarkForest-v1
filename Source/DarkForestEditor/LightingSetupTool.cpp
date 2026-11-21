#include "LightingSetupTool.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/SkyLight.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Components/SkyLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"

#define LOCTEXT_NAMESPACE "DarkForestLighting"

namespace
{
    /** Actors created by this tool carry this tag so re-runs find them. */
    const FName DarkForestLightingTag("DarkForestLighting");

    template <typename T>
    T* FindTaggedActor(UWorld* World)
    {
        for (TActorIterator<T> It(World); It; ++It)
        {
            if (It->Tags.Contains(DarkForestLightingTag))
            {
                return *It;
            }
        }
        return nullptr;
    }

    bool LabelOrTagMentionsFire(const AActor* Actor)
    {
        const FString Label = Actor->GetActorLabel().ToLower();
        if (Label.Contains("campfire") || Label.Contains("fire"))
        {
            return true;
        }
        for (const FName& Tag : Actor->Tags)
        {
            const FString TagString = Tag.ToString().ToLower();
            if (TagString.Contains("campfire") || TagString.Contains("fire"))
            {
                return true;
            }
        }
        return false;
    }
}

bool UDarkForestLightingTool::ApplyDarkForestLighting()
{
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("DarkForest lighting: no editor world available."));
        return false;
    }

    GEditor->BeginTransaction(NSLOCTEXT("DarkForest", "ApplyLighting", "Apply Dark Forest Lighting"));

    // SkyLight: movable, realtime capture, captured-scene source.
    ASkyLight* SkyLight = FindTaggedActor<ASkyLight>(World);
    if (!SkyLight)
    {
        SkyLight = World->SpawnActor<ASkyLight>();
        SkyLight->Tags.Add(DarkForestLightingTag);
    }
    SkyLight->Modify();
    SkyLight->MarkPackageDirty();
    if (USkyLightComponent* Sky = Cast<USkyLightComponent>(
        SkyLight->GetLightComponent()))
    {
        Sky->Modify();
        Sky->Mobility = EComponentMobility::Movable;
        Sky->bRealTimeCapture = true;
        Sky->Intensity = 2.0f;
        Sky->SourceType = ESkyLightSourceType::SLS_CapturedScene;
    }

    // Primary directional light: cool moonlight with strong volumetric fog scattering.
    ADirectionalLight* Directional = FindTaggedActor<ADirectionalLight>(World);
    if (!Directional)
    {
        Directional = World->SpawnActor<ADirectionalLight>();
        Directional->Tags.Add(DarkForestLightingTag);
    }
    Directional->Modify();
    Directional->MarkPackageDirty();
    if (UDirectionalLightComponent* Moon = Cast<UDirectionalLightComponent>(
        Directional->GetLightComponent()))
    {
        Moon->Modify();
        Moon->Intensity = 3.0f;
        Moon->SetLightColor(FLinearColor(0.6f, 0.7f, 1.0f));
        Moon->SetCastShadows(true);
        Moon->ShadowBias = 0.5f;
        Moon->SetVolumetricScatteringIntensity(3.0f);
    }

    // Campfire / fire point lights: warm, small-source, physical temperature.
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!LabelOrTagMentionsFire(Actor))
        {
            continue;
        }
        TArray<UPointLightComponent*> PointLights;
        Actor->GetComponents(PointLights);
        for (UPointLightComponent* Point : PointLights)
        {
            Point->Modify();
            Point->Intensity = 2000.f;
            Point->AttenuationRadius = 400.f;
            Point->SourceRadius = 20.f;
            Point->SetLightColor(FLinearColor(1.0f, 0.6f, 0.25f));
            Point->SetTemperature(2500.f);
            Point->SetUseTemperature(true);
        }
        Actor->Modify();
        Actor->MarkPackageDirty();
    }

    // Exponential height fog: dense, short falloff, volumetric.
    AExponentialHeightFog* Fog = FindTaggedActor<AExponentialHeightFog>(World);
    if (!Fog)
    {
        Fog = World->SpawnActor<AExponentialHeightFog>();
        Fog->Tags.Add(DarkForestLightingTag);
    }
    Fog->Modify();
    Fog->MarkPackageDirty();
    if (UExponentialHeightFogComponent* FogComp = Fog->GetComponentByClass<UExponentialHeightFogComponent>())
    {
        FogComp->Modify();
        FogComp->FogDensity = 0.03f;
        FogComp->FogHeightFalloff = 0.2f;
        FogComp->bEnableVolumetricFog = true;
        FogComp->VolumetricFogScatteringDistribution = 0.6f;
    }

    // Unbound post process: locked exposure, gentle bloom and vignette, DOF on.
    APostProcessVolume* PostProcess = FindTaggedActor<APostProcessVolume>(World);
    if (!PostProcess)
    {
        PostProcess = World->SpawnActor<APostProcessVolume>();
        PostProcess->Tags.Add(DarkForestLightingTag);
    }
    PostProcess->Modify();
    PostProcess->MarkPackageDirty();
    PostProcess->bUnbound = true;
    PostProcess->Settings.bOverride_AutoExposureMinBrightness = true;
    PostProcess->Settings.AutoExposureMinBrightness = 1.0f;
    PostProcess->Settings.bOverride_AutoExposureMaxBrightness = true;
    PostProcess->Settings.AutoExposureMaxBrightness = 1.0f;
    PostProcess->Settings.bOverride_BloomIntensity = true;
    PostProcess->Settings.BloomIntensity = 0.4f;
    PostProcess->Settings.bOverride_VignetteIntensity = true;
    PostProcess->Settings.VignetteIntensity = 0.3f;
    PostProcess->Settings.bOverride_DepthOfFieldFocalDistance = true;
    PostProcess->Settings.DepthOfFieldFocalDistance = 100.0f;
    PostProcess->Settings.bOverride_DepthOfFieldFstop = true;
    PostProcess->Settings.DepthOfFieldFstop = 2.0f;

    GEditor->EndTransaction();

    UE_LOG(LogTemp, Display, TEXT("DarkForest lighting setup applied."));
    return true;
}

#undef LOCTEXT_NAMESPACE
