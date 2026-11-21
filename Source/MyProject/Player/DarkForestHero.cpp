#include "Player/DarkForestHero.h"
#include "Combat/AttackComboComponent.h"
#include "Combat/GuardComponent.h"
#include "Combat/LockOnComponent.h"
#include "Enemy/DarkForestGoblin.h"
#include "Enemy/GoblinKing.h"
#include "Enemy/GoblinKingPhaseComponent.h"
#include "Core/DarkForestGameMode.h"
#include "World/DarkForestStory.h"
#include "Core/DarkForestLog.h"
#include "UI/BossHealthWidget.h"
#include "UI/DamageNumberWidget.h"
#include "UI/DemoFrontendState.h"
#include "Engine/GameInstance.h"
#include "Combat/DamageNumberWorldSubsystem.h"
#include "Combat/DamageNumbers.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Data/AranyaCombatTuning.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

ADarkForestHero::ADarkForestHero()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 340.f;
    CameraBoom->bUsePawnControlRotation = true;
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;
    bUseControllerRotationYaw = false;
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->bOrientRotationToMovement = true;
        Movement->RotationRate = FRotator(0.f, 560.f, 0.f);
    }
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SwordGeometry(TEXT("/Engine/BasicShapes/Cube.Cube"));
    const FVector SwordAxis = FVector(-.55, -.32, .772).GetSafeNormal();
    const FVector GripCenter(-8.64, 0, -.53);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BladeSteel(
        TEXT("/Game/AutoAssets/Characters/Goblin/Goblin-Knife.Goblin-Knife"));
    const auto AddSwordPart = [this, SwordAxis, GripCenter](FName Name, double Offset, FVector Scale)
    {
        UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Part->SetupAttachment(GetMesh(), TEXT("hand_r"));
        Part->SetStaticMesh(SwordGeometry.Object);
        Part->SetRelativeLocation(GripCenter + SwordAxis * Offset);
        Part->SetRelativeRotation(SwordAxis.Rotation());
        if (Name == TEXT("SwordBlade") && BladeSteel.Succeeded()) Part->SetMaterial(0, BladeSteel.Object);
        Part->SetRelativeScale3D(Scale);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetGenerateOverlapEvents(false);
        if (Name == TEXT("SwordBlade")) SwordBlade = Part;
    };
    AddSwordPart(TEXT("SwordGrip"), 0.0, FVector(.12, .027, .027));
    AddSwordPart(TEXT("SwordGuard"), 8.0, FVector(.023, .18, .027));
    AddSwordPart(TEXT("SwordBlade"), 43.0, FVector(.68, .065, .018));

    LightComboDamage = {25.f, 32.f, 45.f};
    AttackComboComponent = CreateDefaultSubobject<UAttackComboComponent>(TEXT("AttackCombo"));
    AttackComboComponent->ComboResetSeconds = ComboResetSeconds;
    GuardComponent = CreateDefaultSubobject<UGuardComponent>(TEXT("Guard"));
    GuardComponent->DeflectWindowSeconds = DeflectWindowSeconds;
    LockOnComponent = CreateDefaultSubobject<ULockOnComponent>(TEXT("LockOn"));
    LockOnComponent->LockOnRange = LockRange;

    static ConstructorHelpers::FObjectFinder<USoundBase> Swing(
        TEXT("/Game/AutoAssets/Audio/Placeholder/PLACEHOLDER_SwingWhoosh.PLACEHOLDER_SwingWhoosh"));
    static ConstructorHelpers::FObjectFinder<USoundBase> Impact(
        TEXT("/Game/AutoAssets/Audio/Placeholder/PLACEHOLDER_BodyImpact.PLACEHOLDER_BodyImpact"));
    static ConstructorHelpers::FObjectFinder<USoundBase> Deflect(
        TEXT("/Game/AutoAssets/Audio/Placeholder/PLACEHOLDER_DeflectPing.PLACEHOLDER_DeflectPing"));
    SwordSwingSound = Swing.Object;
    SwordImpactSound = Impact.Object;
    DeflectSuccessSound = Deflect.Object;
}

void ADarkForestHero::BeginPlay()
{
    Super::BeginPlay();
    USkeletalMeshComponent* HeroMesh = GetMesh();
    const USkeletalMesh* CurrentMesh = HeroMesh->GetSkeletalMeshAsset();
    if (!CurrentMesh || CurrentMesh->GetName().StartsWith(TEXT("SKM_MannyXR")))
    {
        static const TCHAR* MeshPath = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny");
        static const TCHAR* AnimPath = TEXT("/Game/Characters/Mannequins/Animations/ABP_Manny.ABP_Manny_C");
        USkeletalMesh* FullBody = LoadObject<USkeletalMesh>(nullptr, MeshPath);
        UClass* Locomotion = LoadClass<UAnimInstance>(nullptr, AnimPath);
        if (FullBody && Locomotion)
        {
            HeroMesh->SetSkeletalMesh(FullBody);
            HeroMesh->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
            HeroMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
            HeroMesh->SetOwnerNoSee(false);
            HeroMesh->SetHiddenInGame(false);
            HeroMesh->SetAnimInstanceClass(Locomotion);
        }
    }
    Health = MaxHealth;
    Posture = 0.f;
    EnsureInputContext();
    ActivateInputContext();
    EnsureBossWidget();
    EnsureDamageNumberWidget();
    if (AGoblinKing* Boss = FindBoss())
    {
        BindBossWidget(Boss);
    }
}

