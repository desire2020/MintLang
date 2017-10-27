set -e
cd "$(dirname "$0")"
cat > test.mx
./msharp test.mx test.asm 2>log.txt
cat test.asm