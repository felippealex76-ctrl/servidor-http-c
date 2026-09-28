#!/usr/bin/env bash
# Teste de integração: sobe o servidor, faz requisições reais com curl
# e confere os códigos de status HTTP retornados.
set -u

PORT="${PORT:-8765}"
BIN="./build/servidor"
BASE="http://localhost:$PORT"
fails=0

"$BIN" -p "$PORT" -d www > /dev/null 2>&1 &
SERVER_PID=$!
trap 'kill -INT $SERVER_PID 2>/dev/null; wait $SERVER_PID 2>/dev/null' EXIT

# Espera o servidor ficar pronto (até ~3s)
for _ in $(seq 1 30); do
    curl -s -o /dev/null "$BASE/" && break
    sleep 0.1
done

check() {
    local expected="$1" path="$2"; shift 2
    local got
    got=$(curl -s -o /dev/null -w "%{http_code}" "$@" "$BASE$path")
    if [ "$got" = "$expected" ]; then
        printf "[ OK ] %-28s -> %s\n" "$path $*" "$got"
    else
        printf "[FAIL] %-28s -> esperado %s, recebido %s\n" "$path $*" "$expected" "$got"
        fails=$((fails + 1))
    fi
}

check 200 /
check 200 /css/style.css
check 200 /js/app.js
check 200 /docs/
check 301 /docs
check 404 /nao-existe.html
check 403 "/../../etc/passwd" --path-as-is
check 403 "/%2e%2e/%2e%2e/etc/passwd" --path-as-is
check 405 / -X POST
check 200 / -I
check 400 "/arquivo%zz"

echo
if [ "$fails" -eq 0 ]; then
    echo "Todos os testes de integração passaram."
    exit 0
fi
echo "$fails teste(s) de integração falharam."
exit 1
