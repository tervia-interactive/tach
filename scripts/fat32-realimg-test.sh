#!/bin/sh
# Valida o driver FAT32 do tach contra imagens criadas por mkfs.fat + mcopy.
#
# Requer: dosfstools (mkfs.fat, fsck.fat), mtools (mcopy), python3 e o binario
# src/build/host/fat32-test (gerado por `make test-host`).
# Uso: scripts/fat32-realimg-test.sh
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN="$ROOT/src/build/host/fat32-test"
[ -x "$BIN" ] || { echo "falta $BIN: rode 'make test-host' antes" >&2; exit 1; }
for tool in mkfs.fat mcopy python3; do
    command -v "$tool" >/dev/null 2>&1 || { echo "falta '$tool'" >&2; exit 1; }
done

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
export LC_ALL=C.UTF-8 MTOOLS_SKIP_CHECK=1

# Arvore de teste + manifesto ("F <tamanho> <fnv1a hex8> <caminho>", "D <caminho>").
python3 - "$WORK/tree" "$WORK/manifest.txt" <<'PY'
import os, random, sys
tree, manifest = sys.argv[1], sys.argv[2]
rnd = random.Random(1234)

def fnv(data):
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h

files = {
    "UPPER.TXT": 10, "lower.txt": 20, "mixed Case.Name.dat": 700,
    "Documento com acentuação.txt": 1234, "日本語.txt": 99,
    "vazio.bin": 0, "um.bin": 1, "s511.bin": 511, "s512.bin": 512,
    "s513.bin": 513, "s4095.bin": 4095, "s4096.bin": 4096, "s4097.bin": 4097,
    "s65535.bin": 65535, "s65536.bin": 65536, "s65537.bin": 65537,
    "grande.bin": 1048576 + 13, "dois_mib.bin": 2 * 1048576,
    "a/b/c/d/profundo.bin": 70000, "a/b/irmao.txt": 5,
    "nome muito longo " + "x" * 120 + ".txt": 300,
}
for i in range(150):                       # um diretorio que passa de um cluster
    files["muitos/arquivo_numero_%03d.dat" % i] = rnd.randrange(0, 3000)

lines = []
dirs = set()
for rel, size in files.items():
    path = os.path.join(tree, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    data = bytes(rnd.randrange(256) for _ in range(size)) if size < 100000 else rnd.randbytes(size)
    with open(path, "wb") as f:
        f.write(data)
    lines.append("F %d %08x /%s" % (size, fnv(data), rel))
    parts = rel.split("/")[:-1]
    for i in range(1, len(parts) + 1):
        dirs.add("/".join(parts[:i]))
for d in sorted(dirs):
    lines.append("D /" + d)
with open(manifest, "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
PY

run_case() {
    spc=$1; size=$2
    img="$WORK/img-s$spc.img"
    truncate -s "$size" "$img"
    mkfs.fat -F 32 -s "$spc" -R 32 -n TACHREAL "$img" >/dev/null
    mcopy -i "$img" -s -n -m "$WORK"/tree/* ::/
    if command -v fsck.fat >/dev/null 2>&1; then fsck.fat -n "$img" >/dev/null; fi
    echo "== setores por cluster: $spc"
    "$BIN" "$img" "$WORK/manifest.txt"
    echo
}

run_case 1 40M     # 65525+ clusters de 512 B
run_case 2 70M     # clusters de 1 KiB
echo "FAT32 (imagens reais): OK"
