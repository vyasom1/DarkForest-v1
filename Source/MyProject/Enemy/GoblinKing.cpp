#include "Enemy/GoblinKing.h"
#include "Enemy/GoblinKingPhaseComponent.h"
#include "Enemy/GoblinKingTypes.h"
#include "Player/DarkForestHero.h"
#include "Core/DarkForestGameMode.h"
#include "Core/DarkForestLog.h"
#include "AIController.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"

AGoblinKing::AGoblinKing()
{
    MeleeRange = 340.f;
    DeadActorLifetimeSeconds = 30.f; // long enough for a shrine respawn to revive in place

    PhaseComponent = CreateDefaultSubobject<UGoblinKingPhaseComponent>(TEXT("KingPhase"));

    static ConstructorHelpers::FObjectFinder<UAnimSequence> Heavy(
        TEXT("/Game/AutoAssets/Characters/Goblin/Goblin_LOD0_Anim_GoblinArmature_Attack-two_handedAction"));
    HeavySwing = Heavy.Object;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CrownMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PointMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Gold(TEXT("/Game/StarterContent/Materials/M_Metal_Gold.M_Metal_Gold"));
    if (CrownMesh.Succeeded() && PointMesh.Succeeded() && Gold.Succeeded())
    {
        // The imported head bone is scaled up and its local -Y axis points up.
        USceneComponent* CrownAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("KingCrownAnchor"));
        CrownAnchor->SetupAttachment(GetMesh(), TEXT("Head"));
        CrownAnchor->SetAbsolute(false, false, true);
        CrownAnchor->SetRelativeLocation(FVector(0.f, -.75f, 0.f));
        CrownAnchor->SetRelativeRotation(FRotationMatrix::MakeFromZ(FVector(0.f, -1.f, 0.f)).Rotator());
        const auto AddCrownPart = [this, CrownAnchor](FName Name, UStaticMesh* PartMesh, FVector Offset, FVector Scale)
        {
            UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
            Part->SetupAttachment(CrownAnchor);
            Part->SetStaticMesh(PartMesh);
            Part->SetMaterial(0, Gold.Object);
            Part->SetRelativeLocation(Offset);
            Part->SetRelativeScale3D(Scale);
            Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        };
        AddCrownPart(TEXT("KingCrownBand"), CrownMesh.Object, FVector::ZeroVector, FVector(.34f, .32f, .045f));
        AddCrownPart(TEXT("KingCrownPointL"), PointMesh.Object, FVector(-12.f, 0.f, 6.f), FVector(.09f, .09f, .13f));
        AddCrownPart(TEXT("KingCrownPointC"), PointMesh.Object, FVector(0.f, 0.f, 10.f), FVector(.10f, .10f, .20f));
        AddCrownPart(TEXT("KingCrownPointR"), PointMesh.Object, FVector(12.f, 0.f, 6.f), FVector(.09f, .09f, .13f));
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> TellFinder(
        TEXT("/Game/AutoAssets/Audio/Goblin/goblin-2.goblin-2"));
    static ConstructorHelpers::FObjectFinder<USoundBase> HurtFinder(
        TEXT("/Game/AutoAssets/Audio/Goblin/goblin-12.goblin-12"));
    if (TellFinder.Succeeded()) AttackTellSound = TellFinder.Object;
    if (HurtFinder.Succeeded()) HurtVoiceSound = HurtFinder.Object;
}

void AGoblinKing::BeginPlay()
{
    Super::BeginPlay();

    SpawnLocation = GetActorLocation();
    SpawnRotation = GetActorRotation();
    Health = MaxHealth;

    if (PhaseComponent)
    {
        ApplyAttackData();
    }

    if (ADarkForestGameMode* GameMode = Cast<ADarkForestGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        GameMode->RegisterBoss(this);
    }
}

void AGoblinKing::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearKingTimers();
    Super::EndPlay(EndPlayReason);
}

void AGoblinKing::ApplyAttackData()
{
    if (!PhaseComponent)
    {
        return;
    }

    if (IsValid(AttackData) && AttackData->Phases.Num() > 0)
    {
        PhaseComponent->SetPhases(AttackData->Phases);
        PhaseComponent->MinWindupSeconds = FMath::Max(0.35f, AttackData->MinWindupSeconds);
        PhaseComponent->MaxConsecutiveAttacks = FMath::Max(1, AttackData->MaxConsecutiveAttacks);
        PhaseComponent->MaxAttackRepeats = FMath::Max(0, AttackData->MaxAttackRepeats);
    }
    else
    {
        PhaseComponent->EnsureDefaultPhases();
    }
}

