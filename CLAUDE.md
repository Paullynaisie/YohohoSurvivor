# YohohoSurvivor

Jeu **top-down multijoueur en ligne (1 à 4 joueurs par partie)**, Unreal Engine **5.8**, C++.
Langue de travail avec l'utilisateur (Paul) : **français**.

## Build

- Moteur : `C:\Program Files\Epic Games\UE_5.8`
- Compiler (éditeur fermé, ou sinon utiliser Live Coding Ctrl+Alt+F11 dans l'éditeur) :
  ```
  & "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" YohohoSurvivorEditor Win64 Development -Project="C:\UnrealProjects\YohohoSurvivor\YohohoSurvivor.uproject" -WaitMutex
  ```
- Régénérer la solution VS (après ajout/suppression de fichiers) :
  ```
  & "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="C:\UnrealProjects\YohohoSurvivor\YohohoSurvivor.uproject" -game -rocket -progress
  ```
- Si `UnrealEditor` est ouvert avec Live Coding actif, le build en ligne de commande échoue : demander à Paul de fermer l'éditeur ou de lancer Live Coding.
- Les warnings C4996 venant de `Engine/` (Niagara, Character.h) sont connus et ne concernent pas notre code.
- Après chaque modification C++, **compiler pour vérifier** avant de déclarer la tâche terminée.

## Structure

- `Source/YohohoSurvivor/` : module principal (classes de base du template Top Down : Character, GameMode, PlayerController).
- `Source/YohohoSurvivor/Variant_TwinStick/` : base la plus proche d'un « survivor » (projectiles, ennemis IA via StateTree, spawner, pickups, AoE). **Point de départ du gameplay.**
- `Source/YohohoSurvivor/Variant_Strategy/` : variante RTS du template, probablement à supprimer.
- Les dossiers de variantes sont dans `PublicIncludePaths` du `.Build.cs` : à mettre à jour si on déplace/supprime des dossiers.
- `Source/YohohoSurvivor/Player/YSPlayerCharacter` : personnage joueur (ZQSD/WASD/flèches, input créé à l'exécution, `MoveSpeed` réglable). Regarde le curseur (`bUseControllerRotationYaw`), strafe via `StrafeBlendSpace` joué en mode SingleNode (pas d'Anim Blueprint). Parent du BP `/Game/Placeable/Character/PlayerCharacter_mouss`.
- `Source/YohohoSurvivor/Player/YSPlayerController` : curseur viseur, calcule `AimLocation` (souris projetée sur le plan du pawn) et met la rotation de contrôle vers ce point ; le CMC l'envoie au serveur avec les mouvements.
- `/Game/Placeable/Character/Anims/BS_Player_Strafe` : BlendSpace 2D (X = direction -180..180 relative au regard, Y = vitesse 0..600 ; idle / marche 200 / course 600 dans 8 directions).
- Joueur (DA primitives) : `BodyPivot` (scene, origine capsule) porte `BodyMesh` (cylindre ; mesh squelettique vide, donc pas de strafe animé), `FaceDecal`, et les mains `LeftHand` / `RightHand` (scene, non scalées = points d attache des armes, `GetLeftHand()`/`GetRightHand()` ; position monde absolue calculée par `UpdateHands` : cible `HandLocation` (main droite, gauche = Y inversé) ou `HandLocation` de l arme si `bOverrideHandLocation`, rotation `HandRotation` (gauche = Yaw/Roll inversés) ou celle de l arme si `bOverrideHandRotation`, + oscillation Perlin `HandSwayAmplitude`/`HandSwaySpeed`, suivie avec retard `HandFollowSpeed`) avec leurs sphères `LeftHandMesh` / `RightHandMesh` (matériau copié de BodyMesh dans OnConstruction si `bHandsUseBodyMaterial`). Armes : `LeftHandWeapon` / `RightHandWeapon` = `UChildActorComponent` sur les mains (owner = perso, acteur enfant répliqué spawné par le serveur), classes choisies par `LeftHandWeaponClass` / `RightHandWeaponClass` (catégorie 3C|Weapons, appliquées dans OnConstruction -> visuel dans l éditeur). Clic gauche = actions des deux mains ; main principale = droite ; akimbo (même classe d arme dans les deux mains) : droite puis gauche après `AkimboFireDelay` de l arme (catégorie `Akimbo`, 0.5 s). L icône UI suit la main droite, sinon la gauche. Les mains pivotent (yaw, `HandAimSpeed`) vers la cible de visée assistée du Coup d Petoir de leur arme (calculée depuis le regard du perso, donc identique sur toutes les machines ; en jeu seulement).
- `Source/YohohoSurvivor/Weapons/YSWeapon` : arme (Root, `Mesh`, `Muzzle`, `DisplayName`), `GetAction()` = premier `UYSAbilityComponent` de l acteur (ajouté dans le BP de l arme), `GetHolder()` = owner pawn. Recul (catégorie `Recoil`) : `GetRecoilAlpha()` d après la dernière activation (coup EaseOut `RecoilKickDuration` 0.05 s, retour EaseInOut `RecoilRecoverDuration` 0.25 s) ; le perso cabre la main de `RecoilPitch` 25° et la recule de `RecoilKickBack` 10 cm le long de l axe de l arme, après le lissage. BP dans `/Game/Placeable/Weapons` : `BP_Arme` (base), `BP_Pistolet` (action `CoupDPetoir` : PetoirAMMO, 5 dégâts, recharge 1 s), `BP_Epee` (lame en cube, pas encore d action). `UpdateBodyPivot` (visuel, toutes machines) : inclinaison autour des pieds vers la vélocité monde (`LeanMaxAngle` 30, `LeanDuration` 0.14 s, `LeanEasing` EaseIn, `LeanEaseExponent` 2) + sautillement vertical (`HopHeight` 10 cm, `HopDuration` 0.2 s à `MoveSpeed`, cadence proportionnelle à vitesse réelle / `MoveSpeed`, `HopEasing` EaseInOut). Paramètres du perso dans la catégorie `3C` (affichée en premier via `PrioritizeCategories`). Composant BP `Head Feature` (sphère, enfant de BodyPivot) porte `Bandana` = `UYSDanglingChainComponent` (`Player/YSDanglingChainComponent` : chaîne de cylindres en Verlet, gravité + poussée vers l arrière, segments transient en coordonnées absolues ; pose de repos dans l éditeur via tick éditeur, simulation en jeu). `FaceDecal` = `UYSFaceDecalComponent` (`Player/YSFaceDecalComponent`) : propriété `FaceTexture` sur le composant -> MID transient sur `M_FaceDecal` (`/Game/Art/Materials/Decals`), le DecalMaterial de base est restauré avant sauvegarde.
- `Source/YohohoSurvivor/Abilities/YSAbilityComponent` : base des capacités (recharge `Cooldown`, `TryActivate` côté client -> RPC `ServerActivate` -> `ExecuteAbility` sur le serveur ; `ReadyServerTime` répliqué, prédit côté client). Effets visuels : `GetTimeSinceActivation()` (temps local) mis à jour par prédiction chez le tireur et par `MulticastActivated` (Unreliable) chez les autres. Paramètres BlueprintReadWrite pour les futurs upgrades.
- `Source/YohohoSurvivor/Abilities/YSCoupDPetoirAbility` : tir du pistolet (composant de BP_Pistolet ; tire depuis le Muzzle de l arme, Instigator = pawn qui la tient) ; visée assistée `AssistAimDistance` 1000 / `AssistAimAngle` 15° : `FindAssistTarget` = pawn le plus proche portant un HitTag du projectile, utilisée par le serveur au tir ; `Damage` (5) appartient à la capacité et est donné au projectile au spawn (`SpawnActorDeferred` + `SetDamage`).
- `Source/YohohoSurvivor/Abilities/YSCoupDSabreAbility` : attaque AUTO de BP_Epee (`IsAutomatic`). Serveur (TickComponent) : si prête et cible dans le cône `DetectionDistance` 300 / `DetectionAngle` 45° (regard du perso, `HitTags` Team.Foe) -> `StartAttack` (ReadyServerTime = durée anim + `AutoAtkCooldown` 0.8) + `MulticastActivated` (joué par tous, pas de prédiction). Anim évaluée par `EvaluateHandPose` depuis `GetTimeSinceActivation` : préparation `WindupDuration` 0.15 (EaseOut) vers `WindupHandLocation/Rotation` (flanc, lame à plat), coupe `SwingDuration` 0.07 (EaseOut exp 4) vers `SwingEndHandLocation/Rotation` (lame vers l avant), retour `RecoverDuration` 0.3 (EaseInOut) ; poses main droite, symétrisées à gauche, appliquées sans le retard des mains. Pendant la coupe (+0.05 s), overlap de la `UBoxComponent` de l arme (« Hitbox » de BP_Epee, sans collision propre) -> `Damage` 1 une fois par cible. Akimbo : la droite déclenche, la gauche suit après `AkimboFireDelay`. Arme : `GetActiveAction()` (clic, icône UI, recul) / `GetAutoAction()`. Base : `GetCooldownDuration()` virtuel, `FindTargetInCone` partagé avec la visée assistée.
- `Source/YohohoSurvivor/Weapons/YSProjectile` : projectile répliqué, ligne droite sans gravité, `Speed`/`LifeTime`/`Color` réglables (catégorie AMMO`, affichée en premier ; armes : catégorie `Weapon` en premier) ; touche seulement les acteurs portant un de ses `HitTags` (défaut `Team.Foe`), traverse les autres pawns, détruit contre WorldStatic. Parent du BP `/Game/Placeable/AMMO/PetoirAMMO`.
- `Source/YohohoSurvivor/Core/YSGameplayTags` : tags natifs `Team.Player` / `Team.Foe`. Joueur et ennemi implémentent `IGameplayTagAssetInterface` (propriété `GameplayTags`).
- Ennemi : `MaxHealth` (10), `Health` répliquée, `TakeDamage` serveur, `Die()` = Destroy.
- `Source/YohohoSurvivor/UI/YSAbilityIconWidget` : icône carrée construite en C++ (pas de Widget Blueprint), voile + secondes de recharge ; créée et rafraîchie par `AYSPlayerController` (bas-centre de l'écran).
- Widgets C++ sans Widget Blueprint : construire l'arbre dans `Initialize()` (pas `RebuildWidget`, sinon les setters appelés avant le premier affichage sont perdus). Placement viewport : passer un `FGameViewportWidgetSlot` complet à `UGameViewportSubsystem::AddWidgetForPlayer` (`SetPositionInViewport`/`SetDesiredSizeInViewport` remettent l'ancre en haut à gauche en 5.8).
- `Source/YohohoSurvivor/UI/YSHealthBarWidget` : barre de vie (WidgetComponent screen-space `HealthBar` des NME, cachée si PV max, « X / Y »). `MaxHealth` et `Health` répliquées (`OnRep_Health`).
- `Source/YohohoSurvivor/Core/YSGameState` : `ReportDamage` (serveur) -> `MulticastDamageNumber` (montant, impact, PlayerState attaquant/victime) ; chaque machine choisit la couleur selon son joueur local et spawn un `AYSDamageNumber` local (non répliqué, 1 s, monte et s'efface). Couleurs réglables sur `AYSDamageNumber`. GameState de `BP_TestGameMode`.
- Debug : commandes console `YSDamageFoes <dégâts> <délai> <capture 0/1>` et `YSDebugWalk <X nord> <Y est> <durée> <nb captures>` (PlayerController). Vérif visuelle automatisable : `UnrealEditor.exe <uproject> -game -windowed -ResX=1280 -ResY=720 -ExecCmds="YSDamageFoes 3 6 1"` -> capture dans `Saved/Screenshots/WindowsEditor/`.
- `Source/YohohoSurvivor/EditorTools/YSEditorScriptLibrary` : fonctions `WITH_EDITOR` appelées depuis Python pour ce que l'API Python n'expose pas (axes et samples de BlendSpace, sockets de squelette).
- Python : les propriétés de composants hérités d'un BP se modifient via `cdo.get_editor_property("NomDuComposant").set_editor_property(...)` puis `save_asset` (vérifié, ça persiste).
- `Source/YohohoSurvivor/Enemies/YSFoeCharacter` : ennemi de base, attiré en ligne droite vers le joueur le plus proche (logique serveur dans Tick, pas de pathfinding), tourné vers sa cible, couleur via le paramètre `Paint Tint` des matériaux mannequin. Parent du BP `/Game/Placeable/Character/FoeCharacter_Skull` (3 instances de test dans TestGround).
- `/Game/Core/GameModes/BP_TestGameMode` : GameModeBase, DefaultPawnClass = PlayerCharacter_mouss. GameMode global par défaut.
- `/Game/Maps/TestMaps/TestGround` : map de test (sol WorldGridMaterial, lumières, 4 PlayerStart). Map de démarrage éditeur + jeu.
- Assets du template Top Down rangés dans `Content/PresetProject/` (référence, pas le jeu final). Leurs acteurs de map sont dans `Content/__ExternalActors__/PresetProject/...` (c'est normal, ces dossiers doivent rester à la racine de Content).
- **Ne jamais déplacer/renommer des `.uasset` via le système de fichiers** : toujours dans le Content Browser de l'éditeur (il met à jour les références et crée des redirecteurs), puis « Update Redirector References ».

## Automatisation de l'éditeur (Python)

- Plugins `PythonScriptPlugin` + `EditorScriptingUtilities` activés : Claude crée/modifie des assets via des scripts dans `Tools/EditorScripts/`.
- Lancer un script qui modifie des assets / maps (éditeur fermé) : `UnrealEditor.exe <uproject> -ExecutePythonScript="<script.py>"` ; le script finit par `unreal.SystemLibrary.quit_editor()` et écrit un rapport dans `Saved/Logs/`. Un crash (0xC0000005) à la toute fin de la fermeture est connu et sans conséquence si les sauvegardes sont dans le log.
- `-ExecutePythonScript` ferme l éditeur dès la fin du script : pas de callbacks différés (tick). Captures de vérif : SceneCapture2D + `RenderingLibrary.export_render_target` en synchrone.
- Lecture seule / vérifications : `UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<script.py>" -unattended`.
- API Python UE 5.8 : `unreal.ObjectRedirector` et `AssetTools.fixup_referencers` ne sont pas exposés ; filtrer les redirecteurs via `asset_data.asset_class_path.asset_name`.
- `.uasset` / `.umap` sont binaires (Git LFS) : Claude ne peut pas les lire ni les modifier. Toute logique doit vivre en C++ ; les Blueprints servent au paramétrage, aux assets et à l'UI. Quand une étape doit se faire dans l'éditeur, donner à Paul des instructions précises (menu, champ, valeur).

## Réseau (multijoueur)

- **État actuel : le template n'a AUCUNE réplication** (pas de `Replicated`, pas de RPC). Tout est à construire.
- Modèle visé : **listen server** (un joueur héberge), 4 joueurs max. Sessions en ligne : EOS ou Steam (à décider).
- Règles :
  - Serveur autoritaire : logique de jeu (dégâts, spawn d'ennemis, pickups, score) dans `GameMode` / côté `HasAuthority()`.
  - État partagé : `GameState` (partie) et `PlayerState` (par joueur) ; propriétés `UPROPERTY(Replicated)` / `ReplicatedUsing=OnRep_X` + `GetLifetimeReplicatedProps`.
  - Entrées client -> `UFUNCTION(Server, Reliable)` ; effets cosmétiques -> `NetMulticast, Unreliable`.
  - Le `GameMode` n'existe que sur le serveur ; ne jamais y lire l'état côté client.
  - Le déplacement « clic pour aller » du template utilise la navigation côté client : à revoir pour le multi.
- Test : Play → Number of Players = 2-4, Net Mode = **Play As Listen Server**.

## Conventions

- Préfixes Unreal standards (`A`, `U`, `F`, `E`, `I`, `T`), `UPROPERTY`/`UFUNCTION` avec catégories.
- Nouvelles classes du jeu dans `Source/YohohoSurvivor/` (sous-dossiers par domaine : `Core/`, `Player/`, `Enemies/`, `Weapons/`, `UI/`…).
- Commits Git en français, petits et fréquents. Auteur : Paul.Ly <lyaudet.paul@gmail.com>.
