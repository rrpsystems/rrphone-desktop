# RRP Softphone Desktop — PRD Enxuto (Windows)

**Versão:** 1.0
**Data:** 2026-09-02
**Status:** Draft para validação
**Substitui, para o escopo desktop:** RRP_Softphone_PRD_v3.1 (mantido como referência histórica/roadmap, não como escopo atual)

## 1. Motivação e Mudança de Escopo

O PRD v3.1 descrevia um produto multiplataforma completo (Android/iOS/Windows/Linux + módulo Call Center + vídeo + mensageria). O desenvolvimento do app Android já foi iniciado nessa linha. As prioridades do negócio mudaram: a necessidade imediata é um **softphone desktop Windows básico**, sem os módulos avançados (call center, vídeo, mensageria, multi-dispositivo via SIP CANCEL) que o PRD antigo amarrava ao MVP.

Referência de produto: **softphone de mesa clássico** — janela compacta, teclado numérico, hold/transfer, sem frescura. As imagens de referência trocadas na definição deste escopo ilustram apenas o **nível de simplicidade visual** pretendido — não são um layout a ser seguido à risca.

Este documento cobre **somente o desktop Windows**. O app Android segue seu próprio ritmo e, futuramente, o backend Flexisip/push será compartilhado entre as duas frentes.

## 2. Decisão Arquitetural — Engine Única (liblinphone)

- **Engine SIP/RTP: liblinphone**, a mesma usada no app Android — não um fork de softphone baseado em PJSIP.
  - Motivo: um único motor para manter (NAT traversal, codecs, SRTP, lógica de transferência) entre mobile e desktop; integração futura natural com Flexisip (registro/push) sem duplicar comportamento.
- **Não é fork de um softphone existente.** As alternativas avaliadas rodam sobre PJSUA2/PJSIP — adotá-las criaria dois motores SIP divergentes no produto.
- **Ponto de partida técnico:** usar o app de referência oficial `linphone-desktop` (Qt/QML + liblinphone, código aberto da Belledonne) como esqueleto de integração, **podado** para o subconjunto de telas/funcionalidades deste documento e re-skinado para o visual compacto de softphone de mesa. Isso evita reescrever do zero a parte de binding C++↔liblinphone (registro, estados de chamada, transferência), que já existe e é testada no projeto oficial.
- Stack: Qt 6.x + C++ (ou Qt/QML conforme o esqueleto herdado) + liblinphone via CMake, distribuído como instalador MSI assinado — consistente com a Seção 4.1 do PRD v3.1.

## 3. Objetivo

Um softphone desktop Windows simples, estável, com **transferência de chamada com consulta (atendida)** como funcionalidade obrigatória, cobrindo o uso corporativo básico de ramal — sem os módulos de call center, vídeo ou mensageria do PRD anterior.

## 4. Escopo — Funcionalidades

| ID | Funcionalidade | Prior. | Descrição |
|----|----------------|--------|-----------|
| D-01 | Registro SIP (conta única) | Must | Servidor, ramal/usuário, senha, transporte (UDP/TCP/TLS/WSS). Uma conta só, mas **duas chamadas simultâneas** — ver "Chamada em espera" abaixo. |
| D-02 | Chamada efetuada | Must | Discagem por teclado numérico ou campo de texto (ramal ou número externo) |
| D-03 | Chamada recebida | Must | Notificação/toast + janela de chamada, atender/recusar |
| D-04 | Mute / Hold | Must | Controles durante chamada ativa |
| D-05 | DTMF | Must | Envio de tons durante chamada ativa (teclado numérico) |
| D-06 | **Transferência com consulta (atendida)** | **Must — requisito obrigatório** | Coloca a chamada A em espera, disca para C, confere/conversa, e então completa a transferência (A↔C) ou cancela e retoma A. Fluxo padrão de "transferência supervisionada" do liblinphone. |
| D-07 | Transferência cega | Should | Transferir sem consulta prévia |
| D-08 | Histórico de chamadas (local) | Should | Recebidas/efetuadas/perdidas, armazenado localmente |
| D-10 | Bandeja do sistema (tray) | Should | Minimizar para tray, sempre registrado |
| D-11 | Auto-start com o Windows | Should | Entrada no Startup |
| D-12 | Suporte a headset USB (HID) | Could | Atender/desligar/mute pelo fone |
| D-13 | **Provisionamento por arquivo** | Could (bem-vindo, não obrigatório) | Ver Seção 5 |
| D-14 | Configuração básica de codecs | Must | Habilitar/desabilitar e ordenar prioridade (OPUS, G.711 A/U-law, G.722, GSM etc.) — nativo do liblinphone, só precisa de tela de config |
| D-15 | Método de DTMF configurável | Should | RFC2833 (out-of-band) / SIP INFO / in-band — nativo do liblinphone |
| D-16 | Lista de contatos remota via URL (XML) | Must | App baixa um XML de uma URL configurável e popula a agenda local — schema `<contacts>/<contact>` (Seção 8). **Não é um recurso do liblinphone**: é lógica de app (HTTP GET + parse), independente do SDK. |

