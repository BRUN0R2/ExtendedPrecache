# ExtendedPrecache

Extensão em C++26 para ampliar o precache do Counter-Strike 1.6 e do HLDS/ReHLDS,
com cliente e servidor trabalhando de forma coordenada.

**Estado: fundação em desenvolvimento. A DLL ainda não amplia o precache do motor
e ainda não é um plugin carregável pelo Metamod.** Não há versões do motor
declaradas como suportadas nesta etapa.

O objetivo é substituir limites fixos por capacidades configuráveis e verificadas,
respeitando memória, índices e protocolo. Não existe capacidade literalmente infinita.
Sprites de rede compartilham o domínio de modelos; sprites do HUD têm um cache
local separado. Sons, arquivos genéricos, eventos e decals também fazem parte da
auditoria estática concluída para os dois hashes selecionados. Texturas,
entidades e lightstyles possuem restrições relacionadas, mas não são
automaticamente resolvidos pelo precache.

## Documentação

- [Plano de trabalho e estado real](docs/TODO.md)
- [Regras obrigatórias](docs/RULES.md)
- [Escopo e critérios de aceitação](docs/scope.md)
- [Arquitetura e distribuição](docs/architecture.md)
- [Evidências e questões abertas](docs/research.md)
- [Auditoria dos consumidores e ciclos de vida](docs/engineAudit.md)
- [Auditoria do protocolo e fronteiras de índices](docs/protocolAudit.md)
- [Orçamento de memória das estruturas](docs/memoryBudget.md)
- [Compilação e verificações](docs/build.md)
- [Contrato público da DLL](docs/abi.md)
- [Resultados e limites da verificação](docs/verification.md)

## Primeira compilação

Requer CMake 3.31 ou superior, Ninja e compilador estável com modo C++26 e
`std::expected`. A DLL requer Windows e toolchain **x86**; testes do núcleo
podem executar em outra arquitetura com `extendedPrecacheBuildDll=OFF`.

```powershell
cmake --preset windowsX86
cmake --build --preset windowsX86
ctest --preset windowsX86
```

Execute em um ambiente de desenvolvimento x86 com Clang disponível no `PATH`.
Veja [build.md](docs/build.md) para os pré-requisitos e as alternativas.

O servidor será distribuído como plugin do Metamod; o cliente precisará de um
componente complementar. A instalação final depende dos adaptadores ainda pendentes.
Nenhum instalador ou patch dos arquivos originais está incluído.

## Contribuição

Leia [RULES.md](docs/RULES.md) antes de alterar o projeto. Mudanças de código devem
incluir documentação e verificações pertinentes. Não envie binários do jogo,
segredos ou bases de engenharia reversa. A licença do código original é MIT;
referências de pesquisa não são dependências incorporadas ao produto.
