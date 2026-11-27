<#
    Smoke/regression tests for Heart of The Alien Redux.

    Runs the Windows build headlessly (SDL dummy video/audio) against the
    game data and the recorded key sequences in this directory.

    Usage:
        powershell -ExecutionPolicy Bypass -File tests\run-tests.ps1
        powershell -ExecutionPolicy Bypass -File tests\run-tests.ps1 -Exe build\alien.exe

    Exit code is 0 when every test passes, 1 otherwise.
#>
param(
    [string]$Exe = (Join-Path $PSScriptRoot '..\build\alien.exe'),
    [string]$DataDir = (Join-Path $PSScriptRoot '..\Heart Of The Alien (U)'),
    [int]$ReplaySeconds = 8,
    [int]$GameOverSeconds = 30,
    [int]$IdleSeconds = 2
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Exe)) {
    Write-Error "executable not found: $Exe"
}
$Exe = (Resolve-Path -LiteralPath $Exe).Path
if (-not (Test-Path -LiteralPath $DataDir)) {
    Write-Error "game data directory not found: $DataDir"
}
$DataDir = (Resolve-Path -LiteralPath $DataDir).Path

$work = Join-Path $PSScriptRoot 'work'
$artifacts = Join-Path $PSScriptRoot 'artifacts'
New-Item -ItemType Directory -Force -Path $work | Out-Null
New-Item -ItemType Directory -Force -Path $artifacts | Out-Null

# Run in a private directory so test runs never overwrite the player's
# recorded-keys or quicksave. Link the game data from the data directory.
function Initialize-WorkDir {
    $iso = Join-Path $DataDir 'Heart Of The Alien (U).iso'
    if (-not (Test-Path -LiteralPath $iso)) {
        Write-Error "ISO not found: $iso"
    }

    $files = @($iso) + @(Get-ChildItem -LiteralPath $DataDir -Filter '*.mp3' | ForEach-Object { $_.FullName })
    foreach ($source in $files) {
        $target = Join-Path $work (Split-Path -Leaf $source)
        if (Test-Path -LiteralPath $target) {
            continue
        }
        try {
            New-Item -ItemType HardLink -Path $target -Target $source | Out-Null
        } catch {
            Copy-Item -LiteralPath $source -Destination $target
        }
    }
}

function Read-LogFile {
    param([string]$Path)

    for ($i = 0; $i -lt 10; $i++) {
        try {
            if (-not (Test-Path -LiteralPath $Path)) { return '' }
            return (Get-Content -LiteralPath $Path -ErrorAction Stop) -join "`n"
        } catch [System.IO.IOException] {
            Start-Sleep -Milliseconds 100
        }
    }
    return ''
}

function Quote-Argument {
    param([string]$Text)

    if ($Text -match '\s') {
        return '"' + $Text + '"'
    }

    return $Text
}

function Invoke-Game {
    param(
        [string[]]$Arguments,
        [int]$TimeoutSeconds
    )

    $stdout = Join-Path $artifacts 'stdout.log'
    $stderr = Join-Path $artifacts 'stderr.log'
    Remove-Item -LiteralPath $stdout, $stderr -ErrorAction SilentlyContinue

    $argLine = ($Arguments | ForEach-Object { Quote-Argument $_ }) -join ' '

    $process = Start-Process -FilePath $Exe -ArgumentList $argLine `
        -WorkingDirectory $work -PassThru `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr

    $exited = $process.WaitForExit($TimeoutSeconds * 1000)
    if (-not $exited) {
        Stop-Process -Id $process.Id -Force
    }
    $process.WaitForExit()
    $process.Refresh()

    # give the killed process a moment to release its redirected streams,
    # otherwise the tail of stderr can still be missing here
    Start-Sleep -Milliseconds 250

    [pscustomobject]@{
        Exited    = $exited
        ExitCode  = $process.ExitCode
        Arguments = ($Arguments -join ' ')
        StdOut    = (Read-LogFile $stdout)
        StdErr    = (Read-LogFile $stderr)
    }
}

