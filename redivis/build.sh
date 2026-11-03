#!/bin/bash
# Redivis: builds the readstat and extract_metadata binaries into dist/readstat-<version>-linux-<arch>.tar.gz, with a
#   .sha256 beside it. Run from the repo root in a Debian trixie container, the OS of Redivis's app image; see
#   .github/workflows/release.yml. libreadstat is linked statically, so the binaries only need glibc, libcsv3 and zlib
# Usage: redivis/build.sh <version>, which names the tarball
set -euo pipefail

version="$1"
case "$(uname -m)" in
    x86_64) arch=amd64 ;;
    aarch64) arch=arm64 ;;
    *) echo "Unsupported architecture $(uname -m)"; exit 1 ;;
esac

apt-get update -qq
apt-get install -y -qq --no-install-recommends \
    build-essential autoconf automake libtool gettext autopoint pkg-config libcsv-dev zlib1g-dev >/dev/null

./autogen.sh
./configure --disable-shared
make -j"$(nproc)"
make check -j"$(nproc)"

name="readstat-$version-linux-$arch"
staging="$(mktemp -d)/$name"
mkdir -p "$staging/bin" dist
cp readstat extract_metadata "$staging/bin/"
strip "$staging/bin/"*
cp LICENSE "$staging/"
tar -czf "dist/$name.tar.gz" -C "$(dirname "$staging")" "$name"
(cd dist && sha256sum "$name.tar.gz" > "$name.tar.gz.sha256")

echo "Built dist/$name.tar.gz ($(cut -d' ' -f1 "dist/$name.tar.gz.sha256")), linked against:"
ldd "$staging/bin/readstat"
