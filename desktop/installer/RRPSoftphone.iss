; Instalador do RRP Softphone (Inno Setup 6).
;
; Empacota a árvore gerada por:
;     cmake --install build-msvc --prefix dist
;
; Não aponte para build-msvc/ diretamente: aquela pasta tem centenas de MB de
; artefatos de compilação e a ferramenta de teste de registro, que não devem
; chegar ao cliente.
;
; Para gerar:
;     "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\RRPSoftphone.iss
; A saída fica em installer\Output\.

#define AppName        "RRP Softphone"
; A versão vem do CMake (version.iss é gerado ao configurar), para não haver
; duas cópias do mesmo número podendo divergir.
#include "version.iss"
#define AppPublisher   "RRP Systems"
#define AppExe         "RRPSoftphone.exe"
#define DistDir        "..\dist"

[Setup]
; Este GUID identifica o produto para atualizações e desinstalação.
; NÃO mude entre versões: mudá-lo faz o Windows tratar a nova versão como um
; produto diferente e deixar as duas instaladas lado a lado.
AppId={{7B3F1C64-2A9E-4D57-9F0B-5E8C1D6A4B23}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\{#AppExe}
OutputBaseFilename=RRPSoftphone-{#AppVersion}-setup
Compression=lzma2/max
SolidCompression=yes
; O app é 64-bit (Qt MSVC 64 + SDK do liblinphone win64); num Windows 32-bit
; ele não roda, então recusar a instalação é melhor que instalar algo quebrado.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
; Fecha o softphone antes de sobrescrever os arquivos, evitando o clássico
; "arquivo em uso" no meio da atualização.
CloseApplications=yes
RestartApplications=no
WizardStyle=modern

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Tasks]
Name: "desktopicon"; Description: "Criar atalho na área de trabalho"; GroupDescription: "Atalhos:"

; Sem opção de "iniciar com o Windows" aqui, de propósito: a instalação roda
; como administrador, então HKCU seria o perfil do administrador e não o do
; usuário que vai atender as chamadas — a opção ficaria no lugar errado, de um
; jeito difícil de perceber. O próprio app tem esse ajuste no menu da bandeja,
; onde ele roda com a identidade certa.

[Files]
; O executável separado do resto só para poder marcá-lo com ignoreversion —
; o restante entra recursivamente.
Source: "{#DistDir}\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\*"; DestDir: "{app}"; Excludes: "{#AppExe},redist\*"; Flags: ignoreversion recursesubdirs createallsubdirs
; Não instalado junto do app: é executado uma vez e descartado.
Source: "{#DistDir}\redist\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall; Check: NeedsVCRedist

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\Desinstalar {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
; O app é compilado com /MD e depende do runtime do Visual C++ 2015-2022. Numa
; máquina sem ele o executável simplesmente não abre, sem mensagem útil.
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; \
    StatusMsg: "Instalando o runtime do Visual C++..."; Flags: waituntilterminated; Check: NeedsVCRedist
Filename: "{app}\{#AppExe}"; Description: "Executar o {#AppName}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Só o que o app cria dentro da pasta de instalação. As configurações do
; usuário (registro), a senha (Gerenciador de Credenciais), o histórico e os
; contatos ficam em %LOCALAPPDATA% e são preservados de propósito: desinstalar
; para reinstalar uma versão nova não deve apagar os dados de ninguém.
Type: filesandordirs; Name: "{app}"

[Code]
// Pula o redistribuível quando o runtime já está presente — ele demora uns
// bons segundos mesmo quando não tem nada a fazer.
function NeedsVCRedist: Boolean;
var
  Installed: Cardinal;
begin
  Result := True;
  if RegQueryDWordValue(HKEY_LOCAL_MACHINE,
       'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed) then
    Result := (Installed <> 1);
end;
