#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RELEASE_DIR="$ROOT/release"
PKG_NAME="reader-cachyos-git"

usage() {
    cat <<'EOF'
Usage: packaging/build-release.sh [VERSION]

Build and test a CachyOS/Arch x86_64 package from the current working tree,
including uncommitted changes. On success, replace older generated release
assets in ./release. VERSION defaults to a value derived from Git and today's
date. This script builds locally; it does not upload to GitHub.
EOF
}

case "${1:-}" in
    -h|--help) usage; exit 0 ;;
    "") ;;
    -* ) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
esac

if [[ $# -gt 1 ]]; then
    usage >&2
    exit 2
fi

for tool in makepkg cmake ctest tar sha256sum; do
    command -v "$tool" >/dev/null || { echo "Required command not found: $tool" >&2; exit 1; }
done

if [[ -n "${1:-}" ]]; then
    VERSION="$1"
else
    SHORT_HASH="$(git -C "$ROOT" rev-parse --short HEAD)"
    COMMIT_COUNT="$(git -C "$ROOT" rev-list --count HEAD)"
    DIRTY=""
    if [[ -n "$(git -C "$ROOT" status --porcelain)" ]]; then
        DIRTY=".dirty"
    fi
    VERSION="0.1.0.$(date +%Y%m%d).r${COMMIT_COUNT}.g${SHORT_HASH}${DIRTY}"
fi

if [[ ! "$VERSION" =~ ^[A-Za-z0-9._+:-]+$ ]]; then
    echo "Invalid package version: $VERSION" >&2
    exit 2
fi

mkdir -p "$RELEASE_DIR"
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/reader-release.XXXXXX")"
cleanup() { rm -rf -- "$WORK_DIR"; }
trap cleanup EXIT

echo "Creating source snapshot from $ROOT"
tar -C "$ROOT" \
    --exclude='./.git' --exclude='./build' --exclude='./build-*' \
    --exclude='./release' --exclude='./packaging/pkg' --exclude='./packaging/src' \
    --exclude='./packaging/reader-cachyos-git' --exclude='./graft' \
    -czf "$WORK_DIR/reader-source.tar.gz" .
cp "$ROOT/packaging/reader.install" "$WORK_DIR/reader.install"
SOURCE_HASH="$(sha256sum "$WORK_DIR/reader-source.tar.gz" | cut -d' ' -f1)"
INSTALL_HASH="$(sha256sum "$WORK_DIR/reader.install" | cut -d' ' -f1)"

cat > "$WORK_DIR/PKGBUILD" <<EOF
pkgname=$PKG_NAME
pkgver=$VERSION
pkgrel=1
pkgdesc='Offline TXT/EPUB reader for CachyOS/Arch'
arch=('x86_64')
url='https://github.com/wz150432/Reader-CachyOS'
license=('custom')
depends=('qt6-base' 'libarchive')
makedepends=('cmake')
conflicts=('reader')
provides=('reader')
install=reader.install
source=('reader-source.tar.gz' 'reader.install')
sha256sums=('$SOURCE_HASH' '$INSTALL_HASH')
options=('!debug')

build() {
    cmake -S "\$srcdir" -B "\$srcdir/build" \\
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build "\$srcdir/build" -j4
}

check() {
    ctest --test-dir "\$srcdir/build" --output-on-failure
}

package() {
    install -Dm755 "\$srcdir/build/reader" "\$pkgdir/usr/bin/reader"
    install -Dm644 "\$srcdir/packaging/reader.desktop" \\
        "\$pkgdir/usr/share/applications/reader.desktop"
    install -Dm644 "\$srcdir/packaging/reader.svg" \\
        "\$pkgdir/usr/share/icons/hicolor/scalable/apps/reader.svg"
    install -Dm644 "\$srcdir/packaging/reader-mime.xml" \\
        "\$pkgdir/usr/share/mime/packages/reader.xml"
}
EOF

echo "Building and running tests (version $VERSION)"
(cd "$WORK_DIR" && makepkg --clean --force) 2>&1 | tee "$WORK_DIR/build.log"

PACKAGE_FILE="$WORK_DIR/$PKG_NAME-$VERSION-1-x86_64.pkg.tar.zst"
if [[ ! -s "$PACKAGE_FILE" ]]; then
    echo "Expected package was not created: $PACKAGE_FILE" >&2
    exit 1
fi

cp "$WORK_DIR/reader-source.tar.gz" "$RELEASE_DIR/reader-source.tar.gz.new"
cp "$PACKAGE_FILE" "$RELEASE_DIR/$PKG_NAME-$VERSION-1-x86_64.pkg.tar.zst.new"
cp "$WORK_DIR/build.log" "$RELEASE_DIR/build.log.new"
cat > "$RELEASE_DIR/RELEASE-NOTES.md.new" <<'EOF'
# Reader CachyOS @VERSION@

CachyOS / Arch Linux x86_64 安装包。构建包含运行测试的当前工作区源码快照。

## 安装或升级

```bash
sha256sum -c SHA256SUMS
sudo pacman -U ./@PKG_NAME@-@VERSION@-1-x86_64.pkg.tar.zst
```

pacman 会安装 Qt 6 和 libarchive 依赖。启动命令：`reader`。安装新版时执行相同命令即可升级。

## 卸载

```bash
sudo pacman -R @PKG_NAME@
```

卸载会保留个人设置、阅读记录、书签和本地书籍。
EOF
sed -i "s/@VERSION@/$VERSION/g; s/@PKG_NAME@/$PKG_NAME/g" "$RELEASE_DIR/RELEASE-NOTES.md.new"

(
    cd "$WORK_DIR"
    sha256sum "$PKG_NAME-$VERSION-1-x86_64.pkg.tar.zst" reader-source.tar.gz > "$RELEASE_DIR/SHA256SUMS.new"
)

# Replace stale generated release assets only after the new build and tests succeed.
find "$RELEASE_DIR" -maxdepth 1 -type f \
    \( -name 'reader-cachyos-git-*.pkg.tar.zst' -o -name 'reader-source.tar.gz' \
       -o -name 'SHA256SUMS' -o -name 'RELEASE-NOTES.md' -o -name 'build.log' \) \
    -delete
mv "$RELEASE_DIR/reader-source.tar.gz.new" "$RELEASE_DIR/reader-source.tar.gz"
mv "$RELEASE_DIR/$PKG_NAME-$VERSION-1-x86_64.pkg.tar.zst.new" "$RELEASE_DIR/$PKG_NAME-$VERSION-1-x86_64.pkg.tar.zst"
mv "$RELEASE_DIR/build.log.new" "$RELEASE_DIR/build.log"
mv "$RELEASE_DIR/RELEASE-NOTES.md.new" "$RELEASE_DIR/RELEASE-NOTES.md"
mv "$RELEASE_DIR/SHA256SUMS.new" "$RELEASE_DIR/SHA256SUMS"

echo "Release package ready in: $RELEASE_DIR"
echo "Upload the .pkg.tar.zst, reader-source.tar.gz, and SHA256SUMS files to GitHub Releases."
