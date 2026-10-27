# Verificação

Verificações locais em 26/10/2026 e CI em 27/10/2026, America/Fortaleza. O escopo é o código
independente do motor e a DLL de diagnóstico, sem instalação no CS 1.6 ou HLDS.

## Ambiente

- Windows x64 como host; DLL e teste de ABI compilados para x86.
- CMake 4.4.4 e Ninja 1.13.2.
- Clang, clang-tidy e clang-format 23.1.4, distribuição oficial com SHA-256 verificado.
- GCC 16.2.0 do MSYS2 UCRT64 para o núcleo em x64.
- Biblioteca MSVC 14.51.36231 e Windows SDK 10.0.28000.0 do ambiente instalado.

O Visual Studio local é a edição Insiders 18.11. O compilador Clang foi obtido
separadamente em release estável, mas a verificação local da DLL usa a biblioteca
e o SDK desse ambiente. O CI executou também em Visual Studio 2026 estável
18.11.1 com Clang 23.1.4. Os dois ambientes passaram nos checks desta fundação;
isso não declara compatibilidade com os motores.

## Resultados

| Verificação | Resultado |
| --- | --- |
| Configuração C++26 e detecção de `std::expected` | Passou em Clang e GCC |
| Build Windows x86 com warnings como erros e clang-tidy ativo | Passou |
| Teste `resourceCatalog` em x86 | 3130 checks, zero falhas |
| Teste `extensionApi` com carregamento dinâmico da DLL | 15 checks, zero falhas |
| Build e teste `resourceCatalog` em GCC x64 | 3130 checks, zero falhas |
| clang-format 23.1.4, modo `--dry-run --Werror` | Passou |
| Revisão de whitespace no staged diff | Passou |
| Inspeção PE com llvm-readobj | DLL COFF-i386, 32 bits, export `extendedPrecacheQueryInfo` sem decoração |
| GitHub Actions: build x86, clang-tidy, CTest e clang-format | Passou nos primeiros runs de push e PR para `5a96f33` |

O catálogo foi exercitado até 1024 entradas, com overflow explícito, deduplicação,
congelamento, posse das chaves, separação de categorias e validação de caminhos.
As fronteiras de codificação de 9, 12 e 32 bits são verificadas separadamente.
Os testes usam checks próprios ativos em qualquer configuração, sem `assert`.

Na preparação, a extração seletiva do toolchain estava sem `clang-scan-deps`.
A configuração do CMake falhou; a ferramenta foi incluída e o cache recriado.
Os achados de tipagem, operações de bits e conversão de ponteiro foram corrigidos
no código, mantendo os checks habilitados. Não há suppressions locais ou regras
desativadas para obter o resultado acima.

Evidências remotas: [push](https://github.com/BRUN0R2/ExtendedPrecache/actions/runs/40441784269)
e [PR](https://github.com/BRUN0R2/ExtendedPrecache/actions/runs/40441852829).
O histórico completo de revisão e integração está no
[PR #1](https://github.com/BRUN0R2/ExtendedPrecache/pull/1). Cada novo head e o
commit resultante em `main` devem passar nos checks exigidos por `RULES.md`.

## Limitações e pendências

- Não há integração de precache, plugin Metamod ou carregador do cliente nesta DLL.
- Não houve teste de jogo, conexão, mapa, demo, download ou stress do motor.
- A auditoria dos hashes selecionados é estática e tem fronteiras de cobertura
  explícitas; ver `engineAudit.md` e `auditMethod.md`.
- A DLL gerada pelo preset é Debug, destinada à validação local desta fundação.
- A assinatura SSH específica foi autorizada e configurada, e o commit de
  inicialização e o primeiro commit de código foram confirmados como Verified.

## Auditoria de 27/10/2026

- Rebuild Windows x86 em C++26, clang-tidy ativo e warnings como erros: passou.
  CTest: 3.130 checks de catálogo e 15 de ABI, zero falhas. clang-format em modo
  `--dry-run --Werror`: passou. Código de produção permaneceu inalterado.
- Ghidra 12.1.4: exportação integral das instruções/referências definidas dos dois
  artefatos, recuperação explícita do dispatcher e inspeção das funções dos CSVs;
  78 funções do cliente e 119 do servidor decompiladas, sem falhas nesses conjuntos.
- Scripts compilados e executados pelo Ghidra, e checados separadamente com
  `javac -Xlint:all -Werror`; particularidade do classpath em `auditMethod.md`.
- Censo de operandos e interiores de regiões cruzado com scan de cada janela
  de quatro bytes e relocations PE32. Métricas e referências sem código
  proprietário preservadas em `docs/audit/`.
- SHA-256 dos originais e das cópias de pesquisa conferidos. Arquivos do jogo e
  servidor preservados; sem execução de motores nem implementação de adaptadores.
- Fontes ReHLDS fixadas em `196300131858d1e087267fb2fb60cb935f8f8873`;
  `delta.lst` local identificado por hash, sem assumir configuração universal.
- Integridade dos CSVs, métricas de cobertura, requests por hash e 38 links locais
  conferida; nenhuma base de análise ou cópia de código recuperado incluída no Git.

Relatórios de protocolo e memória descrevem evidências estáticas e vetores de
aceitação futuros. Não representam testes reais de sign-on, efeitos, áudio,
consistência, demos ou uso de memória. A extensão funcional permanece pendente.
