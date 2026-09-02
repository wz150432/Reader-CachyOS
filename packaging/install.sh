#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="user"
PREFIX=""
SKIP_UPDATES=0

usage()
{
    cat <<'EOF'
Usage: install.sh [--user|--system] [--prefix DIR] [--no-updates]

  --user       Install into ~/.local (default)
  --system     Install into /usr/local (uses sudo when needed)
  --prefix DIR Install into DIR
  --no-updates Skip desktop/mime database refresh
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --user) MODE="user"; shift ;;
        --system) MODE="system"; shift ;;
        --prefix) PREFIX="$2"; shift 2 ;;
        --no-updates) SKIP_UPDATES=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
    esac
done

if [[ -z "$PREFIX" ]]; then
    if [[ "$MODE" == "system" ]]; then
        PREFIX="/usr/local"
    else
        PREFIX="$HOME/.local"
    fi
fi

BINARY="${READER_BINARY:-$ROOT/build/reader}"
if [[ ! -x "$BINARY" ]]; then
    echo "Reader binary not found: $BINARY" >&2
    echo "Build it first with: cmake --build $ROOT/build" >&2
    exit 1
fi

APPS_DIR="$PREFIX/share/applications"
ICON_DIR="$PREFIX/share/icons/hicolor/scalable/apps"
MIME_DIR="$PREFIX/share/mime/packages"

SUDO=()
if [[ "$MODE" == "system" && "$EUID" -ne 0 ]]; then
    SUDO=(sudo)
fi

run_install()
{
    local mode="$1"
    local target="$2"
    local source="$3"
    if [[ "$mode" == "user" ]]; then
        install -Dm644 "$source" "$target"
    else
        "${SUDO[@]}" install -Dm644 "$source" "$target"
    fi
}

run_install_file()
{
    local mode="$1"
    local target="$2"
    local source="$3"
    if [[ "$mode" == "user" ]]; then
        install -Dm755 "$source" "$target"
    else
        "${SUDO[@]}" install -Dm755 "$source" "$target"
    fi
}

echo "Installing Reader into $PREFIX"
run_install_file "$MODE" "$PREFIX/bin/reader" "$BINARY"
run_install "$MODE" "$APPS_DIR/reader.desktop" "$ROOT/packaging/reader.desktop"
run_install "$MODE" "$ICON_DIR/reader.svg" "$ROOT/packaging/reader.svg"
run_install "$MODE" "$MIME_DIR/reader.xml" "$ROOT/packaging/reader-mime.xml"

if [[ "$SKIP_UPDATES" -eq 1 ]]; then
    exit 0
fi

if command -v update-desktop-database >/dev/null 2>&1; then
    if [[ "$MODE" == "user" ]]; then
        update-desktop-database "$APPS_DIR"
    else
        "${SUDO[@]}" update-desktop-database "$APPS_DIR"
    fi
fi

if command -v update-mime-database >/dev/null 2>&1; then
    if [[ "$MODE" == "user" ]]; then
        update-mime-database "$PREFIX/share/mime"
    else
        "${SUDO[@]}" update-mime-database "$PREFIX/share/mime"
    fi
fi

if command -v xdg-mime >/dev/null 2>&1; then
    xdg-mime default reader.desktop text/plain application/epub+zip application/x-mobipocket-ebook
fi

echo "Done. Reader is now the default app for txt/epub/mobi."
