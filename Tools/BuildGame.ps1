# Build jouable Win64 (Development) de YohohoSurvivor, à copier sur un autre PC pour les playtests réseau.
# Ne se lance que sur demande explicite de Paul.
#
#   powershell -ExecutionPolicy Bypass -File Tools\BuildGame.ps1
#     -> Build\Windows (dossier jouable) + Build\YohohoBuild_X.YYY.zip
#
# Nomenclature YohohoBuild_X.YYY (Tools\BuildVersion.json, versionné dans Git) :
#   X   = version du proto, fixée par Paul (champ Proto)
#   YYY = numéro de la build demandée par Paul (champ NextBuild, incrémenté après chaque build réussie)
#
# Sur l'autre PC : dézipper, lancer YohohoSurvivor.exe (autoriser le pare-feu chez l'hôte).
# Aucun chemin absolu : le projet est le dossier parent de ce script, le moteur est trouvé via le registre.

$ErrorActionPreference = "Stop"

$Root = Split-Path $PSScriptRoot -Parent
$Project = Join-Path $Root "YohohoSurvivor.uproject"
$BuildDir = Join-Path $Root "Build"
$VersionFile = Join-Path $PSScriptRoot "BuildVersion.json"

$Version = Get-Content $VersionFile -Raw | ConvertFrom-Json
$BuildName = "YohohoBuild_{0}.{1:D3}" -f [int]$Version.Proto, [int]$Version.NextBuild

$UE = (Get-ItemProperty "HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.8" -ErrorAction SilentlyContinue).InstalledDirectory
if (-not $UE) { $UE = "C:\Program Files\Epic Games\UE_5.8" }  # clé pas toujours créée par le launcher
$RunUAT = Join-Path $UE "Engine\Build\BatchFiles\RunUAT.bat"
if (-not (Test-Path $RunUAT)) { throw "UE 5.8 introuvable ($RunUAT)" }

Write-Host "Build : $BuildName"

& $RunUAT BuildCookRun `
	"-project=$Project" `
	-noP4 -utf8output -unattended `
	-platform=Win64 -clientconfig=Development `
	-build -cook -stage -pak -prereqs `
	-archive "-archivedirectory=$BuildDir"
if ($LASTEXITCODE -ne 0) { throw "BuildCookRun a échoué (code $LASTEXITCODE)" }

$GameDir = Join-Path $BuildDir "Windows"

# logs / captures / config locale d'un lancement de test : pas à distribuer
$SavedDir = Join-Path $GameDir "YohohoSurvivor\Saved"
if (Test-Path $SavedDir) { Remove-Item -LiteralPath $SavedDir -Recurse -Force }

$ZipPath = Join-Path $BuildDir "$BuildName.zip"
Compress-Archive -Path (Join-Path $GameDir "*") -DestinationPath $ZipPath -Force

# numéro suivant (à commiter avec le reste)
$Version.NextBuild = [int]$Version.NextBuild + 1
$Version | ConvertTo-Json | Set-Content $VersionFile -Encoding UTF8

Write-Host "Build prête : $ZipPath"