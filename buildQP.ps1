# Builds a .qmod file for loading with QP
& $PSScriptRoot/build.ps1

$ArchiveName = "JDFixer.qmod"
$TempArchiveName = "JDFixer.qmod.zip"

Compress-Archive -Path "./libs/arm64-v8a/libJDFixer.so", ".\extern\libbeatsaber-hook_2_3_1.so", ".\mod.json" -DestinationPath $TempArchiveName -Force
Move-Item $TempArchiveName $ArchiveName -Force