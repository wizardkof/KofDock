#!/usr/bin/env bash
# Publicação assistida, executada NO PC do usuário (requer GitHub CLI).
# SPDX-License-Identifier: MIT
set -euo pipefail

usage() {
  printf 'Uso: bash scripts/publicar-github.sh publico|privado\n'
  printf 'Cria wizardkof/KofDock e envia o código da pasta atual.\n'
}

case "${1:-}" in
  publico) visibility=public ;;
  privado) visibility=private ;;
  *) usage; exit 2 ;;
esac

repo="wizardkof/KofDock"
project="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

for cmd in git gh; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    printf 'Falta %s. No Garuda: sudo pacman -S --needed git github-cli\n' "$cmd" >&2
    exit 3
  fi
done

if ! gh auth status >/dev/null 2>&1; then
  printf 'O GitHub CLI precisa de login. Execute: gh auth login\n' >&2
  exit 4
fi

cd "$project"

# Nunca sobrescreve ou atualiza um repositório existente automaticamente.
if gh repo view "$repo" >/dev/null 2>&1; then
  printf 'O repositório https://github.com/%s já existe.\n' "$repo" >&2
  printf 'Para evitar sobrescrever conteúdo, a publicação foi interrompida.\n' >&2
  exit 5
fi

if [[ -d .git ]]; then
  if git remote get-url origin >/dev/null 2>&1; then
    echo 'Já existe um remote origin. Confira-o antes de publicar.' >&2
    exit 6
  fi
  if [[ "$(git branch --show-current)" != "main" ]]; then
    echo 'O projeto Git existente não está no branch main; revise antes de publicar.' >&2
    exit 7
  fi
else
  git init -b main
fi

if ! git var GIT_AUTHOR_IDENT >/dev/null 2>&1; then
  printf 'Configure nome e e-mail do Git antes de publicar, por exemplo:\n' >&2
  printf '  git config --global user.name "wizardkof"\n' >&2
  printf '  git config --global user.email "SEU_EMAIL_DO_GITHUB"\n' >&2
  exit 8
fi

# Faz stage somente dos arquivos oficiais do pacote, para nunca enviar
# por acidente anotações, segredos, capturas ou artefatos locais.
files=(
  .gitignore
  CMakeLists.txt
  LICENSE
  README.md
  qml/DockCell.qml
  qml/DockLogic.js
  qml/FanStack.qml
  qml/Main.qml
  qml/SettingsWindow.qml
  qml/StackCell.qml
  scripts/install.sh
  scripts/uninstall.sh
  src/dockautostart.cpp
  src/dockautostart.h
  src/dockcontroller.cpp
  src/dockcontroller.h
  src/docksectionmodel.cpp
  src/docksectionmodel.h
  src/dockservices.cpp
  src/dockservices.h
  src/docksettings.cpp
  src/docksettings.h
  src/main.cpp
  src/screenrouting.h
  tests/test_dock_logic.cjs
  tests/test_dockautostart.cpp
  tests/test_docksectionmodel.cpp
  tests/test_fan_layout.cjs
  tests/test_hover_order.cjs
  tests/test_launch_safety.cjs
  tests/test_screenrouting.cpp
  tests/validate_sources.py
  scripts/publicar-github.sh
)
for f in "${files[@]}"; do
  if [[ ! -f "$f" ]]; then
    printf 'Arquivo esperado ausente: %s\n' "$f" >&2
    exit 9
  fi
done
git add -- "${files[@]}"
if ! git diff --cached --quiet; then
  git commit -m "Add KofDock source (0.2.10 beta2)"
fi

printf 'Criando https://github.com/%s (%s) e publicando branch main...\n' "$repo" "$visibility"
gh repo create "$repo" "--$visibility" --source="$project" --remote=origin --push
printf 'Publicado: https://github.com/%s\n' "$repo"
