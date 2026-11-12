param([string]$Runtime = "$PSScriptRoot/../artifacts/ui-runtime/DroidMediaLink.exe", [string]$OutputDirectory = "$PSScriptRoot/../artifacts/ui-review")
$ErrorActionPreference='Stop'
$root=(Resolve-Path "$PSScriptRoot/..").Path
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$output=(Resolve-Path $OutputDirectory).Path
$runtimePath=(Resolve-Path $Runtime).Path
$cases=@(
 @{name='warm-dark';cover='warm';size='1200x780'},
 @{name='cool-dark';cover='cool';size='1120x760'},
 @{name='saturated-dark';cover='saturated';size='1200x780'},
 @{name='monochrome-dark';cover='monochrome';size='900x760'},
 @{name='pale-light';cover='pale';size='1120x780';light=$true},
 @{name='narrow-light';cover='cool';size='560x720';light=$true},
 @{name='minimum';cover='saturated';size='560x520'},
 @{name='settings-dark';cover='cool';size='1100x760';page='2'},
 @{name='settings-light';cover='pale';size='800x700';page='2';light=$true},
 @{name='settings-appearance';cover='saturated';size='700x760';page='2';category='4'},
 @{name='devices-search';cover='cool';size='1000x760';page='1';dialog='discovery'},
 @{name='floating';cover='saturated';size='1120x760';surface='floating'},
 @{name='tray';cover='warm';size='1120x760';surface='tray'}
)
$env:QT_FORCE_STDERR_LOGGING='1'
foreach($case in $cases) {
 $profile=Join-Path $output "profiles/$($case.name)"
 $arguments=@('--test-profile',$profile,'--test-style','liquid','--preview-cover',"$root/artifacts/cover-tests/$($case.cover).jpg",'--size',$case.size,'--capture',"$output/$($case.name).png",'--capture-delay','1800')
 if($case.light){$arguments+='--test-light'}
 if($case.page){$arguments+=@('--page',$case.page)}
 if($case.category){$arguments+=@('--category',$case.category)}
 if($case.dialog){$arguments+=@('--dialog',$case.dialog,'--discover')}
 if($case.surface){$arguments+=@('--surface',$case.surface)}
 $process=Start-Process -FilePath $runtimePath -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru -RedirectStandardError "$output/$($case.name).log"
 if($process.ExitCode -ne 0){throw "Capture failed: $($case.name), exit $($process.ExitCode)"}
 Write-Output "Captured $($case.name)"
}
