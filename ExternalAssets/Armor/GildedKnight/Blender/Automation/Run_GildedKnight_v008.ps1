$ErrorActionPreference = 'Stop'
$blender = 'D:\SteamLibrary\steamapps\common\Blender\blender.exe'
$source = 'C:\Panthelia Project\PantheliaProject\ExternalAssets\Armor\GildedKnight\Blender\Panthelia_GildedKnight_Fit_v006_FullVisualFit.blend'
$script = 'C:\Panthelia Project\PantheliaProject\ExternalAssets\Armor\GildedKnight\Blender\Automation\GildedKnight_FullSkin_v008.py'
$console = 'C:\Panthelia Project\PantheliaProject\ExternalAssets\Armor\GildedKnight\Blender\Diagnostics\v008_ExecutableSkinned\console.log'
$exitCode = 'C:\Panthelia Project\PantheliaProject\ExternalAssets\Armor\GildedKnight\Blender\Diagnostics\v008_ExecutableSkinned\exit_code.txt'

& $blender --background $source --python $script *>> $console
$LASTEXITCODE | Set-Content -LiteralPath $exitCode -Encoding ascii
exit $LASTEXITCODE
