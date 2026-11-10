# YohohoSurvivor

Jeu **top-down multijoueur en ligne (1 à 4 joueurs par partie)**, Unreal Engine **5.8**, C++.
Langue de travail avec l'utilisateur (Paul) : **français**.

## Build

- Paul travaille sur plusieurs PC : **aucun chemin absolu en dur**. Le projet = racine du dépôt (dossier de travail courant, `$PWD`). Le moteur se retrouve via le registre (installation Epic Games Launcher) ; si la clé est absente, demander à Paul où est UE 5.8 sur ce PC.
  ```
  $UE = (Get-ItemProperty "HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.8" -ErrorAction SilentlyContinue).InstalledDirectory
  if (-not $UE) { $UE = "C:\Program Files\Epic Games\UE_5.8" }  # clé pas toujours créée par le launcher
  ```
- Toolchain : UE 5.8 exige **MSVC 14.50+** (Visual Studio 2026, composants dans `.vsconfig` ; 14.51 fonctionne). Avec MSVC 14.38/14.44 (VS 2022) le build échoue dans les en-têtes du moteur (erreur C7539 dans `ContainerAllocationPolicies.h`). Les `Target.cs` fixent `WindowsPlatform.CompilerVersion = "Latest"` : prioritaire sur le `%APPDATA%\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml` global, qui peut forcer un vieux compilateur pour les autres projets UE de Paul (ne pas modifier ce fichier global). En PowerShell, **mettre entre guillemets** les arguments contenant des points (`"-CompilerVersion=…"`, `"-Project=…"`), sinon ils sont coupés.
  Exécutables : `$UE\Engine\Binaries\Win64\UnrealEditor.exe` / `UnrealEditor-Cmd.exe`.
