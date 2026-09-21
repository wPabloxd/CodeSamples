#include "IABTYCharacter.h"
#include "IABTYController.h"
#include "IABTYProjectile.h"
#include "Animation/AnimInstance.h"
#include "IABTYGameMode.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Weapons/BaseWeapon.h"
#include "Weapons/ProjectileWeapon.h"
#include "Weapons/Projectiles/BaseProjectile.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Weapons/WeaponPickUp.h"
#include "Weapons/WeaponSpawner.h"
#include <Kismet/GameplayStatics.h>
#include "Components/AudioComponent.h"
#include "Camera/CameraShakeBase.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

AIABTYCharacter::AIABTYCharacter()
{
	bReplicates = true;
	bAlwaysRelevant = true;

	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
		
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));

	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->bCrouchMaintainsBaseLocation = true;

	BalloonInflatingAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponentBalloon"));
	BalloonInflatingAudioComponent->SetupAttachment(GetRootComponent());
	BalloonInflatingAudioComponent->bAutoActivate = false;

	BalloonInflatingLightAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponentBalloonLight"));
	BalloonInflatingLightAudioComponent->SetupAttachment(GetRootComponent());
	BalloonInflatingLightAudioComponent->bAutoActivate = false;
}

void AIABTYCharacter::BeginPlay()
{
	Super::BeginPlay();

}

void AIABTYCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if(!bIsDead)
		GetMesh()->AddForce(FVector(0.f, 0.f, 980.f * 3.f), "head", true);

	UpdateArmsOffset();
	UpdateMovementAnimationSpeed();
	UpdateCameraHeadBob();


	if (bIsCrouchTransition) 
	{
		SmoothCrouchingTransition(DeltaTime);
	}

	if (bBufferUncrouch && IsLocallyControlled()) 
	{
		CrouchToggler(FInputActionValue());
	}

	if (HasAuthority() && BalloonHP < 100.0f)
		RestoreBalloonHP(DeltaTime);

	if (BalloonGrowthPerSecond > 0.05f)
	{
		TimeElapsedToGrow += DeltaTime;
		if (TimeElapsedToGrow >= TimeToGrow)
		{
			TimeElapsedToGrow = 0;
			HeadScale += BalloonGrowthPerSecond / 1000.0f;
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("AMMOUNT: %f"), BalloonGrowthPerSecond));

			FBodyInstance* HeadBody = GetMesh()->GetBodyInstance(FName("head"));
			if (HeadBody)
			{
				HeadBody->UpdateBodyScale(FVector(HeadScale));
			}
		}
	}

	if (HeadScale >= 5.0f && IsLocallyControlled())
	{
		DieInstantly(GetController(), true, EWeapon::None);
	}

	CalculateDeltaRotation(); //TODO
}

void AIABTYCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if(LaunchController)
		LaunchController = nullptr;
}

void AIABTYCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AIABTYCharacter, CurrentWeapon,	COND_None);
	DOREPLIFETIME(AIABTYCharacter, Team);
	DOREPLIFETIME_CONDITION(AIABTYCharacter, bIsDead, COND_None);
	DOREPLIFETIME_CONDITION(AIABTYCharacter, CurrentWeaponType,	COND_None);
	DOREPLIFETIME_CONDITION(AIABTYCharacter, PitchAngle, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AIABTYCharacter, bIsCrouching,	COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AIABTYCharacter, ReplicatedMaxWalkSpeed, COND_SkipOwner);
	DOREPLIFETIME(AIABTYCharacter, BalloonHP);
	DOREPLIFETIME(AIABTYCharacter, bCanPickUpWeapon);
	DOREPLIFETIME(AIABTYCharacter, LaunchController);
}

