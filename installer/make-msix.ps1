# Gera o pacote MSIX para a Microsoft Store: installer\Output\RRPSoftphone-<versão>.msix
#
#     cmake --install build-msvc --prefix dist      (antes, como para o instalador)
#     .\installer\make-msix.ps1
#
# O pacote vai SEM assinatura: no envio pelo Partner Center a Microsoft assina.
# Para testar neste PC antes de enviar:
#
#     .\installer\make-msix.ps1 -Register   # instala a pasta do pacote, sem assinar
#                                           # (exige o Modo de Desenvolvedor do Windows)
#
# Diferenças em relação ao instalador Inno Setup:
# - runtime do Visual C++: um pacote não executa instaladores, então as DLLs do
#   runtime vão junto do .exe (app-local), copiadas do Redist do Visual Studio;
# - "Iniciar com o Windows" usa a StartupTask do pacote (AppxManifest.xml).

param([switch]$Register)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$desktop = Split-Path -Parent $PSScriptRoot
$dist = Join-Path $desktop 'dist'
$outDir = Join-Path $desktop 'installer\Output'
$layout = Join-Path $outDir 'msix'

# --- Versão (a mesma fonte do instalador). A Store exige o 4º número = 0. ---
$cmake = Get-Content (Join-Path $desktop 'CMakeLists.txt') -Raw
if ($cmake -notmatch 'project\(RRPSoftphone VERSION ([0-9]+\.[0-9]+\.[0-9]+)') { throw 'Versão não encontrada no CMakeLists.txt' }
$version = $Matches[1]
$exeVersion = (Get-Item (Join-Path $dist 'RRPSoftphone.exe')).VersionInfo.FileVersion
if ($exeVersion -ne $version) {
    throw "dist\RRPSoftphone.exe está na versão $exeVersion, o CMakeLists diz $version. Rode o build e o cmake --install."
}

# --- Ferramentas -------------------------------------------------------------
$makeappx = Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin' -Recurse -Filter makeappx.exe |
    Where-Object { $_.FullName -match '\\x64\\' } | Sort-Object FullName -Descending | Select-Object -First 1
if (-not $makeappx) { throw 'makeappx.exe não encontrado (instale o Windows SDK).' }
$crt = Get-ChildItem 'C:\Program Files (x86)\Microsoft Visual Studio' -Recurse -Directory -Filter 'Microsoft.VC143.CRT' |
    Where-Object { $_.FullName -match '\\Redist\\MSVC\\[^\\]+\\x64\\' } | Sort-Object FullName -Descending | Select-Object -First 1
if (-not $crt) { throw 'Runtime do Visual C++ (Redist x64) não encontrado.' }

# --- Pasta do pacote ---------------------------------------------------------
if (Test-Path $layout) { Remove-Item $layout -Recurse -Force }
New-Item -ItemType Directory -Force $layout | Out-Null
Get-ChildItem $dist | Where-Object { $_.Name -ne 'redist' } | Copy-Item -Destination $layout -Recurse
Get-ChildItem $crt.FullName -Filter *.dll | Copy-Item -Destination $layout

$manifest = (Get-Content (Join-Path $PSScriptRoot 'msix\AppxManifest.xml') -Raw -Encoding UTF8).Replace('@VERSION@', "$version.0")
[IO.File]::WriteAllText((Join-Path $layout 'AppxManifest.xml'), $manifest, (New-Object Text.UTF8Encoding $false))

# --- Imagens do pacote, a partir do logo -------------------------------------
$assets = Join-Path $layout 'Assets'
New-Item -ItemType Directory -Force $assets | Out-Null
$logo = [Drawing.Bitmap]::FromFile((Join-Path $desktop 'resources\rrp_logo.png'))
function Save-Logo([string]$name, [int]$w, [int]$h, [double]$fraction) {
    $bmp = New-Object Drawing.Bitmap $w, $h, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'; $g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'
    $g.Clear([Drawing.Color]::Transparent)
    $box = [Math]::Min($w, $h) * $fraction
    $scale = [Math]::Min($box / $logo.Width, $box / $logo.Height)
    $lw = $logo.Width * $scale; $lh = $logo.Height * $scale
    $g.DrawImage($logo, [float](($w - $lw) / 2), [float](($h - $lh) / 2), [float]$lw, [float]$lh)
    $bmp.Save((Join-Path $assets $name), [Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
}
Save-Logo 'StoreLogo.png' 50 50 0.9
Save-Logo 'Square44x44Logo.png' 44 44 0.9
Save-Logo 'Square150x150Logo.png' 150 150 0.66
Save-Logo 'Wide310x150Logo.png' 310 150 0.66
$logo.Dispose()

if ($Register) {
    Add-AppxPackage -Register (Join-Path $layout 'AppxManifest.xml')
    Write-Host 'Instalado. Abra "RRP Softphone" pelo menu Iniciar.' -ForegroundColor Green
    return
}

# --- Empacotamento -----------------------------------------------------------
$msix = Join-Path $outDir "RRPSoftphone-$version.msix"
& $makeappx.FullName pack /d $layout /p $msix /o | Out-Null
if ($LASTEXITCODE -ne 0) { throw "makeappx falhou ($LASTEXITCODE)" }
Write-Host "Pacote: $msix ($([math]::Round((Get-Item $msix).Length / 1MB)) MB)" -ForegroundColor Green
