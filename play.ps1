# =====================================================================
#  Heart of The Alien Redux 2.0.3 - setup / configuration screen
#
#  A small BIOS/DOS-style menu for the Windows launcher. It shows the
#  current options next to their defaults, lets the player change them
#  and then starts the game. Settings are kept in play.cfg (JSON) next
#  to this file.
#
#  Normally started by play.bat:
#      play.bat                 open this screen
#      play.bat --setup         open this screen
#      play.bat --fullscreen    start immediately with extra options
# =====================================================================

param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $GameArgs = @()
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $root

$cfgPath = Join-Path $root 'play.cfg'

# ---------------------------------------------------------------------
#  Locate the executable
# ---------------------------------------------------------------------
$exe = Join-Path $root 'alien.exe'
if (-not (Test-Path -LiteralPath $exe)) {
    $built = Join-Path $root 'build\alien.exe'
    if (Test-Path -LiteralPath $built) {
        $exe = $built
    }
    else {
        Write-Host ''
        Write-Host 'ERROR: alien.exe was not found.' -ForegroundColor Red
        Write-Host 'Keep play.bat and play.ps1 next to alien.exe, or build the game'
        Write-Host 'first so that build\alien.exe exists.'
        Write-Host ''
        exit 1
    }
}

# ---------------------------------------------------------------------
#  Settings
# ---------------------------------------------------------------------
$script:defaults = [ordered]@{
    renderer   = 'hd'
    quality    = 'omniscale'
    hdsmooth   = 30
    upscale    = 0
    smooth     = 'sharp'
    filter     = 'off'
    scale      = 0
    fullscreen = 'off'
    sound      = 'on'
    iso        = 'Heart Of The Alien (U)'
    music      = ''
    keyfile    = 'keys.txt'
    room       = 0
    skippassword = 'on'
}

$script:cfg = [ordered]@{}
foreach ($key in $script:defaults.Keys) {
    $script:cfg[$key] = $script:defaults[$key]
}

$script:message = ''
$script:extraArgs = @()

# Split --setup from the rest. Without it, command line arguments start
# the game immediately, exactly like the old play.bat did.
$forceMenu = $false
foreach ($arg in $GameArgs) {
    if ($arg -eq '--setup') {
        $forceMenu = $true
    }
    else {
        $script:extraArgs += $arg
    }
}