void AIABTYCharacter::UpdateTeamColor()
{
	UIABTYAdvancedFriendsGameInstance* GI = Cast<UIABTYAdvancedFriendsGameInstance>(GetGameInstance());
	if (!GI) return;

	FLinearColor TeamColor = GI->GetTeamBalloonColor(Team);

	if (!DynamicMaterialInstance && GetMesh())
	{
		DynamicMaterialInstance = GetMesh()->CreateAndSetMaterialInstanceDynamic(4);
		DynamicMaterialInstanceLight = GetMesh()->CreateAndSetMaterialInstanceDynamic(5);
		DynamicMaterialInstanceLight1P = GetMesh1P()->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (DynamicMaterialInstance)
	{
		DynamicMaterialInstance->SetVectorParameterValue(TEXT("Tint"), TeamColor);
		DynamicMaterialInstanceLight->SetVectorParameterValue(TEXT("Tint"), TeamColor);
		DynamicMaterialInstanceLight1P->SetVectorParameterValue(TEXT("Tint"), TeamColor);
	}
}

void AIABTYCharacter::Multicast_HideHead_Implementation()
{
	GetMesh()->HideBoneByName(FName("head"), EPhysBodyOp::PBO_None);
}

void AIABTYCharacter::ServerSetWeapon_Implementation(TSubclassOf<ABaseWeapon> WeaponClass, int InitialAmmo, EWeapon WeaponType, AWeaponSpawner* WeaponSpawner = nullptr)
{
	if (!WeaponClass) return;

	if (CurrentWeapon)
	{
		DropWeapon();
	}

	CurrentWeaponType = WeaponType;

	bCanPickUpWeapon = false;

	TWeakObjectPtr<AIABTYCharacter> WeakThis(this);

	FTimerHandle ResetPickUp;
	GetWorldTimerManager().SetTimer(ResetPickUp, [WeakThis]()
	{
		if (!WeakThis.IsValid()) return;

		WeakThis->bCanPickUpWeapon = true;
	}, 0.3f, false);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABaseWeapon* NewWeapon = GetWorld()->SpawnActor<ABaseWeapon>(WeaponClass, SpawnParams);
	if (NewWeapon)
	{
		NewWeapon->SetOwner(this);
		NewWeapon->CurrentAmmo = InitialAmmo;

		if (WeaponSpawner)
			NewWeapon->WeaponSpawner = WeaponSpawner;

		CurrentWeapon = NewWeapon;

		ForceNetUpdate();
		NewWeapon->ForceNetUpdate();
		NewWeapon->FlushNetDormancy();

		OnRep_CurrentWeapon();

		APlayerController* OwningPC = Cast<APlayerController>(GetController());
		if (OwningPC && IsLocallyControlled() == false)
		{
			Client_OnWeaponReady(NewWeapon, InitialAmmo);
		}
	}
}

void AIABTYCharacter::OnRep_CurrentWeapon()
{
	const int MaxRetries = 100;
	if (!CurrentWeapon)	return;

	if (!IsValid(CurrentWeapon))
	{
		TWeakObjectPtr<AIABTYCharacter> WeakThis(this);
		GetWorld()->GetTimerManager().SetTimerForNextTick([WeakThis]()
		{
			if (WeakThis.IsValid())
				WeakThis->OnRep_CurrentWeapon();
		});
		return;
	}

	if (CurrentWeapon->GetAttachParentActor() != this)
	{
		CurrentWeapon->AttachWeaponToCorrectMesh(this);
	}

	if (IsLocallyControlled())
	{
		if (AProjectileWeapon* PW = Cast<AProjectileWeapon>(CurrentWeapon))
			PW->ClientUpdateUI();

		Client_PlayWeaponPickUpSound();
	}
}

void AIABTYCharacter::Client_PlayWeaponPickUpSound_Implementation()
{
	UGameplayStatics::PlaySoundAtLocation(GetWorld(), WeaponPickUpSound, GetActorLocation());
}

void AIABTYCharacter::Client_PlayWeaponPickUpAnimation_Implementation()
{
	bPickingUpWeapon = true;

	TWeakObjectPtr<AIABTYCharacter> WeakThis(this);

	GetWorld()->GetTimerManager().SetTimerForNextTick([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->bPickingUpWeapon = false;
		}
	});
}

