# KofDock

Dock experimental para **KDE Plasma 6 / Wayland**, escrito em **C++20, Qt6 e QML**, com ampliação de ícones e empilhamento em leque.

> Projeto em desenvolvimento (base v0.2.10-beta2). A estabilidade de fixar aplicativos e a seleção de monitores ainda precisam de validação completa.

## Requisitos

- Garuda Linux ou Arch Linux com KDE Plasma 6 em sessão Wayland.
- Compilador C++20, CMake, Make, Qt6, LayerShellQt, módulos QML do KDE Task Manager e Kirigami.

No Garuda/Arch:

```bash
sudo pacman -S --needed base-devel cmake qt6-base qt6-declarative qt6-wayland layer-shell-qt plasma-workspace kirigami
```

## Instalar

Na pasta do projeto:

```bash
bash scripts/install.sh
```

O instalador compila, executa os testes disponíveis, verifica o carregamento do QML no Wayland e instala o aplicativo em `~/.local/bin/kofdock`. A inicialização automática pode ser ativada ou desativada nas preferências.

## Executar

```bash
~/.local/bin/kofdock
```

Para escolher uma saída específica:

```bash
~/.local/bin/kofdock --screen HDMI-A-2
```

## Remover

```bash
bash scripts/uninstall.sh
```
