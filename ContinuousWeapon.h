#pragma once

#include "CoreMinimal.h"
#include "BaseWeapon.h"
#include "ContinuousWeapon.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

UCLASS()
class IABTY_API AContinuousWeapon : public ABaseWeapon
{
	GENERATED_BODY()
	
public:
    AContinuousWeapon();

protected:
    virtual void BeginPlay() override;

    virtual void Tick(float DeltaSeconds) override;

    virtual void SetUpWidget() override;

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laser")
    UNiagaraComponent* LaserBeam;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Laser")
    UNiagaraSystem* LaserBeamEffect;

    UPROPERTY(EditDefaultsOnly, Category = "Laser | Stats")
    float DamagePerSecond = 120.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Laser | Stats")
    float BalloonBlowUpPerSecondRate = 0.05f;

    void UpdateLaser();

    virtual void Attack_Implementation(FVector CameraLocation, FRotator CameraRotation) override;

    virtual void AttackLocal(FVector CameraLocation, FRotator CameraRotation) override;

    bool bIsLaserActive = false;

    void ToggleLaser(bool bNewState);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ToggleLaser(bool bNewState);

    UFUNCTION(Server, Reliable)
    void HittingHead(AIABTYCharacter* CharacterHit, FName BoneName, FVector Position);

    UFUNCTION(Server, Reliable)
    void StopHittingHead();

    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    USoundBase* LaserOnSound;

    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    USoundBase* LaserOffSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
    UNiagaraSystem* HeadHitEffectSystem;

    UPROPERTY()
    UNiagaraComponent* HeadHitEffect;

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_SpawnHeadHitEffect(USkeletalMeshComponent* Mesh, FName BoneName, FVector Position);

    void SpawnHeadHitEffect(USkeletalMeshComponent* Mesh, FName BoneName, FVector Position);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_HideHeadHitEffect();

    void HideHeadHitEffect();

    UPROPERTY()
    UAudioComponent* HeadHitAudioComponent;

    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    USoundBase* CracklingSound;
};
