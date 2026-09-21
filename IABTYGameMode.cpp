#include "IABTYGameMode.h"
#include "IABTYCharacter.h"
#include "IABTYGameState.h"
#include "Lobby/IABTYPlayerState.h"
#include "IABTYAdvancedFriendsGameInstance.h"
#include "IABTY/Weapons/AmmunitionPickUp.h"
#include "IABTY/Weapons/MagneticBomb.h"
#include "GameFramework/PlayerStart.h"
#include "IABTYController.h"
#include "UObject/ConstructorHelpers.h"
#include "Gameplay/FreeSpectatorPawn.h"
#include <Kismet/GameplayStatics.h>
#include "Auxiliary/ProjectileSpawnerCaller.h"

AIABTYGameMode::AIABTYGameMode()
	: Super()
{
    bUseSeamlessTravel = true;
    static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
    static ConstructorHelpers::FClassFinder<APlayerController> PlayerControllerClassFinder(TEXT("/Game/Blueprints/Gameplay/PC_GameController"));
    DefaultPawnClass = PlayerPawnClassFinder.Class;
    SpectatorClass = AFreeSpectatorPawn::StaticClass();
    PlayerControllerClass = PlayerControllerClassFinder.Class;
    GameStateClass = AIABTYGameState::StaticClass();
    PlayerStateClass = AIABTYPlayerState::StaticClass();

    TArray<AActor*> FoundStarts;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), FoundStarts);

    AvailablePlayerStarts.Empty();
    for (AActor* StartActor : FoundStarts)
    {
        if (APlayerStart* Start = Cast<APlayerStart>(StartActor))
        {
            AvailablePlayerStarts.Add(Start);
        }
    }
}

void AIABTYGameMode::BeginPlay()
{
    Super::BeginPlay();

    bIsMapOver = false;
}

void AIABTYGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    UIABTYAdvancedFriendsGameInstance* GI = GetGameInstance<UIABTYAdvancedFriendsGameInstance>();
    if (!GI) return;

    if (!GI->AllowedPlayers.Contains(UniqueId) && !GI->bDeveloperMode)
    {
        ErrorMessage = TEXT("MatchInProgress");
        return;
    }

    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
}

void AIABTYGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    InitPlayerAfterLoginOrTravel(NewPlayer);

    if (bRoundStarted)
    {
        UIABTYAdvancedFriendsGameInstance* GI = GetGameInstance<UIABTYAdvancedFriendsGameInstance>();
        GI->NumberOfPlayersConnected++;
    }
}

void AIABTYGameMode::Logout(AController* Exiting)
{
    if (!bTraveling) 
    {
        UIABTYAdvancedFriendsGameInstance* GI = GetGameInstance<UIABTYAdvancedFriendsGameInstance>();
        if (!GI) return;
        
        GI->NumberOfPlayersConnected--;

        if (GI->NumberOfPlayersConnected == 1)
        {
            GetWorldTimerManager().ClearTimer(RoundRestartTimer);
            EndMatch();
        }
    }
}

void AIABTYGameMode::NotifyPlayerReady(APlayerController* PC)
{
    if (bRoundStarted)
        return;

    UIABTYAdvancedFriendsGameInstance* GI = GetGameInstance<UIABTYAdvancedFriendsGameInstance>();

    if (GI->bDeveloperMode) 
    {
        BP_StartRound();
        bRoundStarted = true;
        return;
    }

    ReadyPlayerCount++;
    if (!GetWorldTimerManager().IsTimerActive(WaitingTimedOutTimer))
        GetWorldTimerManager().SetTimer(WaitingTimedOutTimer, this, &AIABTYGameMode::WaitingForPlayersTimedOut, 6.0f, false);

    if (ReadyPlayerCount >= GI->NumberOfPlayersConnected && !bRoundStarted)
    {
        GetWorldTimerManager().ClearTimer(WaitingTimedOutTimer);
        GetWorldTimerManager().SetTimer(BalloonsGrowingTimer, this, &AIABTYGameMode::ToggleBalloonGrowth, TimeToBeginGrowing, false);
        BP_StartRound();
        GetWorldTimerManager().SetTimer(BeginRoundVoiceLineTimer, this, &AIABTYGameMode::BeginRoundVoiceLine, 3.0f, false);
        bRoundStarted = true;
    }
}

