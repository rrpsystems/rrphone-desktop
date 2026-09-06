# RRP Softphone — Desktop (Windows)

Esqueleto inicial do softphone desktop descrito em
[`Docs/RRP_Softphone_Desktop_PRD_Lite_v1.0.md`](../Docs/RRP_Softphone_Desktop_PRD_Lite_v1.0.md).

Engine SIP/RTP: **liblinphone** (mesma engine do app Android), via C++/Qt6.
Não é um fork do MicroSip — ver Seção 2 do PRD para a justificativa.

## Licenciamento

liblinphone é GPLv3. Decisão da RRP: **este repositório vai ser público no
GitHub**, então a obrigação de disponibilizar o código-fonte da GPLv3 é
satisfeita naturalmente — não é necessária a licença comercial da Belledonne
Communications para uso fechado. Se essa decisão mudar no futuro (ex.: algum
módulo específico precisar ficar fechado), revisitar esse ponto antes de
distribuir um binário.

## ✅ Estado deste código

**Compila, linka e a janela abre de verdade** — verificado com Qt 6.11.2
(kit MSVC2022 64-bit) + liblinphone SDK 5.5.17 + Visual Studio 2022 Build
Tools. Isso já passou por uma rodada real de build-fix-build, então os bugs
abaixo já foram corrigidos (não são mais um risco, é só histórico):

- API de codecs desatualizada: o código original usava a API antiga de
  `PayloadType`/mediastreamer2 (`payload_type_get_mime_type` etc.), que nem
  está mais exposta nos headers públicos desta versão do SDK. Trocado por
  `LinphonePayloadType`/`linphone_core_get_audio_payload_types` (ver
  `SipCoreManager::audioCodecs`/`setAudioCodecsOrder`).
- `#include "profile/ProfileStore.h"` faltando em `MainWindow.h` (usava
  `AccountProfile` sem declarar).
- Link faltando: `bctbx_list_*` (usado para percorrer listas de codecs) vive
  em `bctoolbox.lib`, uma import lib separada da `liblinphone.lib` — precisa
  ser linkada também (já ajustado no `CMakeLists.txt`).
- **Sem áudio nenhum / chamadas de saída não completavam**: o driver de áudio
  do Windows (`libmswasapi.dll`) e o cancelador de eco (`libmswebrtc.dll`) são
  **plugins do mediastreamer** e ficam em `lib/mediastreamer/plugins/`, fora do
  `bin/` do SDK. Sem copiá-los e sem apontar `linphone_factory_set_msplugins_dir()`
  para eles, o motor sobe sem nenhum driver de som (`Could not find a suitable
  soundcard`) e a chamada não consegue montar mídia. O `CMakeLists.txt` agora
  copia os dois para `<pasta do .exe>/plugins`. O plugin de vídeo
  (`libmsopenh264.dll`) é deliberadamente **não** copiado: vídeo está fora de
  escopo e ele falha ao carregar sem o `openh264.dll` da Cisco, poluindo o log.
- **Cronômetro contava desde a discagem**, não desde o atendimento: qualquer
  chamada não atendida acumulava "duração". Agora o `SipCoreManager` emite
  `callConnected()` em `LinphoneCallConnected` (o 200 OK do outro lado) e só
  então o relógio parte. O sinal também dispara ao retomar de uma espera, por
  isso a UI só inicia o relógio se ele ainda não estiver rodando — caso
  contrário a duração zeraria a cada hold.
- **Diálogo modal dentro de callback do liblinphone travava a máquina de
  estados.** Uma chamada que falhava abria um `QMessageBox` a partir do
  callback SIP; o diálogo cria um loop de eventos aninhado, então o aviso de
  "chamada encerrada" que vem logo depois nunca era processado e a UI ficava
  presa em "Chamando..." para sempre. O motivo da falha agora aparece na linha
  de gancho ("Temporarily Unavailable", "Busy", ...) por alguns segundos, sem
  diálogo — que também é o comportamento certo para um softphone, onde
  chamada ocupada é rotina e não merece um pop-up para fechar.
- **Encerramento sem desregistrar deixava contatos fantasmas no PBX.** Matar o
  processo (ou fechar sem `linphone_core_stop()`) não envia o unREGISTER, então
  cada execução deixava um binding pendurado no ramal. Acumulando bindings, o
  endpoint PJSIP passa a responder **403 Forbidden** a novos registros quando
  o `max_contacts` estoura com `remove_existing=no` — sintoma confuso, porque
  parece senha errada. Corrigido com `SipCoreManager::shutdown()`
  (`linphone_core_stop()`, que bloqueia justamente para desregistrar), chamado
  no `aboutToQuit` e no destrutor.