function Assert-NoPanic {
    param($Result)
    if ($Result.StdOut -match 'panic|unknown opcode|UNKNOWN|Assertion') {
        throw "game reported an error:`n$($Result.StdOut)"
    }
    if ($Result.StdErr -match 'panic|unknown opcode|UNKNOWN|Assertion') {
        throw "game reported an error:`n$($Result.StdErr)"
    }
}

$failures = 0
$passes = 0

function Check {
    param([string]$Name, [scriptblock]$Body)

    Write-Host -NoNewline ("{0,-42}" -f $Name)
    try {
        & $Body
        $script:passes++
        Write-Host 'PASS' -ForegroundColor Green
    } catch {
        $script:failures++
        Write-Host 'FAIL' -ForegroundColor Red
        Write-Host ("    " + $_.Exception.Message) -ForegroundColor Red
    }
}

Initialize-WorkDir
$env:SDL_VIDEODRIVER = 'dummy'
$env:SDL_AUDIODRIVER = 'dummy'

Check '--help' {
    $r = Invoke-Game @('--help') 10
    if (-not $r.Exited) { throw 'did not exit' }
    if ($r.StdOut -notmatch 'Heart of The Alien Redux') { throw 'missing banner' }
    if ($r.StdOut -notmatch '--skip-password') { throw 'missing skip-password option' }
    if ($r.StdOut -notmatch '--no-skip-password') { throw 'missing no-skip-password option' }
    if ($r.StdOut -notmatch '--music-volume') { throw 'missing music-volume option' }
    if ($r.StdOut -notmatch '--sfx-volume') { throw 'missing sfx-volume option' }
    if ($r.StdOut -notmatch '--integer-scale') { throw 'missing integer-scale option' }
}

Check 'launcher --print-options' {
    $launcher = Join-Path $PSScriptRoot '..\play.ps1'
    if (-not (Test-Path -LiteralPath $launcher)) { throw 'play.ps1 is missing' }
    $out = & powershell -NoProfile -ExecutionPolicy Bypass -File $launcher --print-options 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { throw "launcher exited with $LASTEXITCODE" }
    if ($out -notmatch '"renderer"') { throw "no settings in output: $out" }
    if ($out -notmatch 'ARGS:') { throw "no argument line in output: $out" }
    if ($out -notmatch '--skip-password') { throw "missing default skip-password: $out" }
}

Check '--room validation (9 is invalid)' {
    $r = Invoke-Game @('--room', '9') 10
    if (-not $r.Exited) { throw 'did not exit' }
    if ($r.StdErr -notmatch 'invalid room') { throw "unexpected stderr: $($r.StdErr)" }
}

Check '--upscale validation (5 is invalid)' {
    $r = Invoke-Game @('--upscale', '5') 10
    if (-not $r.Exited) { throw 'did not exit' }
    if ($r.StdErr -notmatch 'invalid upscale') { throw "unexpected stderr: $($r.StdErr)" }
}

Check '--hd-smooth validation (101 is invalid)' {
    $r = Invoke-Game @('--hd-smooth', '101') 10
    if (-not $r.Exited) { throw 'did not exit' }
    if ($r.StdErr -notmatch 'invalid HD smoothing') { throw "unexpected stderr: $($r.StdErr)" }
}

Check '--replay without recorded-keys fails cleanly' {
    Remove-Item -LiteralPath (Join-Path $work 'recorded-keys') -ErrorAction SilentlyContinue
    $r = Invoke-Game @('--iso', '--room', '1', '--replay', '--fastest') 10
    if (-not $r.Exited) { throw 'did not exit' }
    if ($r.StdErr -notmatch 'recorded-keys') { throw "unexpected stderr: $($r.StdErr)" }
}

Check '--intro-test completes all animations' {
    $r = Invoke-Game @('--iso', '--intro-test', '--fastest') 120
    if (-not $r.Exited) { throw 'still running after 120s' }
    Assert-NoPanic $r
}

for ($room = 1; $room -le 8; $room++) {
    Check "sprite test room $room" {
        $r = Invoke-Game @('--iso', '--sprite-test', '--room', "$room", '--fastest') $IdleSeconds
        if ($r.Exited) { throw "exited early (code $($r.ExitCode))" }
        Assert-NoPanic $r
    }
}