**Nota sobre D-09 (múltiplas linhas):** removido do MVP por decisão do produto — conta única. A transferência com consulta (D-06) **não depende** de múltiplas linhas/contas: liblinphone suporta nativamente duas chamadas simultâneas na mesma conta registrada (uma em espera, outra de consulta), então múltiplas linhas fica como possível evolução de UI, não como pré-requisito técnico do D-06.

### D-17 — Chamada em espera (acrescentado após os testes de campo)

Este requisito **não estava neste PRD** e surgiu de uso real: com uma linha
estritamente única, quem ligasse para um ramal já em conversa recebia ocupado.
Num escritório isso costuma ser pior que um aviso discreto, sobretudo se o PBX
não tiver correio de voz configurado para o ramal.

Não é multi-linha, e explicitamente **não** traz os botões de linha que foram
retirados da interface: no máximo duas chamadas coexistem, que é o que telefone
de mesa e celular fazem há décadas com um único botão de alternar.

- Uma segunda chamada durante a conversa é **anunciada**, não atendida nem
  recusada: um bipe discreto (não o toque cheio, que falaria por cima da
  conversa) e o nome de quem chama na linha de gancho.
- **Atender** coloca a conversa atual em espera automaticamente.
- **Recusar** manda 486 Busy só para a nova; a conversa em curso não é tocada.
- **Alternar** troca qual das duas está no ar.
- **Desligar** encerra a que está no ar e **retoma a outra**, em vez de deixar
  o usuário sem chamada nenhuma.
- Uma **terceira** chamada recebe 486 Busy. Duas é o limite que cabe sem virar
  um produto diferente.

### Fora de Escopo (neste PRD)

- Módulo Call Center (status de agente, pausa, filas, discador)
- Videochamada
- Mensageria (chat)
- Multi-dispositivo / SIP CANCEL entre ramal mobile e desktop
- Provisionamento automático via login/JWT contra o backend Laravel (fica para quando houver integração com Flexisip/backend RRP)
- macOS, Linux (retomam se/quando o roadmap voltar a exigir)

Esses itens continuam válidos como **roadmap futuro** (ver PRD v3.1), não como requisito deste app.

## 5. Provisionamento por Arquivo (backup/restore)

Ideia: um arquivo único que carrega a configuração da conta SIP, permitindo "restaurar" o softphone em outra máquina sem digitar tudo de novo — análogo a um backup.

Proposta:

- Arquivo `.rrpprofile` (JSON internamente), contendo: `sip_server`, `sip_user`, `sip_password`, `transport`, `stun_server` (opcional), nome de exibição, codecs habilitados/ordem (D-14), método de DTMF (D-15) e, se configurada, a URL da lista de contatos remota (D-16).
- **Importar**: menu "Importar configuração..." → seleciona o arquivo → preenche e registra automaticamente.
- **Exportar**: menu "Exportar configuração..." → gera o arquivo a partir do estado atual.
- Como o arquivo carrega uma senha SIP, ele deve ser **protegido por senha/passphrase** (o app pergunta uma senha ao exportar e a mesma ao importar; conteúdo sensível fica criptografado, não em texto plano) — mesmo princípio de segurança de credenciais do PRD v3.1 (Seção 7.1), adaptado a arquivo em vez de Keystore.
- Fora de escopo nesta fase: sincronização automática/nuvem do arquivo — é um import/export manual.

## 6. Requisitos Não-Funcionais (reduzidos)