void AIABTYCharacter::DropWeapon_Implementation()
{
	if (!CurrentWeapon)
		return;

	if (CurrentWeapon->WeaponPickUp)
	{
		FVector SpawnLocation = GetActorLocation();
		FRotator SpawnRotation = GetActorRotation();
		SpawnLocation.Z += 10.0f;

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AWeaponPickUp* WeaponPickUp = GetWorld()->SpawnActor<AWeaponPickUp>(
			CurrentWeapon->WeaponPickUp,
			SpawnLocation,
			SpawnRotation,
			SpawnParams
		);

		if (WeaponPickUp)
		{
			WeaponPickUp->InitialAmmo = CurrentWeapon->GetCurrentAmmo();

			if (CurrentWeapon->WeaponSpawner)
				WeaponPickUp->WeaponSpawner = CurrentWeapon->WeaponSpawner;
		}
		Multicast_StopInflatingLightSound();
		CurrentWeapon->ServerDestroyWeapon();
		CurrentWeapon = nullptr;
		CurrentWeaponType = EWeapon::None;
	}
}

void AIABTYCharacter::FellOutOfWorld(const UDamageType& dmgType)
{
	if (LaunchController)	
		DieInstantly(LaunchController, false, EWeapon::MagneticBomb);
	else
		DieInstantly(GetController(), true, EWeapon::None);
}

void AIABTYCharacter::CrouchToggler(const FInputActionValue& Value)
{
	if (!bMovementAllowed) return;

	bool IsInputCrouched = Value.Get<bool>();

	AIABTYController* PC = Cast<AIABTYController>(Controller);
	if (!PC) return;
	bIsCrouchToggled = !bIsCrouchToggled;
	if (!PC->bHoldCrouch)
	{
		if (bIsCrouchToggled) {
			return;
		}

		if (bIsCrouching)
		{
			IsInputCrouched = !IsInputCrouched;
		}
	}

	bIsCrouchTransition = true;

	if (IsInputCrouched)
	{
		bIsCrouching = true;
		bBufferUncrouch = false;
		TargetCapsuleHeight = HeightCrouched;

		GetCharacterMovement()->MaxWalkSpeed = CrouchSpeed;
		Server_SetIsCrouching(bIsCrouching, HeightCrouched);
	}
	else
	{
		if (CanUncrouch()) 
		{
			bIsCrouching = false;
			bBufferUncrouch = false;
			TargetCapsuleHeight = HeightStanding;

			GetCharacterMovement()->MaxWalkSpeed = StandingSpeed;
			Server_SetIsCrouching(bIsCrouching, HeightStanding);
		}
		else
		{
			bBufferUncrouch = true;
		}
	}
}

void AIABTYCharacter::OnRep_MaxWalkSpeed()
{
	if(!IsLocallyControlled())
		GetCharacterMovement()->MaxWalkSpeed = ReplicatedMaxWalkSpeed;
}

void AIABTYCharacter::Server_SetIsCrouching_Implementation(bool bNewIsCrouching, float TargetHeight)
{
	bIsCrouching = bNewIsCrouching;

	Multicast_SetCrouchingTransition(TargetHeight);
	
	ReplicatedMaxWalkSpeed = bIsCrouching ? CrouchSpeed : StandingSpeed;
	GetCharacterMovement()->MaxWalkSpeed = ReplicatedMaxWalkSpeed;
}

void AIABTYCharacter::Multicast_SetCrouchingTransition_Implementation(float TargetHeight)
{
	if (IsLocallyControlled()) return;

	TargetCapsuleHeight = TargetHeight;
	bIsCrouchTransition = true;
}

void AIABTYCharacter::SmoothCrouchingTransition(float DeltaTime)
{
	float CurrentHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	float NewHeight = FMath::FInterpTo(CurrentHeight, TargetCapsuleHeight, DeltaTime, CrouchInterpSpeed);
	GetCapsuleComponent()->SetCapsuleHalfHeight(NewHeight, true);

	if (IsLocallyControlled())
	{
		float DeltaHeight = NewHeight - CurrentHeight;
		FVector CameraLocation = GetFirstPersonCameraComponent()->GetRelativeLocation();
		CameraLocation.Z += DeltaHeight;
		GetFirstPersonCameraComponent()->SetRelativeLocation(CameraLocation);
	}

	if (FMath::IsNearlyEqual(NewHeight, TargetCapsuleHeight, 1.0f)) 
	{
		bIsCrouchTransition = false;

		float FinalHeight = bIsCrouching ? (60.0f - (HeightStanding - HeightCrouched)) : 60.0f;
		GetCapsuleComponent()->SetCapsuleHalfHeight(bIsCrouching ? HeightCrouched : HeightStanding, true);

		if (IsLocallyControlled()) 
		{
			GetFirstPersonCameraComponent()->SetRelativeLocation(FVector(0.0f, 0.0f, FinalHeight));
		}
	}
}

