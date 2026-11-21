#include "Enemy/DarkForestGoblin.h"
#include "Enemy/DarkForestGoblinController.h"
#include "Player/DarkForestHero.h"
#include "Core/DarkForestLog.h"
#include "Core/DarkForestGameMode.h"
#include "World/DarkForestStory.h"
#include "AIController.h"
#include "Animation/AnimSequence.h"
#include "Enemy/GoblinCombatAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

ADarkForestGoblin::ADarkForestGoblin()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Walk(
        TEXT("/Game/AutoAssets/Characters/Goblin/Goblin_LOD0_Anim_GoblinArmature_Walk-fast"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Swing(
        TEXT("/Game/AutoAssets/Characters/Goblin/Goblin_LOD0_Anim_GoblinArmature_Attack-two_handed_002"));
    GoblinWalk = Walk.Object;
    GoblinSwing = Swing.Object;
    AIControllerClass = ADarkForestGoblinController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    // Verified imported clips from /Game/AutoAssets/Audio/Goblin (15 takes).
    // Audition may swap these in BP_DarkForestGoblin; they are editable slots.
    static ConstructorHelpers::FObjectFinder<USoundBase> TellFinder(
        TEXT("/Game/AutoAssets/Audio/Goblin/goblin-5.goblin-5"));
    static ConstructorHelpers::FObjectFinder<USoundBase> HurtFinder(
        TEXT("/Game/AutoAssets/Audio/Goblin/goblin-9.goblin-9"));
    if (TellFinder.Succeeded()) AttackTellSound = TellFinder.Object;
    if (HurtFinder.Succeeded()) HurtVoiceSound = HurtFinder.Object;
}

void ADarkForestGoblin::BeginPlay()
{
    Super::BeginPlay();
    // Character bodies must not retract the camera into the player's mesh.
    // World geometry continues to block the spring-arm camera probe.
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);
    GetMesh()->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);
    SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f));
    const USkeletalMesh* MeshAsset = GetMesh()->GetSkeletalMeshAsset();
    if (MeshAsset && GoblinWalk && MeshAsset->GetSkeleton() == GoblinWalk->GetSkeleton() &&
        !GetMesh()->GetAnimClass())
    {
        GetMesh()->SetAnimInstanceClass(UGoblinCombatAnimInstance::StaticClass());
        if (auto* Anim = Cast<UGoblinCombatAnimInstance>(GetMesh()->GetAnimInstance()))
        {
            Anim->Walk = GoblinWalk;
            UAnimSequence* Death = LoadObject<UAnimSequence>(nullptr,
                TEXT("/Game/AutoAssets/Characters/Goblin/Goblin_LOD0_Anim_GoblinArmature_Die-jump-ground-face4_test"));
            if (Death && Death->GetSkeleton() == MeshAsset->GetSkeleton()) Anim->Death = Death;
            if (GoblinSwing && MeshAsset->GetSkeleton() == GoblinSwing->GetSkeleton())
            {
                Anim->Swing = GoblinSwing;
            }
        }
    }
    Health = MaxHealth;
    Posture = 0.f;
}

void ADarkForestGoblin::PlayAttackVisual(UAnimMontage* Montage, float Windup, UAnimSequence* Alternate)
{
    if (IsValid(Montage) && PlayAnimMontage(Montage) > 0.f) return;
    if (auto* Anim = Cast<UGoblinCombatAnimInstance>(GetMesh()->GetAnimInstance()))
    {
        Anim->Swing = Alternate && GoblinSwing && Alternate->GetSkeleton() == GoblinSwing->GetSkeleton()
            ? Alternate : GoblinSwing.Get();
        Anim->AttackDuration = FMath::Max(.25f, Windup + .24f);
        Anim->AttackStart = GetWorld()->GetTimeSeconds();
        Anim->AttackCancelledAt = -1.f;
    }
}

void ADarkForestGoblin::CancelAttackVisual()
{
    StopAnimMontage();
    if (auto* Anim = Cast<UGoblinCombatAnimInstance>(GetMesh()->GetAnimInstance()))
        Anim->AttackCancelledAt = GetWorld()->GetTimeSeconds();
}

bool ADarkForestGoblin::IsRecovering() const
{
    return !bAttacking && GetWorld() && GetWorld()->GetTimeSeconds() < NextAttackTime;
}

void ADarkForestGoblin::ResetForStoryRespawn(const FVector& Location)
{
    if (!IsAlive() || IsHidden()) return;
    GetWorldTimerManager().ClearTimer(TelegraphTimer);
    AttackTarget.Reset();
    bAttacking = false;
    NextAttackTime = GetWorld()->GetTimeSeconds() + 1.f;
    CancelAttackVisual();
    if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
    SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
}

bool ADarkForestGoblin::CanCommitMelee(const ADarkForestHero* Target) const
{
    UWorld* World = GetWorld();
    if (!World || IsHidden() || !IsValid(Target) || Target->GetWorld() != World || !Target->IsAlive()) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(GoblinMeleeTell), false, this);
    Query.AddIgnoredActor(Target);
    if (World->LineTraceTestByChannel(GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Query))
        return false;

    // DarkForest has one player. There is one live wind-up or chain at a time.
    // Reading actor state avoids stale reservations after a deflect, death or reset.
    for (TActorIterator<ADarkForestGoblin> It(World); It; ++It)
        if (*It != this && It->IsAlive() && It->IsTelegraphing()) return false;
    return true;
}

