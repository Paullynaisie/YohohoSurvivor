# Build jouable Win64 (Development) de YohohoSurvivor, à copier sur un autre PC pour les playtests réseau.
#
#   powershell -ExecutionPolicy Bypass -File Tools\BuildGame.ps1          # build dans Build\Windows
#   powershell -ExecutionPolicy Bypass -File Tools\BuildGame.ps1 -Zip     # + Build\YohohoSurvivor_<date>.zip
#
# Sur l'autre PC : dézipper, lancer YohohoSurvivor.exe (autoriser le pare-feu chez l'hôte).
# Aucun chemin absolu : le projet est le dossier parent de ce script, le moteur est trouvé via le registre.

param(
	[switch]$Zip
)

$ErrorActionPreference = "Stop"

$Root = Split-Path $PSScriptRoot -Parent
$Project = Join-Path $Root "YohohoSurvivor.uproject"
$BuildDir = Join-Path $Root "Build"

$UE = (Get-ItemProperty "HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.8" -ErrorAction SilentlyContinue).InstalledDirectory
if (-not $UE) { $UE = "C:\Program Files\Epic Games\UE_5.8" }  # clé pas toujours créée par le launcher
$RunUAT = Join-Path $UE "Engine\Build\BatchFiles\RunUAT.bat"
if (-not (Test-Path $RunUAT)) { throw "UE 5.8 introuvable ($RunUAT)" }

& $RunUAT BuildCookRun `
	"-project=$Project" `
	-noP4 -utf8output -unattended `
	-platform=Win64 -clientconfig=Development `
	-build -cook -stage -pak -prereqs `
	-archive "-archivedirectory=$BuildDir"
if ($LASTEXITCODE -ne 0) { throw "BuildCookRun a échoué (code $LASTEXITCODE)" }

$GameDir = Join-Path $BuildDir "Windows"
Write-Host "Build prête : $GameDir"

if ($Zip) {
	# logs / captures / config locale d'un lancement de test : pas à distribuer
	$SavedDir = Join-Path $GameDir "YohohoSurvivor\Saved"
	if (Test-Path $SavedDir) { Remove-Item -LiteralPath $SavedDir -Recurse -Force }

	$ZipPath = Join-Path $BuildDir ("YohohoSurvivor_{0}.zip" -f (Get-Date -Format "yyyy-MM-dd_HHmm"))
	Compress-Archive -Path (Join-Path $GameDir "*") -DestinationPath $ZipPath -Force
	Write-Host "Zip : $ZipPath"
}
