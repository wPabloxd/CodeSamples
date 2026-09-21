#include "ContinuousWeapon.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "IABTY/IABTYCharacter.h"
#include "IABTY/IABTYController.h"
#include "IABTY/Gameplay/HiddenBalloon.h"
#include "Camera/CameraComponent.h"
#include "Net/UnrealNetwork.h"
#include <NiagaraFunctionLibrary.h>
#include <Kismet/GameplayStatics.h>
#include "Components/AudioComponent.h"

AContinuousWeapon::AContinuousWeapon()
{
    LaserBeam = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LaserBeam"));
    LaserBeam->SetupAttachment(RootComponent);
    LaserBeam->bAutoActivate = false;
}

void AContinuousWeapon::BeginPlay()
{
    Super::BeginPlay();

}

void AContinuousWeapon::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateLaser();
}

void AContinuousWeapon::SetUpWidget()
{
    Super::SetUpWidget();
}

void AContinuousWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

}

void AContinuousWeapon::UpdateLaser()
{
    if (!LaserBeam || !OwnerCharacter || !bIsLaserActive) return;

    float Pitch = 0.0f;
    FVector Start;

    if (OwnerCharacter->IsLocallyControlled()) 
    {
        Pitch = OwnerCharacter->GetControlRotation().Pitch;
        Start = OwnerCharacter->GetFirstPersonCameraComponent()->GetComponentLocation();
    }
    else 
    {
        Pitch = OwnerCharacter->PitchAngle;
        Start = OwnerCharacter->GetMesh()->GetSocketLocation(FName("head"));
    }

    float Yaw = OwnerCharacter->GetActorRotation().Yaw; 
    FRotator CameraRot(Pitch, Yaw, 0.f);

    FVector End = Start + CameraRot.Vector() * 5000.f;

    FHitResult Hit;
    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(LaserTrace), true, OwnerCharacter);
    TraceParams.AddIgnoredActor(this);
    TraceParams.bReturnPhysicalMaterial = false;

    bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit,
        Start,
        End,
        ECC_Visibility,
        TraceParams
    );

    FVector BeamEnd = bHit ? Hit.Location : End;

    LaserBeam->SetVariableVec3(FName(TEXT("User.Beam End")), BeamEnd);

    if (!OwnerCharacter->IsLocallyControlled())
        return;

    if (bHit && Hit.GetActor())
    {
        AIABTYCharacter* CharacterHit = Cast<AIABTYCharacter>(Hit.GetActor());
        if (CharacterHit)
        {
            if (Hit.BoneName == FName("head"))
            {
                HittingHead(CharacterHit, Hit.BoneName, Hit.ImpactPoint);
                SpawnHeadHitEffect(CharacterHit->GetMesh(), Hit.BoneName, Hit.ImpactPoint);
            }
        }
        else 
        {
            StopHittingHead();
            HideHeadHitEffect();
        }

        if (AHiddenBalloon* HiddenBalloon = Cast<AHiddenBalloon>(Hit.GetActor()))
        {
            AIABTYController* Controller = Cast<AIABTYController>(OwnerCharacter->GetController());
            HiddenBalloon->PopBalloon(Controller);
        }
    }

    //DrawDebugLine(GetWorld(), Start, BeamEnd, FColor::Red, false, 0.f, 0, 1.f);
}

void AContinuousWeapon::Attack_Implementation(FVector CameraLocation, FRotator CameraRotation)
{
    if (!HasAuthority())
        return;

    Super::Attack_Implementation(CameraLocation, CameraRotation);

    if (!LaserBeam)
        return;

    OwnerCharacter->Multicast_SetIsFiring();

    if (!bIsLaserActive)
    {
        Multicast_ToggleLaser(true);
        Multicast_PlayFireSound(LaserOnSound);
    }
    else
    {
        Multicast_ToggleLaser(false);
        Multicast_PlayFireSound(LaserOffSound);
        Multicast_HideHeadHitEffect();
    }
}

