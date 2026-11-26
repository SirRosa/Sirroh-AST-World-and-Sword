# Unified Underground Background: deploy pinned sources, preserve prior assets and saves.
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$p='D:\0 Sirro\Unreal Projects\AST_V1_18'
$e='D:\0 Sirro\UE 5.8.1\UE_5.8'
$u=Join-Path $p 'AST_V1_18.uproject'
$d=Join-Path $p 'Saved/ChatGPT/UnifiedBackground_20261126'
$stage=Join-Path $d 'staging_743f2a6'
$backup=Join-Path $d 'backup_743f2a6'
$commit='743f2a6eea6e283cd74862d433f99564f96bebbf'
$root='Content/Systems/Procedural_World_Generator/PWG_v2'
$save='Saved/SaveGames/PWGv2_asd.sav'
$probe='Saved/SaveGames/PWGv2_VF_POIProbe_20261126.sav'
$entries=@(
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2SemanticVerification.cpp';Old='42084526e293d6d69880996a1eb5500df5c5f9d5';New='5b057acf62252f876feb9858bda95bd6a9809062'}
)
$expectedPrevious=@(
@{Path='Source/Sirroh_Reimagineg_v3/Private/POI/POIShaft.cpp';Hash='12b427daef8e47d7e0090095a2905ef3a9fad4fe'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/POI/PWGSpawnGeometryAudit.cpp';Hash='ea122b988ddc78571c13e7c73a5641d966d29b29'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2AuthoredPOI.cpp';Hash='aac8aff5eddeb6114970a4af5ec3e9868e6313e6'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Decorations.cpp';Hash='d4e0de89466fed8c7a9741f72a916886167d6a15'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Underground.cpp';Hash='6dd15301a5e6d2589f91b61f4d37c39df7afd4e6'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2VisualMasks.cpp';Hash='47307a327f76e35a73e6b8849fa736bf97f126c8'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/POI/POIShaft.h';Hash='f81a117c213c54c1f4a336e6764ea32b375994c1'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/Procedural_World_Generator/PWGv2Generator.h';Hash='56e3b39adfface403ad101983a9e6e785d050707'}
)
New-Item -ItemType Directory -Path $d,$stage,$backup -Force | Out-Null
$result=Join-Path $d 'result_audit_repair.txt'
"COMMIT=$commit" | Set-Content $result
function Note([string]$s){$s | Add-Content $result; Write-Output $s}
function GitHash([string]$f){$h=& git hash-object --no-filters $f;if($LASTEXITCODE -ne 0){throw "git hash failed: $f"};return $h.Trim()}
function CheckNoEditor {if(Get-Process UnrealEditor,UnrealEditor-Cmd,AST_V1_18 -ErrorAction SilentlyContinue){throw 'Editor/game open; stopping without closing it'}}
$protected=@{}
function CheckOriginals {foreach($item in $protected.GetEnumerator()){if((Get-FileHash (Join-Path $p $item.Key) -Algorithm SHA256).Hash -ne $item.Value){throw "Protected file changed: $($item.Key)"}}}
function Render([string]$name,[string[]]$extra,[string[]]$scenes,[string[]]$suffixes) {
 CheckNoEditor;CheckOriginals
 Copy-Item (Join-Path $p $save) (Join-Path $p $probe) -Force
 if((Get-FileHash (Join-Path $p $probe)).Hash -ne $protected[$save]){throw 'Probe save copy mismatch'}
 Note "RENDER_START=$name"
 $log=Join-Path $d ("render_"+$name+"_repair.log")
 $ErrorActionPreference='Continue'
 & "$e\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $u '/Game/Levels/L_SirrohGameplay' -game -SirrohSkipMainMenu -PWGWorld=VF_POIProbe_20261126 -PWGVisualPOIQA -windowed -ResX=1600 -ResY=900 -unattended -nop4 -nosound -nosplash -stdout -FullStdOutLogOutput '-ExecCmds=t.MaxFPS 60' @extra 2>&1 | Out-File -Encoding utf8 $log
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop'
 Note "RENDER_EXIT=$name $rc"
 foreach($line in (Select-String -Path $log -Pattern 'VF_RENDER_|VF_GPU|VF_LANE_|VF_MASK_COUNTS|VF_POI_MASK|VF_SCENE|UBG_ISOLATION')){Note $line.Line}
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
 foreach($rel in @("$root/M_PWGv2_DepthAtmosphere.uasset","$root/M_PWGv2_DepthAtmosphere_V2.uasset","$root/M_PWGv2_DepthAtmosphere_Semantic.uasset","$root/M_PWGv2_DepthAtmosphere_SemanticPOI.uasset",$save,'Saved/SaveGames/SirrohCharacter.sav')){
  if(Test-Path (Join-Path $p $rel)){ $protected[$rel]=(Get-FileHash (Join-Path $p $rel) -Algorithm SHA256).Hash }
 }
 if(!$protected.ContainsKey($save)){throw 'Missing original world'}
 if($protected["$root/M_PWGv2_DepthAtmosphere_SemanticPOI.uasset"] -ne '5ECFB7DB0CF8BF1F9F059B4369D8EE0B31025DE033CFC6F6628F5AB69C90807E'){throw 'Previous shader hash differs'}
 foreach($x in $entries){$local=Join-Path $p $x.Path;if(!(Test-Path $local) -or (GitHash $local) -ne $x.Old){throw "Local mismatch: $($x.Path)"}}
 foreach($x in $expectedPrevious){if((GitHash (Join-Path $p $x.Path)) -ne $x.Hash){throw "Existing implementation differs: $($x.Path)"}}
 Note 'LOCAL_PREFLIGHT=PASS'
 foreach($x in $entries){$dst=Join-Path $stage $x.Path;New-Item -ItemType Directory -Path (Split-Path $dst) -Force|Out-Null;Invoke-WebRequest -UseBasicParsing -Uri "https://raw.githubusercontent.com/SirRosa/Sirroh-AST-World-and-Sword/$commit/$($x.Path)" -OutFile $dst;if((GitHash $dst) -ne $x.New){throw "Staging mismatch $($x.Path)"}}
 Note 'STAGING_HASHES=PASS'
 foreach($x in $entries){$dst=Join-Path $backup $x.Path;New-Item -ItemType Directory -Path (Split-Path $dst) -Force|Out-Null;Copy-Item (Join-Path $p $x.Path) $dst;if((GitHash $dst) -ne $x.Old){throw "Backup mismatch $($x.Path)"}}
 foreach($item in $protected.GetEnumerator()){$dst=Join-Path $backup $item.Key;New-Item -ItemType Directory -Path (Split-Path $dst) -Force|Out-Null;Copy-Item (Join-Path $p $item.Key) $dst;if((Get-FileHash $dst).Hash -ne $item.Value){throw "Backup mismatch $($item.Key)"}}
 if(Test-Path (Join-Path $p $probe)){Copy-Item (Join-Path $p $probe) (Join-Path $d 'probe_before_audit_repair.sav')}
 Note 'BACKUPS=PASS'
 CheckNoEditor;CheckOriginals
 foreach($x in $entries){if((GitHash (Join-Path $p $x.Path)) -ne $x.Old){throw "Local changed during staging $($x.Path)"}}
 foreach($x in $entries){$local=Join-Path $p $x.Path;Copy-Item (Join-Path $stage $x.Path) $local;if((GitHash $local) -ne $x.New){throw "Deploy mismatch $($x.Path)"}}
 Note 'DEPLOYED_HASHES=PASS'
 Note 'BUILD=START'
 $ErrorActionPreference='Continue'
 & "$e\Engine\Build\BatchFiles\Build.bat" AST_V1_18Editor Win64 Development "-Project=$u" -WaitMutex -NoHotReloadFromIDE 2>&1 | Out-File -Encoding utf8 (Join-Path $d 'build_audit_repair.log')
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop';Note "BUILD_EXIT=$rc";if($rc -ne 0){throw 'Build failed'}
 CheckOriginals;CheckNoEditor
 # Only QA sequencing changed; all previously tested production files are verified above.
 $oldAudit=Join-Path $d 'Audit'
 $archive=Join-Path $d 'Audit_first_capture_unreliable'
 if(Test-Path $archive){throw 'Audit archive exists'}
 if(Test-Path $oldAudit){Move-Item $oldAudit $archive}
 Render 'Audit' @('-PWGBackgroundAuditLegacy') @('Tunnel','Shaft','POI','Transition') @('Atmospheric','NoNativeCards','NoProjectedArt','NoShaftWalls','NoCaveBackdrop','NativeOnly')
 CheckOriginals
 Note 'ORIGINALS_UNCHANGED=PASS'
 Note 'COMPLETE=PASS'
}catch{Note "FAILED=$($_.Exception.Message)";exit 1}