void ADarkForestHero::ActivateInputContext()
{
    if (IsValid(InputContext))
    {
        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
            {
                if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                    ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
                {
                    Subsystem->AddMappingContext(InputContext, 0);
                }
            }
        }
    }
}

void ADarkForestHero::EnsureInputContext()
{
    if (IsValid(InputContext) && IsValid(MoveAction) && IsValid(LookAction) &&
        IsValid(JumpAction) && IsValid(AttackAction) && IsValid(GuardAction) &&
        IsValid(DeflectAction) && IsValid(DeathblowAction) && IsValid(LockOnAction)) return;

    // A transient Enhanced Input context makes the native hero playable
    // before designers bind authored assets in BP_DarkForestHero.
    InputContext = NewObject<UInputMappingContext>(this, TEXT("RuntimeForestContext"));
    MoveAction = NewObject<UInputAction>(this, TEXT("RuntimeMove"));
    LookAction = NewObject<UInputAction>(this, TEXT("RuntimeLook"));
    JumpAction = NewObject<UInputAction>(this, TEXT("RuntimeJump"));
    AttackAction = NewObject<UInputAction>(this, TEXT("RuntimeAttack"));
    GuardAction = NewObject<UInputAction>(this, TEXT("RuntimeGuard"));
    DeflectAction = NewObject<UInputAction>(this, TEXT("RuntimeDeflect"));
    DeathblowAction = NewObject<UInputAction>(this, TEXT("RuntimeDeathblow"));
    LockOnAction = NewObject<UInputAction>(this, TEXT("RuntimeLockOn"));
    MoveAction->ValueType = EInputActionValueType::Axis2D;
    LookAction->ValueType = EInputActionValueType::Axis2D;

    auto MapY = [this](UInputAction* Action, FKey Key, bool bNegate)
    {
        FEnhancedActionKeyMapping& Mapping = InputContext->MapKey(Action, Key);
        UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(InputContext);
        Swizzle->Order = EInputAxisSwizzle::YXZ;
        Mapping.Modifiers.Add(Swizzle);
        if (bNegate) Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(InputContext));
    };
    auto MapNegativeX = [this](UInputAction* Action, FKey Key)
    {
        FEnhancedActionKeyMapping& Mapping = InputContext->MapKey(Action, Key);
        Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(InputContext));
    };
    MapY(MoveAction, EKeys::W, false);
    MapY(MoveAction, EKeys::S, true);
    InputContext->MapKey(MoveAction, EKeys::D);
    MapNegativeX(MoveAction, EKeys::A);
    InputContext->MapKey(LookAction, EKeys::MouseX);
    MapY(LookAction, EKeys::MouseY, false);
    InputContext->MapKey(JumpAction, EKeys::SpaceBar);
    InputContext->MapKey(AttackAction, EKeys::LeftMouseButton);
    InputContext->MapKey(GuardAction, EKeys::RightMouseButton);
    InputContext->MapKey(DeflectAction, EKeys::F);
    InputContext->MapKey(DeathblowAction, EKeys::E);
    InputContext->MapKey(LockOnAction, EKeys::Q);
    UE_LOG(LogDarkForest, Log, TEXT("Using runtime Enhanced Input fallback."));
}

void ADarkForestHero::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (!Input)
    {
        UE_LOG(LogDarkForest, Error, TEXT("EnhancedInputComponent unavailable."));
        return;
    }
    EnsureInputContext();
    ActivateInputContext();
    if (IsValid(MoveAction)) Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADarkForestHero::Move);
    if (IsValid(LookAction)) Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADarkForestHero::Look);
    if (IsValid(JumpAction))
    {
        Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ADarkForestHero::StartJump);
        Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ADarkForestHero::StopJumpingInput);
    }
    if (IsValid(AttackAction)) Input->BindAction(AttackAction, ETriggerEvent::Started, this, &ADarkForestHero::InputAttack);
    if (IsValid(GuardAction))
    {
        Input->BindAction(GuardAction, ETriggerEvent::Started, this, &ADarkForestHero::BeginGuard);
        Input->BindAction(GuardAction, ETriggerEvent::Completed, this, &ADarkForestHero::EndGuard);
        Input->BindAction(GuardAction, ETriggerEvent::Canceled, this, &ADarkForestHero::EndGuard);
    }
    if (IsValid(DeflectAction)) Input->BindAction(DeflectAction, ETriggerEvent::Started, this, &ADarkForestHero::InputDeflect);
    if (IsValid(DeathblowAction)) Input->BindAction(DeathblowAction, ETriggerEvent::Started, this, &ADarkForestHero::InputDeathblow);
    if (IsValid(LockOnAction)) Input->BindAction(LockOnAction, ETriggerEvent::Started, this, &ADarkForestHero::ToggleLockOn);
}

void ADarkForestHero::Move(const FInputActionValue& Value)
{
    if (!IsAlive() || bPostureBroken || !GetController()) return;
    const FVector2D Axis = Value.Get<FVector2D>();
    const FRotator Yaw(0.f, GetController()->GetControlRotation().Yaw, 0.f);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Axis.Y);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Axis.X);
}