void AGoblinKing::ReceiveSwordHit(float HealthDamage, float PostureDamage, ADarkForestHero* Source)
{
    (void)Source;
    if (!IsAlive() || !PhaseComponent)
    {
        return;
    }

    if (HealthDamage > 0.f && HurtVoiceSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, HurtVoiceSound, GetActorLocation());
    }

    const bool bWasBroken = PhaseComponent->IsPostureBroken();
    const float PostureMultiplier = PhaseComponent->GetIncomingPostureMultiplier();
    PhaseComponent->ApplySegmentDamage(HealthDamage, PostureDamage * PostureMultiplier);

    if (!bWasBroken && PhaseComponent->IsPostureBroken())
    {
        EnterStagger();
    }
}

void AGoblinKing::ReceiveDeflect(float PostureDamage)
{
    if (!IsAlive() || !PhaseComponent)
    {
        return;
    }

    // A deflect cancels the current tell/chain and feeds the posture gauge.
    const bool bInterruptedAttack = bKingAttacking;
    if (bInterruptedAttack)
    {
        GetWorldTimerManager().ClearTimer(KingTellTimer);
        CancelAttackVisual();
    }

    const bool bWasBroken = PhaseComponent->IsPostureBroken();
    PhaseComponent->ApplySegmentDamage(0.f, PostureDamage);
    if (!bWasBroken && PhaseComponent->IsPostureBroken())
    {
        EnterStagger();
    }
    else if (bInterruptedAttack)
    {
        FinishAttackSequence();
    }
}

bool AGoblinKing::TryBeginMeleeAttack(ADarkForestHero* Target)
{
    if (!IsAlive() || bStaggered || bKingAttacking || !PhaseComponent || !GetWorld() ||
        !IsValid(Target) || !Target->IsAlive() || IsRecovering())
    {
        return false;
    }

    if (FVector::Dist2D(GetActorLocation(), Target->GetActorLocation()) > MeleeRange ||
        FMath::Abs(GetActorLocation().Z - Target->GetActorLocation().Z) > 135.f || !CanCommitMelee(Target))
    {
        return false;
    }

    const int32 AttackIndex = PhaseComponent->ChooseNextAttackIndex();
    const FGoblinKingAttack* Attack = PhaseComponent->GetAttack(AttackIndex);
    if (!Attack || FVector::Dist2D(GetActorLocation(), Target->GetActorLocation()) > Attack->Range)
    {
        return false;
    }

    CurrentAttackIndex = AttackIndex;
    CurrentChainHit = 1;
    KingTarget = Target;
    bKingAttacking = true;
    bTellActive = true;
    PhaseComponent->RegisterAttackStarted(AttackIndex);

    FaceMeleeTarget(Target);

    OnAttackTelegraphed();
    OnAttackTell(Attack->AttackId, Attack->DisplayName, Attack->bUnblockable,
        Attack->bJumpable, Attack->TellColor);
    if (Attack->TellSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, Attack->TellSound, GetActorLocation());
    }
    else if (AttackTellSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, AttackTellSound, GetActorLocation());
    }
    PlayKingAttackVisual(*Attack, PhaseComponent->GetEffectiveWindup(AttackIndex));

    GetWorldTimerManager().SetTimer(KingTellTimer, this, &AGoblinKing::ResolveChainHit,
        PhaseComponent->GetEffectiveWindup(AttackIndex), false);
    return true;
}

void AGoblinKing::PlayKingAttackVisual(const FGoblinKingAttack& Attack, float Windup)
{
    const bool bHeavy = Attack.AttackId == FName(TEXT("King.DelayedHeavy"));
    PlayAttackVisual(Attack.Montage ? Attack.Montage.Get() : AttackMontage.Get(), Windup,
        bHeavy ? HeavySwing.Get() : nullptr);
}

void AGoblinKing::ResolveChainHit()
{
    bTellActive = false;

    if (!IsAlive() || bStaggered || !GetWorld() || !PhaseComponent || !bKingAttacking)
    {
        return;
    }

    const FGoblinKingAttack* Attack = PhaseComponent->GetAttack(CurrentAttackIndex);
    if (!Attack)
    {
        FinishAttackSequence();
        return;
    }

    ADarkForestHero* Target = KingTarget.Get();
    if (IsValid(Target) && Target->IsAlive())
    {
        const bool bInRange = IsInCommittedArc(Target, Attack->Range);
        bool bJumped = false;
        if (Attack->bJumpable)
        {
            if (const UCharacterMovementComponent* Movement = Target->GetCharacterMovement())
            {
                bJumped = Movement->IsFalling();
            }
        }

        if (bInRange && !bJumped)
        {
            Target->ReceiveMeleeHit(Attack->Damage, Attack->PostureDamage, this, Attack->bUnblockable);
            // ReceiveMeleeHit can synchronously deflect this hit and end the chain.
            if (!bKingAttacking || bStaggered || !IsAlive()) return;
        }
    }

    if (CurrentChainHit < Attack->ChainHits)
    {
        ++CurrentChainHit;
        bTellActive = true;

        OnAttackTell(Attack->AttackId, Attack->DisplayName, Attack->bUnblockable,
            Attack->bJumpable, Attack->TellColor);
        const float NextWindup = FMath::Max(PhaseComponent->MinWindupSeconds, Attack->ChainIntervalSeconds);
        if (USoundBase* Tell = Attack->TellSound ? Attack->TellSound.Get() : AttackTellSound.Get())
            UGameplayStatics::PlaySoundAtLocation(this, Tell, GetActorLocation());
        PlayKingAttackVisual(*Attack, NextWindup);
        GetWorldTimerManager().SetTimer(KingTellTimer, this, &AGoblinKing::ResolveChainHit,
            NextWindup, false);
        return;
    }

    FinishAttackSequence();
}

