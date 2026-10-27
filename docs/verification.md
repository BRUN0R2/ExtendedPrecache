# Verificação da fundação

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
- A pesquisa REA é estática e parcial; ver `research.md`.
- A DLL gerada pelo preset é Debug, destinada à validação local desta fundação.
- A assinatura SSH específica foi autorizada e configurada, e o commit de
  inicialização e o primeiro commit de código foram confirmados como Verified.
