#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IABTY/IABTYCharacter.h"
#include "Engine/NetSerialization.h"
#include "BaseProjectile.generated.h"

class UProjectileMovementComponent;
class AAmmunitionPickUp;
class AIABTYCharacter;

UCLASS()
class IABTY_API ABaseProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	ABaseProjectile();

    FORCEINLINE UStaticMeshComponent* GetMeshComponent() {
        return ProjectileMesh;
    }

    FORCEINLINE UProjectileMovementComponent* GetProjectileComponent() {
        return ProjectileMovement;
    }

    AController* PCInstigator;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* ProjectileMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    bool bIsVisualOnly = true;

    void ProjectileSetted();

protected:
	virtual void BeginPlay() override;

    virtual bool IsNetRelevantFor(
        const AActor* RealViewer,
        const AActor* Viewer,
        const FVector& SrcLocation
    ) const override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UProjectileMovementComponent* ProjectileMovement;

    UPROPERTY(EditDefaultsOnly, Category = "Pickup")
    EWeapon WeaponType;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile")
    float LifeSeconds = 5.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile")
    float AngleToStuck = 75.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Projectile")
    float SpawnAmmunitionOffset = 1.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Projectile")
    bool bDoesRotate = false;

    UPROPERTY(EditDefaultsOnly, Category = "Projectile")
    bool bIgnoresBody = false;

    bool bHasImpactedBody = false;

    FTimerHandle ImpactedBodyTimer;

    void ResetImpactBodyTimer();

    float RandomPitchSpeed = 0.0f;
    float RandomYawSpeed = 0.0f;
    float RandomRollSpeed = 0.0f;

    int BalloonsPopped = 0;

    UFUNCTION()
    void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, FVector NormalImpulse,
        const FHitResult& Hit);

    bool bHasProcessedHit = false;

    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    USoundBase* HitWallSound;

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayWallHitSound();

public:	
	virtual void Tick(float DeltaTime) override;

};