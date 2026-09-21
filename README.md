\# I Am Better Than You - Code Samples







These are selected code samples from my game \*I Am Better Than You\*, an online multiplayer shooter released on \[Steam](https://store.steampowered.com/app/4636890/I\_Am\_Better\_Than\_You/).







The purpose of this repository is to showcase some of the gameplay and multiplayer systems I developed in Unreal Engine 5 using C++. You can find more information about the game and other projects in my \[portfolio](https://pablomorenoma.wixsite.com/portfolio/portfolio-collections/my-portfolio/i-am-better-than-you).







\## Code Samples







\*\*IABTYCharacter\*\*



\&#x20; Handles the player character and demonstrates how gameplay works for both the \\\*\\\*host and connected clients\\\*\\\*, including replicated player functionality.







\*\*IABTYGameMode\*\*



\&#x20; Handles the core match flow. The game uses a listen-server architecture, with the host acting as both the server and a player.







\*\*GameModeLobby\*\*



\&#x20; Handles the lobby phase before a match begins, including setting up the game for all connected players.







\*\*ContinuousWeapon\*\*



\&#x20; Used for the game's laser weapon. This script demonstrates how continuous weapons are handled over the network, including their replication and synchronization between clients.







\*\*BaseProjectile\*\*



\&#x20; Base class for the physical projectiles used in the game. Projectiles are handled differently for the local shooter and remote clients: the shooter sees a locally relevant projectile instance, while other players see a replicated version synchronized with the server.







\## Technologies







\- Unreal Engine 5



\- C++



\- Blueprints



\- Multiplayer / Network Replication



\- Steam Systems



\- Listen Servers



\- RPCs

