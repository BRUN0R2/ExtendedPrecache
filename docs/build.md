# Compilação e verificações

## Pré-requisitos

- CMake 3.31 ou superior e Ninja.
- Compilador estável com modo C++26 e biblioteca com `std::expected`.
- Windows SDK e biblioteca C++ do MSVC para a DLL compilada com Clang para MSVC.
- Ambiente x86 e Clang completo, incluindo `clang-cl` e `clang-scan-deps`, no
  `PATH` para o preset `windowsX86`.
- `clang-format` e `clang-tidy` para formatação e análise estática.
- A configuração baixa `nlohmann/json` 3.12.0 com SHA-256 fixado; o cache do CMake
  pode ser reutilizado. Licença e origem em [thirdPartyNotices.md](thirdPartyNotices.md).
- O provider de SHA-256/CSPRNG usa CNG do Windows 10 ou superior. Outros sistemas
  podem compilar o núcleo injetando um provider; isso não declara suporte ao motor.

O projeto solicita C++26 obrigatório, desativa extensões e verifica `std::expected`
na configuração. Aceitar a flag de linguagem não significa implementar todos os
recursos de C++26. Não há fallback para C++23 ou outra versão.

## DLL Windows x86

Em um terminal de desenvolvimento x86, com Clang no `PATH`:

```powershell
cmake --preset windowsX86
cmake --build --preset windowsX86
ctest --preset windowsX86
```

A configuração falha se o alvo não for Windows de 32 bits. O teste da DLL carrega
o arquivo produzido e resolve o nome exportado sem depender da biblioteca de importação.
A DLL não deve ser instalada no Metamod nesta fase; ainda faltam as entradas desse SDK.

## Núcleo sem DLL

Permite validar domínio e sessão com um compilador C++26 em outra arquitetura:

```powershell
cmake -S . -B out/core -G Ninja -DCMAKE_CXX_COMPILER=g++ -DextendedPrecacheBuildDll=OFF
cmake --build out/core
ctest --test-dir out/core --output-on-failure
```

Essa configuração não produz uma DLL do motor e não comprova suporte de integração.
Os testes de configuração rodam em qualquer plataforma do núcleo; testes com CNG
e sessões completas são adicionados em Windows. Em Windows, ambos os builds
incluem esses testes, mesmo com a DLL desativada.

## Análise estática e formatação

```powershell
cmake --preset windowsX86 -DextendedPrecacheEnableAnalysis=ON
cmake --build --preset windowsX86
```

Use `clang-format --dry-run --Werror` nos arquivos `.hpp` e `.cpp` de `include`,
`src` e `tests`. A configuração de análise usa warnings como erros. Os testes
continuam ativos em builds Release, sem depender de `assert`.
No modo MSVC, `clang-tidy` recebe `/EHsc` também em seus argumentos de tooling,
para analisar o mesmo modo de exceções da compilação. Isso permite verificar as
fronteiras que convertem falhas de alocação em erros tipados. Não há checks desativados.

## Artefatos e pesquisa

`out/` contém builds descartáveis. `.local/` contém ferramentas e pesquisa local,
incluindo fontes de referência e evidências REA; ambos são ignorados no Git.
Não versionar executáveis do jogo, arquivos de análise ou o toolchain.

Não há variáveis de ambiente próprias da aplicação nesta etapa. Os presets usam
apenas o ambiente do compilador e não contêm caminhos pessoais.