bool AIABTYCharacter::CanUncrouch()
{
	FVector Start = GetActorLocation();
	FVector End = Start + FVector(0, 0, HeightStanding + 1.0f);
	float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius() - 5.0f;

	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(this);

	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Pawn = *It;
		if (Pawn)
		{
			USkeletalMeshComponent* PawnMesh = Pawn->FindComponentByClass<USkeletalMeshComponent>();
			if (PawnMesh)
			{
				CollisionParams.AddIgnoredComponent(PawnMesh);
			}
		}
	}

	bool bHit = GetWorld()->SweepSingleByChannel(
		HitResult,
		Start,
		End,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(CapsuleRadius),
		CollisionParams
	);

	return !bHit;
}

void AIABTYCharacter::UpdateCameraHeadBob()
{
	if (!IsLocallyControlled())
		return;

	float Speed = GetVelocity().Size();
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController || !PlayerController->PlayerCameraManager) return;

	if (Speed > 450.0f && !GetCharacterMovement()->IsFalling())
	{
		PlayerController->PlayerCameraManager->StartCameraShake(CameraHeadBobRunning, 1.0f);
	}
	else if (Speed > 100.0f && !GetCharacterMovement()->IsFalling())
	{
		PlayerController->PlayerCameraManager->StartCameraShake(CameraHeadBobCrouching, 1.0f);
	}
	else
	{
		PlayerController->PlayerCameraManager->StartCameraShake(CameraHeadBobIdle, 1.0f);
	}
}

void AIABTYCharacter::CalculateDeltaRotation()
{
	const float CurrentYaw = GetActorRotation().Yaw;
	DeltaRotation = FMath::FindDeltaAngleDegrees(PreviousYaw, CurrentYaw);
	PreviousYaw = CurrentYaw;
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("DELTA: %f"), DeltaRotation));
}

void AIABTYCharacter::Jump()
{
	if (!bMovementAllowed) return;

	Super::Jump();
}

void AIABTYCharacter::Client_OnWeaponReady_Implementation(ABaseWeapon* NewWeapon, int InitialAmmo)
{
	if (!NewWeapon || !IsValid(NewWeapon))
	{
		TWeakObjectPtr<AIABTYCharacter> WeakThis(this);
		TWeakObjectPtr<ABaseWeapon> WeakWeapon(NewWeapon);
		GetWorld()->GetTimerManager().SetTimerForNextTick(
			[WeakThis, WeakWeapon, InitialAmmo]()
		{
			if (WeakThis.IsValid())
				WeakThis->Client_OnWeaponReady(
					WeakWeapon.Get(), InitialAmmo);
		});
		return;
	}

	if (CurrentWeapon == NewWeapon && NewWeapon->GetAttachParentActor() == this)
		return;

	CurrentWeapon = NewWeapon;
	CurrentWeapon->CurrentAmmo = InitialAmmo;
	CurrentWeapon->AttachWeaponToCorrectMesh(this);

	if (AProjectileWeapon* PW = Cast<AProjectileWeapon>(CurrentWeapon))
		PW->ClientUpdateUI();
	Client_PlayWeaponPickUpSound();
}

void AIABTYCharacter::DieInstantly_Implementation(AController* KillingController, bool bIsFriendlyFire, EWeapon WeaponType)
{
	if (bIsDead)
		return;

	bIsDead = true;

	DropWeapon();
	Multicast_OnDeathRagdoll();
	Multicast_PlayWallHitSound();
	Multicast_Confetti();
	Multicast_HideHead();
	Multicast_StopInflatingSound();
	Multicast_StopInflatingLightSound();

	AController* MyController = GetController();
	AIABTYGameMode* GM = GetWorld()->GetAuthGameMode<AIABTYGameMode>();

	if (GM && MyController)
	{
		GM->OnPlayerDied(KillingController, MyController, bIsFriendlyFire, WeaponType);
	}
}