void ADarkForestHero::Look(const FInputActionValue& Value)
{
    if (!IsAlive() || LockOnComponent->GetCurrentTarget()) return;
    const FVector2D Axis = Value.Get<FVector2D>();
    const UDemoFrontendState* Frontend = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDemoFrontendState>() : nullptr;
    const float Sensitivity = Frontend && Frontend->Profile ? FMath::Clamp(Frontend->Profile->MouseSensitivity,.25f,2.5f) : 1.f;
    AddControllerYawInput(Axis.X * Sensitivity);
    AddControllerPitchInput(Axis.Y * Sensitivity);
}

void ADarkForestHero::StartJump()
{
    if (IsAlive() && !bPostureBroken) Jump();
}

void ADarkForestHero::StopJumpingInput()
{
    StopJumping();
}

void ADarkForestHero::InputAttack()
{
    if (!IsAlive() || bPostureBroken || GuardComponent->IsGuarding()) return;
    if (bAttacking)
    {
        if (GetWorld()) AttackComboComponent->BufferAttackInput(GetWorld()->GetTimeSeconds());
        return;
    }
    StartAttack();
}

void ADarkForestHero::StartAttack()
{
    if (!GetWorld() || LightComboDamage.Num() == 0) return;
    AttackComboComponent->AdvanceCombo();
    const int32 Step = AttackComboComponent->GetCurrentStep();
    const int32 ComboIndex = Step - 1;
    bAttacking = true;
    AttackComboComponent->ClearBufferedAttack();

    const UAranyaAttackData* Data = AttackDataChain.IsValidIndex(ComboIndex) ? AttackDataChain[ComboIndex].Get() : nullptr;
    float HitDelaySeconds = AttackHitDelaySeconds;
    float AttackDuration = AttackDurationSeconds;
    float HitEndSeconds = FMath::Min(AttackDuration, HitDelaySeconds + 0.15f);
    EnemiesHitThisSwing.Reset();
    EndSwordHitWindow();

    if (Data)
    {
        // Data-driven attack: play via dynamic montage from the attack data.
        if (Data->AnimSequence)
        {
            AttackComboComponent->PlayAttackMontage(Data);
        }
        if (USoundBase* SwingSound = Data->SwingSFX ? Data->SwingSFX.Get() : SwordSwingSound.Get())
        {
            UGameplayStatics::PlaySoundAtLocation(this, SwingSound, GetActorLocation());
        }
        // Hit window and attack duration come from the attack data.
        const float Rate = FMath::Max(0.01f, Data->PlayRate);
        AttackDuration = Data->AnimSequence
            ? FMath::Max(0.02f, Data->AnimSequence->GetPlayLength() / Rate)
            : FMath::Max(0.02f, AttackDurationSeconds);
        HitDelaySeconds = FMath::Clamp(Data->HitWindowStart / Rate, 0.001f, AttackDuration - 0.001f);
        HitEndSeconds = FMath::Clamp(Data->HitWindowEnd / Rate, HitDelaySeconds + 0.001f, AttackDuration);
    }
    else if (LightComboMontages.IsValidIndex(ComboIndex) && IsValid(LightComboMontages[ComboIndex]))
    {
        PlayAnimMontage(LightComboMontages[ComboIndex]);
        if (SwordSwingSound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, SwordSwingSound, GetActorLocation());
        }
        HitDelaySeconds = AttackHitDelaySeconds;
        AttackDuration = AttackDurationSeconds;
    }

    OnSwordSwing(ComboIndex + 1);
    GetWorldTimerManager().SetTimer(HitTimer, this, &ADarkForestHero::BeginSwordHitWindow,
        HitDelaySeconds, false);
    GetWorldTimerManager().SetTimer(HitWindowEndTimer, this, &ADarkForestHero::EndSwordHitWindow,
        HitEndSeconds, false);
    GetWorldTimerManager().SetTimer(AttackEndTimer, this, &ADarkForestHero::FinishAttack,
        AttackDuration, false);
}

void ADarkForestHero::BeginSwordHitWindow()
{
    ResolveSwordHit();
    if (bAttacking && IsAlive())
        GetWorldTimerManager().SetTimer(HitTimer, this, &ADarkForestHero::ResolveSwordHit, 0.025f, true);
}

void ADarkForestHero::EndSwordHitWindow()
{
    bHasPreviousSwordSample = false;
    GetWorldTimerManager().ClearTimer(HitTimer);
    GetWorldTimerManager().ClearTimer(HitWindowEndTimer);
}