# ---------------------------------------------------------------------
#  The rows shown in the menu
# ---------------------------------------------------------------------
$script:fields = @(
    [pscustomobject]@{
        Key = 'renderer'; Label = 'Renderer'; Kind = 'choice'
        Values = @('hd', 'sdl'); Names = @('HD (OpenGL)', 'SDL (classic)')
        Help = 'HD uses the OpenGL shader; SDL is the classic pre-scaled output'
    },
    [pscustomobject]@{
        Key = 'quality'; Label = 'HD filter'; Kind = 'choice'
        Values = @('omniscale', 'nearest'); Names = @('Omniscale', 'Sharp nearest')
        Help = 'Omniscale removes the diagonal stair steps; nearest is pixel-perfect'
    },
    [pscustomobject]@{
        Key = 'hdsmooth'; Label = 'HD smoothing'; Kind = 'number'
        Min = 0; Max = 100; Step = 5
        Help = 'Omniscale strength: 0 = sharp, 30 = default, 100 = full smoothing'
    },
    [pscustomobject]@{
        Key = 'upscale'; Label = 'Classic upscaling'; Kind = 'choice'
        Values = @(0, 1, 2, 3, 4); Names = @('Auto', '1x', '2x', '3x', '4x')
        Help = 'Internal pixel size for the classic SDL renderer'
    },
    [pscustomobject]@{
        Key = 'smooth'; Label = 'Classic smoothing'; Kind = 'choice'
        Values = @('sharp', 'smooth'); Names = @('Sharp', 'Scale2x/3x')
        Help = 'Scale2x/Scale3x smooths the classic renderer'
    },
    [pscustomobject]@{
        Key = 'filter'; Label = 'Classic filtering'; Kind = 'choice'
        Values = @('off', 'on'); Names = @('Off', 'Bilinear')
        Help = 'Hardware bilinear filtering for the classic renderer'
    },
    [pscustomobject]@{
        Key = 'scale'; Label = 'Window scale'; Kind = 'choice'
        Values = @(0, 2, 3, 4, 5, 6); Names = @('Auto', '2x', '3x', '4x', '5x', '6x')
        Help = 'Window size; Auto fits the desktop'
    },
    [pscustomobject]@{
        Key = 'fullscreen'; Label = 'Fullscreen'; Kind = 'choice'
        Values = @('off', 'on'); Names = @('No', 'Yes')
        Help = 'Start in fullscreen; Alt+Enter toggles it in game'
    },
    [pscustomobject]@{
        Key = 'sound'; Label = 'Sound'; Kind = 'choice'
        Values = @('on', 'off'); Names = @('On', 'Off')
        Help = 'Turn CD music and sound effects off'
    },
    [pscustomobject]@{
        Key = 'iso'; Label = 'ISO prefix'; Kind = 'text'
        Help = 'Game data: <prefix>.iso or <prefix>\<prefix>.iso'
    },
    [pscustomobject]@{
        Key = 'music'; Label = 'Music prefix'; Kind = 'text'
        Help = 'Empty = same as the ISO prefix; files are <prefix> NN.mp3'
    },
    [pscustomobject]@{
        Key = 'keyfile'; Label = 'Key bindings'; Kind = 'text'
        Help = 'Bindings file (keys.txt is created with the defaults)'
    },
    [pscustomobject]@{
        Key = 'room'; Label = 'Start room'; Kind = 'choice'
        Values = @(0, 1, 2, 3, 4, 5, 6, 7, 8)
        Names = @('Normal flow', 'Room 1', 'Room 2', 'Room 3', 'Room 4', 'Room 5', 'Room 6', 'Room 7', 'Room 8')
        Help = 'Developer option: skip the intro and start in a room'
    },
    [pscustomobject]@{
        Key = 'skippassword'; Label = 'Skip password'; Kind = 'choice'
        Values = @('off', 'on'); Names = @('No', 'Yes')
        Help = 'Normal flow: skip the code-entry screen after the intro and start room 1'
    }
)

$script:actions = @(
    [pscustomobject]@{ Key = '__start'; Label = 'START GAME' },
    [pscustomobject]@{ Key = '__reset'; Label = 'RESET DEFAULTS' },
    [pscustomobject]@{ Key = '__exit';  Label = 'SAVE AND EXIT' }
)

$script:selected = 0

# ---------------------------------------------------------------------
#  Helpers
# ---------------------------------------------------------------------
function Get-Display {
    param($Field, $Config)

    $value = $Config[$Field.Key]

    if ($Field.Kind -eq 'choice') {
        $index = [array]::IndexOf($Field.Values, $value)
        if ($index -lt 0) { $index = 0 }
        return $Field.Names[$index]
    }

    if ($Field.Kind -eq 'number') {
        return [string]$value
    }

    if ($null -eq $value -or [string]::IsNullOrEmpty([string]$value)) {
        if ($Field.Key -eq 'music') { return '(same as ISO)' }
        if ($Field.Key -eq 'keyfile')  { return '(keys.txt)' }
        return '(default)'
    }

    return [string]$value
}

function Convert-IsoInput {
    param([string]$Text)

    $Text = $Text.Trim().Trim('"')
    if ([string]::IsNullOrWhiteSpace($Text)) {
        return $Text
    }

    if ((Test-Path -LiteralPath $Text -PathType Leaf) -and ($Text -match '(?i)\.iso$')) {
        return $Text.Substring(0, $Text.Length - 4)
    }

    if (Test-Path -LiteralPath $Text -PathType Container) {
        $found = @(Get-ChildItem -LiteralPath $Text -Filter '*.iso' -File -ErrorAction SilentlyContinue)
        if ($found.Count -ge 1) {
            return (Join-Path $Text $found[0].BaseName)
        }
    }

    if ($Text -match '(?i)\.iso$') {
        return $Text.Substring(0, $Text.Length - 4)
    }

    return $Text
}

