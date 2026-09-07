# Avisos de terceiros — RRP Softphone

RRP Softphone — Copyright (C) 2026 RRP Systems Ltda.
CNPJ 39.539.153/0001-47 — contato@rrpsystems.com.br

O RRP Softphone é distribuído sob a **GNU General Public License v3** (ver
[`gpl-3.0.txt`](gpl-3.0.txt)) e incorpora os componentes de terceiros listados
abaixo. Cada um permanece sob a sua própria licença.

O código-fonte completo do RRP Softphone, incluindo o procedimento exato de
compilação das bibliotecas que reconstruímos, está em:

> https://github.com/RRPSystems/RRPhone

Cada versão distribuída tem uma tag correspondente no repositório (`v1.0.2`,
etc.). Para obter o fonte que gerou um instalador específico, use a tag de
mesmo número — é ela que constitui o *Corresponding Source* daquele binário nos
termos da GPLv3.

Se por algum motivo o repositório estiver indisponível, o código-fonte
correspondente a qualquer binário distribuído pode ser solicitado por escrito a
**contato@rrpsystems.com.br** — a GPLv3 dá esse direito a quem recebeu o
binário, e atendê-lo é obrigação de quem o distribuiu.

---

## Qt 6.11.2 — LGPL v3

Copyright The Qt Company Ltd. e colaboradores.
Licença: [`lgpl-3.0.txt`](lgpl-3.0.txt), que opera como permissões adicionais
sobre a [`gpl-3.0.txt`](gpl-3.0.txt).

Módulos redistribuídos: `Qt6Core`, `Qt6Gui`, `Qt6Widgets`, `Qt6Network`,
`Qt6Svg`, mais os plugins de plataforma, estilo, imagem, TLS e rede que o
`windeployqt` inclui.

O Qt é usado **sem modificação alguma** e é **linkado dinamicamente**: as DLLs
ficam ao lado do executável, o que permite ao usuário substituí-las por outra
versão do Qt, conforme a LGPL exige. O fonte correspondente está em
https://download.qt.io/archive/qt/6.11/6.11.2/single/ .

## liblinphone 5.5.17 (linphone-sdk) — GPL v3

Copyright Belledonne Communications SARL.
Licença: [`gpl-3.0.txt`](gpl-3.0.txt).

Inclui `liblinphone`, `mediastreamer2`, `ortp`, `bctoolbox`, `belle-sip`,
`belr`, `belcard`, `bzrtp`, `lime` e os plugins `libmswasapi` e `libmswebrtc`.
Fonte: https://github.com/BelledonneCommunications/linphone-sdk (branch
`release/5.5`).

**`mediastreamer2.dll` e `libbcg729.dll` foram recompilados por nós**, a partir
do fonte upstream não modificado, apenas com sinalizadores de build diferentes
dos oficiais (`-DENABLE_G729=ON`, e o bcg729 como biblioteca compartilhada). O
procedimento completo está documentado em
[`../README.md`](../README.md#recompilando-o-mediastreamer2-com-g729-procedimento-que-funcionou),
de forma que qualquer pessoa possa reproduzir esses binários a partir do fonte
público — que é o que a GPLv3 exige de quem redistribui binários modificados.

## bcg729 — GPL v3

Codec G.729, Copyright Belledonne Communications SARL.
Licença: [`gpl-3.0.txt`](gpl-3.0.txt).
Fonte: https://github.com/BelledonneCommunications/bcg729

A extensão de licença comercial da Belledonne é exigida apenas para criar
aplicações proprietárias; este projeto é aberto sob GPLv3, então não se aplica.

## mbedTLS — Apache License 2.0

Copyright The Mbed TLS Contributors.
Usado para o AES-256-GCM e o PBKDF2 do arquivo `.rrpprofile`.
Fonte: https://github.com/Mbed-TLS/mbedtls

## Demais bibliotecas do linphone-sdk

Redistribuídas como parte do SDK, cada uma sob a sua licença de origem:

| Biblioteca | Licença |
|---|---|
| Opus | BSD 3-Clause |
| Speex / SpeexDSP | BSD 3-Clause |
| libsrtp2 | BSD 3-Clause |
| SQLite3 | Domínio público |
| SOCI | Boost Software License 1.0 |
| libxml2 | MIT |
| zlib | zlib License |
| libjpeg-turbo / turbojpeg | IJG / BSD 3-Clause |
| Xerces-C | Apache License 2.0 |
| JsonCpp | MIT |
| libyuv | BSD 3-Clause |
| ZXing-C++ | Apache License 2.0 |
| decaf (Ed448-Goldilocks) | MIT |
| GSM 06.10 | Licença permissiva do TU-Berlin |
| BV16 | BSD 3-Clause |
| hidapi | BSD 3-Clause / GPLv3 (à escolha) |

Os textos completos e a lista definitiva por versão acompanham o
[linphone-sdk](https://github.com/BelledonneCommunications/linphone-sdk).

## Toque padrão

`sounds/ring_rrp.wav` foi sintetizado especificamente para este projeto e
segue a mesma licença do aplicativo. Não há áudio de terceiros distribuído.