for ($room = 1; $room -le 8; $room++) {
    Check "idle run room $room" {
        $r = Invoke-Game @('--iso', '--room', "$room", '--fastest') $IdleSeconds
        if ($r.Exited) { throw "exited early (code $($r.ExitCode))" }
        Assert-NoPanic $r
    }
}

for ($factor = 1; $factor -le 4; $factor++) {
    Check "upscaler ${factor}x" {
        $r = Invoke-Game @('--iso', '--room', '1', '--upscale', "$factor", '--fastest') $IdleSeconds
        if ($r.Exited) { throw "exited early (code $($r.ExitCode))" }
        Assert-NoPanic $r
    }
}

Check 'integer scaling' {
    $r = Invoke-Game @('--iso', '--room', '1', '--integer-scale', '--fastest', '--no-sound') $IdleSeconds
    if ($r.Exited) { throw "exited early (code $($r.ExitCode))" }
    Assert-NoPanic $r
}

Check 'volume options' {
    $r = Invoke-Game @('--iso', '--room', '1', '--music-volume', '50', '--sfx-volume', '50', '--fastest') $IdleSeconds
    if ($r.Exited) { throw "exited early (code $($r.ExitCode))" }
    Assert-NoPanic $r
}

Check '--debug logging in release' {
    $r = Invoke-Game @('--iso', '--room', '1', '--debug', '--fastest') $IdleSeconds
    if ($r.Exited) { throw "exited early (code $($r.ExitCode))" }
    if ($r.StdOut -notmatch 'loading ROOMS1') { throw 'no debug log output' }
    Assert-NoPanic $r
}

Check 'bilinear filter' {
    $r = Invoke-Game @('--iso', '--room', '1', '--filter', '--fastest') $IdleSeconds
    if ($r.Exited) { throw "exited early (code $($r.ExitCode))" }
    Assert-NoPanic $r
}

Check 'HD startup (falls back when no OpenGL)' {
    $r = Invoke-Game @('--iso', '--hd', '--room', '1', '--fastest') $IdleSeconds
    if ($r.Exited) { throw "exited early (code $($r.ExitCode))" }
    Assert-NoPanic $r
}

Check 'HD sharp shader startup' {
    $r = Invoke-Game @('--iso', '--hd', '--sharp', '--room', '1', '--fastest') $IdleSeconds
    if ($r.Exited) { throw "exited early (code $($r.ExitCode))" }
    Assert-NoPanic $r
}

Check 'HD smoothing startup' {
    $r = Invoke-Game @('--iso', '--hd', '--hd-smooth', '25', '--room', '1', '--fastest') $IdleSeconds
    if ($r.Exited) { throw "exited early (code $($r.ExitCode))" }
    Assert-NoPanic $r
}

Check 'keys.txt is created with defaults' {
    $keys = Join-Path $work 'keys.txt'
    Remove-Item -LiteralPath $keys -ErrorAction SilentlyContinue
    $r = Invoke-Game @('--iso', '--room', '1', '--fastest') $IdleSeconds
    if (-not (Test-Path -LiteralPath $keys)) { throw 'keys.txt was not created' }
    $content = Get-Content -LiteralPath $keys -Raw
    if ($content -notmatch 'screenshot = f12') { throw 'default screenshot binding missing' }
    if ($content -notmatch 'fullscreen = alt\+enter') { throw 'default fullscreen binding missing' }
}

Check 'custom keys file is accepted' {
    $keys = Join-Path $work 'custom-keys.txt'
    @('screenshot = f9', 'quit = f10', 'move up = enter', 'fullscreen = alt+return') |
        Set-Content -LiteralPath $keys -Encoding ASCII
    $r = Invoke-Game @('--check-keys', '--keys', 'custom-keys.txt') 10
    if (-not $r.Exited) { throw 'did not exit' }
    if ($r.StdErr -match 'unknown') { throw "unexpected warnings: $($r.StdErr)" }
    $r = Invoke-Game @('--iso', '--room', '1', '--keys', 'custom-keys.txt', '--fastest') $IdleSeconds
    if ($r.Exited) { throw "exited early (code $($r.ExitCode))" }
    Assert-NoPanic $r
    Remove-Item -LiteralPath $keys -ErrorAction SilentlyContinue
}

