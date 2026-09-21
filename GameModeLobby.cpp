#include "GameModeLobby.h"
#include "IABTY/IABTYAdvancedFriendsGameInstance.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include <Net/UnrealNetwork.h>
#include "GameFramework/GameSession.h"
#include "IABTYPlayerState.h"
#include "IABTY/IABTYController.h"
#include <IABTY/IABTYGameState.h>

AGameModeLobby::AGameModeLobby()
{
    bUseSeamlessTravel = true;
}

void AGameModeLobby::BeginPlay()
{
    Super::BeginPlay();
}

void AGameModeLobby::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    const int32 MaxPlayers = 4;

    if (GetNumPlayers() >= MaxPlayers)
    {
        ErrorMessage = TEXT("SessionFull");
        return;
    }

    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
}

void AGameModeLobby::InitializeAllowedPlayers()
{
    UIABTYAdvancedFriendsGameInstance* GI = GetGameInstance<UIABTYAdvancedFriendsGameInstance>();
    if (!GI) return;

    GI->AllowedPlayers.Empty();

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get())
        {
            if (const APlayerState* PS = PC->PlayerState)
            {
                GI->AllowedPlayers.Add(PS->GetUniqueId());
            }
        }
    }
}

void AGameModeLobby::ChangeTeam(APlayerController* PlayerToChange, int TeamIndex)
{
    UIABTYAdvancedFriendsGameInstance* GI = GetGameInstance<UIABTYAdvancedFriendsGameInstance>();
    if (!GI) return;

    AIABTYPlayerState* PS = Cast<AIABTYPlayerState>(PlayerToChange->PlayerState);
    if (!PS) return;

    const FString PlayerName = PS->PlayerKey;

    switch (TeamIndex)
    {
    case 0:
        PS->Team = ETeam::Blue;
        break;

    case 1:
        PS->Team = ETeam::Red;
        break;

    case 2:
        PS->Team = ETeam::Green;
        break;

    case 3:
        PS->Team = ETeam::Yellow;
        break;

    default:
        PS->Team = ETeam::Blue;
        break;
    }

    GI->SetPlayerTeam(PlayerName, PS->Team);
    BP_UpdatePlayers(PlayerToChange);
}

bool AGameModeLobby::CheckIfEnoughTeams()
{
    ETeam FirstTeam = ETeam::None;
    bool bFirstTeamSet = false;

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get())
        {
            if (AIABTYPlayerState* PS = Cast<AIABTYPlayerState>(PC->PlayerState))
            {
                if (!bFirstTeamSet)
                {
                    FirstTeam = PS->Team;
                    bFirstTeamSet = true;
                }
                else if (PS->Team != FirstTeam)
                {
                    return true;
                }
            }
        }
    }

    UIABTYAdvancedFriendsGameInstance* GI = GetGameInstance<UIABTYAdvancedFriendsGameInstance>();
    if (!GI) return false;
    if (GI->bDeveloperMode)
        return true;

    return false; //CAMBIAR A FALSO!!
}

void AGameModeLobby::SetMatchSettings(TArray<FString> MapNames, int MapNumber, int RoundNumber)
{
    UIABTYAdvancedFriendsGameInstance* GI = GetGameInstance<UIABTYAdvancedFriendsGameInstance>();
    if (!GI) return;

    if (GI->bIsDemo) 
    {
        AvailableMaps = MapNamesPool;

        for (const FString& Name : MapNames)
        {
            if (!Name.Equals(TEXT("Random"), ESearchCase::IgnoreCase))
            {
                AvailableMaps.Remove(Name);
            }
        }
        FColor Color = FColor::MakeRandomColor();
        ShuffleArray(AvailableMaps);

        for (FString& Name : MapNames)
        {
            if (Name.Equals(TEXT("Random"), ESearchCase::IgnoreCase))
            {
                if (AvailableMaps.IsEmpty())
                {
                    AvailableMaps = MapNamesPool;

                    for (const FString& UsedMap : MapNames)
                    {
                        if (!UsedMap.Equals(TEXT("Random"), ESearchCase::IgnoreCase))
                        {
                            AvailableMaps.Remove(UsedMap);
                        }
                    }

                    ShuffleArray(AvailableMaps);
                }

                if (!AvailableMaps.IsEmpty())
                {
                    Name = AvailableMaps.Pop();
                }
            }
            //GEngine->AddOnScreenDebugMessage(-1, 10.f, Color, *Name);
        }
    }
    else {
        AvailableMaps = MapNamesPool;
        ShuffleArray(AvailableMaps);

        for (FString& Name : MapNames)
        {
            if (Name.Equals(TEXT("Random"), ESearchCase::IgnoreCase))
            {
                if (AvailableMaps.IsEmpty())
                {
                    AvailableMaps = MapNamesPool;
                    ShuffleArray(AvailableMaps);
                }
                Name = AvailableMaps.Pop();
            }
            //GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, *Name);
        }
    }

    GI->CurrentMap = 0;
    GI->MapNames = MapNames;
    GI->NumberOfMaps = MapNumber;
    GI->RoundsToWin = RoundNumber;
}

void AGameModeLobby::ShuffleArray(TArray<FString>& Array)
{
    for (int32 i = Array.Num() - 1; i > 0; --i)
    {
        int32 Index = FMath::RandRange(0, i);
        if (i != Index)
        {
            Array.Swap(i, Index);
        }
    }
}

void AGameModeLobby::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    UIABTYAdvancedFriendsGameInstance* GI = GetGameInstance<UIABTYAdvancedFriendsGameInstance>();
    if (!GI) return;
    GI->NumberOfPlayersConnected++;
    InitPlayerAfterLoginOrTravel(NewPlayer);
}