function Get-Fit {
    param([string]$Text, [int]$Width)

    if ($null -eq $Text) {
        $Text = ''
    }

    if ($Text.Length -le $Width) {
        return $Text
    }

    if ($Width -le 3) {
        return $Text.Substring(0, $Width)
    }

    return $Text.Substring(0, $Width - 3) + '...'
}

function Get-IsoPath {
    param($Config)

    $prefix = [string]$Config['iso']
    if ([string]::IsNullOrWhiteSpace($prefix)) {
        $prefix = [string]$script:defaults['iso']
    }

    $prefix = $prefix.Trim().Trim('"')
    if ($prefix -match '(?i)\.iso$') {
        $prefix = $prefix.Substring(0, $prefix.Length - 4)
    }

    $candidates = @("$prefix.iso")
    if ($prefix -notmatch '[\\/]') {
        $candidates += (Join-Path $prefix "$prefix.iso")
    }

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    return $null
}

function Normalize-Config {
    foreach ($field in $script:fields) {
        $value = $script:cfg[$field.Key]

        if ($field.Kind -eq 'choice') {
            $index = [array]::IndexOf($field.Values, $value)
            if ($index -lt 0) {
                $script:cfg[$field.Key] = $script:defaults[$field.Key]
            }
        }
        elseif ($field.Kind -eq 'number') {
            $number = 0
            if (-not [int]::TryParse([string]$value, [ref]$number)) {
                $number = [int]$script:defaults[$field.Key]
            }
            if ($number -lt $field.Min) { $number = $field.Min }
            if ($number -gt $field.Max) { $number = $field.Max }
            $script:cfg[$field.Key] = $number
        }
        elseif ($null -eq $value) {
            $script:cfg[$field.Key] = $script:defaults[$field.Key]
        }
        else {
            $script:cfg[$field.Key] = ([string]$value).Trim()
        }
    }

    $script:cfg['iso'] = Convert-IsoInput ([string]$script:cfg['iso'])
}

function Load-Config {
    if (-not (Test-Path -LiteralPath $cfgPath)) {
        return
    }

    try {
        $saved = Get-Content -LiteralPath $cfgPath -Raw | ConvertFrom-Json
        foreach ($key in $script:defaults.Keys) {
            $value = $saved.$key
            if ($null -ne $value) {
                $script:cfg[$key] = $value
            }
        }
        Normalize-Config
    }
    catch {
        $script:message = 'play.cfg could not be read; using the defaults.'
        foreach ($key in $script:defaults.Keys) {
            $script:cfg[$key] = $script:defaults[$key]
        }
    }
}

function Save-Config {
    try {
        $script:cfg | ConvertTo-Json | Set-Content -LiteralPath $cfgPath -Encoding UTF8
    }
    catch {
        $script:message = "Could not save play.cfg: $($_.Exception.Message)"
    }
}

function Reset-Config {
    foreach ($key in $script:defaults.Keys) {
        $script:cfg[$key] = $script:defaults[$key]
    }
    $script:message = 'Defaults restored. Press S or Enter on START GAME when ready.'
}

function Change-Value {
    param($Field, [int]$Direction)

    if ($Field.Kind -eq 'choice') {
        $index = [array]::IndexOf($Field.Values, $script:cfg[$Field.Key])
        if ($index -lt 0) { $index = 0 }

        $index = $index + $Direction
        if ($index -lt 0) { $index = $Field.Values.Count - 1 }
        if ($index -ge $Field.Values.Count) { $index = 0 }

        $script:cfg[$Field.Key] = $Field.Values[$index]
        $script:message = "$($Field.Label): $(Get-Display $Field $script:cfg)"
        return
    }

    if ($Field.Kind -eq 'number') {
        $number = [int]$script:cfg[$Field.Key] + ($Direction * $Field.Step)
        if ($number -lt $Field.Min) { $number = $Field.Max }
        if ($number -gt $Field.Max) { $number = $Field.Min }

        $script:cfg[$Field.Key] = $number
        $script:message = "$($Field.Label): $number"
        return
    }
}