void AIABTYGameMode::BeginRoundVoiceLine()
{
    if (bBeginRoundVoiceLine)
        return;

    bBeginRoundVoiceLine = true;
    AIABTYGameState* GS = GetWorld()->GetGameState<AIABTYGameState>();
    UIABTYAdvancedFriendsGameInstance* GI = GetGameInstance<UIABTYAdvancedFriendsGameInstance>();

    if (!GS || !GI) return;

    if (GI->RoundNumber == 0)
    {
        AIABTYPlayerState* NoPointsPlayer = nullptr;
        AIABTYPlayerState* LastPlacePlayer = nullptr;

        int32 WorstPosition = INT32_MIN;

        for (APlayerState* PS : GS->PlayerArray)
        {
            if (AIABTYPlayerState* IABTYPS = Cast<AIABTYPlayerState>(PS))
            {
                if (IABTYPS->Points == 0)
                {
                    NoPointsPlayer = IABTYPS;
                }

                if (IABTYPS->Position > WorstPosition)
                {
                    WorstPosition = IABTYPS->Position;
                    LastPlacePlayer = IABTYPS;
                }
            }
        }

        if (NoPointsPlayer && GI->CurrentMap >= 3)
        {
            GS->TriggerVoiceEvent(FVoiceTags::NoPoints, NoPointsPlayer->Team);
            return;
        }

        if (LastPlacePlayer && GI->CurrentMap >= 3)
        {
            GS->TriggerVoiceEvent(FVoiceTags::LastPlace, LastPlacePlayer->Team);
            return;
        }

        GS->TriggerVoiceEvent(FVoiceTags::RandomComment, ETeam::None);
        return;
    }


    int32 MaxRounds = -1;
    int32 MaxCount = 0;

    AIABTYPlayerState* OneToWinPlayer = nullptr;
    AIABTYPlayerState* ZeroRoundsPlayer = nullptr;

    const int32 Target = GI->RoundsToWin - 1;

    for (APlayerState* PS : GS->PlayerArray)
    {
        if (AIABTYPlayerState* IABTYPS = Cast<AIABTYPlayerState>(PS))
        {
            int32 Rounds = IABTYPS->LocalRounds;

            if (Rounds > MaxRounds)
            {
                MaxRounds = Rounds;
                MaxCount = 1;
            }
            else if (Rounds == MaxRounds)
            {
                MaxCount++;
            }

            if (Rounds == Target)
            {
                OneToWinPlayer = IABTYPS;
            }

            if (Rounds == 0 && GI->RoundNumber > 3)
            {
                ZeroRoundsPlayer = IABTYPS;
            }
        }
    }

    bool bTieDetected = (MaxCount >= 2);
    bool bTieAtOneToWin = (bTieDetected && MaxRounds == Target);

    if (bTieAtOneToWin)
    {
        GS->TriggerVoiceEvent(FVoiceTags::BeginRoundTie, ETeam::None);
        return;
    }

    if (OneToWinPlayer)
    {
        GS->TriggerVoiceEvent(FVoiceTags::BeginRoundOneToWin, OneToWinPlayer->Team);
        return;
    }

    if (bTieDetected)
    {
        GS->TriggerVoiceEvent(FVoiceTags::BeginRoundTie, ETeam::None);
        return;
    }

    if (ZeroRoundsPlayer)
    {
        GS->TriggerVoiceEvent(FVoiceTags::BeginRoundZeroRounds, ZeroRoundsPlayer->Team);
        return;
    }

    GS->TriggerVoiceEvent(FVoiceTags::RandomComment, ETeam::None);
}

