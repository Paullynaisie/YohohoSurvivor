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
- Map par défaut : `/Game/TopDown/Lvl_TopDown` ; GameMode par défaut : `BP_TopDownGameMode` (Blueprint dérivé du C++).
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
