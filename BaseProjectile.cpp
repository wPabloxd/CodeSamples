#include "BaseProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "IABTY/IABTYCharacter.h"
#include "IABTY/Lobby/IABTYPlayerState.h"
#include "IABTY/Weapons/AmmunitionPickUp.h"
#include "Components/SphereComponent.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"
#include <IABTY/IABTYController.h>
#include <IABTY/IABTYGameState.h>
#include "CollisionShape.h"
#include "Components/SkeletalMeshComponent.h"
#include "IABTY/Auxiliary/ProjectileSpawnerCaller.h"
#include <IABTY/Weapons/ProjectileWeapon.h>

ABaseProjectile::ABaseProjectile()
{
    PrimaryActorTick.bCanEverTick = true;
    //bReplicates = true;

    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    RootComponent = ProjectileMesh;
    ProjectileMesh->SetNotifyRigidBodyCollision(true);
    ProjectileMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    ProjectileMesh->OnComponentHit.AddDynamic(this, &ABaseProjectile::OnProjectileHit);

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->ProjectileGravityScale = 1.f;
    ProjectileMovement->bSweepCollision = true;
    ProjectileMesh->SetNotifyRigidBodyCollision(true);

    InitialLifeSpan = LifeSeconds;

    SetReplicateMovement(true);

    RandomPitchSpeed = FMath::FRandRange(-200.f, 200.f);
    RandomYawSpeed = FMath::FRandRange(-200.f, 200.f);
    RandomRollSpeed = FMath::FRandRange(-200.f, 200.f);
}

void ABaseProjectile::ProjectileSetted()
{
    if (bIsVisualOnly)
    {
        if (AIABTYController* Controller = Cast<AIABTYController>(GetOwner()))
        {
            if (AIABTYCharacter* Character = Cast<AIABTYCharacter>(Controller->GetCharacter()))
            {
                if (Character->IsLocallyControlled())
                {
                    ProjectileMesh->SetVisibility(false, true);
                    ProjectileMesh->SetHiddenInGame(true, true);
                }
            }
        }
    }
}

void ABaseProjectile::BeginPlay()
{
    Super::BeginPlay();
    ProjectileSetted();
}

bool ABaseProjectile::IsNetRelevantFor(const AActor* RealViewer, const AActor* Viewer, const FVector& SrcLocation) const
{
    return Super::IsNetRelevantFor(RealViewer, Viewer, SrcLocation);
}

void ABaseProjectile::Multicast_PlayWallHitSound_Implementation()
{
    if (HitWallSound)
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), HitWallSound, GetActorLocation());
}

void ABaseProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bDoesRotate)
        AddActorLocalRotation(FRotator(RandomPitchSpeed * DeltaTime, RandomYawSpeed * DeltaTime, RandomRollSpeed * DeltaTime));

    if (bHasImpactedBody)
    {
        AIABTYController* Controller = Cast<AIABTYController>(GetOwner());
        if (!Controller)
            return;

        FVector SphereCenter = GetActorLocation();
        float SphereRadius = 36.0f;

        TArray<FOverlapResult> Overlaps;

        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(this);

        FCollisionObjectQueryParams ObjectParams;
        ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

        bool bHasOverlap = GetWorld()->OverlapMultiByObjectType(
            Overlaps,
            SphereCenter,
            FQuat::Identity,
            ObjectParams,
            FCollisionShape::MakeSphere(SphereRadius),
            QueryParams
        );

        if (bHasOverlap)
        {
            for (const FOverlapResult& Overlap : Overlaps)
            {
                AIABTYCharacter* CharacterHit = Cast<AIABTYCharacter>(Overlap.GetActor());
                if (!CharacterHit)
                    continue;

                if (CharacterHit->GetController() == Controller)
                    continue;

                USkeletalMeshComponent* SkeletalMesh = CharacterHit->GetMesh();
                if (!SkeletalMesh)
                    continue;

                const FVector HeadLocation =
                    SkeletalMesh->GetBoneLocation(TEXT("head"));

                const bool bHeadInside =
                    FVector::DistSquared(HeadLocation, SphereCenter) <=
                    FMath::Square(SphereRadius);

                if (bHeadInside)
                {
                    BalloonsPopped++;
                    if (BalloonsPopped == 2)
                    {
                        Controller->ClientUnlockAchievement("COLLATERAL_KILL");
                    }

                    AIABTYPlayerState* PlayerState = Cast<AIABTYPlayerState>(Controller->PlayerState);
                    if (!PlayerState) return;

                    Controller->ServerReportHit(CharacterHit, Controller, PlayerState->Team == CharacterHit->Team, WeaponType);
                    bHasImpactedBody = false;
                }
            }
        }
    }
}

