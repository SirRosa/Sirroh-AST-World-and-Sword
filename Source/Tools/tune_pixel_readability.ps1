$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$p='D:\0 Sirro\Unreal Projects\AST_V1_18'
$e='D:\Sirro\UE 5.8.1\UE_5.8'
$u=Join-Path $p 'AST_V1_18.uproject'
$d=Join-Path $p 'Saved\ChatGPT\PixelReadability_20261128'
$stage=Join-Path $d 'stage_tuned'
$backup=Join-Path $d 'backup_tuned'
New-Item -ItemType Directory -Force $d,$stage,$backup|Out-Null
$result=Join-Path $d 'result_tuned.txt'
function Note($s){$s|Tee-Object -FilePath $result -Append}
function Hash($f){(& git hash-object --no-filters $f).Trim()}
function Native($exe,$a,$log){& $exe @a *> $log;return $LASTEXITCODE}
function NoEditor {if(Get-Process UnrealEditor,UnrealEditor-Cmd,AST_V1_18 -ErrorAction SilentlyContinue){throw 'Editor/game running; deployment stopped'}}
$commit='b4b947de3ede9229fb43d90e70f9f86fb3e73f01'
$files=@(
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2SemanticVerification.cpp';Old='d650379b2fc3596854141602b23ac4e1fda2f4a8';New='1941d382da06a84a712a9feb49e01fe4b9408c0c'},
@{Path='Source/Tools/create_pwg_pixel_readability.py';Old='4dd57b20df741ea5f1b593c8d31784f78aa280cd';New='85f3f3f21b2431aca610f0268f0e469f8fcc694f'},
@{Path='Source/Tools/tune_pwg_pixel_readability.py';Old='';New='0fcc7c2b11d7d57091607056a663916e1232af27'}
)
try{
 Note "SOURCE_COMMIT=$commit"
 NoEditor
 if((Get-Content (Join-Path $d 'result_verified.txt') -Raw) -notmatch 'RENDER_CAPTURES=10'){throw 'Verified A/B evidence missing'}
 if(Test-Path (Join-Path $d 'RenderTuned')){throw 'Tuned render folder exists'}
 $src=Join-Path $p 'Saved\SaveGames\PWGv2_Serrania.sav'
 $dst=Join-Path $p 'Saved\SaveGames\PWGv2_VF_PixelReadability_20261128.sav'
 if(!(Test-Path $src) -or !(Test-Path $dst) -or (Get-FileHash $src).Hash -ne (Get-FileHash $dst).Hash){throw 'QA slot is not untouched copy'}
 $asset=Get-ChildItem (Join-Path $p 'Content') -Recurse -File -Filter 'M_PWGv2_PixelBackground_Readability.uasset'|Select-Object -First 1
 if(!$asset -or (Get-FileHash $asset.FullName).Hash -ne '360E51D1CDAC084350375B2EA5C564C6C455812BDE02CCA5E4F96C34AF4FD269'){throw 'Existing readability asset hash mismatch'}
 $protected=@{}
 Get-ChildItem (Join-Path $p 'Saved\SaveGames') -File -Filter '*.sav'|Where-Object {$_.FullName -ne $dst}|ForEach-Object {$protected[$_.FullName]=(Get-FileHash $_.FullName).Hash}
 Get-ChildItem (Join-Path $p 'Content') -Recurse -File -Filter 'M_PWGv2*.uasset'|Where-Object {$_.FullName -ne $asset.FullName}|ForEach-Object {$protected[$_.FullName]=(Get-FileHash $_.FullName).Hash}
 foreach($f in $files){
   $local=Join-Path $p $f.Path
   if($f.Old){if(!(Test-Path $local) -or (Hash $local) -ne $f.Old){throw "BASELINE_MISMATCH $($f.Path)"}}
   elseif(Test-Path $local){throw "NEW_PATH_EXISTS $($f.Path)"}
   $target=Join-Path $stage $f.Path;New-Item -ItemType Directory -Force (Split-Path $target)|Out-Null
   Invoke-WebRequest -UseBasicParsing -Uri "https://raw.githubusercontent.com/SirRosa/Sirroh-AST-World-and-Sword/$commit/$($f.Path)" -OutFile $target
   if((Hash $target) -ne $f.New){throw "STAGED_HASH_MISMATCH $($f.Path)"}
 }
 $unchanged=@{
 'Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2PixelField.h'='a69e30c63e7d9beecfdba66d5992dba2975164d3'
 'Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2PixelBackground.cpp'='4f0c192bcec3de681b6c82e9b9f646e4532f1317'
 'Source/Tools/create_pwg_pixel_background.py'='2adb35e586761cd2d6bcf50345b083cf5a6959c6'
 }
 foreach($k in $unchanged.Keys){if((Hash (Join-Path $p $k)) -ne $unchanged[$k]){throw "UNEXPECTED_PAINT_OR_BASELINE_CHANGE $k"}}
 foreach($f in $files){if($f.Old){$b=Join-Path $backup $f.Path;New-Item -ItemType Directory -Force (Split-Path $b)|Out-Null;Copy-Item (Join-Path $p $f.Path) $b;if((Hash $b) -ne $f.Old){throw 'BACKUP_HASH_MISMATCH'}}}
 $assetBackup=Join-Path $backup $asset.Name
 if(Test-Path $assetBackup){throw 'Existing asset backup; stop'}
 Copy-Item $asset.FullName $assetBackup
 if((Get-FileHash $assetBackup).Hash -ne '360E51D1CDAC084350375B2EA5C564C6C455812BDE02CCA5E4F96C34AF4FD269'){throw 'Asset backup hash mismatch'}
 Note 'BACKUP_VERIFIED=1; PAINT_AND_MASKS_UNCHANGED'
 $editor=Join-Path $e 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
 $author=(Join-Path $stage 'Source/Tools/tune_pwg_pixel_readability.py').Replace('\','/')
 $rc=Native $editor @($u,'-run=pythonscript',("-script="+$author),'-unattended','-nop4','-AllowCommandletRendering','-nosplash','-stdout','-FullStdOutLogOutput') (Join-Path $d 'author_tuned.log')
 Note "AUTHOR_EXIT=$rc"
 $al=Get-Content (Join-Path $d 'author_tuned.log') -Raw
 if($rc -ne 0 -or $al -notmatch 'PIXEL_READABILITY_PALETTE_TUNED' -or $al -match 'Traceback \(most recent call last\)|Failed to compile Material.*PixelBackground_Readability'){throw 'Palette tuning not verified'}
 Note "TUNED_ASSET_SHA256=$((Get-FileHash $asset.FullName).Hash)"
 foreach($k in $protected.Keys){if(!(Test-Path $k) -or (Get-FileHash $k).Hash -ne $protected[$k]){throw "ORIGINAL_CHANGED $k"}}
 NoEditor
 foreach($f in $files){$local=Join-Path $p $f.Path;New-Item -ItemType Directory -Force (Split-Path $local)|Out-Null;Copy-Item (Join-Path $stage $f.Path) $local -Force;if((Hash $local) -ne $f.New){throw "DEPLOY_HASH_MISMATCH $($f.Path)"}}
 Note 'DEPLOYED=1'
 $rc=Native (Join-Path $e 'Engine\Build\BatchFiles\Build.bat') @('AST_V1_18Editor','Win64','Development',"-Project=$u",'-WaitMutex','-NoHotReloadFromIDE','-NoXGE','-NoUBA','-NoUBTMakefiles') (Join-Path $d 'build_tuned.log')
 Note "BUILD_EXIT=$rc"
 if($rc -ne 0){Get-Content (Join-Path $d 'build_tuned.log') -Tail 30|ForEach-Object {Note $_};throw 'Build failed'}
 $rc=Native $editor @($u,'/Game/Levels/L_SirrohGameplay','-game','-PWGVisualPOIQA','-PWGPixelReadabilityQA','-PWGPixelReadabilityAfterOnly','-SirrohSkipMainMenu','-PWGWorld=VF_PixelReadability_20261128','-windowed','-ResX=1600','-ResY=900','-unattended','-nop4','-nosound','-nosplash','-stdout','-FullStdOutLogOutput','-ExecCmds=t.MaxFPS 60') (Join-Path $d 'render_tuned.log')
 Note "RENDER_EXIT=$rc"
 $rl=Get-Content (Join-Path $d 'render_tuned.log') -Raw
 foreach($k in $protected.Keys){if(!(Test-Path $k) -or (Get-FileHash $k).Hash -ne $protected[$k]){throw "ORIGINAL_CHANGED $k"}}
 Note 'ORIGINALS_UNCHANGED=1'
 if($rc -ne 0 -or $rl -notmatch 'VF_RENDER_COMPLETE' -or $rl -match 'VF_RENDER_FAIL|VF_RENDER_SCENE_UNAVAILABLE|PWG_PIXEL2D_CHUNK.*ready=0|Failed to compile Material.*PixelBackground_Readability'){throw 'RHI capture failed'}
 foreach($n in @('Tunnel_After','Shaft_After','DeepCave_After','Tunnel_RawPalette')){
   $f=Join-Path $d "RenderTuned\$n.png"
   if(!(Test-Path $f) -or (Get-Item $f).Length -lt 10000){throw "CAPTURE_MISSING $n"}
 }
 $oldLog=Get-Content (Join-Path $d 'render_verified.log')
 foreach($n in @('Tunnel','Shaft','DeepCave')){
   $oldLine=($oldLog|Select-String -Pattern ('VF_RENDER_CAPTURE '+$n+'_Before camera=')|Select-Object -First 1).Line
   $newLine=($rl -split "\r?\n"|Select-String -Pattern ('VF_RENDER_CAPTURE '+$n+'_After camera=')|Select-Object -First 1).Line
   if(!$oldLine -or !$newLine){throw "CAMERA_LOG_MISSING $n"}
   $oldView=($oldLine.Substring($oldLine.IndexOf('camera=')) -split ' pawn=')[0]
   $newView=($newLine.Substring($newLine.IndexOf('camera=')) -split ' pawn=')[0]
   if($oldView -ne $newView){throw "CAMERA_MISMATCH $n"}
 }
 Note 'MATCHED_CAMERA=3'
 Note 'RENDER_CAPTURES=4'
 Note 'COMPLETE=1'
}catch{Note "FAIL=$($_.Exception.Message)";exit 1}