void AGoblinKing::FinishAttackSequence()
{
    bKingAttacking = false;
    bTellActive = false;

    if (!PhaseComponent)
    {
        return;
    }

    const bool bForcedRecovery = PhaseComponent->RegisterAttackFinished();
    if (PhaseComponent->IsPunishWindowOpen()) OnPunishWindowOpened();
    if (bForcedRecovery && GetWorld())
    {
        float Recovery = 1.5f;
        const TArray<FGoblinKingPhase>& Phases = PhaseComponent->GetPhases();
        const int32 PhaseIndex = PhaseComponent->GetPhase() - 1;
        if (Phases.IsValidIndex(PhaseIndex))
        {
            Recovery = FMath::Max(0.2f, Phases[PhaseIndex].RecoverySeconds * 1.5f);
        }
        GetWorldTimerManager().SetTimer(KingRecoveryTimer, this, &AGoblinKing::EndForcedRecovery,
            Recovery, false);
    }
}

void AGoblinKing::EndForcedRecovery()
{
    if (PhaseComponent)
    {
        PhaseComponent->RegisterRecoveryFinished();
    }
    OnBossRecovered();
}

void AGoblinKing::EnterStagger()
{
    if (!GetWorld())
    {
        return;
    }

    ClearKingTimers();
    bKingAttacking = false;
    bTellActive = false;
    bStaggered = true;
    CancelAttackVisual();

    if (AAIController* AI = Cast<AAIController>(GetController()))
    {
        AI->StopMovement();
    }
    if (IsValid(StaggerMontage))
    {
        PlayAnimMontage(StaggerMontage);
    }
    OnPostureBroken();

    GetWorldTimerManager().SetTimer(KingStaggerTimer, this, &AGoblinKing::RecoverFromStagger,
        PostureBrokenSeconds, false);
}

void AGoblinKing::RecoverFromStagger()
{
    bStaggered = false;
    if (PhaseComponent)
    {
        PhaseComponent->RecoverPosture();
    }
    OnBossRecovered();
}

void AGoblinKing::ApplyDeathblow(ADarkForestHero* Source)
{
    (void)Source;
    if (!IsAlive() || !PhaseComponent || !PhaseComponent->IsPostureBroken())
    {
        return;
    }

    ClearKingTimers();
    bKingAttacking = false;
    bTellActive = false;
    bStaggered = false;
    KingTarget = nullptr;

    if (!PhaseComponent->TryDeathblow()) return;
    if (PhaseComponent->IsDefeated())
    {
        Die();
        return;
    }

    CurrentAttackIndex = INDEX_NONE;
    CurrentChainHit = 0;
    OnSegmentRemoved(PhaseComponent->GetSegmentsRemaining(), PhaseComponent->GetPhase());

    if (GetWorld())
    {
        GetWorldTimerManager().SetTimer(KingRecoveryTimer, this, &AGoblinKing::EndForcedRecovery,
            1.0f, false);
    }
}

void AGoblinKing::Die()
{
    ClearKingTimers();
    bKingAttacking = false;
    bTellActive = false;
    bStaggered = false;

    Super::Die();
    OnBossDefeated();

    if (ADarkForestGameMode* GameMode = Cast<ADarkForestGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        GameMode->NotifyBossDefeated(this);
    }

    UE_LOG(LogDarkForest, Log, TEXT("Goblin King defeated: %s"), *GetName());
}

