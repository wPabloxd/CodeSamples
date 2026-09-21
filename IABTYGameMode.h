#pragma once

#include "CoreMinimal.h"
#include "IABTY/IABTYCharacter.h"
#include "GameFramework/GameModeBase.h"
#include "IABTYGameMode.generated.h"

class AIABTYCharacter;
class AIABTYPlayerState;
class APlayerStart;

UCLASS(minimalapi)
class AIABTYGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AIABTYGameMode();

protected:
	virtual void BeginPlay() override;

public:

	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

	virtual void PostLogin(APlayerController* NewPlayer) override;

	virtual void Logout(AController* Exiting) override;

	bool bTraveling = false;

	UPROPERTY()
	int32 ReadyPlayerCount = 0;

	UFUNCTION(BlueprintCallable)
	void NotifyPlayerReady(APlayerController* PC);

	FTimerHandle BeginRoundVoiceLineTimer;
	void BeginRoundVoiceLine();

	bool bBeginRoundVoiceLine = false;

	UFUNCTION(BlueprintImplementableEvent)
	void BP_StartRound();

	bool bRoundStarted = false;

	UFUNCTION(BlueprintImplementableEvent)
	void BP_InitPlayerAfterLoginOrTravel(APlayerController* NewPlayer);

	void OnPlayerDied(AController* KillingController, AController* DeadController, bool bIsFriendlyFire, EWeapon WeaponType);

	virtual void HandleSeamlessTravelPlayer(AController*& C) override;

	void InitPlayerAfterLoginOrTravel(AController* Controller);

	UFUNCTION()
	void StartNewRound();

	void StartNextMap();

	void EndMatch();

	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	UFUNCTION(BlueprintImplementableEvent)
	void BP_UpdateScoreboard();

	UFUNCTION(BlueprintImplementableEvent)
	void BP_UpdateFeed(AIABTYPlayerState* PSK, AIABTYPlayerState* PSD, EWeapon WeaponType);

	UFUNCTION(BlueprintImplementableEvent)
	void BP_PointOver(ETeam Winner);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	float TimeToBeginGrowing = 45.0f;

	UFUNCTION(Server, Reliable)
	void SpawnAmmunition(FVector Velocity, FVector SpawnLocation, FRotator SpawnRotation, FVector HitNormal, bool bIsStucked, EWeapon WeaponType, USoundBase* Sound, FVector OriginalPosition, APlayerController* FiringPlayerController);

private:

	FTimerHandle WaitingTimedOutTimer;

	void WaitingForPlayersTimedOut();

	FTimerHandle BalloonsGrowingTimer;

	void ToggleBalloonGrowth();

	FTimerHandle RoundRestartTimer;

	UPROPERTY()
	TArray<APlayerStart*> AvailablePlayerStarts;

	void CheckForRoundWin();

	void PlayEndPointVoiceLine();

	bool bIsRoundOver = false;

	bool bIsMapOver = false;

	int KillStreak1 = 4;
	int KillStreak2 = 7;
	int KillStreak3 = 10;
	int BreakKillStreak = 5;

	int DeathStreak1 = 4;
	int DeathStreak2 = 7;
	int DeathStreak3 = 10;
	int BreakDeathStreak = 5;

	int KillsForAce = 3;

	int RoundStreak1 = 3;
	int RoundStreak2 = 5;
	int RoundStreak3 = 7;
	int BreakRoundStreak = 3;

	int PointStreak1 = 3;
	int PointStreak2 = 5;
	int PointStreak3 = 7;
	int BreakPointStreak = 3;
};