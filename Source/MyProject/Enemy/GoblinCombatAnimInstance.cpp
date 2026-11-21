#include "Enemy/GoblinCombatAnimInstance.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Engine/World.h"
#include "Enemy/DarkForestGoblin.h"

namespace
{
class FGoblinCombatAnimProxy final : public FAnimInstanceProxy
{
public:
    explicit FGoblinCombatAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

    virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
    {
        FAnimInstanceProxy::PreUpdate(Instance, DeltaSeconds);
        const auto* Combat = CastChecked<UGoblinCombatAnimInstance>(Instance);
        Walk = Combat->Walk;
        Swing = Combat->Swing;
        Death = Combat->Death;
        const AActor* Owner = Instance->GetOwningActor();
        const auto* Goblin = Cast<ADarkForestGoblin>(Owner);
        bDead = Goblin && !Goblin->IsAlive();
        DeathTime = bDead ? FMath::Max(0.f, DeathTime) + DeltaSeconds : -1.f;
        const float Speed = Owner ? Owner->GetVelocity().Size2D() : 0.f;
        if (Walk && Speed > 10.f)
        {
            WalkTime = FMath::Fmod(WalkTime + DeltaSeconds * FMath::Clamp(Speed / 230.f, .55f, 1.5f),
                FMath::Max(.01f, Walk->GetPlayLength()));
        }

        const float Elapsed = Instance->GetWorld()->GetTimeSeconds() - Combat->AttackStart;
        const float Duration = Combat->AttackDuration;
        AttackWeight = 0.f;
        if (Swing && Elapsed >= 0.f && Elapsed < Duration)
        {
            const float Fade = FMath::Clamp(FMath::Min(Elapsed / .12f, (Duration - Elapsed) / .16f), 0.f, 1.f);
            AttackWeight = FMath::SmoothStep(0.f, 1.f, Fade);
            if (Combat->AttackCancelledAt >= 0.f)
                AttackWeight *= FMath::Clamp(1.f - (Instance->GetWorld()->GetTimeSeconds() - Combat->AttackCancelledAt) / .12f, 0.f, 1.f);
            SwingTime = Elapsed / Duration * FMath::Min(1.3f, Swing->GetPlayLength());
        }
    }

    virtual bool Evaluate(FPoseContext& Output) override
    {
        if (!Walk) return false;
        FAnimationPoseData OutputData(Output);
        if (bDead && Death)
        {
            // Determine the import-axis correction from the upright first frame,
            // not the final prone pose (head-vs-foot height is no longer valid there).
            FPoseContext First(Output);
            FAnimationPoseData FirstData(First);
            Death->GetAnimationPose(FirstData, FAnimExtractContext(0., false));
            const FCompactPoseBoneIndex Root(0);
            const FQuat Original = First.Pose[Root].GetRotation();
            KeepPoseUpright(First.Pose);
            const FQuat Correction = First.Pose[Root].GetRotation() * Original.Inverse();
            Death->GetAnimationPose(OutputData, FAnimExtractContext(
                FMath::Min(DeathTime, FMath::Max(0.f, Death->GetPlayLength() - .001f)), false));
            Output.Pose[Root].SetRotation(Correction * Output.Pose[Root].GetRotation());
            return true; // Hold the final death pose instead of returning to walk.
        }
        if (AttackWeight <= 0.f || !Swing)
        {
            Walk->GetAnimationPose(OutputData, FAnimExtractContext(WalkTime, false));
            KeepPoseUpright(Output.Pose);
            return true;
        }

        FPoseContext Locomotion(Output);
        FPoseContext Attack(Output);
        FAnimationPoseData LocomotionData(Locomotion);
        FAnimationPoseData AttackData(Attack);
        Walk->GetAnimationPose(LocomotionData, FAnimExtractContext(WalkTime, false));
        Swing->GetAnimationPose(AttackData, FAnimExtractContext(SwingTime, false));
        KeepPoseUpright(Locomotion.Pose);
        KeepPoseUpright(Attack.Pose);
        const FCompactPoseBoneIndex Root(0);
        Attack.Pose[Root].SetTranslation(Locomotion.Pose[Root].GetTranslation());
        FAnimationRuntime::BlendTwoPosesTogether(LocomotionData, AttackData, 1.f - AttackWeight, OutputData);
        KeepPoseUpright(Output.Pose);
        return true;
    }

private:
    static void KeepPoseUpright(FCompactPose& Pose)
    {
        const FBoneContainer& Bones = Pose.GetBoneContainer();
        const auto Head = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Bones.GetPoseBoneIndexForBoneName(TEXT("Head"))));
        const auto Foot = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Bones.GetPoseBoneIndexForBoneName(TEXT("LeftFoot"))));
        if (Head.GetInt() == INDEX_NONE || Foot.GetInt() == INDEX_NONE) return;

        FCSPose<FCompactPose> ComponentPose;
        ComponentPose.InitPose(Pose);
        // This imported mesh uses negative component Z as up; its level mesh is rolled 180 degrees.
        if (ComponentPose.GetComponentSpaceTransform(Head).GetLocation().Z >
            ComponentPose.GetComponentSpaceTransform(Foot).GetLocation().Z)
        {
            FTransform& Root = Pose[FCompactPoseBoneIndex(0)];
            Root.SetRotation(FQuat(FVector::ForwardVector, PI) * Root.GetRotation());
        }
    }
    const UAnimSequence* Walk = nullptr;
    const UAnimSequence* Swing = nullptr;
    const UAnimSequence* Death = nullptr;
    float DeathTime = -1.f;
    bool bDead = false;
    float WalkTime = .15f;
    float SwingTime = 0.f;
    float AttackWeight = 0.f;
};
}

FAnimInstanceProxy* UGoblinCombatAnimInstance::CreateAnimInstanceProxy()
{
    return new FGoblinCombatAnimProxy(this);
}

void UGoblinCombatAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy)
{
    delete Proxy;
}