- Compiler (éditeur fermé, ou sinon utiliser Live Coding Ctrl+Alt+F11 dans l'éditeur) :
  ```
  & "$UE\Engine\Build\BatchFiles\Build.bat" YohohoSurvivorEditor Win64 Development -Project="$PWD\YohohoSurvivor.uproject" -WaitMutex
  ```
- Régénérer la solution VS (après ajout/suppression de fichiers) :
  ```
  & "$UE\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="$PWD\YohohoSurvivor.uproject" -game -rocket -progress
  ```
- Si `UnrealEditor` est ouvert avec Live Coding actif, le build en ligne de commande échoue : demander à Paul de fermer l'éditeur ou de lancer Live Coding.
- Les warnings C4996 venant de `Engine/` (Niagara, Character.h) sont connus et ne concernent pas notre code.
- Le packaging compile en **unity build** (plusieurs `.cpp` fusionnés) alors que les builds éditeur incrémentales non : les helpers `namespace { }` doivent avoir des noms uniques au projet (sinon C2084 seulement au packaging) ; mettre ceux d un fichier dans un namespace nommé (ex. `YSMainMenu::`).
- Après chaque modification C++, **compiler pour vérifier** avant de déclarer la tâche terminée.
- Ne pas réécrire un fichier source avec `Get-Content -Raw` / `Set-Content` de PowerShell 5.1 : il lit l UTF-8 sans BOM comme de l ANSI et casse les accents (passer par l outil Edit).

## Structure

- `Source/YohohoSurvivor/` : module principal (classes de base du template Top Down : Character, GameMode, PlayerController).
- `Source/YohohoSurvivor/Variant_TwinStick/` : base la plus proche d'un « survivor » (projectiles, ennemis IA via StateTree, spawner, pickups, AoE). **Point de départ du gameplay.**
- `Source/YohohoSurvivor/Variant_Strategy/` : variante RTS du template, probablement à supprimer.
- Les dossiers de variantes sont dans `PublicIncludePaths` du `.Build.cs` : à mettre à jour si on déplace/supprime des dossiers.
- Widgets C++ sans Widget Blueprint : construire l'arbre dans `Initialize()` (pas `RebuildWidget`, sinon les setters appelés avant le premier affichage sont perdus). Placement viewport : passer un `FGameViewportWidgetSlot` complet à `UGameViewportSubsystem::AddWidgetForPlayer` (`SetPositionInViewport`/`SetDesiredSizeInViewport` remettent l'ancre en haut à gauche en 5.8).
- Python / maps : une valeur d'instance placée dans une map peut rester en override (ex. `Chest_Test` à 0 doublon, `ZoneMaterial` vidé après suppression d'un asset référencé) : vérifier les instances, pas seulement le CDO. Ne pas supprimer un asset référencé par une map : le reconstruire ou le remplacer. Un graphe de matériau vidé avec `delete_all_material_expressions` garde des nœuds : recréer l'asset.
- Python : les propriétés de composants hérités d'un BP se modifient via `cdo.get_editor_property("NomDuComposant").set_editor_property(...)` puis `save_asset` (vérifié, ça persiste).
- **Rangement des assets (Paul)** : personnages dans `/Game/Placeable/Character/Player` (PlayerCharacter_mouss, PlayerCharacter_JohnnyPlaytest), `Character/Foe` (FoeCharacter_Skull*), `Character/NPC` (BP_NPC_Brooker) ; armes du joueur dans `/Game/Placeable/Weapons/Player` (BP_Epee, BP_Pistolet, BP_BattlePetoir, BP_BattleSword ; BP_Arme reste dans Weapons, armes de NME dans Weapons/Foe) ; map de test `/Game/Maps/TestMap`. Les chemins plus anciens cités ailleurs dans ce fichier et dans les vieux scripts sont à lire avec ces dossiers. `YSSpawnFoe` / `YSEquipWeapon` cherchent le BP par nom dans tous les sous-dossiers (Asset Registry). Après un rangement : `Tools/EditorScripts/fix_redirectors.py` (réenregistre ce qui passe par un redirecteur = « Update Redirector References », puis supprime les redirecteurs ; `delete_asset` ne supprime pas le FICHIER d un redirecteur : le retirer ensuite avec `git rm`, il n est plus référencé). Vérifié après le rangement de Paul : 0 redirecteur, aucune référence cassée hors `PresetProject` (13 anciennes références du template vers `/Game/Characters/...`, sans effet sur le jeu).
- `/Game/Core/GameModes/BP_TestGameMode` : enfant de `AYSGameMode`. GameMode global par défaut.
- Assets du template Top Down rangés dans `Content/PresetProject/` (référence, pas le jeu final). Leurs acteurs de map sont dans `Content/__ExternalActors__/PresetProject/...` (c'est normal, ces dossiers doivent rester à la racine de Content).
- **Ne jamais déplacer/renommer des `.uasset` via le système de fichiers** : toujours dans le Content Browser de l'éditeur (il met à jour les références et crée des redirecteurs), puis « Update Redirector References ».

## Documentation détaillée (`Docs/`)

Le détail de chaque système (classes, réglages, valeurs, scripts, commandes de debug, ce qui a été vérifié) est dans `Docs/`, un fichier par domaine :

- [Docs/Personnages.md](Docs/Personnages.md) : YSPlayerCharacter / Controller, mains, Mouss, Cannoneer, Harquebusier, Quartermaster, Captain et minions, passifs, stamina / MoveAction, sélection et personnalisation, mort / spectateur.
- [Docs/Armes_Actions.md](Docs/Armes_Actions.md) : YSWeapon, YSAbilityComponent, priorités des actions, shoot system, Coup d Petoir, Rolling shot, Dashing Slash, Auto swing, projectiles, armes à deux mains.
- [Docs/Upgrades.md](Docs/Upgrades.md) : XP et niveaux, menu d upgrade, shards, upgrades d arme, critiques.
- [Docs/NME.md](Docs/NME.md) : ennemis, combat (hurtbox / hitbox), attaques des NME, pushback, bump, escalade, élites / boss, spawn, jour / nuit.
- [Docs/Bateaux.md](Docs/Bateaux.md) : bateaux, pilotage, accès, postes, postes de combat, Galion / Jolly Ship, équipage, étapes du GameBoat, réparation, game over.
- [Docs/Flow_Iles.md](Docs/Flow_Iles.md) : FlowMod, génération d îles, parties 1 et 2, batailles navales, île finale, objectifs, carte au trésor, plumes de phoenix, nage.
- [Docs/Kraken.md](Docs/Kraken.md) : partie 3, combat contre le Kraken.
- [Docs/Maps_Tests_Debug.md](Docs/Maps_Tests_Debug.md) : GameMap / TestMap, deadzone, props, codes de triche, commandes de debug.
- [Docs/Reseau_Menus_UI.md](Docs/Reseau_Menus_UI.md) : pause et réplication, Steam, menu principal, widgets C++.
- [Docs/Journal_Reglages.md](Docs/Journal_Reglages.md) : paquets de réglages datés qui touchent plusieurs domaines.

Règles :
- **Avant de travailler sur un domaine, lire le fichier `Docs/` concerné** (seulement celui-là ; chercher avec Grep dans `Docs/` si on ne sait pas où est un système).
- **Après une modification, la documenter dans le fichier `Docs/` du domaine, PAS dans ce CLAUDE.md** (nouveau système = nouvelle entrée ; changement = mettre à jour l entrée existante plutôt qu ajouter un paragraphe « remplace ce qui est écrit ailleurs »). Nouveau domaine = nouveau fichier + une ligne dans la liste ci-dessus.
- Ce CLAUDE.md est chargé à chaque requête : n y ajouter que des règles générales courtes (build, conventions, façon de travailler).

## Automatisation de l'éditeur (Python)

- Plugins `PythonScriptPlugin` + `EditorScriptingUtilities` activés : Claude crée/modifie des assets via des scripts dans `Tools/EditorScripts/`.
- Lancer un script qui modifie des assets / maps (éditeur fermé) : `UnrealEditor.exe <uproject> -ExecutePythonScript="<script.py>"` ; le script finit par `unreal.SystemLibrary.quit_editor()` et écrit un rapport dans `Saved/Logs/`. Un crash (0xC0000005) à la toute fin de la fermeture est connu et sans conséquence si les sauvegardes sont dans le log.
- `-ExecutePythonScript` ferme l éditeur dès la fin du script : pas de callbacks différés (tick). Captures de vérif : SceneCapture2D + `RenderingLibrary.export_render_target` en synchrone.
- Lecture seule / vérifications : `UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<script.py>" -unattended`.
- API Python UE 5.8 : `unreal.ObjectRedirector` et `AssetTools.fixup_referencers` ne sont pas exposés ; filtrer les redirecteurs via `asset_data.asset_class_path.asset_name`.
- `.uasset` / `.umap` sont binaires (Git LFS) : Claude ne peut pas les lire ni les modifier. Toute logique doit vivre en C++ ; les Blueprints servent au paramétrage, aux assets et à l'UI. Quand une étape doit se faire dans l'éditeur, donner à Paul des instructions précises (menu, champ, valeur).

## Réseau (multijoueur)

- **État actuel : le template n'a AUCUNE réplication** (pas de `Replicated`, pas de RPC). Tout est à construire.
- Modèle visé : **listen server** (un joueur héberge), 4 joueurs max. Sessions en ligne : Steam (AppID 480 pour l instant, voir « Steam » plus haut), mode IP gardé en secours.
- Règles :
  - Serveur autoritaire : logique de jeu (dégâts, spawn d'ennemis, pickups, score) dans `GameMode` / côté `HasAuthority()`.
  - État partagé : `GameState` (partie) et `PlayerState` (par joueur) ; propriétés `UPROPERTY(Replicated)` / `ReplicatedUsing=OnRep_X` + `GetLifetimeReplicatedProps`.
  - Entrées client -> `UFUNCTION(Server, Reliable)` ; effets cosmétiques -> `NetMulticast, Unreliable`.
  - Le `GameMode` n'existe que sur le serveur ; ne jamais y lire l'état côté client.
  - Le déplacement « clic pour aller » du template utilise la navigation côté client : à revoir pour le multi.
- Test : Play → Number of Players = 2-4, Net Mode = **Play As Listen Server**.
- Test local hors PIE (2 fenêtres, sans packager) : `UnrealEditor.exe <uproject> /Game/Maps/GameMap?listen -game -windowed -log=YSHost.log` puis `UnrealEditor.exe <uproject> 127.0.0.1 -game -windowed -log=YSClient.log` ; vérifier « Join succeeded » (hôte) / « Welcomed by server » (client) dans `Saved/Logs`.
- **Build de playtest** (autre PC) : `powershell -ExecutionPolicy Bypass -File Tools\BuildGame.ps1 -Zip` (BuildCookRun Win64 Development, `-prereqs`) -> `Build\Windows\YohohoSurvivor.exe` + `Build\YohohoSurvivor_<date>.zip` (`Build/` ignoré par Git). Development = console et commandes de debug disponibles. Maps cookées : MainMenu + GameMap seulement (TestMap volontairement absente des builds, demande de Paul) ; `/Game/Core` et `/Game/Placeable` toujours cookés (ajouter toute nouvelle map jouable à `MapsToCook` dans DefaultGame.ini). Tous les joueurs doivent avoir **la même build**. `r.PSOPrecaching=0` (DefaultEngine.ini `[SystemSettings]`) : avec, la build packagée plantait en ~20 s sur RTX 3060 Ti / pilote NVIDIA 560.94 (8007000e puis crash `nvwgf2umx.dll`, code 0xc0000409). Crash d une build : log dans `<build>\YohohoSurvivor\Saved\Logs`, et l Observateur d événements (Application, ID 1000) donne le module fautif. Hôte : accepter le pare-feu Windows (UDP 7777). Hors réseau local : partie Steam (relais Steam, rien à ouvrir), sinon en IP : VPN (Tailscale, ZeroTier, Radmin) ou redirection du port UDP 7777 sur la box de l hôte. Les DLL Steam sont copiées par le plugin et, en Development, le moteur écrit lui-même `steam_appid.txt` (480) au lancement.

## Conventions

- Préfixes Unreal standards (`A`, `U`, `F`, `E`, `I`, `T`), `UPROPERTY`/`UFUNCTION` avec catégories.
- Nouvelles classes du jeu dans `Source/YohohoSurvivor/` (sous-dossiers par domaine : `Core/`, `Player/`, `Enemies/`, `Weapons/`, `UI/`…).
- Commits Git en français, petits et fréquents. Auteur : Paul.Ly <lyaudet.paul@gmail.com>.

## Façon de travailler avec Paul

- **Toujours terminer ses messages par « Ave César »** tant qu on travaille sur ce projet (demande de Paul, remplace « Yohoho » depuis le 2026-11-05). **Exception : quand on a supprimé quelque chose**, terminer à la place par « Ils sont fous ces gaulois ». **Quand on ne fait qu'acquiescer** (réponse courte de prise en compte d'une consigne, sans autre travail), terminer à la place par « Lorem Ipsum Dolor es partum ».
- Nouveau PC : si le `.uproject` refuse de s'ouvrir (« Select Unreal Engine Version ») alors que UE 5.8 est installé, c'est que `C:\ProgramData\Epic\UnrealEngineLauncher\LauncherInstalled.dat` ne le liste pas encore : quitter complètement l'Epic Games Launcher puis le relancer. **Ne pas** choisir le moteur à la main (Unreal crée une copie du projet `YohohoSurvivor 5.8` liée à un GUID propre au PC, à ne pas commiter).
- **Ne jamais lancer de build packagée (`Tools/BuildGame.ps1`) sans que Paul le demande explicitement**, et **ne jamais lui proposer d'en lancer une** (pas de « je lance la build ? ») : c'est lui qui dit quand en faire. Les tests en `-game` depuis l'éditeur restent permis pour vérifier.
- **Map du jeu = FlowMap (consigne de Paul du 2026-11-06, jusqu à nouvel ordre)** : `HostMap` (DefaultGame.ini) pointe sur `/Game/Maps/FlowMap` ; toute build et tout test de partie se font sur la FlowMap (BP_FlowMod), plus sur GameMap. Ne pas remettre GameMap sans que Paul le demande.
- **Les builds ne vont jamais sur Git** (ni commit, ni Release GitHub) : `Build/` reste ignoré, seuls le script et `Tools/BuildVersion.json` sont versionnés (choix de Paul).
- Paul suit **sa propre roadmap** (documentation perso) : ne pas proposer de roadmap, attendre ses étapes.
- Tous les paramètres de gameplay / gamefeel doivent être réglables dans les Blueprints, regroupés dans des catégories affichées en tête (`PrioritizeCategories` : 3C, AMMO, Weapon, Akimbo, Recoil…).
- **HUD / UI : Paul veut pouvoir tweaker lui-même tous les placements et tailles à la fin** : chaque nouvel élément d interface expose ses valeurs (UPROPERTY EditAnywhere, catégorie `Layout`) et passe par un Widget Blueprint enfant réglable dans Class Defaults (modèle : `WBP_CharacterSelect` choisi par `BP_YSPlayerController`).
- Rotations données par Paul en X / Y / Z = Roll / Pitch / Yaw (affichage de l'éditeur). Positions en cm.
- DA actuelle : « primitives partout » (formes de base, matériaux `/Game/Art/Materials/PH_materials`).
- En début de session : `git status` ; Paul modifie souvent des assets entre les sessions -> les commiter à part (« Modifs de Paul : … ») avant nos changements.
- Raccourcis éditeur ZQSD (AZERTY) : à régler dans Edit > Editor Preferences > Keyboard Shortcuts > Viewport Navigation (fichier `%LOCALAPPDATA%\UnrealEngine\5.8\Saved\Config\WindowsEditor\EditorKeyBindings.ini`, propre à chaque PC).
- Bug connu laissé de côté à la demande de Paul : glitch visuel des bandanas en allant vers le nord-ouest (aucune collision en cause).