void ADarkForestHero::ResolveSwordHit()
{
    const int32 Step = AttackComboComponent->GetCurrentStep();
    const int32 ComboIndex = Step - 1;
    if (!bAttacking || !IsAlive() || !GetWorld() || !LightComboDamage.IsValidIndex(ComboIndex)) return;
    const UAranyaAttackData* Data = AttackDataChain.IsValidIndex(ComboIndex) ? AttackDataChain[ComboIndex].Get() : nullptr;
    // The native blade uses the 100cm engine cube, scaled along its local X.
    // Transform its actual endpoints (including socket, rotation and scale).
    // Do not silently fall back to invisible cone damage if the weapon is missing.
    if (!IsValid(SwordBlade) || !SwordBlade->GetStaticMesh()) return;
    const FTransform BladeTransform = SwordBlade->GetComponentTransform();
    const FVector CurrentBase = BladeTransform.TransformPosition(FVector(-50.f, 0.f, 0.f));
    const FVector CurrentTip = BladeTransform.TransformPosition(FVector(50.f, 0.f, 0.f));
    const FVector PreviousBase = bHasPreviousSwordSample ? PreviousSwordBase : CurrentBase;
    const FVector PreviousTip = bHasPreviousSwordSample ? PreviousSwordTip : CurrentTip;
    PreviousSwordBase = CurrentBase;
    PreviousSwordTip = CurrentTip;
    bHasPreviousSwordSample = true;
    bool bPlayedImpact = false;

    for (TActorIterator<ADarkForestGoblin> It(GetWorld()); It; ++It)
    {
        ADarkForestGoblin* Enemy = *It;
        if (!IsValid(Enemy) || Enemy->IsHidden() || !Enemy->IsAlive() || EnemiesHitThisSwing.Contains(Enemy)) continue;
        const FVector Direction = Enemy->GetActorLocation() - GetActorLocation();
        const UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
        if (!Capsule) continue;
        const float TargetRadius = Capsule->GetScaledCapsuleRadius();
        // Broad phase only; final damage requires visible blade contact below.
        if (Direction.Size2D() > AttackRange + TargetRadius || FMath::Abs(Direction.Z) > 160.f) continue;
        const float AxisHalfLength = FMath::Max(0.f, Capsule->GetScaledCapsuleHalfHeight() - TargetRadius);
        const FVector CapsuleBase = Capsule->GetComponentLocation() - Capsule->GetUpVector() * AxisHalfLength;
        const FVector CapsuleTip = Capsule->GetComponentLocation() + Capsule->GetUpVector() * AxisHalfLength;
        const float ContactDistanceSquared = FMath::Square(TargetRadius + FMath::Clamp(SwordContactRadius, 0.f, 15.f));
        FVector ImpactLocation = FVector::ZeroVector;
        FVector ContactTargetPoint = FVector::ZeroVector;
        const auto TouchesTarget = [&](const FVector& A, const FVector& B)
        {
            FVector OnBlade, OnTarget;
            FMath::SegmentDistToSegmentSafe(A, B, CapsuleBase, CapsuleTip, OnBlade, OnTarget);
            if (FVector::DistSquared(OnBlade, OnTarget) > ContactDistanceSquared) return false;
            ImpactLocation = OnBlade;
            ContactTargetPoint = OnTarget;
            return true;
        };
        bool bContact = TouchesTarget(CurrentBase, CurrentTip);
        // Sweep five points along the blade between samples to catch fast motion.
        // This approximates the swept blade surface; it is not per-triangle combat.
        for (int32 Sample = 0; !bContact && Sample <= 4; ++Sample)
        {
            const float Alpha = Sample / 4.f;
            bContact = TouchesTarget(FMath::Lerp(PreviousBase, PreviousTip, Alpha),
                FMath::Lerp(CurrentBase, CurrentTip, Alpha));
        }
        if (!bContact) continue;
        FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SwordContactOcclusion), false, this);
        QueryParams.AddIgnoredActor(Enemy);
        FHitResult Obstruction;
        if (GetWorld()->LineTraceSingleByChannel(Obstruction, GetActorLocation(), ContactTargetPoint,
            ECC_Visibility, QueryParams)) continue;
        EnemiesHitThisSwing.Add(Enemy);
        const float Damage = Data ? Data->Damage : LightComboDamage[ComboIndex];
        const float PostureDamage = Data ? Data->PostureDamage : AttackPostureDamage;

        // Capture pre-hit state so we can emit the actually-applied deltas.
        const float HealthBefore = Enemy->GetHealth();
        const float PostureBefore = Enemy->GetPosture();
        AGoblinKing* King = Cast<AGoblinKing>(Enemy);
        UGoblinKingPhaseComponent* Phase = King ? King->PhaseComponent : nullptr;
        const float BossSegmentHealthBefore = Phase ? Phase->GetSegmentHealth() : 0.f;
        const float BossPostureBefore = Phase ? (Phase->GetPostureFraction() * (Phase->GetSegmentMaxPosture())) : 0.f;
        // Phase component lacks a max-posture getter; capture fraction-based before via a safe fallback below.

        Enemy->ReceiveSwordHit(Damage, PostureDamage, this);

        // Emit floating damage numbers from this authoritative hit path only.
        if (UWorld* World = GetWorld())
        {
            FVector NumberLocation = Enemy->GetComponentsBoundingBox(true).GetCenter();
            NumberLocation.Z += Enemy->GetComponentsBoundingBox(true).GetExtent().Z;

            if (King && Phase)
            {
                const float AppliedHealth = FMath::Max(0.f, BossSegmentHealthBefore - Phase->GetSegmentHealth());
                if (AppliedHealth > 0.f)
                {
                    UDamageNumberWorldSubsystem::Broadcast(World, AppliedHealth, NumberLocation, EDamageNumberType::Health);
                }
                // Posture: use fraction delta scaled by max posture captured before hit.
                const float BossPostureAfter = Phase->GetPostureFraction() * (Phase->GetSegmentMaxPosture());
                const float AppliedPosture = FMath::Max(0.f, BossPostureBefore - BossPostureAfter);
                if (AppliedPosture > 0.f)
                {
                    UDamageNumberWorldSubsystem::Broadcast(World, AppliedPosture, NumberLocation, EDamageNumberType::Posture);
                }
            }
            else
            {
                const float AppliedHealth = FMath::Max(0.f, HealthBefore - Enemy->GetHealth());
                if (AppliedHealth > 0.f)
                {
                    UDamageNumberWorldSubsystem::Broadcast(World, AppliedHealth, NumberLocation, EDamageNumberType::Health);
                }
                const float AppliedPosture = FMath::Max(0.f, Enemy->GetPosture() - PostureBefore);
                if (AppliedPosture > 0.f)
                {
                    UDamageNumberWorldSubsystem::Broadcast(World, AppliedPosture, NumberLocation, EDamageNumberType::Posture);
                }
            }
        }

        UE_LOG(LogAranyaCombat, Log, TEXT("[COMBAT] Sword hit %s damage=%.1f step=%d"), *Enemy->GetName(), Damage, Step);
        if (!bHitStopActive && CombatTuning && CombatTuning->HitStopDuration > 0.f)
        {
            AWorldSettings* Settings = GetWorld()->GetWorldSettings();
            PreviousTimeDilation = Settings->TimeDilation;
            Settings->SetTimeDilation(FMath::Clamp(CombatTuning->HitStopTimeScale, 0.01f, 1.f));
            bHitStopActive = true;
            GetWorldTimerManager().SetTimer(HitStopTimer, this, &ADarkForestHero::EndHitStop,
                CombatTuning->HitStopDuration * Settings->GetEffectiveTimeDilation(), false);
        }
        USoundBase* ImpactSound = Data && Data->HitSFX ? Data->HitSFX.Get() : SwordImpactSound.Get();
        if (ImpactSound && !bPlayedImpact)
        {
            UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, ImpactLocation);
            bPlayedImpact = true;
        }
    }
}