void AIABTYGameMode::OnPlayerDied(AController* KillingController, AController* DeadController, bool bIsFriendlyFire, EWeapon WeaponType)
{
    if (!DeadController) return;

    APawn* OldPawn = DeadController->GetPawn();
    FVector SpawnLocation = OldPawn ? OldPawn->GetActorLocation() : FVector::ZeroVector;
    SpawnLocation = FVector(SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z + 50.0f);
    FRotator SpawnRotation = OldPawn ? OldPawn->GetActorRotation() : FRotator::ZeroRotator;

    FActorSpawnParameters Params;
    Params.Owner = DeadController;
    Params.Instigator = OldPawn;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AFreeSpectatorPawn* NewSpectator = GetWorld()->SpawnActor<AFreeSpectatorPawn>(SpectatorClass, SpawnLocation, SpawnRotation, Params);

    AIABTYPlayerState* KillerPS = nullptr;
    if (KillingController && KillingController->PlayerState)
        KillerPS = Cast<AIABTYPlayerState>(KillingController->PlayerState);
    AIABTYPlayerState* DeadPS = DeadController->PlayerState ? Cast<AIABTYPlayerState>(DeadController->PlayerState) : nullptr;
   
    UIABTYAdvancedFriendsGameInstance* GI = GetGameInstance<UIABTYAdvancedFriendsGameInstance>();
    
    if (KillerPS)
    {
        AIABTYGameState* GS = GetWorld()->GetGameState<AIABTYGameState>();

        if (bIsFriendlyFire) 
        {
            if(KillingController == DeadController)
                GS->TriggerVoiceEvent(FVoiceTags::Suicide, KillerPS->Team);
            else
                GS->TriggerVoiceEvent(FVoiceTags::FriendlyFireKill, KillerPS->Team);

            KillerPS->Kills--;
        }
        else 
        {

            KillerPS->KillStreak++;
            KillerPS->KillsCurrentRound++;

            if (KillerPS->KillsCurrentRound == KillsForAce) 
            {
                if (AIABTYController* Controller = Cast<AIABTYController>(KillerPS->GetOwningController()))
                    Controller->ClientUnlockAchievement("END_ROUND_ACE");

                GS->TriggerVoiceEvent(FVoiceTags::Ace, KillerPS->Team);
            }

            if (KillerPS->KillStreak == KillStreak3) 
            {
                if (AIABTYController* Controller = Cast<AIABTYController>(KillerPS->GetOwningController()))
                    Controller->ClientUnlockAchievement("KILL_STREAK_10");
            }

            if(KillerPS->DeathStreak >= BreakDeathStreak)
                GS->TriggerVoiceEvent(FVoiceTags::BreakDeathStreak, KillerPS->Team);

            KillerPS->DeathStreak = 0;
            KillerPS->Kills++;

            if(KillerPS->KillStreak == KillStreak1)
                GS->TriggerVoiceEvent(FVoiceTags::KillStreak1, KillerPS->Team);
            else if (KillerPS->KillStreak == KillStreak2)
                GS->TriggerVoiceEvent(FVoiceTags::KillStreak2, KillerPS->Team);
            else if (KillerPS->KillStreak == KillStreak3)
                GS->TriggerVoiceEvent(FVoiceTags::KillStreak3, KillerPS->Team);
        }

        if (GI)
        {
            FPlayerStats& KillerStats = GI->GetOrCreatePlayerStats(KillerPS->PlayerKey);
            KillerStats.Kills = KillerPS->Kills;
            KillerStats.KillStreak = KillerPS->KillStreak;
            KillerStats.DeathStreak = KillerPS->DeathStreak;
        }
    }

    if (DeadPS)
    {
        AIABTYGameState* GS = GetWorld()->GetGameState<AIABTYGameState>();

        DeadPS->Deaths++;

        if (DeadPS->KillStreak >= BreakKillStreak)
            GS->TriggerVoiceEvent(FVoiceTags::BreakKillStreak, DeadPS->Team);

        DeadPS->KillStreak = 0;
        DeadPS->DeathStreak++;

        if (DeadPS->DeathStreak == DeathStreak1)
            GS->TriggerVoiceEvent(FVoiceTags::DeathStreak1, DeadPS->Team);
        else if (DeadPS->DeathStreak == DeathStreak2)
            GS->TriggerVoiceEvent(FVoiceTags::DeathStreak2, DeadPS->Team);
        else if (DeadPS->DeathStreak == DeathStreak3)
            GS->TriggerVoiceEvent(FVoiceTags::DeathStreak3, DeadPS->Team);

        if (GI)
        {
            FPlayerStats& DeadStats = GI->GetOrCreatePlayerStats(DeadPS->PlayerKey);
            DeadStats.Deaths = DeadPS->Deaths;
            DeadStats.KillStreak = DeadPS->KillStreak;
            DeadStats.DeathStreak = DeadPS->DeathStreak;
        }
    }

    if (NewSpectator)
    {
        DeadController->Possess(NewSpectator);
    }

    if (AIABTYGameState* GS = GetWorld()->GetGameState<AIABTYGameState>())
    {
        GS->UpdatePlayerPositions();
    }

    BP_UpdateScoreboard();
    BP_UpdateFeed(KillerPS, DeadPS, WeaponType);
    CheckForRoundWin();
}

void AIABTYGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
    Super::HandleSeamlessTravelPlayer(C);

    InitPlayerAfterLoginOrTravel(C);
}

void AIABTYGameMode::InitPlayerAfterLoginOrTravel(AController* Controller)
{
    APlayerController* NewPlayer = Cast<APlayerController>(Controller);

    UIABTYAdvancedFriendsGameInstance* GI = GetGameInstance<UIABTYAdvancedFriendsGameInstance>();
    if (!GI || !NewPlayer) return;

    AIABTYCharacter* NewChar = Cast<AIABTYCharacter>(NewPlayer->GetPawn());

    AIABTYPlayerState* PS = Cast<AIABTYPlayerState>(NewPlayer->PlayerState);
    if (!PS) return;

    FString SteamID;

    AIABTYGameState* GS = GetGameState<AIABTYGameState>();
    if (!GS) return;

#if WITH_EDITOR
    if (GetWorld()->WorldType == EWorldType::PIE)
    {
        int32 PlayerIndex = GS->PlayerArray.Num() - 1;
        SteamID = FString::Printf(TEXT("PIE_Player_%d"), PlayerIndex);

        static const TArray<ETeam> Teams = { ETeam::Blue, ETeam::Red, ETeam::Green, ETeam::Yellow };
        ETeam AssignedTeam = Teams[PlayerIndex % Teams.Num()];

        FPlayerStats& Stats = GI->GetOrCreatePlayerStats(SteamID);

        GI->SetPlayerTeam(SteamID, AssignedTeam);
    }
    else
#endif
    {
        SteamID = PS->GetUniqueId()->ToString();
        GS->AddPlayerAfterTeam(PS);

        FPlayerStats& Stats = GI->GetOrCreatePlayerStats(SteamID);

        GI->SetPlayerTeam(SteamID, Stats.Team);
    }

    PS->PlayerKey = SteamID;
    FPlayerStats& Stats = GI->GetOrCreatePlayerStats(SteamID);


    PS->SetTeam(Stats.Team);
    PS->Position = Stats.Position;
    PS->Points = Stats.Points;
    PS->Rounds = Stats.Rounds;
    PS->LocalRounds = Stats.LocalRounds;
    PS->Kills = Stats.Kills;
    PS->Deaths = Stats.Deaths;
    PS->KillStreak = Stats.KillStreak;
    PS->DeathStreak = Stats.DeathStreak;
    PS->RoundStreak = Stats.RoundStreak;
    PS->PointStreak = Stats.PointStreak;
    PS->PlayerStrokes = Stats.PaintStrokes;

    if (NewChar)
    {
        NewChar->Team = Stats.Team;
        NewChar->UpdateTeamColor();
    }

    GS->UpdatePlayerPositions();

    BP_InitPlayerAfterLoginOrTravel(NewPlayer);
}
void AIABTYGameMode::StartNewRound()
{
    UWorld* World = GetWorld();
    if (!World) return;

    FString CurrentLevel = World->GetMapName();
    CurrentLevel.RemoveFromStart(World->StreamingLevelsPrefix);

    bTraveling = true;
    GetWorld()->ServerTravel(*CurrentLevel);
}

void AIABTYGameMode::StartNextMap()
{
    UWorld* World = GetWorld();
    if (!World) return;

    UIABTYAdvancedFriendsGameInstance* GI = GetGameInstance<UIABTYAdvancedFriendsGameInstance>();
    if (!GI) return;

    for (APlayerState* PS : GetGameState<AIABTYGameState>()->PlayerArray)
    {
        if (AIABTYPlayerState* IABTYPS = Cast<AIABTYPlayerState>(PS))
        {
            IABTYPS->LocalRounds = 0;
            FPlayerStats& Stats = GI->GetOrCreatePlayerStats(IABTYPS->PlayerKey);
            Stats.LocalRounds = IABTYPS->LocalRounds;
            Stats.Position = IABTYPS->Position;
        }
    }

    GI->CurrentMap++;
    GI->RoundNumber = 0;

    if (GI->CurrentMap == GI->NumberOfMaps) {
        EndMatch();
        return;
    }

    FString NextLevel = GI->MapNames[GI->CurrentMap];
    NextLevel.RemoveFromStart(World->StreamingLevelsPrefix);

    bTraveling = true;
    GetWorld()->ServerTravel(*NextLevel);
}