void AGoblinKing::ResetEncounter(bool bReturnToSpawn)
{
    ClearKingTimers();
    bKingAttacking = false;
    bTellActive = false;
    bStaggered = false;
    CurrentAttackIndex = INDEX_NONE;
    CurrentChainHit = 0;
    KingTarget = nullptr;
    Health = MaxHealth;

    if (PhaseComponent)
    {
        PhaseComponent->ResetEncounter();
    }

    SetActorEnableCollision(true);
    SetActorHiddenInGame(false);
    SetLifeSpan(0.f);
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->SetMovementMode(MOVE_Walking);
    }
    if (bReturnToSpawn)
    {
        SetActorLocationAndRotation(SpawnLocation, SpawnRotation);
    }
    if (AAIController* AI = Cast<AAIController>(GetController()))
    {
        AI->StopMovement();
    }
    OnBossRecovered();
}

void AGoblinKing::SetPhase(int32 NewPhase)
{
    if (!PhaseComponent)
    {
        return;
    }

    ClearKingTimers();
    bKingAttacking = false;
    bTellActive = false;
    bStaggered = false;
    CurrentAttackIndex = INDEX_NONE;
    CurrentChainHit = 0;
    Health = MaxHealth;
    SetActorEnableCollision(true);
    SetLifeSpan(0.f);

    PhaseComponent->SetPhase(NewPhase);
    OnBossRecovered();
}

void AGoblinKing::ClearKingTimers()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(KingTellTimer);
        World->GetTimerManager().ClearTimer(KingStaggerTimer);
        World->GetTimerManager().ClearTimer(KingRecoveryTimer);
    }
}

bool AGoblinKing::IsAlive() const
{
    return Super::IsAlive() && PhaseComponent && !PhaseComponent->IsDefeated() &&
        PhaseComponent->GetSegmentsRemaining() > 0;
}

bool AGoblinKing::IsPostureBroken() const
{
    return PhaseComponent ? PhaseComponent->IsPostureBroken() : Super::IsPostureBroken();
}

bool AGoblinKing::IsTelegraphing() const
{
    return bTellActive;
}

bool AGoblinKing::IsRecovering() const
{
    return !bKingAttacking && PhaseComponent && GetWorld() &&
        (!PhaseComponent->CanStartAttack() || GetWorldTimerManager().IsTimerActive(KingRecoveryTimer));
}

float AGoblinKing::GetHealthFraction() const
{
    if (!PhaseComponent)
    {
        return Super::GetHealthFraction();
    }

    const float SegmentFraction = PhaseComponent->GetSegmentHealthFraction();
    const float Total = static_cast<float>(FMath::Max(1, PhaseComponent->TotalSegments));
    return FMath::Clamp((PhaseComponent->GetSegmentsRemaining() - 1 + SegmentFraction) / Total, 0.f, 1.f);
}

float AGoblinKing::GetPostureFraction() const
{
    return PhaseComponent ? PhaseComponent->GetPostureFraction() : Super::GetPostureFraction();
}

int32 AGoblinKing::GetPhaseNumber() const
{
    return PhaseComponent ? PhaseComponent->GetPhase() : 1;
}

int32 AGoblinKing::GetSegmentsRemaining() const
{
    return PhaseComponent ? PhaseComponent->GetSegmentsRemaining() : 0;
}

float AGoblinKing::GetSegmentHealthFraction() const
{
    return PhaseComponent ? PhaseComponent->GetSegmentHealthFraction() : 0.f;
}

FText AGoblinKing::GetCurrentAttackName() const
{
    if (PhaseComponent)
    {
        if (const FGoblinKingAttack* Attack = PhaseComponent->GetAttack(CurrentAttackIndex))
        {
            if (Attack->ChainHits > 1 && bTellActive)
                return FText::Format(NSLOCTEXT("GoblinKing", "ChainTell", "{0} {1}/{2}"),
                    Attack->DisplayName, CurrentChainHit, Attack->ChainHits);
            return Attack->DisplayName;
        }
    }
    return FText::FromString(TEXT("Idle"));
}

bool AGoblinKing::IsCurrentAttackUnblockable() const
{
    const FGoblinKingAttack* Attack = PhaseComponent ? PhaseComponent->GetAttack(CurrentAttackIndex) : nullptr;
    return Attack && Attack->bUnblockable;
}

bool AGoblinKing::IsCurrentAttackJumpable() const
{
    const FGoblinKingAttack* Attack = PhaseComponent ? PhaseComponent->GetAttack(CurrentAttackIndex) : nullptr;
    return Attack && Attack->bJumpable;
}

bool AGoblinKing::IsPunishWindowOpen() const
{
    return PhaseComponent && PhaseComponent->IsPunishWindowOpen();
}

FLinearColor AGoblinKing::GetCurrentTellColor() const
{
    const FGoblinKingAttack* Attack = PhaseComponent ? PhaseComponent->GetAttack(CurrentAttackIndex) : nullptr;
    return Attack ? Attack->TellColor : FLinearColor(1.f, 0.5f, 0.1f, 1.f);
}
