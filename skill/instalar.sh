#!/bin/sh
# Pone la skill «nora» (hacer apps y juegos para Nora OS) en los agentes del PC:
# Claude Code (~/.claude/skills) y Codex (~/.codex/skills), como enlace a esta
# carpeta: al actualizar el repo, la skill se actualiza sola.
set -e
aqui=$(cd "$(dirname "$0")" && pwd)/nora
for dir in "$HOME/.claude/skills" "$HOME/.codex/skills"; do
    [ -d "$(dirname "$dir")" ] || continue          # ese agente no esta instalado
    mkdir -p "$dir"
    if [ -e "$dir/nora" ] && [ ! -L "$dir/nora" ]; then
        echo "$dir/nora ya existe y no es un enlace: no lo toco"; continue
    fi
    ln -sfn "$aqui" "$dir/nora"
    echo "skill nora → $dir/nora"
done
command -v nora-puente >/dev/null || echo "ojo: falta nora-puente (cargo install --path ../nora-puente)"
