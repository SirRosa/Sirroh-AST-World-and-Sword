# Stage 1 final verification: read-only source check and immutable binary material copies.
$ErrorActionPreference='Stop'
$p='D:\0 Sirro\Unreal Projects\AST_V1_18'
$d=Join-Path $p 'Saved/ChatGPT/VisualFoundation_Stage1_Checkpoint_20261126'
New-Item -ItemType Directory -Path $d -Force|Out-Null
$report=Join-Path $d 'final_verification.txt'
'CHECKPOINT=324423826fd2eab99b1ffcd59016df89a125822b'|Set-Content $report
$bad=0
$files=@(
    @('Source/Sirroh_Reimagineg_v3/Private/Debug/SirrohDebugSubsystem.cpp','1a2c8ad42c85aa7cee201c5e76ec6ab37c744d01'),
    @('Source/Sirroh_Reimagineg_v3/Private/POI/POIKitBlock.cpp','fc20bd97f42d0fa93ec6fce2102c80d02409f63a'),
    @('Source/Sirroh_Reimagineg_v3/Private/POI/POIShaft.cpp','12b427daef8e47d7e0090095a2905ef3a9fad4fe'),
    @('Source/Sirroh_Reimagineg_v3/Private/POI/PWGSpawnGeometryAudit.cpp','ea122b988ddc78571c13e7c73a5641d966d29b29'),
    @('Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2.cpp','d8ca53b341dfe3f604d481a3ab0634288c17e3d2'),
    @('Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2SemanticVerification.cpp','5b057acf62252f876feb9858bda95bd6a9809062'),
    @('Source/Sirroh_Reimagineg_v3/Private/PWG/PWGVisualProfileTests.cpp','e474d52571e3c5bcd5d7e8744e4999958851f454'),
    @('Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2AuthoredPOI.cpp','aac8aff5eddeb6114970a4af5ec3e9868e6313e6'),
    @('Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Debug.cpp','663a3014f43cf4fb2882777845e42388da6b1168'),
    @('Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Decorations.cpp','d4e0de89466fed8c7a9741f72a916886167d6a15'),
    @('Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Generator.cpp','7043038aced6ff3a3f7e8131ea38d5a03e1a41c2'),
    @('Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Underground.cpp','6dd15301a5e6d2589f91b61f4d37c39df7afd4e6'),
    @('Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2VisualMasks.cpp','47307a327f76e35a73e6b8849fa736bf97f126c8'),
    @('Source/Sirroh_Reimagineg_v3/Public/POI/POIKitBlock.h','2cd22e5d48fd39e736030e8f3d87e6c85f33013f'),
    @('Source/Sirroh_Reimagineg_v3/Public/POI/POIShaft.h','f81a117c213c54c1f4a336e6764ea32b375994c1'),
    @('Source/Sirroh_Reimagineg_v3/Public/PWG/PWGEnvironmentV2.h','239cd90b543a4924b3412744709a9ec41996b57f'),
    @('Source/Sirroh_Reimagineg_v3/Public/Procedural_World_Generator/PWGv2Generator.h','56e3b39adfface403ad101983a9e6e785d050707'),
    @('Source/Sirroh_Reimagineg_v3/Sirroh_Reimagineg_v3.Build.cs','6f3091ec9a1692e2b00ab9e8ac476e55977de0f4')
)
foreach($x in $files){
    $f=Join-Path $p $x[0]
    if(!(Test-Path $f)){$bad++; "MISSING $($x[0])"|Add-Content $report;continue}
    $hash=(& git hash-object --no-filters $f).Trim()
    if($LASTEXITCODE -ne 0){throw "hash error: $f"}
    if($hash -ne $x[1]){$bad++; "MISMATCH $($x[0]) actual=$hash expected=$($x[1])"|Add-Content $report}
    else{"OK $($x[0]) $hash"|Add-Content $report}
}
$assets='M_PWGv2_DepthAtmosphere','M_PWGv2_DepthAtmosphere_V2','M_PWGv2_DepthAtmosphere_Semantic','M_PWGv2_DepthAtmosphere_SemanticPOI'
$root='Content/Systems/Procedural_World_Generator/PWG_v2'
foreach($a in $assets){
    $f=Join-Path $p "$root/$a.uasset"
    if(!(Test-Path $f)){$bad++; "MISSING_ASSET $a"|Add-Content $report;continue}
    $h=(Get-FileHash $f -Algorithm SHA256).Hash
    $dest=Join-Path $d "$a.uasset"
    if(Test-Path $dest){if((Get-FileHash $dest).Hash -ne $h){throw "Existing checkpoint asset differs $a"}}
    else{Copy-Item $f $dest}
    if((Get-FileHash $dest).Hash -ne $h){throw "Checkpoint asset backup mismatch $a"}
    "ASSET $a $h"|Add-Content $report
    if($a -eq 'M_PWGv2_DepthAtmosphere_SemanticPOI' -and $h -ne '5ECFB7DB0CF8BF1F9F059B4369D8EE0B31025DE033CFC6F6628F5AB69C90807E'){$bad++}
}
"SOURCE_FILES=$($files.Count)"|Add-Content $report
"MISMATCHES=$bad"|Add-Content $report
Get-Content $report
if($bad -ne 0){exit 1}
'CHECKPOINT_VERIFICATION=PASS'|Add-Content $report
Write-Output 'CHECKPOINT_VERIFICATION=PASS'