void ABaseProjectile::ResetImpactBodyTimer()
{
    if (!bHasImpactedBody)
        return;

    AIABTYController* Controller = Cast<AIABTYController>(GetOwner());
    if (!Controller) return;

    FVector NewVelocity = FVector(-GetVelocity().X, -GetVelocity().Y, GetVelocity().Z);

    Controller->ServerRequestSpawnAmmo(NewVelocity / 2, GetActorLocation(), GetActorRotation(), FVector::Zero(), false, WeaponType, HitWallSound, GetActorLocation());

    bHasProcessedHit = true;
    bHasImpactedBody = false;

    AIABTYCharacter* Shooter = Cast<AIABTYCharacter>(Controller->GetCharacter());
    if (!Shooter)
        return;

    Shooter->ServerCallBodyHitSound(GetActorLocation());

    Destroy();
}

void ABaseProjectile::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (bIsVisualOnly)
    {
        bHasProcessedHit = true;
        Destroy();
        return;
    }

    if (bHasProcessedHit)
        return;

    AIABTYController* Controller = Cast<AIABTYController>(GetOwner());
    if (!Controller)
        return;

    if (OtherActor && OtherActor != this && OtherComp)
    {
        if (AIABTYCharacter* CharacterHit = Cast<AIABTYCharacter>(OtherActor))
        {
            AIABTYPlayerState* PlayerState = Cast<AIABTYPlayerState>(Controller->PlayerState);
            if (!PlayerState) return;

            if (Hit.BoneName == "head")
            {
                if (ProjectileMesh)
                    ProjectileMesh->IgnoreActorWhenMoving(CharacterHit, true);

                FVector NewVelocity = GetVelocity();

                if (!bIsVisualOnly)
                {
                    BalloonsPopped++;
                    if (BalloonsPopped == 2)
                    {
                        Controller->ClientUnlockAchievement("COLLATERAL_KILL");
                    }
                    Controller->ServerReportHit(CharacterHit, Controller, PlayerState->Team == CharacterHit->Team, WeaponType);
                }

                TWeakObjectPtr<ABaseProjectile> WeakThis(this);

                GetWorld()->GetTimerManager().SetTimerForNextTick([WeakThis, NewVelocity]()
                {
                    if (!WeakThis.IsValid())
                        return;

                    ABaseProjectile* Projectile = WeakThis.Get();

                    if (!IsValid(Projectile) || !Projectile->ProjectileMovement) return;

                    if (Projectile->ProjectileMovement)
                    {
                        Projectile->ProjectileMovement->StopMovementImmediately();
                        Projectile->ProjectileMovement->SetUpdatedComponent(Projectile->ProjectileMesh);
                        Projectile->ProjectileMovement->bSimulationEnabled = true;
                        Projectile->ProjectileMovement->Activate(true);
                        Projectile->ProjectileMovement->Velocity = NewVelocity;
                    }
                });
                return;
            }
            else if (!bIgnoresBody)
            {
                CharacterHit->ServerCallBodyHitSound(Hit.ImpactPoint);

                float Dot = FVector::DotProduct(GetVelocity().GetSafeNormal(), Hit.ImpactNormal);
                float ImpactAngleDegrees = FMath::Acos(FMath::Abs(Dot)) * (180.0f / PI);
                FVector SpawnLocation = Hit.ImpactPoint + (Hit.ImpactNormal * SpawnAmmunitionOffset);
                Controller->ServerRequestSpawnAmmo(-GetVelocity() / 35, GetActorLocation(), GetActorRotation(), Hit.Normal, false, WeaponType, nullptr, GetActorLocation());
                bHasProcessedHit = true;
                Destroy();

                return;
            }
            else if (bIgnoresBody)
            {
                if (Hit.BoneName == "spine_01" || Hit.BoneName == "upperarm_l" || Hit.BoneName == "upperarm_r" || Hit.BoneName == "lowerarm_r" || Hit.BoneName == "lowerarm_l" || Hit.BoneName == "hand_r" || Hit.BoneName == "hand_l")
                {
                    if (!bHasImpactedBody && !bIsVisualOnly)
                    {
                        bHasImpactedBody = true;
                        GetWorldTimerManager().ClearTimer(ImpactedBodyTimer);
                        GetWorldTimerManager().SetTimer(ImpactedBodyTimer, this, &ABaseProjectile::ResetImpactBodyTimer, 0.05f, false);
                    }

                    if (ProjectileMesh)
                        ProjectileMesh->IgnoreActorWhenMoving(CharacterHit, true);

                    FVector NewVelocity = GetVelocity();

                    TWeakObjectPtr<ABaseProjectile> WeakThis(this);

                    GetWorld()->GetTimerManager().SetTimerForNextTick([WeakThis, NewVelocity]()
                    {
                        if (!WeakThis.IsValid())
                            return;

                        ABaseProjectile* Projectile = WeakThis.Get();

                        if (!IsValid(Projectile) || !Projectile->ProjectileMovement) return;

                        if (Projectile->ProjectileMovement)
                        {
                            Projectile->ProjectileMovement->StopMovementImmediately();
                            Projectile->ProjectileMovement->SetUpdatedComponent(Projectile->ProjectileMesh);
                            Projectile->ProjectileMovement->bSimulationEnabled = true;
                            Projectile->ProjectileMovement->Activate(true);
                            Projectile->ProjectileMovement->Velocity = NewVelocity;
                        }
                    });

                    return;
                }
                else if (CharacterHit->GetCurrentWeapon())
                {
                    CharacterHit->ServerCallBodyHitSound(GetActorLocation());

                    FVector NewVelocity = FVector(-GetVelocity().X, -GetVelocity().Y, GetVelocity().Z);
                    Controller->ServerRequestSpawnAmmo(NewVelocity / 2, GetActorLocation(), GetActorRotation(), FVector::Zero(), false, WeaponType, HitWallSound, GetActorLocation());

                    bHasProcessedHit = true;
                    Destroy();

                    return;
                }
            }
        }

        float Dot = FVector::DotProduct(GetVelocity().GetSafeNormal(), Hit.ImpactNormal);
        float ImpactAngleDegrees = FMath::Acos(FMath::Abs(Dot)) * (180.0f / PI);
        FVector SpawnLocation = Hit.ImpactPoint + (Hit.ImpactNormal * SpawnAmmunitionOffset);
        Controller->ServerRequestSpawnAmmo(GetVelocity(), ImpactAngleDegrees < AngleToStuck ? SpawnLocation : GetActorLocation(), GetActorRotation(), Hit.Normal, (OtherActor->ActorHasTag("WorldStatic") || OtherComp->GetCollisionObjectType() == ECC_WorldStatic) && ImpactAngleDegrees < AngleToStuck, WeaponType, HitWallSound, GetActorLocation());
        bHasProcessedHit = true;
        Destroy();
    }
}