- **Crash na abertura** (`bctbx-fatal: Unable to load VCARD grammar`,
  `STATUS_STACK_BUFFER_OVERRUN`): liblinphone precisa saber onde está seu
  "top resources dir" (gramáticas SIP/SDP/vCard, sons, CA root) — sem isso
  ele aborta o processo na criação do `LinphoneCore`. Resolvido com
  `linphone_factory_set_top_resources_dir()` apontando para `<pasta do
  .exe>/share`, e o `CMakeLists.txt` copia para lá as três subpastas do SDK
  que são usadas em execução — `belr/`, `linphone/` e `sounds/` (ver "Gerando
  o instalador" para por que não se copia o `share/` inteiro).

**Registro SIP testado contra o PBX real da RRP** (`escritorio.rrpsystems.com.br:5090`,
ramal 2125), das duas formas: pela ferramenta de linha de comando
`tools/sip_register_test.cpp` (ver "Testando o registro SIP") **e pela
própria GUI** (Configurações → conta → Salvar), que passou a exibir
"● Disponível / Ramal 2125" e o ramal no título da janela. Resultado:
**registrou com sucesso via TCP**; via UDP a chamada ficou presa em
"Registration in progress" e nunca
respondeu, dentro do ambiente onde isso foi validado (sandbox com saída UDP
possivelmente restrita) — TCP na porta 5090 respondeu normalmente
(`Test-NetConnection` confirmou). Vale testar UDP também numa rede sem essa
restrição antes de concluir que é um problema do servidor.

Aparece um log `Unable to subscribe to the conference event package (RFC
4575) ... because no handler is available` — é esperado, é o liblinphone
tentando um recurso de chat/conferência que este app não implementa; não
afeta registro nem chamada de voz.

O que ainda não foi validado com hardware real: **chamada de voz com áudio**
de fato (o ambiente onde isso rodou não tem placa de som — apareceu
`Could not find a suitable soundcard`; isso não impede registro/discagem,
só a validação de áudio bidirecional precisa de uma máquina com som).

### Codecs disponíveis e o caso do G.729

O app oferece **quatro** codecs, nesta ordem de prioridade: **PCMA**, **PCMU**
(G.711 a-law/µ-law), **G.729** e **OPUS**. A lista é definida num único lugar,
`Codecs::supported()` em [`src/core/CodecInfo.cpp`](src/core/CodecInfo.cpp), e
governa a tela de Configurações, a oferta SDP e o `.rrpprofile` ao mesmo tempo.

O liblinphone habilita mais uma dúzia (Speex em três taxas, GSM, G.722, BV16,
dois sabores de L16) que nenhum PBX deste parque fala; no startup o app poda os
payload types do motor para os quatro acima, então eles somem do INVITE também.
A ordem importa: liderar com OPUS empurraria transcodificação para o Asterisk,
já que a operadora fala G.711/G.729.

Habilitar G.729 **não é um problema de licença para nós**: o aviso do próprio
SDK diz que a extensão de licença da bcg729 é exigida apenas *"to create a
proprietary application"*, e este projeto é aberto/GPL (ver Licenciamento).

O problema é técnico. Em `mediastreamer2/src/CMakeLists.txt`:

```cmake
if(BCG729_FOUND AND ENABLE_G729)
    list(APPEND VOIP_SOURCE_FILES_C audiofilters/g729.c)
endif()
```

O G.729 é compilado **dentro do `mediastreamer2.dll`**, não é um plugin
carregável — então não dá para simplesmente adicionar uma DLL na pasta
`plugins/` como fizemos com o WASAPI. Seria necessário **recompilar o SDK**
com `-DENABLE_G729=ON`, o que implica:

- `git clone --recursive` do linphone-sdk (o zip do master vem com os
  submódulos vazios, inclusive `bcg729/`)
- MSYS2 e 7-Zip no PATH, além de componentes extras do Visual Studio
  (UWP, .NET, Windows 8.1 SDK) que hoje não estão instalados
- E o próprio README do SDK avisa: *"The build works on Linux or MacOS hosts.
  Building on Windows may work, but is **not recommended**."*

**O G.729 é requisito real para a RRP**: a operadora não fala OPUS, então toda
chamada externa obriga o Asterisk a transcodificar — custo de CPU por chamada
no servidor. Com G.729 no cliente, a chamada passa direta.

**Annex B desligado de propósito.** O liblinphone oferece G.729 com
`a=fmtp:18 annexb=yes` (supressão de silêncio) por padrão, mas a maioria das
implementações do lado do PBX não faz Annex B. O descasamento produz um
sintoma enganoso: a chamada negocia normalmente, o RTP flui nos dois sentidos
e mesmo assim o áudio sai mudo ou picotado. `SipCoreManager::configureG729()`
força `annexb=no` no envio e na recepção.

#### Recompilando o mediastreamer2 com G.729 (procedimento que funcionou)

**Isto já está aplicado nesta máquina** — o app registra e lista `G729/8000`
entre os codecs. O procedimento fica aqui porque `third_party/` é ignorado
pelo git: quem clonar o repositório recebe o SDK oficial, **sem G.729**, e
precisa refazer estes passos.

A ideia central: não recompilar o SDK inteiro (inviável, ver bloqueio abaixo),
e sim **apenas o `mediastreamer2`**, ligando-o às dependências já
pré-compiladas que vêm no pacote oficial.

Pré-requisitos: MSYS2 (fornece `awk` e `sed`), Python 3, VS 2022 Build Tools,
CMake e Ninja (os do Qt servem).

```bash
# 1. Fontes (o GitHub espelha os projetos da própria Belledonne)
cd desktop/third_party
git clone --branch release/5.5 --depth 1 https://github.com/BelledonneCommunications/linphone-sdk.git linphone-sdk-src
git clone https://github.com/BelledonneCommunications/bcg729.git bcg729-src

# 2. bcg729 como DLL (estático não serve: o g729.c espera símbolos __imp_*)
cmake -S bcg729-src -B bcg729-build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DBUILD_SHARED_LIBS=ON -DENABLE_STRICT=NO -DENABLE_UNIT_TESTS=NO \
      -DCMAKE_INSTALL_PREFIX=<abs>/third_party/bcg729-install
cmake --build bcg729-build && cmake --install bcg729-build

# 3. mediastreamer2 com G.729, usando o shim versionado em desktop/cmake/
#    (PATH precisa incluir C:\msys64\usr\bin por causa do awk/sed)
cmake -S linphone-sdk-src/mediastreamer2 -B ms2-build -G Ninja \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_SHARED_LIBS=ON -DENABLE_TOOLS=OFF \
      -DCMAKE_PREFIX_PATH="<abs>/third_party/linphone-sdk/win64;<abs>/third_party/bcg729-install" \
      -DCMAKE_PROJECT_INCLUDE_BEFORE=<abs>/cmake/mediastreamer2-g729-shim.cmake \
      -DRRP_SDK_DIR=<abs>/third_party/linphone-sdk/win64 \
      -DRRP_BCG729_DIR=<abs>/third_party/bcg729-install \
      -DENABLE_G729=ON -DENABLE_UNIT_TESTS=OFF -DENABLE_STRICT=OFF \
      -DENABLE_FFMPEG=OFF -DENABLE_THEORA=OFF -DENABLE_VPX=OFF
cmake --build ms2-build

# 4. Instalar no SDK (guardando o original) e reconfigurar o app
copy third_party\linphone-sdk\win64\bin\mediastreamer2.dll ...\mediastreamer2.dll.orig
copy third_party\ms2-build\src\mediastreamer2.dll ...\linphone-sdk\win64\bin\
copy third_party\bcg729-install\bin\libbcg729.dll ...\linphone-sdk\win64\bin\
cmake -S . -B build-msvc -DLINPHONESDK_DIR=third_party/linphone-sdk/win64
cmake --build build-msvc --target RRPSoftphone
```

O passo 4 **precisa reconfigurar**, não só rebuildar: a cópia das DLLs usa
`file(GLOB ...)`, que é avaliado na configuração — sem isso a `libbcg729.dll`
não vai para junto do `.exe` e o app morre no carregamento, antes do `main()`,
sem mensagem nenhuma.

Verificação: rode com `RRP_SIP_DEBUG=1 QT_ASSUME_STDERR_HAS_CONSOLE=1` e
procure a linha `[audio] codecs:` — deve conter `G729/8000` sem `(off)`.

Detalhes que custaram tempo e estão resolvidos no shim
(`desktop/cmake/mediastreamer2-g729-shim.cmake`): os módulos `FindBV16`,
`FindGSM` etc. da Belledonne têm um bug no Windows (declaram o alvo como
`UNKNOWN IMPORTED` mas só definem `IMPORTED_IMPLIB`, nunca
`IMPORTED_LOCATION`), e vários pontos do build consomem alvos importados como
caminho de arquivo — se o alvo apontar para a `.dll` o linker recusa com
`LNK1107`. O shim resolve os dois definindo os alvos contra as `.lib`.

Compatibilidade: conferida com `dumpbin` — a DLL recompilada exporta
**exatamente os mesmos 1351 símbolos** da oficial, então a `liblinphone.dll`
pré-compilada continua funcionando com ela.

#### Por que não dá para recompilar o SDK inteiro (bloqueado por terceiros)

Preparação já concluída nesta máquina:

- MSYS2, 7-Zip, Python 3.12 e VS 2022 Build Tools instalados
- `linphone-sdk` clonado em `third_party/linphone-sdk-src/` na branch
  `release/5.5` (mesma linha do binário 5.5.17 que usamos)
- Os repositórios da própria Belledonne (bctoolbox, ortp, mediastreamer2,
  liblinphone, **bcg729**) vieram do espelho do GitHub sem problema

**Bloqueio:** o `gitlab.linphone.org` está retornando **HTTP 500 no servidor
inteiro**. No `.gitmodules`, os submódulos da própria Belledonne usam URL
*relativa* (`../bcg729.git`, então clonar do GitHub já os resolve), mas os
~15 externos têm URL **absoluta cravada** naquele servidor.

Tentativas já descartadas (não repetir):

- **Reescrever o `.gitmodules` para o GitHub**: consultando a API do GitHub
  repositório por repositório, a organização `BelledonneCommunications`
  espelha as libs dela (`bcg729`, `bctoolbox`, `ortp`, `mediastreamer2`) mas
  **nenhum** dos forks externos — `speex`, `srtp`, `mbedtls`, `zlib`, `opus`,
  `gsm`, `sqlite3`, `soci`, `xerces-c`, `libxml2`, `libjpeg-turbo`, `decaf`,
  `jsoncpp`, `bv16-floatingpoint`, `zxing-cpp` retornam todos 404.
- **Baixar zip em vez de clonar**: zip do GitHub/GitLab não inclui submódulos
  (foi por isso que o `linphone-sdk-master.zip` veio com `bcg729/` vazia).
- **Espelho em `gitlab.com/linphone2`**: só publica a linha 5.3.x e não expõe
  os externos (pede autenticação, sinal de que o caminho não existe).
- **Trocar pelos upstreams originais** (xiph/speex, cisco/libsrtp, madler/zlib):
  a Belledonne fixa commits que não existem lá e adiciona cola de CMake nos
  forks — o speex original, por exemplo, usa autotools.

Ou seja: só resta esperar o servidor deles voltar (ou pedir um build com G.729
direto à Belledonne).

Quando o GitLab voltar, o caminho é:

```bash
cd third_party/linphone-sdk-src
git submodule update --init --recursive
cmake --preset=windows-64bits -B build-g729 -DENABLE_G729=ON
cmake --build build-g729 --config RelWithDebInfo --parallel
```

E então apontar `LINPHONESDK_DIR` para o SDK recém-compilado.

Nada disso impede, porém, a recompilação **apenas do mediastreamer2** descrita
acima — que é o caminho que usamos. Chegamos a descartá-lo por engano: o
`mediastreamer2/CMakeLists.txt` exige `find_package(BCToolbox)` e
`find_package(Ortp)` em modo *config*, e num primeiro momento procuramos esses
pacotes só em `lib/cmake/` e concluímos que o SDK não os distribuía. Eles
existem, em `share/`:

```
share/BCToolbox/cmake/BCToolboxConfig.cmake
share/Ortp/cmake/OrtpConfig.cmake
share/BZRTP/cmake/BZRTPConfig.cmake
```

### Transferência: um fluxo só

O PRD separava transferência cega (D-07) e com consulta (D-06). Na prática
isso obriga o usuário a decidir, antes de discar, algo que ele só descobre
depois (se o destino vai atender). O app faz diferente — **um caminho só**,
dentro da janela principal, sem diálogo:

1. Durante a chamada, **Transferir** (ícone à direita do botão de ação): a
   chamada vai para espera e o display vira campo para o ramal de destino.
2. **Chamar**: liga para o destino. Você pode anunciar a chamada.
3. **Transferir**: conecta as duas pessoas e você sai da linha.
   **Cancelar**, em qualquer um dos dois passos, retoma a chamada original.

O que decide entre cega e assistida é apenas **se o destino já atendeu** no
momento em que você conclui, e isso é resolvido pelo `SipCoreManager`: se
atendeu, é `linphone_call_transfer_to_another` (REFER+Replaces, preservando a
conversa em andamento); se ainda está tocando, cai automaticamente em
`linphone_call_transfer` (cega). O mesmo gesto funciona nos dois casos.

### Quem está do outro lado (identidade conectada)

O número discado nem sempre é quem atende: grupo de captura, desvio, captura
de chamada e transferência mudam o interlocutor real. O display acompanha
isso — a cada progresso da chamada o app relê a identidade e, se ela mudou,
atualiza a tela. Quando o atendente difere do que foi discado, os dois
aparecem (`Suporte (2131)` / `discado: 2130`), e o histórico anota
`atendida por 2131`.

De onde vem a informação, nesta ordem:

1. **`P-Asserted-Identity`** (RFC 3325) — o cabeçalho moderno
2. **`Remote-Party-ID`** — o formato antigo, ainda muito usado em dialplans
3. O endereço remoto do diálogo SIP, que o liblinphone mantém atualizado
   através de transferências

Os dois primeiros são lidos com `linphone_call_params_get_custom_header()`
sobre `linphone_call_get_remote_params()`.

> **Depende de configuração no Asterisk.** Se o PBX não anunciar a identidade,
> não há o que exibir e o app cai no item 3. No `pjsip.conf` do endpoint:
>
> ```ini
> send_rpid = yes          ; ou send_pai = yes
> trust_id_outbound = yes
> connected_line_method = invite   ; atualiza durante a chamada
> ```

### Histórico de chamadas (D-08)

Botão do relógio na barra inferior. Registra recebidas, efetuadas e não
atendidas, com data, duração e motivo; duplo clique liga de volta.

Guardamos em JSON próprio (`call_history.json`, ao lado do log) em vez de usar
os *call logs* do liblinphone, porque estes vivem só em memória enquanto o
core não recebe uma base de dados — e nós o criamos deliberadamente sem
arquivos de configuração.

Um detalhe que importa na prática: chamadas interceptadas pelo **não perturbe**
ou pelo **siga-me** também entram no histórico, com a anotação correspondente
(`recusada (não perturbe)`, `encaminhada para 2130`). Sem isso o usuário
simplesmente não fica sabendo quem ligou enquanto o modo estava ativo.

### Agenda: contatos do servidor + contatos locais

A página de contatos mostra as duas origens juntas. Os **locais aparecem em
destaque** e só eles podem ser editados ou excluídos — os do servidor
pertencem ao provisionamento. Ficam em `contacts_local.json`.

Em Configurações → Contatos há a opção **"Apagar os contatos locais quando a
lista do servidor chegar"**, para implantações em que a agenda central deve ser
a única fonte. Detalhe deliberado: a limpeza só acontece quando o download
**dá certo** — uma falha de rede nunca destrói os contatos que o usuário
criou.

### Áudio: microfone, alto-falante e toque separados

Em Configurações → Áudio dá para escolher os três dispositivos
independentemente. O **toque é separado do alto-falante** de propósito: quem
usa headset USB quer a conversa no fone, mas o toque precisa sair na caixa do
PC, senão perde chamadas enquanto não está com o fone na cabeça.

"Padrão do sistema" (opção inicial) deixa o motor decidir, que é o certo para
quem nunca abre essa tela.

Os ids ficam em `QSettings` e **não** entram no `.rrpprofile`: um id de
dispositivo não significa nada em outra máquina.

#### Três APIs de dispositivo, e escolher a errada é silencioso

Esta é a armadilha que custou uma madrugada de depuração — o app registrava,
negociava codec, trocava RTP nos dois sentidos e não saía som nenhum:

| API | O que ela realmente faz |
| --- | --- |
| `linphone_core_set_output_audio_device` | Reaponta **as chamadas já em andamento**. Chamada da tela de configurações, onde por definição não há chamada, **não faz nada.** |
| `linphone_core_set_default_output_audio_device` | Define o que **a próxima chamada** vai usar. É esta que faz a configuração valer. |
| `linphone_core_set_playback_device` (string) | Sound card antigo, usado pelo tocador de arquivos: toque e "Testar som". |

`SipCoreManager::setPlaybackDevice()` seta **as três**, e o mesmo vale para a
entrada. Também vale saber que `linphone_core_get_output_audio_device()` é
escopo-de-chamada e devolve `NULL` fora de uma chamada — para responder "onde
o som vai sair" use `get_default_output_audio_device()`.

#### Toque: o padrão do SDK não toca no Windows

O toque padrão do liblinphone é um `.mkv`, mas o pacote Windows do SDK **não
traz o plugin Matroska** — o motor escolhe um arquivo que não consegue
decodificar e a chamada entra em silêncio absoluto. Isso aparece só como um
aviso fácil de ignorar (`No ringtone has been defined in sound config, using
default one`).

Por isso `configureSounds()` aponta explicitamente para arquivos WAV. O toque
padrão é o nosso [`resources/sounds/ring_rrp.wav`](resources/sounds/README.md),
copiado para `<pasta do .exe>/sounds` no build; se ele faltar, cai no
`rings/oldphone-mono.wav` do SDK — que funciona, mas é literalmente uma
campainha de telefone antigo. O ringback usa o `ringback.wav` do SDK.

O usuário pode trocar o toque por qualquer WAV em **Configurações → Áudio →
Som do toque**, com um botão "Ouvir" que toca pelo dispositivo de toque
escolhido. Um arquivo ilegível é recusado na hora e **não** é persistido:
gravar um toque quebrado deixaria as chamadas recebidas mudas a cada
inicialização, que é exatamente o defeito descrito acima.

O toque precisa ser um arquivo de verdade em disco — o liblinphone recebe um
caminho, então ele não pode vir do bundle de recursos do Qt como os ícones.

O DTMF é outro caso: como ele viaja out-of-band (RFC 2833), nada é ouvido
localmente a menos que o app peça — daí `linphone_core_play_dtmf()` no
`sendDtmf()`.

#### Cancelador de eco

O padrão (`MSWebRTCAEC`) não roda abaixo de 16 kHz e **se desativa sozinho em
toda chamada narrowband** — que aqui é toda chamada, já que a operadora fala
G.711/G.729. Ficaríamos sem cancelamento nenhum para quem não usa headset.
Trocado por `MSSpeexEC`, que é embutido no mediastreamer2 e aceita 8 kHz.

#### Diagnóstico

Sem instrumentação esse tipo de problema é indistinguível de dez outros. O que
existe hoje:

- **Logs do motor no arquivo do app.** Por padrão os logs do
  liblinphone/mediastreamer vão para o console e somem; `SipCoreManager::start()`
  instala um callback no `LinphoneLoggingService` para trazê-los ao log. Sem
  isso o log parece saudável enquanto o driver de áudio falha em silêncio.
- **"Testar som"** (Configurações → Áudio) toca um WAV pelo dispositivo
  selecionado sem precisar de chamada — separa "dispositivo morto" de "problema
  de mídia".
- **`[audio] rtp`** a cada 5 s durante a chamada: `recebendo: 0.0 kbit/s`
  significa que nada chega ao socket (firewall/NAT); ~80 kbit/s é G.711 normal
  e ~24 kbit/s é G.729 normal.

#### Avisos benignos

Estes aparecem sempre e **não** indicam defeito:

- `mswasapi: changing output rate to 8000 Hz is not supported... Keep 48000 Hz`
  — o dispositivo está em modo compartilhado a 48 kHz estéreo e o
  mediastreamer reamostra.
- `Filter MSBCG729Enc does not implement the MS_FILTER_GET_SAMPLE_RATE method`
  — probe do mediastreamer no bcg729.
- `getReceiverLossRate: there is no RTCP packet received` — o PBX não manda
  RTCP; afeta só a estatística de perda.
- `Getting reference signal but no echo to synchronize on` / `Not enough ref
  samples, using zeroes` — o `MSSpeexEC` convergindo nos primeiros
  milissegundos da chamada. Aparecem uma vez e param; é sinal de que o
  cancelador **está** rodando.
- `ERROR Unable to subscribe to the conference event package (RFC 4575)...
  because no handler is available` — efeito colateral de `disable_chat()`: o
  core procura um handler de sala de chat que, por design, não existe aqui.
  Dispara uma vez por registro. Não há API para desligar essa tentativa.
- `No ringtone has been defined in sound config, using default one` — emitido
  durante a criação do core, antes de `configureSounds()` poder rodar. O toque
  correto é aplicado logo em seguida.
- `Could not apply gain on sent RTP packets: gain control wasn't activated` —
  o `audio_stream_enable_gain_control()` que ele pede só existe no nível do
  mediastreamer2 e não é exposto pelo `LinphoneCore`. **Suspeita em aberto:**
  o slider de microfone pode não ter efeito sobre o volume enviado. Precisa ser
  confirmado numa chamada real antes de virar bug.

### Não perturbe e siga-me

- **Não perturbe**: botão ao lado do microfone. Recusa a chamada com
  `linphone_call_decline(LinphoneReasonBusy)` — o chamador ouve ocupado na
  hora, sem tocar aqui.
- **Siga-me**: botão ao lado do DND, na tela principal — sem precisar entrar em
  Configurações. Desligado, um clique pergunta o ramal ali mesmo, usando o
  display e o teclado (mesma mecânica da transferência); ligado, um clique
  desliga. O campo em Configurações → Chamadas continua existindo para quem
  preferir digitar um número completo, e os dois ficam sincronizados.
  Redireciona com `linphone_call_redirect_to()`, ou seja, um **SIP 302**
  respondido imediatamente: o telefone não chega a tocar e o PBX reencaminha
  na hora, sem espera nem "toca aqui primeiro".

Se os dois estiverem ativos, o **siga-me tem precedência** — a chamada vai
para o ramal escolhido em vez de morrer em ocupado.

Ambos sobrevivem a reinícios (quem encaminhou o ramal espera que continue
encaminhado) e são sinalizados em dois lugares: no rótulo do topo
(`● Não perturbe` em vermelho, `● Siga-me → 2130` em azul) e como destaque na
linha de gancho, junto dos ícones (`No gancho · Não perturbe`, `Em chamada ·
Mudo`). Isso é deliberado e vale a redundância: qualquer coisa que impeça o
telefone de tocar — ou que impeça o outro lado de ouvir — precisa ser óbvia,
senão o usuário só percebe que "as chamadas sumiram" ou que ninguém responde.

A linha de gancho é composta em `MainWindow::refreshHookLabel()` a partir do
estado (situação da chamada + modos ativos), em vez de escrita direto — assim
os modos não somem quando a chamada muda de estado.

### Registros presos no PBX

Cada execução do app usa uma porta de origem nova, então aparece como um
contato novo no PBX. Se o processo for morto (travamento, Gerenciador de
Tarefas), nenhum unREGISTER é enviado e aquele contato fica pendurado até
expirar — acumulando, estoura o `max_contacts` e o servidor passa a responder
**403 Forbidden**, que parece erro de senha.

Do lado do app, duas medidas: o encerramento limpo (`linphone_core_stop()`) e
um **tempo de registro curto, 300s** em vez do padrão (~1h), então um contato
órfão some em no máximo 5 minutos.

Mas a correção definitiva é **no servidor**, porque o cliente nunca consegue
avisar quando é morto. Para um ramal de um dispositivo só, no `pjsip.conf`:

```ini
max_contacts = 1
remove_existing = yes
```

Assim um registro novo substitui o anterior em vez de somar.

### Onde a configuração fica salva

O app guarda a configuração entre execuções e volta registrado sozinho ao
abrir. A senha SIP **nunca** vai para o registro nem para arquivo:

| O quê | Onde |
|---|---|
| Conta (ramal, servidor, transporte, DTMF), URL de contatos, ordem de codecs, volumes | `HKCU\Software\RRP Systems\RRPSoftphone` (via `QSettings`) — `SettingsStore` |
| **Senha SIP** | **Windows Credential Manager**, credencial genérica de destino `RRPSoftphone` — `CredentialStore` (`CredWriteW`/`CredReadW`) |

Escolhemos o Credential Manager em vez de DPAPI em arquivo próprio porque é o
padrão do Windows para isso: criptografado pelo OS, atrelado à conta do
usuário e **gerenciável pelo próprio usuário** (Painel de Controle →
Gerenciador de Credenciais). A limitação inerente — qualquer processo rodando
como o mesmo usuário consegue ler de volta — vale igual para DPAPI; fugir
disso exigiria hardware (TPM/smartcard), o que não se justifica aqui.

### Provisionamento por arquivo (D-13) e como ele se encaixa

O fluxo de importar um `.rrpprofile` já se combina corretamente com a
persistência: importar → aplica na hora (registra) → grava em QSettings +
Credential Manager. Ou seja, **o arquivo é necessário uma única vez**; depois
disso ele pode ser apagado da máquina e o app continua voltando registrado.

#### Criptografia do arquivo, e o que ela realmente protege

O arquivo é **AES-256-GCM**, com a chave derivada por **PBKDF2-HMAC-SHA256**
(200 000 iterações, salt de 16 bytes novo a cada arquivo), tudo via mbedTLS —
que já acompanha o SDK, então não há dependência nova para distribuir. O
formato é a versão 3; as versões 1 e 2 continuam sendo importadas.

**Todo o conteúdo** vai dentro do envelope cifrado, não só a senha SIP. Isso é
deliberado: a URL de contatos costuma carregar credenciais próprias
(`http://usuario:senha@host/...`) e nos formatos anteriores ela era gravada em
texto puro, de modo que o arquivo entregava a agenda a quem recebesse o anexo.

Sobre a chave, sem rodeios:

- **Padrão (sem senha):** a chave é injetada no build. Sem configurar nada, usa
  a **chave de desenvolvimento publicada** que está em `ProfileCipher.cpp` —
  então um clone novo compila e funciona, e essa chave não protege nada.
  Para builds oficiais, crie `desktop/profile_key.txt` (ignorado pelo git):

  ```bash
  openssl rand -hex 32 > desktop/profile_key.txt
  ```

  e reconfigure. O CMake avisa qual das duas está em uso.

  Isso tira a chave do repositório público, o que vale a pena. **Não a torna
  secreta**: ela viaja dentro de todo binário, em máquinas que não
  controlamos, e extrair 32 bytes de um executável é trabalho de rotina.
  Trate como *ofuscação com verificação de integridade* — mantém senhas fora
  da vista de quem abre o anexo num editor de texto e detecta adulteração; não
  resiste a quem quer entrar.

  Um efeito colateral a ter em mente: **perfis só são intercambiáveis entre
  builds que compartilham a chave.** Um binário com a chave da RRP não importa
  um perfil exportado por um build de clone público, e vice-versa.

  Um arquivo `.env` lido em runtime seria pior: texto puro ao lado do `.exe` é
  mais fácil de ler que uma constante no binário. O paralelo com aplicações web
  não vale aqui — lá o segredo fica num servidor que você controla; aqui o app
  roda na máquina do cliente e precisa da chave para funcionar.
- **Com senha (caixa marcada na exportação):** a senha entra na derivação da
  chave e o arquivo passa a ser de fato confidencial — tão confidencial quanto
  a senha, que precisa viajar por um canal diferente do e-mail que leva o
  arquivo.

A escolha do padrão foi consciente: provisionar uma máquina nova não deveria
exigir senha nenhuma. A caixa de seleção existe para quando o destino justificar.

Em ambos os casos a cifra é **autenticada**: senha errada, arquivo truncado ou
um único byte alterado são rejeitados. O placeholder XOR anterior "decriptava"
lixo alegremente e registrava uma conta quebrada.

O que ainda falta para usar isso como provisionamento de verdade em escala:

1. **Importação automática.** Hoje exige clicar em Configurações → Importar.
   Para instalar em várias máquinas o normal é o app procurar sozinho um arquivo
   de provisionamento (ao lado do `.exe` ou em `%ProgramData%`) na primeira
   execução, ou aceitar `--import <arquivo>` por linha de comando.
3. **Um arquivo por usuário.** Um único arquivo provisiona um único ramal. Para
   vários usuários é preciso gerar um arquivo por pessoa — ou partir para
   provisionamento remoto, que o liblinphone suporta nativamente
   (`linphone_core_set_provisioning_uri`, busca um XML de configuração numa URL
   ao iniciar). Esse caminho combinaria bem com a lista de contatos remota que
   já usamos.

> **Cuidado ao testar por automação/ferramentas em contêiner.** Se o processo
> que grava a configuração rodar dentro de um contêiner MSIX (é o caso de
> assistentes empacotados como o app do Claude), tanto o `HKCU` quanto o
> `%LOCALAPPDATA%` são **virtualizados**: a conta é salva no armazenamento do
> pacote e o app iniciado normalmente pelo usuário, fora do contêiner, não
> enxerga nada — mostra "Configure em ⚙" como se nunca tivesse sido
> configurado. O sintoma engana porque, de dentro do contêiner, tudo parece
> perfeito. A configuração precisa ser feita **na sessão real do usuário**,
> pela própria tela de Configurações.

Para zerar a configuração de uma máquina:

```bash
reg delete "HKCU\Software\RRP Systems\RRPSoftphone" /f
cmdkey /delete:RRPSoftphone
```

### Testando o registro SIP

Sem precisar abrir a GUI nem digitar senha em lugar nenhum do projeto:

```bash
build-msvc\sip_register_test.exe <ramal> <senha> <servidor[:porta]> <udp|tcp|tls>
```

Ele imprime cada mudança de estado de registro e sai sozinho (ou por timeout
de 15s, ou 0.5s depois de registrar com sucesso). Não loga a senha.

## Pré-requisitos

1. **Qt 6.x via Qt Creator/Qt Online Installer** (o instalador já traz CMake e Ninja em `C:\Qt\Tools\`).
   **Importante:** o kit padrão que o instalador oferece é `win64_mingw`
   (MinGW/GCC) — mas o SDK do liblinphone pré-compilado da Belledonne é
   **MSVC**. Misturar os dois dá problema de ABI/CRT. É preciso adicionar o
   kit **MSVC 2022 64-bit** também, pelo Qt Maintenance Tool
   (`C:\Qt\MaintenanceTool.exe`):
   ```bash
   C:\Qt\MaintenanceTool.exe install qt6.11.2-msvc2022-essentials --accept-licenses --accept-obligations --confirm-command --accept-messages
   ```
   (troque `6.11.2` pela versão do Qt que você tem instalada — rode
   `C:\Qt\MaintenanceTool.exe search msvc2022` pra ver os aliases disponíveis).
   Isso instala em `C:\Qt\<versão>\msvc2022_64\`.
2. **Visual Studio 2022 Build Tools** com o workload C++ (dá o `cl.exe`/MSVC
   que casa com o SDK do liblinphone e com o kit acima):
   ```bash
   winget install --id Microsoft.VisualStudio.2022.BuildTools -e --accept-package-agreements --accept-source-agreements --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
   ```
3. **liblinphone SDK (Windows)** — build pré-compilado da Belledonne Communications.
   Índice oficial de releases: https://download.linphone.org/releases/windows/sdk/
   (o link antigo `linphone.org/en/technical-corner/liblinphone` hoje é só uma
   notícia, não a página de download).

   **Já resolvido nesta cópia do projeto:** o zip `linphone-sdk-win64-5.5.17.zip`
   foi baixado e extraído em `desktop/third_party/linphone-sdk/win64/`
   (ignorado pelo git — ver `.gitignore` — porque é enorme e é uma dependência
   externa, não código nosso). Estrutura confirmada:
   - `third_party/linphone-sdk/win64/include/linphone/core.h`
   - `third_party/linphone-sdk/win64/lib/liblinphone.lib` — **atenção**: o nome
     do import lib é `liblinphone.lib`, não `linphone.lib` como eu tinha
     assumido inicialmente; o `CMakeLists.txt` já foi ajustado para procurar
     os dois nomes.
   - `third_party/linphone-sdk/win64/bin/*.dll` (34 DLLs — liblinphone e todas
     as dependências transitivas: bctoolbox, belle-sip, ortp, mediastreamer,
     opus, etc.) — copiadas automaticamente para perto do `.exe` no build.

   `desktop/third_party/downloads/` guarda os zips originais (também
   ignorados pelo git): o `linphone-sdk-win64-5.5.17.zip` usado acima, e um
   `linphone-sdk-master.zip` (código-fonte do superprojeto CMake da
   Belledonne — só necessário se um dia precisarmos compilar o SDK do zero
   em vez de usar o binário pronto; não é usado no build atual).

## Build

O build **precisa** rodar com o ambiente do MSVC carregado (`vcvars64.bat`) e
usando o CMake/Ninja do Qt, senão o `find_package(Qt6 ...)` não acha o kit
certo. A partir da pasta `desktop/`:

```bash
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
"C:\Qt\Tools\CMake_64\bin\cmake.exe" -S . -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\Qt\6.11.2\msvc2022_64" -DLINPHONESDK_DIR="third_party/linphone-sdk/win64" -DCMAKE_MAKE_PROGRAM="C:\Qt\Tools\Ninja\ninja.exe"
"C:\Qt\Tools\CMake_64\bin\cmake.exe" --build build-msvc
```

(ajuste `C:\Qt\6.11.2\msvc2022_64` pra sua versão do Qt instalada). Isso é
exatamente o comando usado pra validar o build — **confirmado funcionando**.

O executável e as DLLs/recursos do liblinphone ficam direto em `build-msvc/`
(gerador Ninja é single-config, sem subpasta `Release/`):

```bash
build-msvc/RRPSoftphone.exe
```

O build roda `windeployqt` automaticamente, então a pasta `build-msvc/` fica
**autossuficiente**: dá para dar duplo clique no `.exe`, ou zipar a pasta
inteira e mandar para outra máquina Windows, sem precisar do Qt instalado lá.

## Gerando o instalador

Duas etapas: montar uma árvore limpa e empacotá-la.

```bash
cmake --install build-msvc --prefix dist
```

```bash
"%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" installer\RRPSoftphone.iss
```

O resultado é `installer\Output\RRPSoftphone-<versão>-setup.exe`, um arquivo
único de ~58 MB. O Inno Setup se instala com
`winget install JRSoftware.InnoSetup`.

**Não zipe `build-msvc/`.** Ela tem ~400 MB de `CMakeFiles/`, `*_autogen/` e a
ferramenta `sip_register_test.exe`, nada disso distribuível. O passo
`cmake --install` existe justamente para separar o que roda do que só serve
para compilar; `dist/` fica em ~124 MB.

### O que o instalador resolve, e por quê

- **Runtime do Visual C++.** O app é compilado com `/MD`, então numa máquina
  sem o runtime 2015-2022 ele simplesmente não abre — sem mensagem útil. O
  instalador executa o `vc_redist.x64.exe` (que o `windeployqt` já deixa na
  pasta de build), e o pula quando o runtime já está presente, porque ele
  demora vários segundos mesmo sem ter o que fazer.
- **Recursos do SDK, sem o peso.** Só três subpastas de `share/` são usadas em
  execução: `belr/` (as gramáticas SIP/SDP/vCard, cuja ausência **aborta o
  processo**), `linphone/` (rootca.pem) e `sounds/` (ringback e toque de
  reserva). O resto são 274 MB de documentação da API e suítes de teste. Os
  `.mkv` também ficam de fora — sem o plugin Matroska eles não tocam.
- **App aberto durante a atualização.** `CloseApplications=yes` fecha o
  softphone antes de sobrescrever os arquivos.
- **Dados do usuário sobrevivem à desinstalação.** Conta, histórico, contatos
  locais e a senha (Gerenciador de Credenciais) ficam fora da pasta de
  instalação e são preservados de propósito: desinstalar para instalar uma
  versão nova não deve apagar os dados de ninguém.

Não há opção de "iniciar com o Windows" no instalador, e isso é deliberado: a
instalação roda como administrador, então uma escrita em `HKCU` cairia no
perfil do administrador e não no do usuário que atende as chamadas. O ajuste
existe no menu da bandeja do próprio app, onde ele roda com a identidade certa.

### Assinatura de código (pendente)

O instalador **não é assinado**, então o SmartScreen mostra "O Windows
protegeu o computador" e exige clicar em "Mais informações → Executar assim
mesmo". Isso foi adiado conscientemente enquanto a distribuição é interna
(ver Seção 4 do PRD Lite). Para distribuição externa é preciso um certificado
de assinatura de código (OV ou EV) e acrescentar `SignTool` ao `[Setup]`.

O `AppId` no `.iss` **não deve mudar entre versões**: é ele que faz o Windows
reconhecer uma instalação existente e atualizá-la. Trocá-lo instala a nova
versão ao lado da antiga.

## Rodando para testar

```
C:\dev\RRPhone\desktop\build-msvc\RRPSoftphone.exe
```

Duplo clique resolve. A configuração fica salva entre execuções, então depois
da primeira vez ele já abre registrado.

### Log do aplicativo

O app grava um log próprio, sem precisar de terminal nem variável de ambiente:

```
%LOCALAPPDATA%\RRP Systems\RRPSoftphone\rrpsoftphone.log
```

É o primeiro lugar a olhar quando alguém disser "não registrou aqui". Ele
mostra a conta lida da configuração salva, se a senha foi encontrada no
Credential Manager, os dispositivos de áudio detectados, a lista de codecs e
as falhas de chamada. Não registra a senha, apenas se ela existe. Gira sozinho
ao passar de 2 MB.

Para ver também o tráfego SIP completo (útil quando algo não registra ou não
completa a chamada):

```bash
set RRP_SIP_DEBUG=1
set QT_ASSUME_STDERR_HAS_CONSOLE=1
RRPSoftphone.exe
```

`RRP_SIP_DEBUG` liga o trace completo do liblinphone (inclusive os pacotes
SIP de REGISTER/INVITE e as respostas do servidor). `QT_ASSUME_STDERR_HAS_CONSOLE`
é necessário porque o app é GUI e, sem isso, o Qt manda as mensagens para o
depurador em vez do terminal. Os avisos do liblinphone saem em **stdout** e os
erros em **stderr** — redirecione os dois.

## Estrutura

```
src/
  core/       SipCoreManager — wrapper Qt em cima do LinphoneCore (registro,
              chamada, hold/mute/DTMF, transferência, codecs)
  contacts/   ContactsXmlFetcher — download + parse do XML de contatos (D-16,
              formato MicroSip). Não depende de liblinphone.
  profile/    ProfileStore — import/export do arquivo .rrpprofile (D-13);
              SettingsStore — persistência entre execuções (QSettings);
              CredentialStore — senha SIP no Windows Credential Manager
  ui/         MainWindow (tela única: status, display, linha de gancho,
              abas de linha, teclado, botão de ação e barra de ícones),
              DialPadWidget + DialKeyButton (teclas com dígito grande e
              letrinhas inline, desenhadas à mão), TransferDialog,
              SettingsDialog, ContactsPanel, Theme (paleta escura com as
              cores da RRP, aplicada globalmente)
resources/    Logo da RRP e ícones SVG monocromáticos, embutidos no binário
              via Qt Resource System (resources.qrc) — usados no ícone da
              janela, da bandeja, no display e nas barras de ação.
tools/        sip_register_test — utilitário de linha de comando pra testar
              registro SIP sem precisar da GUI (ver "Testando o registro SIP")
```

## Mapeamento para o PRD

| ID | Onde está |
|---|---|
| D-01 Registro SIP | `SipCoreManager::configureAccount`, `SettingsDialog` (aba Conta) — **testado com sucesso contra o PBX real da RRP via TCP** (ver "Testando o registro SIP") |
| D-02/D-03 Chamada efetuada/recebida | `SipCoreManager::call/answer/hangup`, `MainWindow::onIncomingCall` |
| D-04 Mute/Hold | `SipCoreManager::setMuted/setHeld`, `CallBar` |
| D-05 DTMF | `SipCoreManager::sendDtmf` — durante uma chamada o próprio teclado (mouse ou teclado físico) envia DTMF em vez de compor número (`MainWindow::onKeyPressed`) |
| **D-06 + D-07 Transferência (obrigatório)** | Fluxo único dentro da janela, sem escolher entre cega e assistida — ver abaixo. `SipCoreManager::beginAttendedTransfer/completeAttendedTransfer/cancelAttendedTransfer` |
| D-08 Histórico de chamadas | `CallHistoryStore`, `HistoryPanel` — inclui chamadas barradas por DND/siga-me |
| D-10 Bandeja do sistema | `MainWindow::setupTrayIcon`, `closeEvent` |
| D-11 Auto-start | `MainWindow::isAutoStartEnabled/setAutoStartEnabled` (registro `HKCU...Run` via `QSettings`) |
| D-12 Headset USB (HID) | **não implementado ainda** |
| D-13 Provisionamento por arquivo | `ProfileStore`, botões Importar/Exportar em `SettingsDialog` |
| D-14 Codecs | `SipCoreManager::audioCodecs/setAudioCodecsOrder`, lista arrastável em `SettingsDialog` |
| D-15 Método de DTMF | `SipCoreManager::setDtmfMethod`, combo em `SettingsDialog` |
| D-16 Lista de contatos remota | `ContactsXmlFetcher`, `ContactsPanel` — mesclada com contatos locais (`LocalContactsStore`, `ContactEditDialog`) |

## Known gaps / TODO antes de considerar isto pronto para uso real

- **Chave padrão do `.rrpprofile` é pública (D-13).** O arquivo agora usa AES-256-GCM de verdade, mas a chave padrão está compilada no app e este repositório é aberto: qualquer um decripta um perfil exportado sem senha. É ofuscação com integridade, não sigilo — ver "Criptografia do arquivo" acima. Para credenciais que não podem vazar, exporte com a opção de senha marcada.
- A identidade conectada (`P-Asserted-Identity`/`Remote-Party-ID`) foi implementada mas **não pôde ser verificada de ponta a ponta**: exige uma chamada realmente atendida, e o ambiente de teste só consegue discar para ramais inexistentes. Depende também de o Asterisk estar configurado para enviá-la.
- Suporte a controles físicos de headset USB (D-12/HID): não iniciado. Note que isso é diferente do mapeamento de dispositivos de áudio, que já funciona.
- A metade positiva do cronômetro (começar a contar **quando atendem**) só foi
  verificada por código — falta uma chamada real atendida para confirmar na
  prática. O caso negativo (não contar enquanto chama) foi testado.
- **Provisionamento em massa** (ver "Provisionamento por arquivo" abaixo): a importação do `.rrpprofile` ainda é manual (Configurações → Importar). Para distribuir em várias máquinas falta auto-importação.
- **Sem G.729** (ver seção de codecs) — exigiria recompilar o SDK.
- Não há seleção de dispositivo de áudio na UI: o app usa o padrão do Windows. Os dispositivos detectados são logados na inicialização (`[audio] dispositivos encontrados: ...`), mas escolher fone/headset específico ainda não dá.
- Logs do Qt (incluindo o diagnóstico de áudio) só aparecem com `QT_ASSUME_STDERR_HAS_CONSOLE=1`, porque o app é GUI e não tem console. Vale instalar um `qInstallMessageHandler` gravando em arquivo para suporte em campo.
- Encerramento do `LinphoneCore` no destrutor de `SipCoreManager` é abrupto (comentado no código) — falta esperar `LinphoneGlobalOff` antes de `unref` para um desligamento limpo.
- Sem instalador MSI ainda (fora do MVP de código, ver PRD Seção 8 — code signing fica para depois da validação interna).
- Nenhuma automação de teste (unit/integration) ainda — só o smoke test manual de registro em `tools/`.
- DLLs do Qt (`Qt6Core.dll` etc.) não são copiadas pro lado do `.exe` pelo
  CMake ainda (só as do liblinphone são) — falta rodar `windeployqt` como
  passo de build/empacotamento, ou aceitar que só funciona com o Qt no PATH.
- Chamada de voz com áudio bidirecional ainda não foi validada (falta uma
  máquina com placa de som) — registro já foi confirmado (ver acima).
- UDP não completou o registro no ambiente onde isso foi testado (TCP sim)
  — vale confirmar se é só uma restrição desse ambiente de sandbox ou algo a
  ajustar na config do lado do servidor/rede antes de assumir TCP como padrão.