void AIABTYCharacter::Multicast_OnDeathRagdoll_Implementation()
{
	if (!GetMesh()) return;

	GetMesh()->bPauseAnims = true;
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetAllBodiesBelowSimulatePhysics(FName("pelvis"), true, true);
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	FVector Momentum = GetVelocity();
	GetMesh()->SetAllPhysicsLinearVelocity(Momentum);
}

void AIABTYCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AIABTYCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{	
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AIABTYCharacter::Move);

		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AIABTYCharacter::Look);
		
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &AIABTYCharacter::AttackInput);
	
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Triggered, this, &AIABTYCharacter::CrouchToggler);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AIABTYCharacter::Multicast_Confetti_Implementation()
{
	USkeletalMeshComponent* TargetMesh = GetMesh();

	if (IsLocallyControlled() && GetMesh1P())
	{
		TargetMesh = GetMesh1P();
	}

	if (!TargetMesh) return;

	UNiagaraFunctionLibrary::SpawnSystemAttached(
		ConfettiSystem,
		TargetMesh,
		TEXT("head"),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget,
		true
	);
}

void AIABTYCharacter::Multicast_PlayWallHitSound_Implementation()
{
	if (BalloonPop)
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), BalloonPop, GetActorLocation());
}

void AIABTYCharacter::EmptyWeapon(bool bIsSwapped)
{
	if (!CurrentWeapon) return;

	if (CurrentWeapon->WeaponSpawner && !bIsSwapped)
		CurrentWeapon->WeaponSpawner->StartCountdown();

	CurrentWeapon = nullptr;
	CurrentWeaponType = EWeapon::None;
}

void AIABTYCharacter::OnRep_Team()
{
	UpdateTeamColor();
}

void AIABTYCharacter::Multicast_IncreaseHeadScale_Implementation(float AmmountToIncrease)
{
	HeadScale += AmmountToIncrease;

	if (HeadScale < 1.0f)
		HeadScale = 1.0f;

	FBodyInstance* HeadBody = GetMesh()->GetBodyInstance(FName("head"));
	if (HeadBody)
	{
		HeadBody->UpdateBodyScale(FVector(HeadScale));
	}
}

void AIABTYCharacter::Multicast_StopInflatingSound_Implementation()
{
	ToggleBalloonSound(false);
}

void AIABTYCharacter::Multicast_StopInflatingLightSound_Implementation()
{
	ToggleBalloonLightSound(false);
}

void AIABTYCharacter::SetIsFiring()
{
	bIsFiring = true;

	TWeakObjectPtr<AIABTYCharacter> WeakThis(this);

	GetWorldTimerManager().SetTimer(FiringResetHandle, [WeakThis]()
	{
		if (WeakThis.IsValid()) WeakThis->bIsFiring = false;
	}, 0.05f, false);
}

void AIABTYCharacter::Multicast_SetIsFiring_Implementation()
{
	if (!IsLocallyControlled())
	{
		SetIsFiring();
	}
}

void AIABTYCharacter::Server_SetPitchAngle_Implementation(float NewPitchAngle)
{
	PitchAngle = NewPitchAngle;
}

void AIABTYCharacter::RestoreBalloonHP(float DeltaTime)
{
	BalloonHP += 5.f * DeltaTime;
	BalloonHP = FMath::Clamp(BalloonHP, 0.f, 100.f);
}

void AIABTYCharacter::ToggleBalloonSound(bool bNewState)
{
	bNewState ? BalloonInflatingAudioComponent->Play() : BalloonInflatingAudioComponent->Stop();	
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("State: %s"), bNewState ? TEXT("true") : TEXT("false")));
}

void AIABTYCharacter::ToggleBalloonLightSound(bool bNewState)
{
	bNewState ? BalloonInflatingLightAudioComponent->Play() : BalloonInflatingLightAudioComponent->Stop();
}

void AIABTYCharacter::Client_PlayExplosionShake_Implementation(float Scale)
{
	if (!IsLocallyControlled())
		return;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->ClientStartCameraShake(ExplosionShakeClass, Scale);
	}
}

void AIABTYCharacter::Move(const FInputActionValue& Value)
{
	if (!bMovementAllowed) return;

	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		AddMovementInput(GetActorForwardVector(), MovementVector.Y);
		AddMovementInput(GetActorRightVector(), MovementVector.X);
	}
}

void AIABTYCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		AIABTYController* PC = Cast<AIABTYController>(Controller);
		if (!PC) return;
		AddControllerYawInput(LookAxisVector.X * PC->SensitivityMultiplierX);
		AddControllerPitchInput(LookAxisVector.Y * PC->SensitivityMultiplierY);

		PitchAngle = GetFirstPersonCameraComponent()->GetComponentRotation().Pitch;
		Server_SetPitchAngle(PitchAngle);
	}
}

void AIABTYCharacter::UpdateArmsOffset()
{
	float CurrentCameraPitch = GetControlRotation().Pitch;
	float CurrentCharacterYaw = GetActorRotation().Yaw;

	float CameraYawDelta = FMath::UnwindDegrees(CurrentCameraPitch - PreviousCameraPitch);
	float CharacterYawDelta = FMath::UnwindDegrees(CurrentCharacterYaw - PreviousCharacterYaw);

	float TargetOffsetX = FMath::Clamp(-CharacterYawDelta * 5.0f, -50.0f, 50.0f);
	float TargetOffsetY = FMath::Clamp(-CameraYawDelta * 5.0f, -50.0f, 50.0f);

	float InterpSpeed = 8.0f;

	PreviousCameraPitch = CurrentCameraPitch;
	PreviousCharacterYaw = CurrentCharacterYaw;

	FVector CurrentCharacterLocation = GetActorLocation();
	FVector CharacterLocationDelta = CurrentCharacterLocation - PreviousCharacterLocation;

	FVector CameraForward = GetControlRotation().Vector();
	FVector CameraRight = FRotationMatrix(GetControlRotation()).GetScaledAxis(EAxis::Y);
	FVector CameraUp = FRotationMatrix(GetControlRotation()).GetScaledAxis(EAxis::Z);

	float LocalDeltaX = FVector::DotProduct(CharacterLocationDelta, CameraRight);
	float LocalDeltaY = FVector::DotProduct(CharacterLocationDelta, CameraUp);
	float LocalDeltaZ = FVector::DotProduct(CharacterLocationDelta, CameraForward);

	TargetOffsetX += FMath::Clamp(-LocalDeltaX * 5.0f, -50.0f, 50.0f);
	TargetOffsetY += FMath::Clamp(-LocalDeltaY * 5.0f, -50.0f, 50.0f);
	float TargetOffsetZ = FMath::Clamp(-LocalDeltaZ * 5.0f, -100.0f, 100.0f);

	CameraOffsetX = FMath::FInterpTo(CameraOffsetX, TargetOffsetX, GetWorld()->GetDeltaSeconds(), InterpSpeed);
	CameraOffsetY = FMath::FInterpTo(CameraOffsetY, TargetOffsetY, GetWorld()->GetDeltaSeconds(), InterpSpeed);
	CameraOffsetZ = FMath::FInterpTo(CameraOffsetZ, TargetOffsetZ, GetWorld()->GetDeltaSeconds(), InterpSpeed);

	PreviousCharacterLocation = CurrentCharacterLocation;
}

void AIABTYCharacter::UpdateMovementAnimationSpeed()
{
	float Speed = GetVelocity().Size();
	MovementAnimationRate = Speed / 235.0f;
}

void AIABTYCharacter::AttackInput()
{
	if (!bMovementAllowed) return;
	if (!CurrentWeapon) return;

	FVector CameraLocation = GetFirstPersonCameraComponent()->GetComponentLocation();
	FRotator CameraRotation = GetFirstPersonCameraComponent()->GetComponentRotation();

	if (IsLocallyControlled())
	{
		CurrentWeapon->AttackLocal(CameraLocation, CameraRotation);
	}
}

void AIABTYCharacter::ServerAttackInput_Implementation(FVector CameraLocation, FRotator CameraRotation)
{
	if (CurrentWeapon) {
		CurrentWeapon->Attack(CameraLocation, CameraRotation);
	}
}

void AIABTYCharacter::ServerCallBodyHitSound_Implementation(FVector Location)
{
	Multicast_PlayBodyHitSound(Location);
}

void AIABTYCharacter::Multicast_PlayBodyHitSound_Implementation(FVector Location)
{
	UGameplayStatics::PlaySoundAtLocation(GetWorld(), BodyHitSound, Location);
}