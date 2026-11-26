$ErrorActionPreference='Stop'
$p='D:\0 Sirro\Unreal Projects\AST_V1_18'
$e='D:\0 Sirro\UE 5.8.1\UE_5.8'
$d=Join-Path $p 'Saved\ChatGPT\ProceduralPixel2D_20261126'
$u=Join-Path $p 'AST_V1_18.uproject'
$result=Join-Path $d 'result_render.txt'
function Note($s){$s|Tee-Object -FilePath $result -Append}
try{
 if(Get-Process UnrealEditor,UnrealEditor-Cmd,AST_V1_18 -ErrorAction SilentlyContinue){throw 'Editor/game running'}
 if((Get-Content (Join-Path $d 'result_resume.txt') -Raw) -notmatch 'TECHNICAL_TESTS=PASS'){throw 'Technical test checkpoint absent'}
 $f=Join-Path $p 'Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2PixelBackground.cpp'
 if((& git hash-object --no-filters $f).Trim() -ne 'f2d57d0d0719532bf5b6ff9071bce21b9ebed994'){throw 'Source hash changed'}
 $src=Join-Path $p 'Saved\SaveGames\PWGv2_VF_POIProbe_20261126.sav'
 $dst=Join-Path $p 'Saved\SaveGames\PWGv2_VF_Pixel2D_20261126.sav'
 if(!(Test-Path $src) -or !(Test-Path $dst)){throw 'QA slot missing'}
 if((Get-FileHash $src).Hash -ne (Get-FileHash $dst).Hash){throw 'QA slot not an untouched copy; inspect without overwriting'}
 $protected=@{}
 Get-ChildItem (Join-Path $p 'Saved\SaveGames') -File -Filter '*.sav'|Where-Object {$_.FullName -ne $dst}|ForEach-Object {$protected[$_.FullName]=(Get-FileHash $_.FullName).Hash}
 Get-ChildItem (Join-Path $p 'Content') -Recurse -File -Filter 'M_PWGv2*.uasset'|ForEach-Object {$protected[$_.FullName]=(Get-FileHash $_.FullName).Hash}
 Note 'PREFLIGHT=PASS'
 & (Join-Path $e 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $u '/Game/Levels/L_SirrohGameplay' -game -SirrohSkipMainMenu -PWGWorld=VF_Pixel2D_20261126 -PWGVisualPOIQA -PWGPixel2DQA -windowed -ResX=1600 -ResY=900 -unattended -nop4 -nosound -nosplash -stdout -FullStdOutLogOutput '-ExecCmds=t.MaxFPS 60' *> (Join-Path $d 'render_gameplay.log')
 $rc=$LASTEXITCODE
 Note "RENDER_EXIT=$rc"
 $rl=Get-Content (Join-Path $d 'render_gameplay.log') -Raw
 $caps=@(Get-ChildItem (Join-Path $d 'Render') -Filter '*.png' -ErrorAction SilentlyContinue)
 foreach($c in $caps){Note "CAPTURE=$($c.Name) bytes=$($c.Length)"}
 foreach($k in $protected.Keys){if(!(Test-Path $k) -or (Get-FileHash $k).Hash -ne $protected[$k]){throw "ORIGINAL_CHANGED $k"}}
 Note 'ORIGINALS_UNCHANGED=1'
 if($rc -ne 0 -or $rl -notmatch 'VF_RENDER_COMPLETE' -or $rl -match 'VF_RENDER_FAIL|VF_RENDER_SCENE_UNAVAILABLE|PWG_PIXEL2D_CHUNK.*ready=0' -or $caps.Count -ne 6){throw 'Render QA incomplete; see render_gameplay.log'}
 Note 'RENDER_CAPTURES=6'
 Note 'COMPLETE=1'
}catch{Note "FAIL=$($_.Exception.Message)";exit 1}
