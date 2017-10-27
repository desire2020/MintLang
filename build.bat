set -e
cd "$(dirname "$0")"
g++ -O2 -std=c++11 compiler.cpp parser.cpp scanner.cpp main.cpp -o ./msharp