void AGameModeLobby::Logout(AController* Exiting)
{
    Super::Logout(Exiting);

    FString SteamID;

    APlayerController* NewPlayer = Cast<APlayerController>(Exiting);

    UIABTYAdvancedFriendsGameInstance* GI = GetGameInstance<UIABTYAdvancedFriendsGameInstance>();
    if (!GI || !NewPlayer) return;

    AIABTYPlayerState* PS = Cast<AIABTYPlayerState>(NewPlayer->PlayerState);
    if (!PS) return;

    AIABTYGameState* GS = GetGameState<AIABTYGameState>();
    if (!GS) return;

#if WITH_EDITOR
    if (GetWorld()->WorldType == EWorldType::PIE)
    {
        int32 PlayerIndex = GS->PlayerArray.Num() - 1;
        SteamID = FString::Printf(TEXT("PIE_Player_%d"), PlayerIndex);
    }
    else
#endif
    {
        if(PS->GetUniqueId() != nullptr)
            SteamID = PS->GetUniqueId()->ToString();
    }

    AvailableTeams.Insert(PS->Team, 0);

    GS->NumberOfTeams--;
    FPlayerStats& Stats = GI->GetOrCreatePlayerStats(SteamID);
    GI->PlayerTeams.Remove(SteamID);
    BP_PlayerLeft();

    if (!bIsTraveling) 
    {
        GI->NumberOfPlayersConnected--;
    }
}

void AGameModeLobby::HandleSeamlessTravelPlayer(AController*& C)
{
    Super::HandleSeamlessTravelPlayer(C);

    InitPlayerAfterLoginOrTravel(C);
}

void AGameModeLobby::InitPlayerAfterLoginOrTravel(AController* Controller)
{
    FString LevelName = GetWorld()->GetMapName();
    LevelName.RemoveFromStart(GetWorld()->StreamingLevelsPrefix);

    FString SteamID;

    APlayerController* NewPlayer = Cast<APlayerController>(Controller);
    if (!NewPlayer) return;

    UIABTYAdvancedFriendsGameInstance* GI = GetGameInstance<UIABTYAdvancedFriendsGameInstance>();
    if (!GI) return;

    AIABTYPlayerState* PS = Cast<AIABTYPlayerState>(NewPlayer->PlayerState);
    if (!PS) return;

    AIABTYGameState* GS = GetGameState<AIABTYGameState>();
    if (!GS) return;

#if WITH_EDITOR
    if (GetWorld()->WorldType == EWorldType::PIE)
    {
        int32 PlayerIndex = GS->PlayerArray.Num() - 1;
        SteamID = FString::Printf(TEXT("PIE_Player_%d"), PlayerIndex);
    }
    else
#endif
    {
        int32 PlayerIndex = GS->PlayerArray.Num() - 1;
        SteamID = PS->GetUniqueId()->ToString();
    }

    FPlayerStats& Stats = GI->GetOrCreatePlayerStats(SteamID);

    if (LevelName == "PostMatch") 
    {
        PS->Team = Stats.Team;
        PS->Position = Stats.Position;
        PS->Points = Stats.Points;
        PS->Rounds = Stats.Rounds;
        PS->LocalRounds = Stats.LocalRounds;
        PS->Kills = Stats.Kills;
        PS->Deaths = Stats.Deaths;
        PS->PlayerKey = SteamID;
        PS->PlayerStrokes = Stats.PaintStrokes;

        BP_InitPlayerAfterLoginOrTravel(Controller);

        if (PS->Position == 1) {

            AIABTYController* IABTYController = Cast<AIABTYController>(PS->GetOwningController());
            if (!IABTYController) return;

            IABTYController->ClientAddOneToSteamStat("MatchesWon_MatchesWon");
        }
        return;
    }

    if (AvailableTeams.IsEmpty())
        return;

    PS->Team = AvailableTeams[0];
    GI->SetPlayerTeam(SteamID, AvailableTeams[0]);

    AvailableTeams.RemoveAt(0);

    GS->NumberOfTeams++;
    PS->PlayerKey = SteamID;
    PS->Position = 0;
    PS->Points = 0;
    PS->Rounds = 0;
    PS->LocalRounds = 0;
    PS->Kills = 0;
    PS->Deaths = 0;
    PS->Team = Stats.Team;
    Stats.Position = 0;
    Stats.Points = 0;
    Stats.Rounds = 0;
    Stats.LocalRounds = 0;
    Stats.Kills = 0;
    Stats.Deaths = 0;

    BP_InitPlayerAfterLoginOrTravel(Controller);
}

void AGameModeLobby::AssignTeams()
{
    UIABTYAdvancedFriendsGameInstance* GI = GetGameInstance<UIABTYAdvancedFriendsGameInstance>();
    if (!GI) return;

    TArray<AIABTYPlayerState*> PlayerStates;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (APlayerController* PC = It->Get())
        {
            if (AIABTYPlayerState* PS = Cast<AIABTYPlayerState>(PC->PlayerState))
            {
                PlayerStates.Add(PS);
            }
        }
    }

    const int32 NumPlayers = PlayerStates.Num();

    static const TArray<ETeam> Teams = { ETeam::Blue, ETeam::Red, ETeam::Green, ETeam::Yellow };

    for (int32 i = 0; i < PlayerStates.Num(); ++i)
    {
        const FString PlayerName = PlayerStates[i]->PlayerKey;
        const ETeam Team = Teams[i % Teams.Num()];

        PlayerStates[i]->Team = Team;
        GI->SetPlayerTeam(PlayerName, Team);
    }
}