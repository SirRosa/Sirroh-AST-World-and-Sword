# Independent cave reference only: deploy pinned sources, preserve prior assets and saves.
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$p='D:\0 Sirro\Unreal Projects\AST_V1_18'
$e='D:\0 Sirro\UE 5.8.1\UE_5.8'
$u=Join-Path $p 'AST_V1_18.uproject'
$d=Join-Path $p 'Saved/ChatGPT/GeologicalCave_20261126'
$stage=Join-Path $d 'staging_555d41b'
$backup=Join-Path $d 'backup_555d41b'
$commit='555d41b1b3c9086027ae7332f55fbd3f953d718e'
$root='Content/Systems/Procedural_World_Generator/PWG_v2'
$newAsset="$root/M_PWGv2_DepthAtmosphere_CaveReference.uasset"
$newRock="$root/M_PWGv2_CaveReferenceRock.uasset"
$save='Saved/SaveGames/PWGv2_VF_POIProbe_20261126.sav'
$probe='Saved/SaveGames/PWGv2_VF_CaveReference_20261126.sav'
$entries=@(
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2SemanticVerification.cpp';Old='131bbb04bc45fef992443ebd78b5b87cde7ad87d';New='51a3f1f42bf43e968c7deae5f0bf1184eddd8b82'}
)
$unchanged=@(
@{Path='Source/Sirroh_Reimagineg_v3/Private/Debug/SirrohDebugSubsystem.cpp';Hash='092276aba14d048ccc1d3d440d02bf4b600b4edc'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2.cpp';Hash='151a39f02b817ef67104ca6badae273a9a595b75'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Decorations.cpp';Hash='24af637c5391716500970854bbe985a586570d6c'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2GeologicalCave.h';Hash='5ee3aec3b5c9da81a91170adce95954735d56005'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2IndependentCave.cpp';Hash='9cbc151abeafcad8a8463f31cad07e5c832259f9'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2IndependentCaveTests.cpp';Hash='50e755c8562bd517d368326221eaf477371b08af'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/PWG/PWGEnvironmentV2.h';Hash='dbe8fe0e3937df8d182cdc38573ea903d822e0d1'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/Procedural_World_Generator/PWGv2Generator.h';Hash='ba9527b91d0c3d2e3e933ff0349127af48fb0a04'},
@{Path='Source/Tools/create_pwg_cave_textured_ambient_pp.py';Hash='be991b5015247ebee0ec5ffff280872c4236cfd3'},
@{Path='Source/Tools/create_pwg_geological_cave_rock.py';Hash='61278c65002614a744dcea4f5a28f0b9d9ea23ce'}
)
New-Item -ItemType Directory -Path $d,$stage,$backup -Force | Out-Null
$result=Join-Path $d 'result_geological_resume.txt'
if(Test-Path $result){Copy-Item $result (Join-Path $d ('result_before_'+(Get-Date -Format 'yyyyMMdd_HHmmss')+'.txt'))}
"COMMIT=$commit" | Set-Content $result
function Note([string]$s){for($i=0;$i -lt 30;$i++){try{$s | Add-Content -Path $result -ErrorAction Stop; Write-Output $s; return}catch{if($i -eq 29){throw};Start-Sleep -Milliseconds 300}}}
function GitHash([string]$f){$h=& git hash-object --no-filters $f;if($LASTEXITCODE -ne 0){throw "git hash failed: $f"};return $h.Trim()}
function CheckNoEditor {
 if(Get-Process UnrealEditor,UnrealEditor-Cmd,AST_V1_18 -ErrorAction SilentlyContinue){
  throw 'Editor/game open; stopping without closing it'
 }
}
$protected=@{}
function CheckSources {foreach($x in $entries){$local=Join-Path $p $x.Path;if($x.Old){if(!(Test-Path $local) -or (GitHash $local) -ne $x.Old){throw "Local mismatch: $($x.Path)"}}elseif(Test-Path $local){throw "New path already exists: $($x.Path)"}}}
function CheckOriginals {foreach($item in $protected.GetEnumerator()){if((Get-FileHash (Join-Path $p $item.Key) -Algorithm SHA256).Hash -ne $item.Value){throw "Protected file changed: $($item.Key)"}}}
function Render([string]$name,[string[]]$extra,[string[]]$scenes,[string[]]$suffixes) {
 CheckNoEditor;CheckOriginals
 Copy-Item (Join-Path $p $save) (Join-Path $p $probe) -Force
 if((Get-FileHash (Join-Path $p $probe)).Hash -ne $protected[$save]){throw 'Probe save copy mismatch'}
 Note "RENDER_START=$name"
 $log=Join-Path $d ("render_"+$name+".log")
 $ErrorActionPreference='Continue'
 & "$e\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $u '/Game/Levels/L_SirrohGameplay' -game -SirrohSkipMainMenu -PWGWorld=VF_CaveReference_20261126 -PWGVisualPOIQA -PWGDepthLayersQA -PWGCaveReferenceQA -PWGGeologicalArtQA -windowed -ResX=1600 -ResY=900 -unattended -nop4 -nosound -nosplash -stdout -FullStdOutLogOutput '-ExecCmds=t.MaxFPS 60' @extra 2>&1 | Out-File -Encoding utf8 $log
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop'
 Note "RENDER_EXIT=$name $rc"
 foreach($line in (Select-String -Path $log -Pattern 'VF_RENDER_|VF_DEPTH_GPU|VF_DEPTH_CHUNK|VF_CAVE_HYBRID|VF_CAVE_GEOLOGICAL|VF_CAVE_REFERENCE|VF_CAVE_QA_OPENING|VF_CAVE_NO_LARGE_OPENING|VF_LANE_|VF_MASK_COUNTS|VF_POI_MASK|VF_SCENE|UBG_ISOLATION')){Note $line.Line}
 CheckOriginals
 if($rc -ne 0 -or !(Select-String -Path $log -Pattern 'VF_RENDER_COMPLETE' -Quiet) -or (Select-String -Path $log -Pattern 'VF_RENDER_FAIL|VF_RENDER_SCENE_UNAVAILABLE' -Quiet)){throw "Real render sequence failed $name"}
 foreach($scene in $scenes){foreach($suffix in $suffixes){
   $f=Join-Path $d "$name/${scene}_${suffix}.png"
   if(!(Test-Path $f) -or (Get-Item $f).Length -lt 10000){throw "Missing render $f"}
 }}
 Note "RENDER_CAPTURES_PASS=$name"
}
try{
 CheckNoEditor
 foreach($rel in @("$root/M_PWGv2_DepthAtmosphere.uasset","$root/M_PWGv2_DepthAtmosphere_V2.uasset","$root/M_PWGv2_DepthAtmosphere_Semantic.uasset","$root/M_PWGv2_DepthAtmosphere_SemanticPOI.uasset","$root/M_PWGv2_DepthAtmosphere_DepthLayersPrototype.uasset","$root/M_PWGv2_BackgroundLit.uasset",$save,'Saved/SaveGames/PWGv2_Serrania.sav','Saved/SaveGames/SirrohCharacter.sav')){
  if(Test-Path (Join-Path $p $rel)){ $protected[$rel]=(Get-FileHash (Join-Path $p $rel) -Algorithm SHA256).Hash }
 }
 if(!$protected.ContainsKey($save)){throw 'Missing original world'}
 $checkpoint=@{
  "$root/M_PWGv2_DepthAtmosphere.uasset"='565DB3F82136F15033C3F6B4652C8D26B1F1BF214AF1D85B64AAE956F64F3F62'
  "$root/M_PWGv2_DepthAtmosphere_V2.uasset"='D9AFA66C19389C4D69A2BA6121FDCF40C0A1F4A2F740EC3021DE0741EF2E5820'
  "$root/M_PWGv2_DepthAtmosphere_Semantic.uasset"='D736F40A523F9DB74F43F5B12C3C6AC8B4332B0A38E6433922C1698064DC8F34'
  "$root/M_PWGv2_DepthAtmosphere_SemanticPOI.uasset"='5ECFB7DB0CF8BF1F9F059B4369D8EE0B31025DE033CFC6F6628F5AB69C90807E'
  "$root/M_PWGv2_DepthAtmosphere_DepthLayersPrototype.uasset"='0CCAB2019FD82284B0AC2E4115F2D5BEB9FBE45F1970902818A3AD94377DB31A'
 }
 foreach($item in $checkpoint.GetEnumerator()){if($protected[$item.Key] -ne $item.Value){throw "Stage 1 asset mismatch: $($item.Key)"}}
 if(!(Select-String -Path (Join-Path $p 'Saved/ChatGPT/IndependentCaveReference_20261126/result_organic.txt') -Pattern '^COMPLETE=PASS$' -Quiet)){throw 'Organic reference not verified'}
 $stratified="$root/M_PWGv2_StratifiedCaveRock.uasset"
 if((Get-FileHash (Join-Path $p $stratified)).Hash -ne '919E582FDC2881201036AD019EE81370BC6B4954743188272E5F95E8E94A49A1'){throw 'Stratified rock mismatch'}
 $protected[$stratified]=(Get-FileHash (Join-Path $p $stratified)).Hash
 if((Get-FileHash (Join-Path $p $newAsset)).Hash -ne '287249DFBEE615AED918F5EF9EB1C5AB5D90064E4977AB12C9CA8083E64B2C6C'){throw 'Reference PP mismatch'}
 if((Get-FileHash (Join-Path $p $newRock)).Hash -ne 'C3F137208F657E2969D2127A594AE1BE3CF55691E06AE21F07A562EA3AD7EEA8'){throw 'Reference rock mismatch'}
 $protected[$newAsset]=(Get-FileHash (Join-Path $p $newAsset)).Hash
 $protected[$newRock]=(Get-FileHash (Join-Path $p $newRock)).Hash
 if(!(Select-String -Path (Join-Path $p 'Saved/ChatGPT/StratifiedCave_20261126/result_hybrid.txt') -Pattern '^COMPLETE=PASS$' -Quiet)){throw 'Previous Hybrid deployment not verified'}
 $ambient="$root/M_PWGv2_DepthAtmosphere_CaveTexturedAmbient.uasset"
 if((Get-FileHash (Join-Path $p $ambient)).Hash -ne '7E9ADFB1A424E11EFB5B9680BEFEC746ACBB8E945DF655F33F5B02A491E76D56'){throw 'Ambient material mismatch'}
 $georock="$root/M_PWGv2_GeologicalCaveRock.uasset"
 if((Get-FileHash (Join-Path $p $georock)).Hash -ne '04DABAA785D42E286F48A077C8AD87FF95CCB2D05D9636E9FFCE5433E6101A98'){throw 'Geological rock mismatch'}
 $protected[$ambient]=(Get-FileHash (Join-Path $p $ambient)).Hash
 $protected[$georock]=(Get-FileHash (Join-Path $p $georock)).Hash
 foreach($x in $unchanged){if((GitHash (Join-Path $p $x.Path)) -ne $x.Hash){throw "Deployed source mismatch $($x.Path)"}}
 $hybrid="$root/M_PWGv2_HybridCaveRock.uasset"
 if((Get-FileHash (Join-Path $p $hybrid)).Hash -ne 'B24EDA36A6580A11D7753B920484F63DAC6F16266EC089494603BF59A696BD7A'){throw 'Hybrid material mismatch'}
 $protected[$hybrid]=(Get-FileHash (Join-Path $p $hybrid)).Hash
 CheckSources
 foreach($x in $entries){
   $local=Join-Path $p $x.Path; $bak=Join-Path $backup $x.Path; $st=Join-Path $stage $x.Path
   New-Item -ItemType Directory -Path (Split-Path $bak),(Split-Path $st) -Force|Out-Null
   if($x.Old){Copy-Item $local $bak
   if((GitHash $bak) -ne $x.Old){throw "Backup mismatch $($x.Path)"}}
   Invoke-WebRequest -UseBasicParsing -Uri "https://raw.githubusercontent.com/SirRosa/Sirroh-AST-World-and-Sword/$commit/$($x.Path)" -OutFile $st
   if((GitHash $st) -ne $x.New){throw "Staging mismatch $($x.Path)"}
 }
 CheckNoEditor;CheckOriginals;CheckSources
 CheckNoEditor;CheckOriginals;CheckSources
 foreach($x in $entries){
   $local=Join-Path $p $x.Path;New-Item -ItemType Directory -Path (Split-Path $local) -Force|Out-Null;Copy-Item (Join-Path $stage $x.Path) $local -Force
   if((GitHash $local) -ne $x.New){throw "Deploy mismatch $($x.Path)"}
 }
 Note 'GEOLOGICAL_DEPLOYED=PASS'
 Note 'BUILD=START'
 $ErrorActionPreference='Continue'
 & "$e\Engine\Build\BatchFiles\Build.bat" AST_V1_18Editor Win64 Development "-Project=$u" -WaitMutex -NoHotReloadFromIDE 2>&1 | Out-File -Encoding utf8 (Join-Path $d 'geological_build_resume.log')
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop';Note "BUILD_EXIT=$rc";if($rc -ne 0){throw 'Build failed'}
 CheckOriginals;CheckNoEditor
 Note 'TESTS=START'
 $ErrorActionPreference='Continue'
 & "$e\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $u '-ExecCmds=Automation RunTests Sirroh.PWG.IndependentCave+Sirroh.PWG.DepthPrototype+Sirroh.PWG.VisualProfile+Sirroh.PWG.SpawnGeometryAudit+Sirroh.PWG.StreamingCost+Sirroh.POI.RoomChains' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$d/geological_test_report" -unattended -nop4 -nullrhi -nosplash -stdout -FullStdOutLogOutput 2>&1 | Out-File -Encoding utf8 (Join-Path $d 'geological_test_resume.log')
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop';Note "TEST_EXIT=$rc"
 $testLines=Select-String -Path (Join-Path $d 'geological_test_resume.log') -Pattern 'Test Completed|Found [0-9]+ automation tests|No automation tests'
 foreach($line in $testLines){Note $line.Line}
 if($rc -ne 0 -or (Select-String -Path (Join-Path $d 'geological_test_resume.log') -Pattern 'Result=\{Fail\}' -Quiet)){throw 'Technical tests failed'}
 foreach($n in @('IndependentCave','DepthPrototype','VisualProfile','SpawnGeometryAudit','StreamingCost','RoomChains')){if(!($testLines | Where-Object {$_.Line -match 'Result=\{Success\}' -and $_.Line -match $n})){throw "No verified success for $n"}}
 Note 'TECHNICAL_TESTS=PASS'

 Render 'Render' @() @('Chamber') @('Stage1','Composition','DepthMasks','AmbientOff','RawLit','BaseColor','Geometry','Dark','TorchDark')
 CheckOriginals
 Note 'ORIGINALS_UNCHANGED=PASS'
 Note 'COMPLETE=PASS'
}catch{Note "FAILED=$($_.Exception.Message)";exit 1}
