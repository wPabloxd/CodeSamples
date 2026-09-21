#pragma once

#include "CoreMinimal.h"
#include "IABTY/Gameplay/ETeam.h"
#include "GameFramework/GameModeBase.h"
#include "GameModeLobby.generated.h"

UCLASS()
class IABTY_API AGameModeLobby : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGameModeLobby();

protected:
	virtual void BeginPlay() override;

public:
	virtual void PreLogin(const FString& Options,	const FString& Address,	const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	UPROPERTY(BlueprintReadWrite)
	bool bIsTraveling = false;

	virtual void HandleSeamlessTravelPlayer(AController*& C) override;

	void InitPlayerAfterLoginOrTravel(AController* Controller);

	UFUNCTION(BlueprintImplementableEvent)
	void BP_InitPlayerAfterLoginOrTravel(AController* Controller);

	UFUNCTION(BlueprintImplementableEvent)
	void BP_UpdatePlayers(APlayerController* Controller);

	UFUNCTION(BlueprintImplementableEvent)
	void BP_PlayerLeft();

	UFUNCTION(BlueprintCallable)
	void InitializeAllowedPlayers();

	UFUNCTION(BlueprintCallable)
	void ChangeTeam(APlayerController* PlayerToChange, int TeamIndex);

	UFUNCTION(BlueprintCallable)
	bool CheckIfEnoughTeams();

	UFUNCTION(BlueprintCallable)
	void SetMatchSettings(TArray<FString> MapNames, int MapNumber, int RoundNumber);

	UPROPERTY()
	TArray<FString> AvailableMaps;

	UPROPERTY(BlueprintReadWrite)
	TArray<FString> MapNamesPool;

	void ShuffleArray(TArray<FString>& Array);

protected:
	void AssignTeams();

	TArray<ETeam> AvailableTeams = { ETeam::Blue, ETeam::Red, ETeam::Green, ETeam::Yellow };
};