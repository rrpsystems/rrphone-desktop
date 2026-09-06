# Toques

**`ring_rrp.wav`** — o toque padrão do aplicativo, sintetizado para este
projeto: motivo de duas notas G5→C6 em cadência de quatro pulsos com pausa,
osciladores levemente desafinados para dar corpo, harmônicos agudos contidos e
decaimento de 0,30 s (as notas se sobrepõem de propósito, formando um acorde
contínuo). Sem procedência de terceiros, portanto sem nenhuma dúvida de
licença num repositório público.

Formato: WAV PCM 16 kHz mono 16-bit, 3 s, tocado em loop pelo liblinphone
enquanto a chamada toca.

**Não use `.mkv` aqui.** O pacote Windows do SDK do liblinphone não traz o
plugin Matroska, então o arquivo tocaria em silêncio absoluto — foi exatamente
essa a causa das chamadas recebidas não emitirem som nenhum (ver o README
principal, "Toque: o padrão do SDK não toca no Windows").

O usuário pode apontar qualquer outro WAV em **Configurações → Áudio → Som do
toque**, com botão "Ouvir" para conferir antes de salvar. A escolha vale só
para a máquina dele; o padrão acima é o que vai no instalador.
