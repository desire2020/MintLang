set -e
cd "$(dirname "$0")"
cat > test.mx
./msharp test.mx
