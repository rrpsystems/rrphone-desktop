# Publica uma release no GitHub a partir do instalador já compilado.
#
#     .\installer\publish-release.ps1
#
# A versão não é passada como argumento: ela é lida do CMakeLists.txt, a mesma
# fonte que alimenta o instalador e as propriedades do executável. Digitá-la
# aqui de novo seria só mais um lugar para divergir.
#
# O asset publicado tem nome FIXO (RRPSoftphone-setup.exe), sem a versão. Isso
# é o que permite à página de download apontar para
#
#     /releases/latest/download/RRPSoftphone-setup.exe
#
# um redirecionamento permanente do GitHub para a release mais recente — sem
# editar a página a cada versão e sem publicar o mesmo binário duas vezes.
# A versão continua rastreável pela tag, pelas propriedades do .exe
# (Detalhes -> Versão do arquivo) e pela tela Sobre do aplicativo.
#
# O arquivo versionado permanece em installer\Output\ como histórico local de
# builds; ele só não vai para o release.

$ErrorActionPreference = 'Stop'
$repo = 'RRPSystems/rrphone-desktop'
$desktopDir = Split-Path -Parent $PSScriptRoot

# --- Versão, lida da única fonte de verdade ---------------------------------
$cmake = Get-Content (Join-Path $desktopDir 'CMakeLists.txt') -Raw
if ($cmake -notmatch 'project\(RRPSoftphone VERSION ([0-9]+\.[0-9]+\.[0-9]+)') {
    throw "Não consegui ler a versão de CMakeLists.txt"
}
$version = $Matches[1]
$tag = "v$version"
Write-Host "Versão: $version" -ForegroundColor Cyan

# --- O instalador precisa existir e ser desta versão ------------------------
$versioned = Join-Path $desktopDir "installer\Output\RRPSoftphone-$version-setup.exe"
if (-not (Test-Path $versioned)) {
    throw "Instalador não encontrado: $versioned`nCompile primeiro (ver README, 'Gerando o instalador')."
}

# Guarda contra o erro mais provável aqui: publicar um instalador antigo porque
# a versão foi bumpada e o build não foi refeito.
$exeVersion = (Get-Item (Join-Path $desktopDir 'build-msvc\RRPSoftphone.exe')).VersionInfo.FileVersion
if ($exeVersion -ne $version) {
    throw "O executável compilado está na versão $exeVersion, mas o CMakeLists diz $version.`nRefaça o build antes de publicar."
}

# --- Tag ---------------------------------------------------------------------
if (-not (git tag --list $tag)) {
    git tag -a $tag -m "RRP Softphone $version"
    Write-Host "Tag $tag criada." -ForegroundColor Green
}
git push origin main --tags

# --- Asset de nome fixo ------------------------------------------------------
# Cópia temporária: o release leva o nome estável, o Output\ mantém o versionado.
$staging = Join-Path $env:TEMP "rrp-release-$version"
New-Item -ItemType Directory -Force -Path $staging | Out-Null
$asset = Join-Path $staging 'RRPSoftphone-setup.exe'
Copy-Item $versioned $asset -Force

# --- Release -----------------------------------------------------------------
$notes = @"
Instalador para Windows 10/11 (64-bit).

O arquivo não é assinado digitalmente: o Windows mostra "O Windows protegeu o
computador" — clique em **Mais informações** e depois em **Executar assim mesmo**.

A versão instalada aparece nas propriedades do executável (Detalhes -> Versão
do arquivo) e na tela Sobre, no menu da bandeja.

Distribuído sob a GNU GPL v3. O código-fonte correspondente a este binário é
esta tag: $tag
"@

gh release create $tag $asset --repo $repo --title "RRP Softphone $version" --notes $notes
Remove-Item $staging -Recurse -Force

Write-Host "`nPublicado: https://github.com/$repo/releases/tag/$tag" -ForegroundColor Green
Write-Host "Download:  https://github.com/$repo/releases/latest/download/RRPSoftphone-setup.exe" -ForegroundColor Green
