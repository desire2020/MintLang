set -e
cd "$(dirname "$0")"
g++ -O2 -std=c++11 *.cpp -o ./msharp