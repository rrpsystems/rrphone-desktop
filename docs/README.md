# Página de download

`index.html` é a página pública de download do RRP Softphone, servida via
GitHub Pages a partir deste diretório (`main` / `docs/`).

Não precisa ser editada a cada release: o botão de download aponta para o
redirecionamento permanente do GitHub

    /releases/latest/download/RRPSoftphone-setup.exe

que sempre resolve para a release mais recente, porque o asset publicado tem
sempre esse nome fixo — sem a versão, que já está na tag, nas propriedades do
`.exe` e na tela Sobre. Publicar é um comando só
(`.\installer\publish-release.ps1`); o procedimento está em
[`../desktop/README.md`](../desktop/README.md#publicando-uma-release-download-hospedado-no-github-sem-custo).

## Habilitando o GitHub Pages

No repositório, em **Settings → Pages**:

- **Source**: Deploy from a branch
- **Branch**: `main`, pasta `/docs`

A URL padrão fica em `https://rrpsystems.github.io/rrphone-desktop/`. Para um
domínio próprio (ex. `download.rrpsystems.com.br`), adicione um arquivo
`CNAME` aqui com o domínio e configure um registro `CNAME` apontando para
`rrpsystems.github.io` no DNS.

## Arquivos

- `index.html` — a página.
- `assets/rrp_logo.png` — logo redimensionada para uso web (240×240).
- `assets/favicon.ico` — mesmo ícone do aplicativo desktop.