function Read-TextValue {
    param($Field)

    Write-Host ''
    Write-Host "  $($Field.Label)" -ForegroundColor Yellow
    Write-Host "  $($Field.Help)" -ForegroundColor Gray
    Write-Host "  Current: $(Get-Display $Field $script:cfg)" -ForegroundColor Gray
    Write-Host '  Enter a new value, or just press Enter to keep it.' -ForegroundColor Gray
    Write-Host '  Type "default" to restore the built-in default.' -ForegroundColor DarkGray
    Write-Host ''
    Write-Host -NoNewline '  > ' -ForegroundColor Cyan

    $entered = Read-Host
    if ($null -eq $entered) {
        return
    }

    $entered = $entered.Trim().Trim('"')
    if ($entered -eq '') {
        $script:message = "$($Field.Label): unchanged"
        return
    }

    if ($entered -eq 'default') {
        $script:cfg[$Field.Key] = $script:defaults[$Field.Key]
        $script:message = "$($Field.Label): restored the default"
        return
    }

    if ($Field.Key -eq 'iso') {
        $entered = Convert-IsoInput $entered
    }

    $script:cfg[$Field.Key] = $entered
    $script:message = "$($Field.Label): $entered"
}

function Build-Args {
    $result = New-Object System.Collections.Generic.List[string]
    $config = $script:cfg

    if ($config['renderer'] -eq 'hd') {
        $result.Add('--hd')
        if ($config['quality'] -eq 'nearest') {
            $result.Add('--sharp')
        }
        $result.Add('--hd-smooth')
        $result.Add([string]$config['hdsmooth'])
    }
    else {
        $result.Add('--no-hd')
        if ([int]$config['upscale'] -ne 0) {
            $result.Add('--upscale')
            $result.Add([string]$config['upscale'])
        }
        if ($config['smooth'] -eq 'smooth') {
            $result.Add('--smooth')
        }
        if ($config['filter'] -eq 'on') {
            $result.Add('--filter')
        }
    }

    if ([int]$config['scale'] -ne 0) {
        $result.Add('--scale')
        $result.Add([string]$config['scale'])
    }

    if ($config['fullscreen'] -eq 'on') {
        $result.Add('--fullscreen')
    }

    if ($config['sound'] -eq 'off') {
        $result.Add('--no-sound')
    }

    $iso = [string]$config['iso']
    if (-not [string]::IsNullOrWhiteSpace($iso)) {
        $result.Add('--iso')
        $result.Add($iso.Trim().Trim('"'))
    }

    $music = [string]$config['music']
    if (-not [string]::IsNullOrWhiteSpace($music)) {
        $result.Add('--music')
        $result.Add($music.Trim().Trim('"'))
    }

    $keyfile = [string]$config['keyfile']
    if (-not [string]::IsNullOrWhiteSpace($keyfile) -and $keyfile -ne 'keys.txt') {
        $result.Add('--keys')
        $result.Add($keyfile.Trim().Trim('"'))
    }

    if ([int]$config['room'] -ne 0) {
        $result.Add('--room')
        $result.Add([string]$config['room'])
    }

    if ($config['skippassword'] -eq 'off') {
        $result.Add('--no-skip-password')
    }
    else {
        $result.Add('--skip-password')
    }

    foreach ($extra in $script:extraArgs) {
        $result.Add($extra)
    }

    return $result.ToArray()
}

function Start-Game {
    $isoPath = Get-IsoPath $script:cfg
    if ($null -eq $isoPath) {
        Write-Host ''
        Write-Host '  The game ISO was not found.' -ForegroundColor Yellow
        Write-Host "  Looked for: $($script:cfg['iso']).iso" -ForegroundColor Gray
        if ($script:cfg['iso'] -notmatch '[\\/]') {
            Write-Host "          and: $($script:cfg['iso'])\$($script:cfg['iso']).iso" -ForegroundColor Gray
        }
        Write-Host ''
        $answer = Read-Host '  Start the game anyway? (y/N)'
        if ($answer -notmatch '^(?i)y') {
            $script:message = 'Set the ISO prefix to the folder containing your ISO.'
            return
        }
    }

    Save-Config

    $gameArgs = Build-Args
    Write-Host ''
    Write-Host "  Starting: $exe" -ForegroundColor Green
    if ($gameArgs.Count -gt 0) {
        Write-Host "  Options : $($gameArgs -join ' ')" -ForegroundColor DarkGray
    }
    Write-Host ''

    [Console]::CursorVisible = $true
    $code = 0
    try {
        & $exe @gameArgs
        $code = $LASTEXITCODE
    }
    catch {
        Write-Host "  Could not start the game: $($_.Exception.Message)" -ForegroundColor Red
        $code = 1
    }

    Write-Host ''
    if ($code -ne 0) {
        Write-Host "  The game exited with code $code." -ForegroundColor Yellow
    }
    else {
        Write-Host '  The game has ended.' -ForegroundColor Gray
    }
    Write-Host '  Press any key to return to the setup screen, or Esc to quit.' -ForegroundColor Gray

    $key = [Console]::ReadKey($true)
    [Console]::CursorVisible = $false
    if ($key.Key -eq [ConsoleKey]::Escape) {
        exit 0
    }

    $script:message = 'Settings are still available. Press S to play again.'
}

