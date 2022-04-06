Param(
    [String]$qmodname="JDFixer"
)

if ($qmodName -eq "")
{
    echo "Give a proper qmod name and try again"
    exit
}
$mod = "./mod.json"
$modJson = Get-Content $mod -Raw | ConvertFrom-Json

$filelist = @($mod)

$cover = "./" + $modJson.coverImage
if ((-not ($cover -eq "./")) -and (Test-Path $cover))
{
    $filelist += ,$cover
}

foreach ($mod in $modJson.modFiles)
{
        $path = "./build/" + $mod
    if (-not (Test-Path $path))
    {
        $path = "./extern/libs/" + $mod
    }
    $filelist += $path
}

foreach ($lib in $modJson.libraryFiles)
{
        $path = "./build/" + $lib
    if (-not (Test-Path $path))
    {
        $path = "./extern/libs/" + $lib
    }
    $filelist += $path
}

$zip = $qmodName + ".zip"
$qmod = $qmodName + ".qmod"

Compress-Archive -Path $filelist -DestinationPath $zip -Update
Move-Item $zip $qmod -Force