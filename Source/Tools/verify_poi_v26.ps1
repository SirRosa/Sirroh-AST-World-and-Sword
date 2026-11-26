$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path.Replace('\','/')
$scriptRoot = $PSScriptRoot.Replace('\','/')
$editor = 'D:/Sirro/UE 5.8.1/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$project = "$projectRoot/AST_V1_18.uproject"
$logs = "$projectRoot/Saved/POICheckpoints"
& $editor $project -unattended -NullRHI -nosound -run=pythonscript "-script=$scriptRoot/fix_push_v26.py" "-abslog=$logs/Assets-v26.log" *> "$logs/Assets-v26-stdout.log"
if ($LASTEXITCODE -ne 0) { throw 'Asset update failed. See Assets-v26.log.' }
if (!(Select-String -Path "$logs/Assets-v26.log" -SimpleMatch 'PUSH_V26_ASSETS_READY' -Quiet)) { throw 'Missing asset completion marker.' }
& $editor $project /Engine/Maps/Entry -unattended -NullRHI -nosound '-ExecCmds=Automation RunTests Sirroh.POI.PushSupport+Sirroh.POI.MovingTraversal' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$logs/POI-v26-tests" "-abslog=$logs/Tests-v26.log" *> "$logs/Tests-v26-stdout.log"
if ($LASTEXITCODE -ne 0) { throw 'Automation process failed. See Tests-v26.log.' }
$completed = Select-String -Path "$logs/Tests-v26.log" -Pattern 'Test Completed. Result=\{Success\}'
if (@($completed).Count -ne 2 -or (Select-String -Path "$logs/Tests-v26.log" -Pattern 'Test Completed. Result=\{Fail\}' -Quiet)) { throw 'Both POI tests must pass; process exit code alone is insufficient.' }
$completed.Line
Select-String -Path "$logs/Tests-v26.log" -Pattern 'physical mass' | ForEach-Object { $_.Line }