void AIABTYGameMode::EndMatch()
{
    UWorld* World = GetWorld();
    FString PostMatchLevel = "PostMatch";
    PostMatchLevel.RemoveFromStart(World->StreamingLevelsPrefix);

    bTraveling = true;
    GetWorld()->ServerTravel(*PostMatchLevel);
}

AActor* AIABTYGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    if (AvailablePlayerStarts.Num() == 0)
    {
        return Super::ChoosePlayerStart_Implementation(Player);
    }
    int32 RandomIndex = FMath::RandRange(0, AvailablePlayerStarts.Num() - 1);
    APlayerStart* ChosenStart = AvailablePlayerStarts[RandomIndex];

    AvailablePlayerStarts.RemoveAt(RandomIndex);

    return ChosenStart;
}

void AIABTYGameMode::SpawnAmmunition_Implementation(FVector Velocity, FVector SpawnLocation, FRotator SpawnRotation, FVector HitNormal, bool bIsStucked, EWeapon WeaponType, USoundBase* Sound, FVector OriginalPosition, APlayerController* FiringPlayerController)
{
    if (bTraveling)
        return;

    UWorld* World = GetWorld();
    if (!World || World->bIsTearingDown)
    {
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AIABTYGameState* GS = GetWorld()->GetGameState<AIABTYGameState>();

    if (!IsValid(GS)) return;

    TSubclassOf<AAmmunitionPickUp> PickupClass = GS->AmmoPickupMap.FindRef(WeaponType);

    if (!PickupClass) return;

    AAmmunitionPickUp* PickUpProjectile = GetWorld()->SpawnActor<AAmmunitionPickUp>(
        PickupClass,
        WeaponType == EWeapon::MagneticBomb ? OriginalPosition : SpawnLocation,
        SpawnRotation,
        SpawnParams
    );

    if (!PickUpProjectile)
        return;

    if (bIsStucked)
    {
        PickUpProjectile->InitPickupStuck(true, FVector::Zero());

        if (Sound)
            GS->Multicast_PlayWallHitSound(HitNormal, Sound, SpawnLocation);
    }
    else
    {
        PickUpProjectile->InitPickupStuck(false, Velocity);
    }

    if (WeaponType == EWeapon::MagneticBomb) {
        AMagneticBomb* MagneticBomb = Cast<AMagneticBomb>(PickUpProjectile);
        if (MagneticBomb)
        {
            MagneticBomb->InitializeMagnet(SpawnLocation, HitNormal);
            MagneticBomb->SetOwner(FiringPlayerController);
        }
    }
}

void AIABTYGameMode::WaitingForPlayersTimedOut()
{
    if (!bRoundStarted) 
    {
        AIABTYGameState* GS = GetGameState<AIABTYGameState>();
        UIABTYAdvancedFriendsGameInstance* GI = GetGameInstance<UIABTYAdvancedFriendsGameInstance>();
        GI->NumberOfPlayersConnected = GS->PlayerArray.Num();

        GetWorldTimerManager().SetTimer(BalloonsGrowingTimer, this, &AIABTYGameMode::ToggleBalloonGrowth, TimeToBeginGrowing, false);
        BP_StartRound();
        GetWorldTimerManager().SetTimer(BeginRoundVoiceLineTimer, this, &AIABTYGameMode::BeginRoundVoiceLine, 3.0f, false);
        bRoundStarted = true;
    }
}

void AIABTYGameMode::ToggleBalloonGrowth()
{
    if (bIsRoundOver)
        return;

    if (AIABTYGameState* GS = GetGameState<AIABTYGameState>())
    {
        if(!GS->bBalloonGrowthStarted)
            GS->TriggerVoiceEvent(FVoiceTags::TimeElapsed, ETeam::None);

        GS->bBalloonGrowthStarted = !GS->bBalloonGrowthStarted;
        GS->OnRep_BalloonGrowthStarted();
    }
}

void AIABTYGameMode::CheckForRoundWin()
{
    if (bIsRoundOver)
        return;

    bool bBlueAlive = false;
    bool bRedAlive = false;
    bool bGreenAlive = false;
    bool bYellowAlive = false;

    TArray<AActor*> Pawns;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APawn::StaticClass(), Pawns);

    for (AActor* Actor : Pawns)
    {
        AIABTYCharacter* Character = Cast<AIABTYCharacter>(Actor);
        if (Character && !Character->GetIsPlayerDead())
        {
            switch (Character->Team)
            {
            case ETeam::Blue:  bBlueAlive = true; break;
            case ETeam::Red:   bRedAlive = true; break;
            case ETeam::Green: bGreenAlive = true; break;
            case ETeam::Yellow:bYellowAlive = true; break;
            default: break;
            }
        }
    }

    int32 AliveTeams = (bRedAlive ? 1 : 0) + (bBlueAlive ? 1 : 0) + (bGreenAlive ? 1 : 0) + (bYellowAlive ? 1 : 0);

    if (AliveTeams <= 1)
    {
        ETeam WinningTeam = ETeam::None;
        if (bBlueAlive) WinningTeam = ETeam::Blue;
        else if (bRedAlive) WinningTeam = ETeam::Red;
        else if (bGreenAlive) WinningTeam = ETeam::Green;
        else if (bYellowAlive) WinningTeam = ETeam::Yellow;

        UIABTYAdvancedFriendsGameInstance* GI = GetGameInstance<UIABTYAdvancedFriendsGameInstance>();
        if (!GI) return;

        AIABTYGameState* GS = GetWorld()->GetGameState<AIABTYGameState>();

        TArray<APlayerState*> Players = GetGameState<AIABTYGameState>()->PlayerArray;
        for (APlayerState* PS : Players)
        {
            if (AIABTYPlayerState* IABTYPS = Cast<AIABTYPlayerState>(PS))
            {
                if (IABTYPS->Team == WinningTeam)
                {
                    if (IABTYPS->KillsCurrentRound == 0) 
                    {
                        if(AIABTYController* Controller = Cast<AIABTYController>(IABTYPS->GetOwningController()))
                            Controller->ClientUnlockAchievement("WIN_ROUND_0_KILLS");
                    }

                    IABTYPS->Rounds++;
                    IABTYPS->LocalRounds++;
                    IABTYPS->RoundStreak++;

                    FPlayerStats& Stats = GI->GetOrCreatePlayerStats(IABTYPS->PlayerKey);
                    Stats.Rounds = IABTYPS->Rounds;
                    Stats.LocalRounds = IABTYPS->LocalRounds;
                    Stats.RoundStreak = IABTYPS->RoundStreak;


                    if (IABTYPS->RoundStreak == RoundStreak3)
                    {
                        if (AIABTYController* Controller = Cast<AIABTYController>(IABTYPS->GetOwningController()))
                            Controller->ClientUnlockAchievement("ROUND_STREAK_7");
                    }

                    if (IABTYPS->LocalRounds == GI->RoundsToWin) {
                        IABTYPS->Points++;
                        IABTYPS->PointStreak++;
                        Stats.Points = IABTYPS->Points;
                        Stats.PointStreak = IABTYPS->PointStreak;
                        bIsMapOver = true;
                    }
                    else 
                    {
                        if (IABTYPS->RoundStreak == RoundStreak1)
                            GS->TriggerVoiceEvent(FVoiceTags::EndRoundWinningStreak1, IABTYPS->Team);
                        else if (IABTYPS->RoundStreak == RoundStreak2)
                            GS->TriggerVoiceEvent(FVoiceTags::EndRoundWinningStreak2, IABTYPS->Team);
                        else if (IABTYPS->RoundStreak == RoundStreak3)
                            GS->TriggerVoiceEvent(FVoiceTags::EndRoundWinningStreak3, IABTYPS->Team);
                    }              
                }
                else 
                {
                    if (IABTYPS->RoundStreak >= BreakRoundStreak && !bIsMapOver)
                        GS->TriggerVoiceEvent(FVoiceTags::EndRoundBreakWinningStreak, IABTYPS->Team);

                    FPlayerStats& Stats = GI->GetOrCreatePlayerStats(IABTYPS->PlayerKey);
                    IABTYPS->RoundStreak = 0;
                    Stats.RoundStreak = IABTYPS->RoundStreak;
                }
            }
        }

        GS->UpdatePlayerPositions();

        BP_UpdateScoreboard();

        if(GS->bBalloonGrowthStarted)
            ToggleBalloonGrowth();

        bIsRoundOver = true;

        if (GI->NumberOfPlayersConnected == 1)
        {
            GetWorldTimerManager().SetTimer(RoundRestartTimer, this, &AIABTYGameMode::EndMatch, 5.0f, false);
        }
        else if (bIsMapOver) 
        {
            BP_PointOver(WinningTeam);
            PlayEndPointVoiceLine();
            GetWorldTimerManager().SetTimer(RoundRestartTimer, this, &AIABTYGameMode::StartNextMap, 5.0f, false);
        }
        else 
        {
            GI->RoundNumber++;
            GetWorldTimerManager().SetTimer(RoundRestartTimer, this, &AIABTYGameMode::StartNewRound, 5.0f, false);
        }

        GS->TriggerVoiceEvent(FVoiceTags::EndRoundWinnerDefault, WinningTeam);
    }
}