void ADarkForestHero::FinishAttack()
{
    EndSwordHitWindow();
    bAttacking = false;
    const bool bFollowUp = GetWorld() && AttackComboComponent->ConsumeBufferedAttack(GetWorld()->GetTimeSeconds());
    // A held defensive input takes priority at recovery; it never cancels the strike.
    if (bGuardInputHeld) BeginGuard();
    if (bFollowUp && IsAlive() && !bPostureBroken && !GuardComponent->IsGuarding()) StartAttack();
}

void ADarkForestHero::BeginGuard()
{
    if (!IsAlive() || bPostureBroken) return;
    bGuardInputHeld = true;
    if (IsAlive() && !bPostureBroken && !bAttacking)
    {
        GuardComponent->SetGuarding(true);
        bGuarding = true;
    }
}

void ADarkForestHero::EndGuard()
{
    bGuardInputHeld = false;
    // Releasing RMB must not cancel the separate, timed F deflect window.
    bGuarding = bDeflecting;
    GuardComponent->SetGuarding(bGuarding);
}

void ADarkForestHero::InputDeflect()
{
    if (!IsAlive() || bPostureBroken || bAttacking || bDeflecting || !GetWorld()) return;
    bDeflecting = true;
    bGuarding = true;
    GuardComponent->SetGuarding(true);
    if (DeflectData) GuardComponent->PlayDeflectAnimation(DeflectData);
    else if (IsValid(DeflectMontage)) PlayAnimMontage(DeflectMontage);
    GetWorldTimerManager().SetTimer(DeflectTimer, this, &ADarkForestHero::EndDeflect,
        DeflectWindowSeconds, false);
}

void ADarkForestHero::EndDeflect()
{
    bDeflecting = false;
    bGuarding = bGuardInputHeld && IsAlive() && !bPostureBroken;
    GuardComponent->SetGuarding(bGuarding);
}

void ADarkForestHero::ReceiveMeleeHit(float HealthDamage, float PostureDamage, ADarkForestGoblin* Source,
    bool bUnblockable)
{
    if (!IsAlive() || bPostureBroken || bGodMode) return;
    if (IsValid(Source))
    {
        const UWorld* World = GetWorld();
        if (!World || Source->GetWorld() != World) return;

        // Cover cancels contact before damage, guard, or deflect can resolve.
        FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyMeleeOcclusion), false, Source);
        QueryParams.AddIgnoredActor(this);
        if (World->LineTraceTestByChannel(Source->GetActorLocation(), GetActorLocation(),
            ECC_Visibility, QueryParams)) return;
    }
    if (!bUnblockable && bDeflecting && IsValid(Source))
    {
        const float SourcePostureBefore = Source->GetPosture();
        Source->ReceiveDeflect(DeflectPostureDamage);
        const float SourcePostureAfter = Source->GetPosture();
        const float AppliedDeflectPosture = FMath::Max(0.f, SourcePostureAfter - SourcePostureBefore);

        OnDeflectSuccess();
        if (DeflectSuccessSound)
        {
            UGameplayStatics::PlaySoundAtLocation(this, DeflectSuccessSound, GetActorLocation());
        }
        EndDeflect();
        GetWorldTimerManager().ClearTimer(DeflectTimer);

        // Emit DEFLECT only on a successful deflect.
        if (UWorld* World = GetWorld())
        {
            FVector NumberLocation = Source->GetComponentsBoundingBox(true).GetCenter();
            NumberLocation.Z += Source->GetComponentsBoundingBox(true).GetExtent().Z;
            UDamageNumberWorldSubsystem::Broadcast(World, AppliedDeflectPosture > 0.f ? AppliedDeflectPosture : DeflectPostureDamage,
                NumberLocation, EDamageNumberType::Deflect);
        }
        return;
    }
    const float GuardMultiplier =
        (!bUnblockable && GuardComponent->IsGuarding()) ? GuardHealthDamageMultiplier : 1.f;
    const float HealthBefore = Health;
    const float PostureBefore = Posture;
    Health = FMath::Max(0.f, Health - HealthDamage * GuardMultiplier);
    Posture = FMath::Min(MaxPosture, Posture + PostureDamage);
    if (UWorld* World = GetWorld())
    {
        FVector NumberLocation = GetComponentsBoundingBox(true).GetCenter();
        NumberLocation.Z += GetComponentsBoundingBox(true).GetExtent().Z;
        const float AppliedHealth = FMath::Max(0.f, HealthBefore - Health);
        if (AppliedHealth > 0.f)
        {
            UDamageNumberWorldSubsystem::Broadcast(World, AppliedHealth, NumberLocation, EDamageNumberType::Health);
        }
        const float AppliedPosture = FMath::Max(0.f, Posture - PostureBefore);
        if (AppliedPosture > 0.f)
        {
            UDamageNumberWorldSubsystem::Broadcast(World, AppliedPosture, NumberLocation, EDamageNumberType::Posture);
        }
    }
    if (Health <= 0.f) { Defeat(); return; }
    if (Posture >= MaxPosture)
    {
        bPostureBroken = true;
        bGuarding = false;
        bGuardInputHeld = false;
        bDeflecting = false;
        GuardComponent->SetGuarding(false);
        AttackComboComponent->ClearBufferedAttack();
        GetWorldTimerManager().ClearTimer(DeflectTimer);
        GetWorldTimerManager().SetTimer(StaggerTimer, this, &ADarkForestHero::RecoverPosture,
            PostureBrokenSeconds, false);
    }
}

