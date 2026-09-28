# Servidor HTTP em C

[C](https://img.shields.io/badge/linguagem-C11-blue)
[POSIX](https://img.shields.io/badge/API-POSIX%20sockets-lightgrey)
[Testes](https://img.shields.io/badge/testes-11%20unit%C3%A1rios%20%2B%2011%20integra%C3%A7%C3%A3o-brightgreen)
[Licença](https://img.shields.io/badge/licen%C3%A7a-MIT-green)

Um servidor HTTP de arquivos estáticos desenvolvido em **C puro**, usando a **API de sockets POSIX**, sem frameworks ou bibliotecas externas.

A ideia deste projeto é entender, na prática, o que acontece quando um navegador acessa um site: como uma conexão TCP é criada, como uma requisição HTTP chega ao servidor, como ela é interpretada, como o arquivo é encontrado e, por fim, como a resposta volta para o navegador.

Além de fazer o servidor funcionar, o projeto também foi desenvolvido com foco em **segurança, testes e organização do código**.

---

## O que o servidor faz

* Suporta requisições **GET** e **HEAD**
* Trabalha com **HTTP/1.0 e HTTP/1.1**
* Identifica o **Content-Type** de arquivos como HTML, CSS, JavaScript, imagens, JSON e PDF
* Serve `index.html` automaticamente ao acessar um diretório
* Redireciona `/docs` para `/docs/` usando **301**
* Retorna páginas de erro para diferentes situações:

  * `400 Bad Request`
  * `403 Forbidden`
  * `404 Not Found`
  * `405 Method Not Allowed`
  * `414 URI Too Long`
  * `500 Internal Server Error`
  * `505 HTTP Version Not Supported`

### Segurança

O servidor possui algumas proteções importantes para evitar que uma requisição consiga acessar arquivos fora do diretório permitido:

* Bloqueio de **path traversal**, como `/../../etc/passwd`
* Detecção de tentativas codificadas, como `%2e%2e`
* Rejeição de byte nulo (`%00`)
* Validação de percent-encoding
* Uso de `realpath()` para verificar o caminho final e evitar acesso indevido por **symlinks**
* Limite para o tamanho dos cabeçalhos HTTP
* Timeout para evitar que clientes mantenham uma conexão aberta indefinidamente durante o envio da requisição

### Logs

Cada requisição é registrada no terminal com informações como:

* Data e hora
* Endereço IP
* Método HTTP
* Caminho solicitado
* Código de status
* Quantidade de bytes enviados

### Encerramento

O servidor trata `SIGINT` e `SIGTERM`, permitindo encerrá-lo de forma limpa com `Ctrl+C`.

A porta e o diretório servido também podem ser configurados pela linha de comando.

---

## Demonstração

```text
$ ./build/servidor -p 8080 -d www

Servindo '/home/user/servidor-http-c/www' em http://localhost:8080 (Ctrl+C para sair)

[2026-09-22 19:32:48] 127.0.0.1 "GET /" 200 947B
[2026-09-22 19:32:48] 127.0.0.1 "GET /css/style.css" 200 1101B
[2026-09-22 19:32:49] 127.0.0.1 "GET /nao-existe" 404 248B
[2026-09-22 19:32:51] 127.0.0.1 "GET /../../etc/passwd" 403 248B

^C

Servidor encerrado.
```

Depois de iniciar o servidor, basta acessar:

[**http://localhost:8080**](http://localhost:8080)

para visualizar o site de exemplo localizado em `www/`.

---

## Como compilar e executar

### Requisitos

* GCC ou Clang
* Make
* Linux, macOS ou Windows usando WSL
* `curl` para os testes de integração

Clone o repositório e compile:

```bash
git clone https://github.com/felippealex76-ctrl/servidor-http-c.git

cd servidor-http-c

make
```

O executável será criado em:

```text
build/servidor
```

Para iniciar o servidor usando a configuração padrão:

```bash
make run
```

Ou escolha a porta e o diretório manualmente:

```bash
./build/servidor -p 3000 -d /caminho/do/meu/site
```

Para ver todas as opções:

```bash
./build/servidor -h
```

---

## Testes

O projeto possui testes unitários e testes de integração para verificar tanto componentes individuais quanto o comportamento do servidor funcionando de verdade.

```bash
make test
```

Executa os **11 testes unitários**, cobrindo partes como:

* Parser HTTP
* Decodificação de URL
* Validação de caminhos
* Segurança
* Tipos MIME

Para executar os testes de integração:

```bash
make integration
```

Nesse caso, o servidor é iniciado e são feitas **11 requisições reais usando `curl`**.

Também existem comandos para ferramentas de análise:

```bash
make valgrind
```

Verifica possíveis vazamentos de memória.

```bash
make debug
```

Compila utilizando **AddressSanitizer** e **UndefinedBehaviorSanitizer**.

O projeto também utiliza:

```text
-Wall
-Wextra
-Wpedantic
-Werror
```

para manter o código mais rigoroso durante a compilação.

O **GitHub Actions** executa automaticamente o build, os testes, o Valgrind e os sanitizers a cada push.

---

## Como funciona

De forma simplificada, o fluxo de uma requisição é:

```text
Navegador                         Servidor HTTP

    │
    │  GET /css/style.css HTTP/1.1
    │─────────────────────────────────────▶
    │
    │                              1. accept()
    │                                 aceita a conexão TCP
    │
    │                              2. recv()
    │                                 recebe a requisição
    │
    │                              3. parser
    │                                 identifica método,
    │                                 caminho e versão
    │
    │                              4. validação
    │                                 verifica método e segurança
    │
    │                              5. stat() / open()
    │                                 localiza o arquivo
    │
    │                              6. send()
    │                                 envia cabeçalhos
    │                                 e conteúdo
    │
    │  HTTP/1.1 200 OK
    │  Content-Type: text/css
    │◀─────────────────────────────────────
    │
    │                              7. close()
    │                                 encerra a conexão
    │                                 e registra o acesso
```

---

## Estrutura do projeto

```text
servidor-http-c/
│
├── include/
│   ├── http.h          # Parser HTTP e validação de caminhos
│   ├── mime.h          # Tipos MIME
│   └── server.h        # Configuração e servidor
│
├── src/
│   ├── main.c          # Argumentos da linha de comando
│   ├── http.c          # Parser e tratamento HTTP
│   ├── mime.c          # Mapeamento de extensões
│   └── server.c        # Sockets, respostas, logs e sinais
│
├── tests/
│   ├── test_main.c     # Testes unitários
│   └── integration.sh  # Testes de integração
│
├── www/                # Site usado como exemplo
│
├── .github/
│   └── workflows/
│       └── ci.yml      # Pipeline de CI
│
├── Makefile
└── README.md
```

---

## O que aprendi com o projeto

Mais do que simplesmente criar um servidor que responde `200 OK`, o objetivo foi entender as partes que normalmente ficam escondidas quando usamos frameworks.

Algumas decisões importantes do projeto foram:

### Parser separado da rede

O código responsável por interpretar uma requisição HTTP não depende diretamente dos sockets.

Isso permite testar o parser isoladamente, sem precisar iniciar o servidor para cada teste.

### `send_all()`

Uma chamada para `send()` não garante que todos os bytes serão enviados de uma vez.

Por isso, o projeto possui uma função que continua enviando os dados até que todo o conteúdo seja transmitido, tratando também interrupções por `EINTR`.

### Arquivos enviados em blocos

Arquivos não são carregados inteiros para a memória antes do envio.

O servidor trabalha com blocos de **16 KB**, tornando o consumo de memória mais previsível.

### Proteção contra Path Traversal

A segurança não depende de uma única verificação.

O caminho passa por diferentes validações e, no final, `realpath()` é utilizado para confirmar que o arquivo realmente está dentro do diretório que o servidor pode disponibilizar.

### `SIGPIPE`

O servidor ignora `SIGPIPE` para evitar que uma conexão fechada pelo cliente durante o envio derrube o processo.

### `sigaction()`

O tratamento de sinais utiliza `sigaction()` sem `SA_RESTART`, permitindo que `accept()` seja interrompido quando o servidor recebe `Ctrl+C` e possa finalizar corretamente.

### Modelo inicial simples

O servidor utiliza um modelo **iterativo**, atendendo uma conexão por vez.

Essa escolha foi intencional: primeiro entender o funcionamento básico do servidor e só depois partir para concorrência.

---

## Próximos passos

Algumas ideias para evoluir o projeto:

* [ ] Atender várias conexões simultaneamente

  * `fork()`
  * threads
  * `poll()`
* [ ] Suporte a `Connection: keep-alive`
* [ ] Listagem automática de diretórios
* [ ] `Last-Modified` / `If-Modified-Since`
* [ ] Cache de arquivos
* [ ] Suporte a IPv6
* [ ] Melhorar o sistema de configuração
* [ ] Adicionar mais testes de segurança
* [ ] Medir desempenho e testar concorrência

---

## Autor

Desenvolvido por **Alex V. Felippe**

[LinkedIn](https://www.linkedin.com/in/alex-felippe-27b1a1262) · [GitHub](https://github.com/felippealex76-ctrl/servidor-http-c)
