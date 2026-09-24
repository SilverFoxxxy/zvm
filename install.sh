#!/usr/bin/env bash
set -euo pipefail

# Переходим в папку, где лежит скрипт (корень репозитория)
cd "$(dirname "$0")"

# Определяем компилятор
CXX="${CXX:-}"
if [ -z "$CXX" ]; then
    if command -v g++ >/dev/null 2>&1; then
        CXX=g++
    elif command -v clang++ >/dev/null 2>&1; then
        CXX=clang++
    else
        echo "Не найден C++ компилятор (g++ или clang++)" >&2
        echo "На macOS установите Xcode Command Line Tools: xcode-select --install" >&2
        exit 1
    fi
fi

# Определяем префикс установки
# macOS: по умолчанию ~/.local, Linux: /usr/local
if [ -z "${PREFIX:-}" ]; then
    case "$(uname -s)" in
        Darwin) PREFIX="$HOME/.local" ;;
        *)      PREFIX="/usr/local"   ;;
    esac
fi
BINDIR="$PREFIX/bin"

SRC="zvm.cpp"
BIN="zvm"

echo "Компилятор: $CXX"
echo "Установка в: $BINDIR"
echo

# Сборка
"$CXX" -std=c++17 -O2 -Wall -Wextra -o "$BIN" "$SRC"

# Установка
install -d "$BINDIR"
install -m 755 "$BIN" "$BINDIR/$BIN"

# Подсказка про PATH
case ":$PATH:" in
    *":$BINDIR:"*) ;;
    *)
        echo
        echo "Внимание: $BINDIR не в PATH."
        echo "Добавьте в ~/.bashrc или ~/.zshrc:"
        echo "    export PATH=\"$BINDIR:\$PATH\""
        ;;
esac

echo
echo "Готово: $BINDIR/$BIN"
echo "Проверка: $BIN program.asm"
