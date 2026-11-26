$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$p='D:\0 Sirro\Unreal Projects\AST_V1_18'
$e='D:\0 Sirro\UE 5.8.1\UE_5.8'
$u=Join-Path $p 'AST_V1_18.uproject'
$d=Join-Path $p 'Saved\ChatGPT\ProceduralPixel2D_20261126'
$stage=Join-Path $d 'stage'
$backup=Join-Path $d 'backup'
New-Item -ItemType Directory -Force $d,$stage,$backup|Out-Null
$result=Join-Path $d 'result.txt'
function Note($s){$s|Tee-Object -FilePath $result -Append}
function Hash($f){(& git hash-object --no-filters $f).Trim()}
function Native($exe,$a,$log){& $exe @a *> $log;return $LASTEXITCODE}
function NoEditor {if(Get-Process UnrealEditor,UnrealEditor-Cmd,AST_V1_18 -ErrorAction SilentlyContinue){throw 'Editor/game running; deployment stopped'}}
$commit='d0112fe2c8f4a44b0bca76227bc3be0c6eeb6fab'
$files=@(
@{Path='Source/Sirroh_Reimagineg_v3/Private/Debug/SirrohDebugSubsystem.cpp';Old='092276aba14d048ccc1d3d440d02bf4b600b4edc';New='1531f8e790f037559edd4b41743b35264afa5a74'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2.cpp';Old='151a39f02b817ef67104ca6badae273a9a595b75';New='30fc136e6a1d6bbec02fb22ccde632a3617fe595'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2SemanticVerification.cpp';Old='51a3f1f42bf43e968c7deae5f0bf1184eddd8b82';New='2c8fe1cf320f9a4eac0031a0a2fc158d40a13ed6'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Decorations.cpp';Old='24af637c5391716500970854bbe985a586570d6c';New='5353f6c4e1778fc67d51c8c9cbf62bbcb2f7cffe'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2PixelBackground.cpp';Old='';New='0a57d7329778c7a33a9f402552ab19fb399da561'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2PixelField.h';Old='';New='2bf7e842d514a2b2943a98fd70a12cf11d647143'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2PixelFieldTests.cpp';Old='';New='9c6befa152e9955f29b66a692e043cb73042cd34'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Underground.cpp';Old='7c31c1dc50821744ffed646743685884cb605c48';New='a928157f7ab90aa49cf545b4796448ac56fb49a2'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/PWG/PWGEnvironmentV2.h';Old='dbe8fe0e3937df8d182cdc38573ea903d822e0d1';New='63280343ba72d3170ecd682735d542e9b1485e2b'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/Procedural_World_Generator/PWGv2Generator.h';Old='ba9527b91d0c3d2e3e933ff0349127af48fb0a04';New='c4fa46342d9a07c40bee4830153da261e13b854e'},
@{Path='Source/Tools/create_pwg_pixel_background.py';Old='';New='2adb35e586761cd2d6bcf50345b083cf5a6959c6'},
@{Path='Source/Tools/create_pwg_pixel_depth_pp.py';Old='';New='adbe58cedeaf343cc40eb11de0b8fb1b29cc3607'}
)
try {
 Note "SOURCE_COMMIT=$commit"
 NoEditor
 foreach($f in $files){
  $local=Join-Path $p $f.Path
  if($f.Old){if(!(Test-Path $local) -or (Hash $local) -ne $f.Old){throw "BASELINE_MISMATCH $($f.Path)"}}
  elseif(Test-Path $local){throw "NEW_PATH_EXISTS $($f.Path)"}
  $target=Join-Path $stage $f.Path
  New-Item -ItemType Directory -Force (Split-Path $target)|Out-Null
  Invoke-WebRequest -UseBasicParsing -Uri "https://raw.githubusercontent.com/SirRosa/Sirroh-AST-World-and-Sword/$commit/$($f.Path)" -OutFile $target
  if((Hash $target) -ne $f.New){throw "STAGED_HASH_MISMATCH $($f.Path)"}
 }
 Note 'STAGED_HASHES_OK'
 $protected=@{}
 Get-ChildItem (Join-Path $p 'Saved\SaveGames') -File -Filter '*.sav' -ErrorAction SilentlyContinue|Where-Object {$_.Name -notlike '*Test*' -and $_.Name -notlike '*Audit*'}|ForEach-Object {$protected[$_.FullName]=(Get-FileHash $_.FullName).Hash}
 Get-ChildItem (Join-Path $p 'Content') -Recurse -File -Filter 'M_PWGv2*.uasset'|ForEach-Object {$protected[$_.FullName]=(Get-FileHash $_.FullName).Hash}
 $copySource=Join-Path $p 'Saved\SaveGames\PWGv2_VF_POIProbe_20261126.sav'
 $copyDest=Join-Path $p 'Saved\SaveGames\PWGv2_VF_Pixel2D_20261126.sav'
 if(!(Test-Path $copySource)){throw 'QA source slot missing'}
 if(Test-Path $copyDest){throw 'QA destination already exists'}
 foreach($f in $files){if($f.Old){$b=Join-Path $backup $f.Path;New-Item -ItemType Directory -Force (Split-Path $b)|Out-Null;Copy-Item (Join-Path $p $f.Path) $b;if((Hash $b) -ne $f.Old){throw 'BACKUP_HASH_MISMATCH'}}}
 Note 'LOCAL_SOURCE_BACKUP_OK'
 $author=Join-Path $d 'author.py'
 $s1=(Join-Path $stage 'Source/Tools/create_pwg_pixel_background.py').Replace('\','/')
 $s2=(Join-Path $stage 'Source/Tools/create_pwg_pixel_depth_pp.py').Replace('\','/')
 @("import runpy","runpy.run_path(r'$s1',run_name='__main__')","runpy.run_path(r'$s2',run_name='__main__')")|Set-Content -Encoding UTF8 $author
 $editor=Join-Path $e 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
 $rc=Native $editor @($u,'-run=pythonscript',("-script="+$author.Replace('\','/')),'-unattended','-nop4','-AllowCommandletRendering','-nosplash','-stdout','-FullStdOutLogOutput') (Join-Path $d 'author.log')
 Note "AUTHOR_EXIT=$rc"
 $al=Get-Content (Join-Path $d 'author.log') -Raw
 if($rc -ne 0 -or $al -notmatch 'PIXEL_BACKGROUND_SAVED' -or $al -notmatch 'PIXEL_DEPTH_PP_SAVED' -or $al -match 'Traceback \(most recent call last\)|Failed to compile Material.*Pixel'){throw 'Material authoring not verified; see author.log'}
 foreach($k in $protected.Keys){if(!(Test-Path $k) -or (Get-FileHash $k).Hash -ne $protected[$k]){throw "ORIGINAL_CHANGED $k"}}
 NoEditor
 foreach($f in $files){$local=Join-Path $p $f.Path;New-Item -ItemType Directory -Force (Split-Path $local)|Out-Null;Copy-Item (Join-Path $stage $f.Path) $local -Force;if((Hash $local) -ne $f.New){throw "DEPLOY_HASH_MISMATCH $($f.Path)"}}
 Note 'DEPLOYED=1'
 $rc=Native (Join-Path $e 'Engine\Build\BatchFiles\Build.bat') @('AST_V1_18Editor','Win64','Development',"-Project=$u",'-WaitMutex','-NoHotReloadFromIDE') (Join-Path $d 'build.log')
 Note "BUILD_EXIT=$rc"
 if($rc -ne 0){Get-Content (Join-Path $d 'build.log') -Tail 35|ForEach-Object {Note $_};throw 'Build failed'}
 $rc=Native $editor @($u,'-ExecCmds=Automation RunTests Sirroh.PWG.PixelField+Sirroh.PWG.VisualProfile+Sirroh.PWG.SpawnGeometryAudit+Sirroh.PWG.StreamingCost+Sirroh.POI.RoomChains','-TestExit=Automation Test Queue Empty','-unattended','-nop4','-nullrhi','-nosplash','-stdout','-FullStdOutLogOutput') (Join-Path $d 'tests.log')
 Note "TEST_EXIT=$rc"
 $tests=Select-String -Path (Join-Path $d 'tests.log') -Pattern 'Test Completed|Automation Test Queue Empty'
 $tests|ForEach-Object {Note $_.Line}
 if($rc -ne 0 -or @($tests|Where-Object {$_.Line -match 'Result=\{Success\}'}).Count -lt 5 -or @($tests|Where-Object {$_.Line -match 'Result=\{Fail'}).Count -gt 0){throw 'Technical tests not verified'}
 Note 'TECHNICAL_TESTS=PASS'
 Copy-Item $copySource $copyDest
 $rc=Native $editor @($u,'-game','-PWGVisualPOIQA','-PWGPixel2DQA','-SirrohSkipMainMenu','-PWGWorld=VF_Pixel2D_20261126','-windowed','-ResX=1600','-ResY=900','-unattended','-nop4','-nosound','-nosplash','-stdout','-FullStdOutLogOutput','-ExecCmds=t.MaxFPS 60') (Join-Path $d 'render.log')
 Note "RENDER_EXIT=$rc"
 $rl=Get-Content (Join-Path $d 'render.log') -Raw
 if($rc -ne 0 -or $rl -notmatch 'VF_RENDER_COMPLETE' -or $rl -match 'VF_RENDER_FAIL|VF_RENDER_SCENE_UNAVAILABLE|PWG_PIXEL2D_CHUNK.*ready=0'){throw 'Render QA failed or incomplete'}
 foreach($n in @('Tunnel_Composition','Tunnel_Dark','Tunnel_TorchDark','Shaft_Composition','POI_Composition','DeepCave_Composition')){$f=Join-Path $d "Render\$n.png";if(!(Test-Path $f) -or (Get-Item $f).Length -lt 10000){throw "CAPTURE_MISSING $n"}}
 Note 'RENDER_CAPTURES=6'
 foreach($k in $protected.Keys){if(!(Test-Path $k) -or (Get-FileHash $k).Hash -ne $protected[$k]){throw "ORIGINAL_CHANGED $k"}}
 Note 'ORIGINALS_UNCHANGED=1'
 Note 'COMPLETE=1'
}catch{Note "FAIL=$($_.Exception.Message)";exit 1}