bool ADarkForestGoblin::IsInCommittedArc(const ADarkForestHero* Target, float Reach) const
{
    const FVector Delta = Target->GetActorLocation() - GetActorLocation();
    if (Delta.SizeSquared2D() > FMath::Square(Reach) || FMath::Abs(Delta.Z) > 135.f) return false;
    return FMath::IsNearlyZero(Delta.SizeSquared2D()) ||
        FVector::DotProduct(GetActorForwardVector(), Delta.GetSafeNormal2D()) >= .423f;
}

void ADarkForestGoblin::FaceMeleeTarget(const ADarkForestHero* Target)
{
    if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
    SetActorRotation(FRotator(0.f, (Target->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.f));
}
void ADarkForestGoblin::ReceiveSwordHit(float HealthDamage, float PostureDamage, ADarkForestHero* Source)
{
    if (!IsAlive()) return;
    if (HealthDamage > 0.f && HurtVoiceSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, HurtVoiceSound, GetActorLocation());
    }
    Health = FMath::Max(0.f, Health - FMath::Max(0.f, HealthDamage));
    if (Health <= 0.f) { Die(); return; }
    AddPosture(PostureDamage);
}

void ADarkForestGoblin::ReceiveDeflect(float PostureDamage)
{
    if (!IsAlive()) return;
    GetWorldTimerManager().ClearTimer(TelegraphTimer);
    bAttacking = false;
    NextAttackTime = FMath::Max(NextAttackTime, GetWorld()->GetTimeSeconds() + .55f);
    CancelAttackVisual();
    AddPosture(PostureDamage);
}

void ADarkForestGoblin::AddPosture(float Amount)
{
    if (bPostureBroken || !IsAlive()) return;
    Posture = FMath::Min(MaxPosture, Posture + FMath::Max(0.f, Amount));
    if (Posture >= MaxPosture)
    {
        bPostureBroken = true;
        bAttacking = false;
        GetWorldTimerManager().ClearTimer(TelegraphTimer);
        CancelAttackVisual();
        if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
        if (IsValid(StaggerMontage)) PlayAnimMontage(StaggerMontage);
        OnPostureBroken();
        if (GetWorld())
        {
            GetWorldTimerManager().SetTimer(StaggerTimer, this, &ADarkForestGoblin::RecoverPosture,
                PostureBrokenSeconds, false);
        }
    }
}

void ADarkForestGoblin::RecoverPosture()
{
    bPostureBroken = false;
    Posture = 0.f;
}

bool ADarkForestGoblin::TryBeginMeleeAttack(ADarkForestHero* Target)
{
    if (!IsAlive() || bPostureBroken || bAttacking || !IsValid(Target) ||
        !Target->IsAlive() || !GetWorld() ||
        GetWorld()->GetTimeSeconds() < NextAttackTime ||
        FVector::Dist2D(GetActorLocation(), Target->GetActorLocation()) > MeleeRange ||
        FMath::Abs(GetActorLocation().Z - Target->GetActorLocation().Z) > 135.f || !CanCommitMelee(Target))
    {
        return false;
    }
    AttackTarget = Target;
    bAttacking = true;
    NextAttackTime = GetWorld()->GetTimeSeconds() + FMath::Max(AttackCooldownSeconds, TelegraphSeconds + .45f);
    FaceMeleeTarget(Target);
    OnAttackTelegraphed();
    if (AttackTellSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, AttackTellSound, GetActorLocation());
    }
    PlayAttackVisual(AttackMontage, TelegraphSeconds);
    GetWorldTimerManager().SetTimer(TelegraphTimer, this, &ADarkForestGoblin::ResolveAttack,
        TelegraphSeconds, false);
    return true;
}

void ADarkForestGoblin::ResolveAttack()
{
    bAttacking = false;
    ADarkForestHero* Target = AttackTarget.Get();
    if (!IsAlive() || bPostureBroken || !IsValid(Target) || !Target->IsAlive()) return;
    if (IsInCommittedArc(Target, MeleeRange))
    {
        Target->ReceiveMeleeHit(MeleeDamage, MeleePostureDamage, this);
    }
}

void ADarkForestGoblin::ApplyDeathblow(ADarkForestHero* Source)
{
    if (IsAlive() && bPostureBroken && IsValid(Source)) Die();
}

void ADarkForestGoblin::Die()
{
    Health = 0.f;
    bAttacking = false;
    GetWorldTimerManager().ClearAllTimersForObject(this);
    if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
    if (UCharacterMovementComponent* Movement = GetCharacterMovement()) Movement->DisableMovement();
    SetActorEnableCollision(false);
    OnGoblinDefeated();
    if (auto* Mode = Cast<ADarkForestGameMode>(UGameplayStatics::GetGameMode(this)))
        if (auto* Story = Mode->GetStory()) Story->EnemyDefeated(this);
    SetLifeSpan(DeadActorLifetimeSeconds);
    UE_LOG(LogDarkForest, Log, TEXT("Goblin defeated: %s"), *GetName());
}
