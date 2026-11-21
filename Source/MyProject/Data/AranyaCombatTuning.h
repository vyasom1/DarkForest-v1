#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AranyaCombatTuning.generated.h"

/**
 * Data-driven tuning bundle for the Aranya combat kit.
 */
class UCameraShakeBase;
class UParticleSystem;

UCLASS(BlueprintType)
class MYPROJECT_API UAranyaCombatTuning : public UDataAsset
{
    GENERATED_BODY()

public:
    /** Hit-stop duration (seconds of frozen world time). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HitStop")
    float HitStopDuration = 0.08f;

    /** Hit-stop time scale applied during the hit-stop window. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="HitStop")
    float HitStopTimeScale = 0.1f;

    /** Camera shake played on light hits. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
    TSubclassOf<UCameraShakeBase> LightHitShake;

    /** Camera shake played on heavy hits. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
    TSubclassOf<UCameraShakeBase> HeavyHitShake;

    /** VFX spawned at hit locations. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    TObjectPtr<UParticleSystem> HitSparkEffect;

    /** Dodge duration. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
    float DodgeDuration = 0.5f;

    /** Dodge invulnerability window. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge")
    float DodgeInvulnerabilityWindow = 0.3f;

    // -----------------------------------------------------------------------
    // Floating damage numbers
    // -----------------------------------------------------------------------

    /** Master switch for floating damage numbers. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DamageNumbers")
    bool bDamageNumbersEnabled = true;

    /** Lifetime of a floating number in seconds. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DamageNumbers", meta=(ClampMin="0.1", ClampMax="5.0"))
    float DamageNumberLifetime = 0.8f;

    /** Vertical rise in viewport UI units over the lifetime. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DamageNumbers", meta=(ClampMin="0.0"))
    float DamageNumberRise = 100.f;

    /** Pop-in scale applied at spawn. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DamageNumbers", meta=(ClampMin="1.0", ClampMax="3.0"))
    float DamageNumberPopScale = 1.3f;

    /** Duration of the pop-in scale animation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DamageNumbers", meta=(ClampMin="0.0", ClampMax="1.0"))
    float DamageNumberPopDuration = 0.1f;

    /** Fraction of lifetime at which fade-out begins. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DamageNumbers", meta=(ClampMin="0.0", ClampMax="1.0"))
    float DamageNumberFadeFraction = 0.7f;

    /** Color used for health damage numbers. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DamageNumbers")
    FLinearColor DamageNumberHealthColor = FLinearColor::White;

    /** Color used for posture damage numbers. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DamageNumbers")
    FLinearColor DamageNumberPostureColor = FLinearColor(1.f, 0.78f, 0.2f, 1.f);

    /** Color used for successful deflect numbers. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DamageNumbers")
    FLinearColor DamageNumberDeflectColor = FLinearColor(0.2f, 0.85f, 1.f, 1.f);

    /** Color used for deathblow numbers. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DamageNumbers")
    FLinearColor DamageNumberDeathblowColor = FLinearColor(1.f, 0.2f, 0.15f, 1.f);

    /** Font size for normal damage numbers. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DamageNumbers", meta=(ClampMin="4.0", ClampMax="96.0"))
    float DamageNumberFontSize = 24.f;

    /** Font size for deathblow numbers. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DamageNumbers", meta=(ClampMin="4.0", ClampMax="96.0"))
    float DamageNumberDeathblowFontSize = 36.f;

    /** Maximum number of simultaneously active damage numbers. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DamageNumbers", meta=(ClampMin="1", ClampMax="256"))
    int32 MaxDamageNumbersActive = 24;
};
