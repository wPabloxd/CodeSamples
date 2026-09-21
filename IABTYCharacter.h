#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "IABTYAdvancedFriendsGameInstance.h"
#include "Logging/LogMacros.h"
#include "IABTYCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UMaterialInstanceDynamic;
class ABaseWeapon;
class AWeaponSpawner;
class UNiagaraSystem;
class ABaseProjectile;
struct FInputActionValue;

UENUM(BlueprintType)
enum class EWeapon : uint8
{
	None,
	Dart,
	Stapler,
	Laser,
	Needle,
	Ball,
	MagneticBomb,
	Random
};

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class AIABTYCharacter : public ACharacter
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Mesh, meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* Mesh1P;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* FireAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* CrouchAction;

public:
	AIABTYCharacter();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	virtual void Landed(const FHitResult& Hit) override;

	UPROPERTY(Replicated, BlueprintReadOnly, ReplicatedUsing = OnRep_Team, Category = "Team")
	ETeam Team;

	UFUNCTION()
	void OnRep_Team();

	FORCEINLINE bool GetIsPlayerDead() {
		return bIsDead;
	}

	ABaseWeapon* GetCurrentWeapon() {
		return CurrentWeapon;
	}

	void EmptyWeapon(bool bIsSwapped);

	UPROPERTY(Replicated)
	bool bCanPickUpWeapon = true;

	void UpdateTeamColor();

	UPROPERTY(BlueprintReadOnly)
	float HeadScale = 1.0f;

	bool BalloonGrowing = false;

	UPROPERTY(EditDefaultsOnly, Category = "Balloon")
	float TimeToGrow = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Balloon")
	float BalloonGrowthPerSecondTimeOut = 8.0f;

	float BalloonGrowthPerSecond = 0.0f;

	float TimeElapsedToGrow = 0.0f;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StopInflatingSound();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StopInflatingLightSound();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_HideHead();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_IncreaseHeadScale(float AmmountToIncrease);

	UPROPERTY(BlueprintReadOnly, Replicated)
	EWeapon CurrentWeaponType = EWeapon::None;

	UFUNCTION()
	void SetIsFiring();
	FTimerHandle FiringResetHandle;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetIsFiring();

	UPROPERTY(BlueprintReadOnly)
	bool bIsFiring = false;

	UPROPERTY(BlueprintReadOnly, Replicated)
	float PitchAngle = 0.0f;

	UFUNCTION(Server, Unreliable)
	void Server_SetPitchAngle(float NewPitchAngle);

	UPROPERTY(Replicated)
	float BalloonHP = 100.0f;

	void RestoreBalloonHP(float DeltaTime);

	void AttackInput();

	UPROPERTY(EditDefaultsOnly, Category = "Sounds")
	USoundBase* BodyHitSound;

	UPROPERTY(EditDefaultsOnly)
	UAudioComponent* BalloonInflatingAudioComponent;

	UPROPERTY(EditDefaultsOnly)
	UAudioComponent* BalloonInflatingLightAudioComponent;

	void ToggleBalloonSound(bool bNewState);

	void ToggleBalloonLightSound(bool bNewState);

	UFUNCTION(Server, Reliable)
	void ServerCallBodyHitSound(FVector Location);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayBodyHitSound(FVector Location);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Particles")
	UNiagaraSystem* ConfettiSystem;

	UFUNCTION(Client, Reliable)
	void Client_PlayExplosionShake(float Scale);

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UCameraShakeBase> ExplosionShakeClass;

protected:
	void Move(const FInputActionValue& Value);

	void Look(const FInputActionValue& Value);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offset System")
	float CameraOffsetX = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offset System")
	float CameraOffsetY = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offset System")
	float CameraOffsetZ = 0.0f;

	float PreviousCameraPitch = 0.0f;
	float PreviousCharacterYaw = 0.0f;
	FVector PreviousCharacterLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anim System")
	float SpeedX = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anim System")
	float SpeedY = 0.0f;

	void UpdateArmsOffset();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offset System")
	float MovementAnimationRate = 0.0f;

	void UpdateMovementAnimationSpeed();

	UFUNCTION(Server, Reliable)
	void ServerAttackInput(FVector CameraLocation, FRotator CameraRotation);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon)
	ABaseWeapon* CurrentWeapon;

	UFUNCTION()
	void OnRep_CurrentWeapon();

	UMaterialInstanceDynamic* DynamicMaterialInstance;
	UMaterialInstanceDynamic* DynamicMaterialInstanceLight;
	UMaterialInstanceDynamic* DynamicMaterialInstanceLight1P;

	UPROPERTY(Replicated)
	bool bIsDead = false;

	virtual void FellOutOfWorld(const UDamageType& dmgType) override;

	/** Crouch Section */

	void CrouchToggler(const FInputActionValue& Value);

	UPROPERTY(ReplicatedUsing = OnRep_MaxWalkSpeed)
	float ReplicatedMaxWalkSpeed = 600.0f;

	UFUNCTION()
	void OnRep_MaxWalkSpeed();

	float TargetCapsuleHeight = 0.f;
	float CrouchInterpSpeed = 13.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crouch System")
	float StandingSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crouch System")
	float CrouchSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crouch System")
	float HeightStanding = 96.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crouch System")
	float HeightCrouched = 64.0f;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "Crouch System")
	bool bIsCrouching = false;

	bool bPredictedIsCrouching = false;

	UFUNCTION(Server, Reliable)
	void Server_SetIsCrouching(bool bNewIsCrouching, float TargetHeight);

	bool bIsCrouchTransition = false;

	bool bBufferUncrouch = false;

	void SmoothCrouchingTransition(float DeltaTime);

	bool CanUncrouch();

	bool bIsCrouchToggled = true;
	
	void UpdateCameraHeadBob();

	void CalculateDeltaRotation();

	float PreviousYaw = 0.0f;
	float DeltaRotation = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool bMovementAllowed = false;

	virtual void Jump() override;

public:
	UPROPERTY(Replicated)
	APlayerController* LaunchController = nullptr;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SetCrouchingTransition(float TargetHeight);

	UFUNCTION(Server, Reliable)
	void ServerSetWeapon(TSubclassOf<ABaseWeapon> WeaponClass, int InitialAmmo, EWeapon WeaponType, AWeaponSpawner* WeaponSpawner = nullptr);

	UFUNCTION(Client, Reliable)
	void Client_OnWeaponReady(ABaseWeapon* NewWeapon, int InitialAmmo);

	UFUNCTION(Client, Reliable)
	void Client_PlayWeaponPickUpSound();

	UFUNCTION(Client, Reliable)
	void Client_PlayWeaponPickUpAnimation();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anim System")
	bool bPickingUpWeapon = false;

	UPROPERTY(EditDefaultsOnly, Category = "Sounds")
	USoundBase* WeaponPickUpSound;

	UFUNCTION(Server, Reliable)
	void DropWeapon();

	UFUNCTION(Server, Reliable)
	void DieInstantly(AController* KillingController, bool bIsFriendlyFire, EWeapon WeaponType);

protected:
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnDeathRagdoll();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayWallHitSound();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Confetti();

	UPROPERTY(EditDefaultsOnly, Category = "Sounds")
	USoundBase* BalloonPop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Shake")
	TSubclassOf<UCameraShakeBase> CameraHeadBobRunning;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Shake")
	TSubclassOf<UCameraShakeBase> CameraHeadBobCrouching;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Shake")
	TSubclassOf<UCameraShakeBase> CameraHeadBobIdle;

public:
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }

	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }
};