function Show-Menu {
    $width = 78
    $line = '+' + ('-' * ($width - 2)) + '+'

    Clear-Host

    Write-Host $line -ForegroundColor DarkCyan
    Write-Host ('| {0,-76} |' -f ' HEART OF THE ALIEN REDUX 2.0.3  -  SETUP / CONFIGURATION') -ForegroundColor White
    Write-Host ('| {0,-76} |' -f ' The options below are used the next time the game starts.') -ForegroundColor Gray
    Write-Host $line -ForegroundColor DarkCyan

    Write-Host ('  {0,-2} {1,-26} {2,-21} {3,-21}' -f '', 'OPTION', 'CURRENT', 'DEFAULT') -ForegroundColor DarkGray
    Write-Host ('  ' + ('-' * 73)) -ForegroundColor DarkGray

    $index = 0
    foreach ($field in $script:fields) {
        $selected = ($index -eq $script:selected)
        $label = Get-Fit $field.Label 26
        $current = Get-Fit (Get-Display $field $script:cfg) 21
        $default = Get-Fit (Get-Display $field $script:defaults) 21

        # Mark options that have no effect for the selected renderer.
        $dim = $false
        if ($script:cfg['renderer'] -eq 'sdl') {
            if ($field.Key -in @('quality', 'hdsmooth')) { $dim = $true }
        }
        else {
            if ($field.Key -in @('upscale', 'smooth', 'filter')) { $dim = $true }
        }

        $text = '  {0} {1,-26} {2,-21} {3,-21}' -f $(if ($selected) { '>' } else { ' ' }), $label, $current, $default

        if ($selected) {
            Write-Host ('  > ' + ('{0,-26} {1,-21} {2,-21}' -f $label, $current, $default)) -ForegroundColor Black -BackgroundColor Cyan
        }
        elseif ($dim) {
            Write-Host $text -ForegroundColor DarkGray
        }
        else {
            Write-Host $text -ForegroundColor Gray
        }

        $index++
    }

    Write-Host ('  ' + ('-' * 73)) -ForegroundColor DarkGray

    foreach ($action in $script:actions) {
        $selected = ($index -eq $script:selected)
        if ($selected) {
            Write-Host ('  > {0,-72}' -f $action.Label) -ForegroundColor Black -BackgroundColor Cyan
        }
        elseif ($action.Key -eq '__start') {
            Write-Host ('    {0,-72}' -f $action.Label) -ForegroundColor Green
        }
        else {
            Write-Host ('    {0,-72}' -f $action.Label) -ForegroundColor Gray
        }
        $index++
    }

    Write-Host $line -ForegroundColor DarkCyan

    $isoPath = Get-IsoPath $script:cfg
    if ($null -eq $isoPath) {
        Write-Host '  ISO: NOT FOUND - use the ISO prefix row to point at your game data.' -ForegroundColor Yellow
    }
    else {
        Write-Host "  ISO: $isoPath" -ForegroundColor DarkGreen
    }

    Write-Host '  Arrows: move/change   Enter: change or activate   S: start   R: defaults   Esc: exit' -ForegroundColor DarkGray
    Write-Host ('  {0,-76}' -f $script:message) -ForegroundColor Cyan | Out-Host
}