ADarkForestGoblin* ADarkForestHero::GetLockedTarget() const
{
    return Cast<ADarkForestGoblin>(LockOnComponent->GetCurrentTarget());
}

void ADarkForestHero::RecoverPosture()
{
    bPostureBroken = false;
    Posture = 0.f;
}

float ADarkForestHero::GetDeathAnimationDuration() const
{
    return DeathAnimSequence ? DeathAnimSequence->GetPlayLength() : 0.f;
}

void ADarkForestHero::Defeat()
{
    ReleaseLockCamera(true);
    if (AController* MyController = GetController())
    {
        FRotator View = MyController->GetControlRotation();
        View.Pitch = -30.f;
        View.Roll = 0.f;
        MyController->SetControlRotation(View);
    }
    EndSwordHitWindow();
    Health = 0.f;
    bGuardInputHeld = false;
    bDeflecting = false;
    bAttacking = false;
    AttackComboComponent->ClearBufferedAttack();
    GetWorldTimerManager().ClearTimer(AttackEndTimer);
    GetWorldTimerManager().ClearTimer(DeflectTimer);
    GuardComponent->SetGuarding(false);
    bGuarding = false;
    LockOnComponent->ClearTarget();
    SetActorTickEnabled(false);
    if (UCharacterMovementComponent* Movement = GetCharacterMovement()) Movement->DisableMovement();
    if (UAnimSequence* DeathSequence = DeathAnimSequence)
    {
        if (UAnimMontage* DeathMontage = PlayAnimSequence(DeathSequence, 1.f, 1, 0.2f, 0.2f))
        {
            DeathMontage->bEnableAutoBlendOut = false;
            if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
                if (FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(DeathMontage))
                    Instance->bEnableAutoBlendOut = false;
        }
    }
    if (BossHealthWidget)
    {
        BossHealthWidget->ShowDeathState();
    }
    OnHeroDefeated();
    if (ADarkForestGameMode* GameMode = Cast<ADarkForestGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        GameMode->HandleHeroDefeated(this);
    }
}

UAnimMontage* ADarkForestHero::PlayAnimSequence(UAnimSequence* Sequence, float InPlayRate, int32 LoopCount, float BlendInTime, float BlendOutTime)
{
    if (!Sequence) { return nullptr; }
    USkeletalMeshComponent* HeroMesh = GetMesh();
    if (!HeroMesh) { return nullptr; }
    UAnimInstance* AnimInst = HeroMesh->GetAnimInstance();
    if (!AnimInst)
    {
        UE_LOG(LogAranyaCombat, Error, TEXT("[COMBAT] No AnimInstance on hero mesh for %s"), *Sequence->GetName());
        return nullptr;
    }
    UAnimMontage* Montage = AnimInst->PlaySlotAnimationAsDynamicMontage(Sequence, FName("DefaultSlot"),
        BlendInTime, BlendOutTime, InPlayRate, LoopCount, -1.f, 0.f);
    if (!Montage)
    {
        UE_LOG(LogAranyaCombat, Error, TEXT("[COMBAT] Failed to create dynamic montage for %s"), *Sequence->GetName());
    }
    return Montage;
}

void ADarkForestHero::EndHitStop()
{
    if (bHitStopActive && GetWorld()) GetWorld()->GetWorldSettings()->SetTimeDilation(PreviousTimeDilation);
    bHitStopActive = false;
    GetWorldTimerManager().ClearTimer(HitStopTimer);
}

void ADarkForestHero::SetGodMode(bool bEnabled)
{
    bGodMode = bEnabled;
    UE_LOG(LogDarkForest, Log, TEXT("Aranya.God %s"), bGodMode ? TEXT("ON") : TEXT("OFF"));
}

