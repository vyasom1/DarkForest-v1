#include "World/ShrineCheckpoint.h"
#include "Components/SphereComponent.h"
#include "Player/DarkForestHero.h"
#include "Core/DarkForestGameMode.h"
#include "Core/DarkForestLog.h"
#include "World/DarkForestStory.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

AShrineCheckpoint::AShrineCheckpoint()
{
    PrimaryActorTick.bCanEverTick = false;

    ActivationSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ActivationSphere"));
    SetRootComponent(ActivationSphere);
    ActivationSphere->SetSphereRadius(ActivationRadius);
    ActivationSphere->SetCollisionProfileName(TEXT("Trigger"));
    ActivationSphere->SetGenerateOverlapEvents(true);

    static ConstructorHelpers::FObjectFinder<USoundBase> BellFinder(
        TEXT("/Game/AutoAssets/Audio/Placeholder/PLACEHOLDER_ShrineBell.PLACEHOLDER_ShrineBell"));
    if (BellFinder.Succeeded())
    {
        ShrineBellSound = BellFinder.Object;
    }
}

void AShrineCheckpoint::BeginPlay()
{
    Super::BeginPlay();

    RespawnTransform = GetActorTransform();
    if (ActivationSphere)
    {
        ActivationSphere->SetSphereRadius(ActivationRadius);
        ActivationSphere->OnComponentBeginOverlap.AddDynamic(this, &AShrineCheckpoint::HandleBeginOverlap);
    }
}

bool AShrineCheckpoint::Activate(ADarkForestHero* Hero)
{
    if (!IsValid(Hero) || !Hero->IsAlive())
    {
        return false;
    }
    if (const auto* Mode = Cast<ADarkForestGameMode>(UGameplayStatics::GetGameMode(this)))
        if (const auto* Story = Mode->GetStory(); Story && !Story->AllowsShrine(this)) return false;

    if (!bActivated)
    {
        bActivated = true;
        RespawnTransform = GetActorTransform();

        if (ShrineBellSound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, ShrineBellSound, GetActorLocation());
        }

        OnShrineActivated(Hero);

        if (ADarkForestGameMode* GameMode = Cast<ADarkForestGameMode>(UGameplayStatics::GetGameMode(this)))
        {
            GameMode->RegisterShrine(this);
        }

        UE_LOG(LogDarkForest, Log, TEXT("Shrine checkpoint activated: %s"), *GetName());
    }

    if (bHealOnTouch)
    {
        Hero->RestoreVitals();
    }
    return true;
}

FTransform AShrineCheckpoint::GetRespawnTransform() const
{
    return bActivated ? RespawnTransform : GetActorTransform();
}

void AShrineCheckpoint::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    (void)OverlappedComponent;
    (void)OtherComp;
    (void)OtherBodyIndex;
    (void)bFromSweep;
    (void)SweepResult;

    if (!bActivateOnOverlap)
    {
        return;
    }
    if (ADarkForestHero* Hero = Cast<ADarkForestHero>(OtherActor))
    {
        Activate(Hero);
    }
}