void AContinuousWeapon::AttackLocal(FVector CameraLocation, FRotator CameraRotation)
{
    Super::AttackLocal(CameraLocation, CameraRotation);

    if (!LaserBeam)
        return;

    Attack(CameraLocation, CameraRotation);

    if (!IsValid(OwnerCharacter)) return;
    OwnerCharacter->SetIsFiring();

    if (!bIsLaserActive)
    {
        ToggleLaser(true);
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), LaserOnSound, GetActorLocation());
    }
    else
    {
        ToggleLaser(false);
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), LaserOffSound, GetActorLocation());
        HideHeadHitEffect();
    }
}

void AContinuousWeapon::ToggleLaser(bool bNewState)
{
    if (bNewState)
    {
        LaserBeam->Activate(true);
        OwnerCharacter->BalloonGrowthPerSecond += 4.0f;
        OwnerCharacter->ToggleBalloonLightSound(true);
    }
    else
    {
        LaserBeam->DeactivateImmediate();
        OwnerCharacter->BalloonGrowthPerSecond -= 4.0f;
        OwnerCharacter->ToggleBalloonLightSound(false);
    }

    bIsLaserActive = bNewState;
}

void AContinuousWeapon::StopHittingHead_Implementation()
{
    if (!HasAuthority())
        return;

    Multicast_HideHeadHitEffect();
}

void AContinuousWeapon::HittingHead_Implementation(AIABTYCharacter* CharacterHit, FName BoneName, FVector Position)
{
    if (!HasAuthority()) return;
    if (!IsValid(CharacterHit) || !IsValid(OwnerCharacter)) return;
    if (CharacterHit->GetIsPlayerDead()) return;

    CharacterHit->BalloonHP -= DamagePerSecond * GetWorld()->GetDeltaSeconds();
    if (CharacterHit->BalloonHP <= 0.0f)
    {
        CharacterHit->DieInstantly(OwnerCharacter->GetController(), OwnerCharacter->Team == CharacterHit->Team, WeaponType);
    }

    Multicast_SpawnHeadHitEffect(CharacterHit->GetMesh(), BoneName, Position);
}

void AContinuousWeapon::Multicast_ToggleLaser_Implementation(bool bNewState)
{
    if (!IsValid(OwnerCharacter)) return;
    if (!OwnerCharacter->IsLocallyControlled())
        ToggleLaser(bNewState);
}

void AContinuousWeapon::Multicast_SpawnHeadHitEffect_Implementation(USkeletalMeshComponent* Mesh, FName BoneName, FVector Position)
{
    if (!IsValid(OwnerCharacter)) return;
    if (!OwnerCharacter->IsLocallyControlled())
        SpawnHeadHitEffect(Mesh, BoneName, Position);
}

void AContinuousWeapon::SpawnHeadHitEffect(USkeletalMeshComponent* Mesh, FName BoneName, FVector Position)
{
    if (!IsValid(Mesh)) return;
    if (!HeadHitEffect && HeadHitEffectSystem)
    {
        HeadHitEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
            HeadHitEffectSystem,
            Mesh,
            BoneName,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            EAttachLocation::KeepWorldPosition,
            true
        );
    }

    if (HeadHitEffect)
    {
        HeadHitEffect->SetWorldLocation(Position);
    }

    if (!CracklingSound || !Mesh) return;

    if (!HeadHitAudioComponent)
    {
        HeadHitAudioComponent = UGameplayStatics::SpawnSoundAttached(
            CracklingSound,
            Mesh,
            BoneName,
            FVector::ZeroVector,
            EAttachLocation::SnapToTarget,
            true,
            1.f
        );
    }

    if (HeadHitAudioComponent)
    {
        HeadHitAudioComponent->SetWorldLocation(Position);
    }
}

void AContinuousWeapon::HideHeadHitEffect()
{
    if (HeadHitEffect)
    {
        HeadHitEffect->Deactivate();
        HeadHitEffect = nullptr;
    }

    if (HeadHitAudioComponent)
    {
        HeadHitAudioComponent->Stop();
        HeadHitAudioComponent = nullptr;
    }
}

void AContinuousWeapon::Multicast_HideHeadHitEffect_Implementation()
{
    if (!IsValid(OwnerCharacter)) return;
    if(!OwnerCharacter->IsLocallyControlled())
        HideHeadHitEffect();
}