# ---------------------------------------------------------------------
#  Non-interactive fallback (run from a script or with redirected input)
# ---------------------------------------------------------------------
Load-Config

# Developer/test helper: show what would be launched and exit.
if ($GameArgs -contains '--print-options') {
    $script:extraArgs = @($script:extraArgs | Where-Object { $_ -ne '--print-options' })
    $script:cfg | ConvertTo-Json
    Write-Host ('ARGS: ' + ((Build-Args) -join ' '))
    exit 0
}

if ($GameArgs -contains '--save-options') {
    Save-Config
    exit 0
}

if ($GameArgs -contains '--show-menu') {
    $script:message = 'Welcome. Press S or Enter on START GAME to play.'
    Show-Menu
    exit 0
}

# Command line arguments launch immediately, like the old launcher.
# No arguments (or --setup) always opens the setup screen, even when the
# console input is redirected by a terminal or a script.
if ($script:extraArgs.Count -gt 0 -and -not $forceMenu) {
    $gameArgs = Build-Args
    & $exe @gameArgs
    exit $LASTEXITCODE
}

$oldForeground = [Console]::ForegroundColor
$oldBackground = [Console]::BackgroundColor
try {
    [Console]::CursorVisible = $false
    [Console]::BackgroundColor = 'DarkBlue'
    $Host.UI.RawUI.WindowTitle = 'Heart of The Alien Redux - Setup'
    Write-Host ''
}
catch {
    # Some hosts do not expose colors or a cursor; the menu still works.
}

$menuFailed = $false

try {
    $script:message = 'Welcome. Press S or Enter on START GAME to play.'

    while ($true) {
        Show-Menu

        $key = [Console]::ReadKey($true)
        $count = $script:fields.Count + $script:actions.Count

        if ($key.Key -eq [ConsoleKey]::UpArrow) {
            $script:selected--
            if ($script:selected -lt 0) { $script:selected = $count - 1 }
        }
        elseif ($key.Key -eq [ConsoleKey]::DownArrow) {
            $script:selected++
            if ($script:selected -ge $count) { $script:selected = 0 }
        }
        elseif ($key.Key -eq [ConsoleKey]::LeftArrow) {
            if ($script:selected -lt $script:fields.Count) {
                Change-Value $script:fields[$script:selected] -1
            }
        }
        elseif ($key.Key -eq [ConsoleKey]::RightArrow) {
            if ($script:selected -lt $script:fields.Count) {
                Change-Value $script:fields[$script:selected] 1
            }
        }
        elseif ($key.Key -eq [ConsoleKey]::Escape) {
            Save-Config
            break
        }
        elseif ($key.Key -eq [ConsoleKey]::Enter) {
            if ($script:selected -lt $script:fields.Count) {
                $field = $script:fields[$script:selected]
                if ($field.Kind -eq 'text') {
                    Read-TextValue $field
                }
                else {
                    Change-Value $field 1
                }
            }
            else {
                $action = $script:actions[$script:selected - $script:fields.Count]
                if ($action.Key -eq '__start') {
                    Start-Game
                }
                elseif ($action.Key -eq '__reset') {
                    Reset-Config
                }
                else {
                    Save-Config
                    break
                }
            }
        }
        elseif ($key.Key -eq [ConsoleKey]::S) {
            Start-Game
        }
        elseif ($key.Key -eq [ConsoleKey]::R) {
            Reset-Config
        }
        elseif ($key.Key -eq [ConsoleKey]::Q) {
            Save-Config
            break
        }
    }
}
catch [System.InvalidOperationException] {
    # No interactive console (for example a script redirected stdin).
    $menuFailed = $true
}
catch [System.IO.IOException] {
    $menuFailed = $true
}
finally {
    try {
        [Console]::CursorVisible = $true
        [Console]::ForegroundColor = $oldForeground
        [Console]::BackgroundColor = $oldBackground
        Clear-Host
    }
    catch {
    }
}

if ($menuFailed) {
    Write-Host 'No interactive console; starting the game with the saved settings.'
    $gameArgs = Build-Args
    & $exe @gameArgs
    exit $LASTEXITCODE
}

exit 0