void ADarkForestHero::RestoreVitals()
{
    ReleaseLockCamera(true);
    EndSwordHitWindow();
    EndHitStop();
    USkeletalMeshComponent* HeroMesh = GetMesh();
    const USkeletalMesh* CurrentMesh = HeroMesh->GetSkeletalMeshAsset();
    if (UAnimInstance* Anim = HeroMesh->GetAnimInstance()) Anim->StopAllMontages(0.f);
    if (!CurrentMesh || CurrentMesh->GetName().StartsWith(TEXT("SKM_MannyXR")))
    {
        static const TCHAR* MeshPath = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny");
        static const TCHAR* AnimPath = TEXT("/Game/Characters/Mannequins/Animations/ABP_Manny.ABP_Manny_C");
        USkeletalMesh* FullBody = LoadObject<USkeletalMesh>(nullptr, MeshPath);
        UClass* Locomotion = LoadClass<UAnimInstance>(nullptr, AnimPath);
        if (FullBody && Locomotion)
        {
            HeroMesh->SetSkeletalMesh(FullBody);
            HeroMesh->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
            HeroMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
            HeroMesh->SetOwnerNoSee(false);
            HeroMesh->SetHiddenInGame(false);
            HeroMesh->SetAnimInstanceClass(Locomotion);
        }
    }
    Health = MaxHealth;
    Posture = 0.f;
    bPostureBroken = false;
    bGuarding = false;
    bAttacking = false;
    bDeflecting = false;
    bGuardInputHeld = false;
    AttackComboComponent->ResetCombo();

    if (GetWorld())
    {
        GetWorldTimerManager().ClearTimer(HitTimer);
        GetWorldTimerManager().ClearTimer(AttackEndTimer);
        GetWorldTimerManager().ClearTimer(DeflectTimer);
        GetWorldTimerManager().ClearTimer(StaggerTimer);
        GetWorldTimerManager().ClearTimer(HitStopTimer);
    }
    GuardComponent->SetGuarding(false);
    LockOnComponent->ClearTarget();
    SetActorTickEnabled(false);
    SetActorEnableCollision(true);
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->SetMovementMode(MOVE_Walking);
    }
}