| Requisito | Especificação |
|---|---|
| Credenciais SIP | Windows Credential Manager (ou DPAPI) — nunca em arquivo de config em texto plano fora do fluxo de export explícito da Seção 5 |
| Transporte SIP | UDP/TCP/TLS conforme conta; WSS não é prioridade sem backend Flexisip |
| Mídia | SRTP quando o servidor suportar; sem SRTP obrigatório no MVP (depende do PBX do cliente) |
| Reconexão | Re-registro automático após queda de rede |
| Instalador | MSI assinado |
| Tamanho/instalação | Instalação e primeiro registro em poucos minutos, sem dependências externas manuais (runtime do liblinphone embutido no instalador) |

## 7. Critérios de Aceite (MVP)

| ID | Critério |
|---|---|
| AC-D1 | Registrar uma conta SIP e efetuar/receber chamada com áudio bidirecional |
| AC-D2 | Colocar chamada em espera, discar terceiro ramal, conversar com ele, e completar a transferência — chamada original é conectada ao terceiro; ou cancelar e retomar a chamada original |
| AC-D3 | Mute, hold e DTMF funcionando durante chamada ativa |
| AC-D4 | Exportar configuração em um arquivo e importar em outra instalação, restaurando o registro sem digitação manual |
| AC-D5 | Minimizar para bandeja mantendo o registro SIP ativo |
| AC-D6 | Tela de configurações permite habilitar/reordenar codecs e trocar o método de DTMF, e a alteração reflete na chamada seguinte |
| AC-D7 | Com uma URL de lista de contatos configurada, o app baixa e exibe os contatos na agenda local |

## 8. Decisões Registradas

- **D-09 (múltiplas linhas): fora do MVP.** Conta única. Confirmado que a transferência com consulta (D-06) não exige múltiplas linhas — fica como possível evolução de v1.1, não como bloqueador.
- **Code signing do instalador: fora do MVP.** Primeira fase distribuída internamente sem assinatura; assinatura de código entra quando o app for validado e for para distribuição mais ampla.
- **Integração com backend RRP: nenhuma nesta fase.** Configuração 100% local (conta SIP manual ou via arquivo, Seção 5). A única integração externa prevista é o **download da lista de contatos remota (D-16)**, que é um HTTP GET simples para uma URL configurável — não depende de backend próprio da RRP, qualquer servidor pode hospedar o XML.

### D-16 — Schema do XML da Lista de Contatos

Adotado um schema `<contacts>/<contact>` já consolidado no mercado, compatível com o que clientes/telefonia podem já ter em uso — a intenção é que um XML existente funcione sem conversão.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<contacts refresh="0">
<contact name="" number="" firstname="" lastname="" phone="" mobile="" email="" address="" city="" state="" zip="" comment="" presence="0" info=""/>
</contacts>
```

Notas de implementação:

- `contacts/@refresh`: intervalo de atualização automática **em minutos**. `0` = não atualiza sozinho (só ao iniciar o app ou por ação manual do usuário, ex. botão "Atualizar contatos").
- `contact/@name`: nome de exibição na agenda (campo obrigatório de fato, mesmo sem schema formal).
- `contact/@number`: número/ramal principal — o usado para discar com um clique/duplo clique no contato.
- `contact/@phone`, `@mobile`: números alternativos, exibidos como campos secundários (não usados para discagem padrão).
- `contact/@presence`: `"1"`/`"0"` — reservado para indicar presença/status do contato; **não implementado no MVP** (não há canal de presença nesta fase — Seção "Fora de Escopo"), o campo é apenas lido/ignorado por enquanto para manter compatibilidade com arquivos existentes.
- `contact/@info`: texto livre (tipicamente usado para "empresa/setor") — exibido como informação secundária/tooltip.
- `firstname`, `lastname`, `email`, `address`, `city`, `state`, `zip`, `comment`: aceitos e armazenados, mas sem exibição obrigatória na UI do MVP (podem ficar disponíveis num "detalhe do contato" simples).
- Parsing tolerante: atributos ausentes tratados como string vazia, sem quebrar o import.
- **D-16 pode ser desenvolvido em paralelo** ao core de chamadas — é uma tela de agenda + fetch HTTP + parser XML, sem dependência do liblinphone.