void AIABTYGameMode::PlayEndPointVoiceLine()
{
    UIABTYAdvancedFriendsGameInstance* GI = GetGameInstance<UIABTYAdvancedFriendsGameInstance>();
    AIABTYGameState* GS = GetWorld()->GetGameState<AIABTYGameState>();

    if (!GS || !GI) return;

    AIABTYPlayerState* Winner = nullptr;
    AIABTYPlayerState* RunnerUp = nullptr;

    int32 HighestRounds = -1;
    int32 SecondHighestRounds = -1;

    AIABTYPlayerState* BreakStreakPlayer = nullptr;

    for (APlayerState* PS : GS->PlayerArray)
    {
        if (AIABTYPlayerState* IABTYPS = Cast<AIABTYPlayerState>(PS))
        {
            int32 Rounds = IABTYPS->LocalRounds;

            if (Rounds > HighestRounds)
            {
                SecondHighestRounds = HighestRounds;
                RunnerUp = Winner;

                HighestRounds = Rounds;
                Winner = IABTYPS;
            }
            else if (Rounds > SecondHighestRounds)
            {
                SecondHighestRounds = Rounds;
                RunnerUp = IABTYPS;
            }

            if (IABTYPS->PointStreak >= BreakPointStreak && Rounds < GI->RoundsToWin)
            {
                BreakStreakPlayer = IABTYPS;
            }
        }
    }

    if (Winner && Winner->LocalRounds == GI->RoundsToWin)
    {
        if (Winner->PointStreak == PointStreak1)
        {
            GS->TriggerVoiceEvent(FVoiceTags::PointStreak1, Winner->Team);
            return;
        }
        else if (Winner->PointStreak == PointStreak2)
        {
            GS->TriggerVoiceEvent(FVoiceTags::PointStreak2, Winner->Team);
            return;
        }
        else if (Winner->PointStreak == PointStreak3)
        {
            GS->TriggerVoiceEvent(FVoiceTags::PointStreak3, Winner->Team);
            return;
        }
    }

    if (BreakStreakPlayer)
    {
        GS->TriggerVoiceEvent(FVoiceTags::BreakPointWinningStreak, BreakStreakPlayer->Team);

        FPlayerStats& Stats = GI->GetOrCreatePlayerStats(BreakStreakPlayer->PlayerKey);
        Stats.PointStreak = 0;

        return;
    }

    if (Winner && RunnerUp && Winner->LocalRounds == GI->RoundsToWin)
    {
        int32 Diff = Winner->LocalRounds - RunnerUp->LocalRounds;

        if (Diff == 1)
        {
            GS->TriggerVoiceEvent(FVoiceTags::WinPointBy1, Winner->Team);
        }
        else if (Diff <= 3)
        {
            GS->TriggerVoiceEvent(FVoiceTags::WinPointBy3OrLess, Winner->Team);
        }
        else if (Diff >= 4)
        {
            GS->TriggerVoiceEvent(FVoiceTags::WinPointBy4OrMore, Winner->Team);
        }
    }
}