void ADarkForestHero::ReviveAt(const FTransform& RespawnTransform)
{
    RestoreVitals();
    SetActorTransform(RespawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
    if (AController* MyController = GetController())
    {
        MyController->SetControlRotation(RespawnTransform.Rotator());
    }
    OnRespawned();
}

void ADarkForestHero::EnsureBossWidget()
{
    if (IsValid(BossHealthWidget) || !GetWorld())
    {
        return;
    }

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        BossHealthWidget = CreateWidget<UBossHealthWidget>(PC, BossHealthWidgetClass ? BossHealthWidgetClass.Get() : UBossHealthWidget::StaticClass());
        if (BossHealthWidget)
        {
            BossHealthWidget->AddToViewport(10);
            BossHealthWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

void ADarkForestHero::EnsureDamageNumberWidget()
{
    if (IsValid(DamageNumberWidget) || !GetWorld())
    {
        return;
    }

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        DamageNumberWidget = CreateWidget<UDamageNumberWidget>(PC, DamageNumberWidgetClass ? DamageNumberWidgetClass.Get() : UDamageNumberWidget::StaticClass());
        if (DamageNumberWidget)
        {
            DamageNumberWidget->SetTuning(CombatTuning);
            DamageNumberWidget->AddToViewport(20);
            DamageNumberWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
    }
}

AGoblinKing* ADarkForestHero::FindBoss() const
{
    if (ADarkForestGameMode* GameMode = Cast<ADarkForestGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        if (AGoblinKing* Tracked = GameMode->GetActiveBoss())
        {
            if (IsValid(Tracked))
            {
                return Tracked;
            }
        }
    }

    if (GetWorld())
    {
        for (TActorIterator<AGoblinKing> It(GetWorld()); It; ++It)
        {
            if (IsValid(*It))
            {
                return *It;
            }
        }
    }
    return nullptr;
}

void ADarkForestHero::BindBossWidget(AGoblinKing* Boss)
{
    EnsureBossWidget();
    if (IsValid(BossHealthWidget))
    {
        BossHealthWidget->BindToBoss(IsValid(Boss) && !Boss->IsHidden() ? Boss : nullptr);
    }
}

void ADarkForestHero::HandleBossDefeated(AGoblinKing* Boss)
{
    UE_LOG(LogDarkForest, Log, TEXT("Boss defeated: %s"),
        IsValid(Boss) ? *Boss->GetName() : TEXT("unknown"));
    if (IsValid(BossHealthWidget))
    {
        BossHealthWidget->ShowVictoryState();
    }
    OnBossVictory();
}

void ADarkForestHero::HandleRespawned()
{
    EnsureDamageNumberWidget();
    if (IsValid(DamageNumberWidget))
    {
        DamageNumberWidget->SetTuning(CombatTuning);
    }
    if (IsValid(BossHealthWidget))
    {
        AGoblinKing* Boss = FindBoss();
        BindBossWidget(Boss);
        BossHealthWidget->ShowAliveState();
    }
}

ADarkForestGoblin* ADarkForestHero::FindTarget(float Range, bool bRequireBroken) const
{
    if (!GetWorld()) return nullptr;
    ADarkForestGoblin* Best = nullptr;
    float BestDistanceSq = FMath::Square(Range);
    for (TActorIterator<ADarkForestGoblin> It(GetWorld()); It; ++It)
    {
        ADarkForestGoblin* Enemy = *It;
        if (!IsValid(Enemy) || Enemy->IsHidden() || !Enemy->IsAlive() || (bRequireBroken && !Enemy->IsPostureBroken())) continue;
        const float DistanceSq = FVector::DistSquared(GetActorLocation(), Enemy->GetActorLocation());
        if (DistanceSq < BestDistanceSq)
        {
            BestDistanceSq = DistanceSq;
            Best = Enemy;
        }
    }
    return Best;
}

void ADarkForestHero::InputDeathblow()
{
    if (!IsAlive() || bPostureBroken) return;
    if (auto* Mode = Cast<ADarkForestGameMode>(UGameplayStatics::GetGameMode(this)))
        if (auto* Story = Mode->GetStory(); Story && Story->Interact(this)) return;
    ADarkForestGoblin* Enemy = FindTarget(DeathblowRange, true);
    if (IsValid(Enemy))
    {
        const bool bWasBroken = Enemy->IsPostureBroken();
        const bool bWasAlive = Enemy->IsAlive();
        Enemy->ApplyDeathblow(this);
        if (bWasBroken && bWasAlive && GetWorld())
        {
            FVector NumberLocation = Enemy->GetComponentsBoundingBox(true).GetCenter();
            NumberLocation.Z += Enemy->GetComponentsBoundingBox(true).GetExtent().Z;
            UDamageNumberWorldSubsystem::Broadcast(GetWorld(), 0.f, NumberLocation, EDamageNumberType::Deathblow);
        }
    }
}

void ADarkForestHero::ToggleLockOn()
{
    if (!IsAlive()) return;
    if (LockOnComponent->GetCurrentTarget())
    {
        LockOnComponent->ClearTarget();
        ReleaseLockCamera(false);
        SetActorTickEnabled(bRestoringFreeCamera);
        return;
    }
    if (LockOnComponent->SetTarget(FindTarget(LockRange, false)))
    {
        BeginLockCamera();
        SetActorTickEnabled(true);
    }
    else
    {
        ReleaseLockCamera(false);
        SetActorTickEnabled(bRestoringFreeCamera);
    }
}

void ADarkForestHero::BeginLockCamera()
{
    if (!CameraBoom) return;
    if (!bSavedFreeCamera)
    {
        SavedFreeCameraArm = CameraBoom->TargetArmLength;
        SavedFreeCameraOffset = CameraBoom->TargetOffset;
        bSavedFreeCamera = true;
    }
    bLockCameraActive = true;
    bRestoringFreeCamera = false;
}

void ADarkForestHero::ReleaseLockCamera(bool bImmediate)
{
    bLockCameraActive = false;
    bRestoringFreeCamera = bSavedFreeCamera && CameraBoom;
    if (bImmediate && bRestoringFreeCamera)
    {
        CameraBoom->TargetArmLength = SavedFreeCameraArm;
        CameraBoom->TargetOffset = SavedFreeCameraOffset;
        bSavedFreeCamera = false;
        bRestoringFreeCamera = false;
    }
}

void ADarkForestHero::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    AActor* Target = LockOnComponent->GetCurrentTarget();
    AController* MyController = GetController();
    if (!Target || !MyController || !IsAlive() || !CameraBoom)
    {
        if (bLockCameraActive) ReleaseLockCamera(false);
        if (bRestoringFreeCamera && CameraBoom)
        {
            CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength,SavedFreeCameraArm,DeltaSeconds,7.f);
            CameraBoom->TargetOffset = FMath::VInterpTo(CameraBoom->TargetOffset,SavedFreeCameraOffset,DeltaSeconds,7.f);
            if (FMath::IsNearlyEqual(CameraBoom->TargetArmLength,SavedFreeCameraArm,.5f) && CameraBoom->TargetOffset.Equals(SavedFreeCameraOffset,.5f))
                ReleaseLockCamera(true);
        }
        SetActorTickEnabled(bRestoringFreeCamera);
        return;
    }
    if (!bLockCameraActive) BeginLockCamera();
    const FVector Separation = Target->GetActorLocation() - GetActorLocation();
    const float Distance = Separation.Size2D();
    // Move the pivot between the fighters and pull back for distant targets.
    // TargetOffset is world-space; the spring arm still collides with scenery.
    const FVector Pivot = SavedFreeCameraOffset + Separation * .3f + FVector(0,0,FMath::Clamp(LockCameraHeight,0.f,200.f));
    const float Arm = FMath::Clamp(LockCameraArmLength + FMath::Max(0.f,Distance-200.f)*.55f,340.f,1100.f);
    CameraBoom->TargetOffset = FMath::VInterpTo(CameraBoom->TargetOffset,Pivot,DeltaSeconds,7.f);
    CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength,Arm,DeltaSeconds,7.f);
    FRotator Current = MyController->GetControlRotation();
    Current.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Current.Pitch),-65.f,-15.f);
    const float Yaw = Distance > 1.f ? Separation.Rotation().Yaw : Current.Yaw;
    const FRotator Desired(FMath::Clamp(LockCameraPitch,-65.f,-15.f),Yaw,0.f);
    MyController->SetControlRotation(FMath::RInterpTo(Current,Desired,DeltaSeconds,LockRotationSpeed));
}