Check 'broken keys file only warns' {
    $keys = Join-Path $work 'broken-keys.txt'
    @('no such action = f1', 'move up = not-a-key', 'screenshot = f12') |
        Set-Content -LiteralPath $keys -Encoding ASCII
    $r = Invoke-Game @('--check-keys', '--keys', 'broken-keys.txt') 10
    if (-not $r.Exited) { throw 'did not exit' }
    if ($r.StdErr -notmatch "unknown action 'no such action'") { throw "missing action warning: $($r.StdErr)" }
    if ($r.StdErr -notmatch "unknown key name 'not-a-key'") { throw "missing key warning: $($r.StdErr)" }
    Remove-Item -LiteralPath $keys -ErrorAction SilentlyContinue
}

Check 'UTF-8 game data path' {
    $folder = Join-Path $work 'Coração'
    New-Item -ItemType Directory -Force -Path $folder | Out-Null
    $target = Join-Path $folder 'Coração.iso'
    if (-not (Test-Path -LiteralPath $target)) {
        $source = Join-Path $DataDir 'Heart Of The Alien (U).iso'
        try {
            New-Item -ItemType HardLink -Path $target -Target $source | Out-Null
        } catch {
            Copy-Item -LiteralPath $source -Destination $target
        }
    }
    $r = Invoke-Game @('--iso', (Join-Path $folder 'Coração'), '--room', '1', '--fastest') $IdleSeconds
    if ($r.Exited) { throw "exited early (code $($r.ExitCode))" }
    Assert-NoPanic $r
}

Check 'smooth Scale2x/3x upscaling' {
    $r = Invoke-Game @('--iso', '--room', '1', '--smooth', '--upscale', '4', '--fastest') $IdleSeconds
    if ($r.Exited) { throw "exited early (code $($r.ExitCode))" }
    Assert-NoPanic $r
}

$replays = @(
    @{ Name = 'level1'; Room = 1 },
    @{ Name = 'level2'; Room = 2 },
    @{ Name = 'boss';   Room = 7 },
    @{ Name = 'gameover-start'; Room = 1 }
)

foreach ($replay in $replays) {
    Check "recorded keys: $($replay.Name)" {
        $source = Join-Path $PSScriptRoot "recorded-keys.$($replay.Name)"
        if (-not (Test-Path -LiteralPath $source)) { throw "missing $source" }
        Copy-Item -LiteralPath $source -Destination (Join-Path $work 'recorded-keys') -Force

        $r = Invoke-Game @('--iso', '--room', "$($replay.Room)", '--replay', '--fastest') $ReplaySeconds
        if ($r.Exited) { throw "exited early (code $($r.ExitCode))" }
        Assert-NoPanic $r
    }
}

Check 'game over restarts the level' {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'recorded-keys.gameover-start') -Destination (Join-Path $work 'recorded-keys') -Force

    $r = Invoke-Game @('--iso', '--room', '1', '--replay', '--fastest', '--no-sound', '--debug') $GameOverSeconds
    if ($r.Exited) { throw "exited early (code $($r.ExitCode))" }
    $loads = [regex]::Matches($r.StdOut, 'loading room (\d+)') | ForEach-Object { $_.Groups[1].Value }
    if (($loads -join ',') -notmatch '^1(,\d+)*,1$') { throw "level was not restarted (rooms: $($loads -join ','))" }
    Assert-NoPanic $r
}

Check 'game over shows the password screen with --no-skip-password' {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'recorded-keys.gameover-start') -Destination (Join-Path $work 'recorded-keys') -Force

    $r = Invoke-Game @('--iso', '--room', '1', '--replay', '--fastest', '--no-sound', '--debug', '--no-skip-password') $GameOverSeconds
    if ($r.Exited) { throw "exited early (code $($r.ExitCode))" }
    if ($r.StdOut -notmatch 'loading room 7') { throw 'password screen was not loaded' }
    Assert-NoPanic $r
}

Write-Host ''
Write-Host ("{0} passed, {1} failed" -f $passes, $failures)
if ($failures -ne 0) {
    exit 1
}
exit 0
