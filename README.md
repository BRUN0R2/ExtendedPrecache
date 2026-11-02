# ExtendedPrecache

> [!WARNING]
> **Risco de banimento VAC.** O adaptador do cliente altera a memória do jogo em execução, e o VAC
> pode tratar isso como trapaça. [Não há apelação](https://help.steampowered.com/faqs/view/571A-97DA-70E9-FF74):
> o Suporte Steam não remove banimentos VAC. O uso é por sua conta e risco.
>
> - Nunca use em servidores protegidos por VAC.
> - Nunca use na sua conta principal, nem em uma conta que jogue com a biblioteca compartilhada dela.
> - Use uma cópia separada do jogo, fora da pasta da Steam, e não coloque arquivos deste projeto
>   na pasta da Steam.
>
> O projeto ainda não foi testado contra o VAC real. Mesmo depois de testado, não ser banido não
> prova segurança: um banimento pode vir muito depois, e a detecção muda com o tempo.
>
> **VAC ban risk.** The client adapter changes the memory of the running game, and VAC may treat
> that as cheating. [There is no appeal](https://help.steampowered.com/faqs/view/571A-97DA-70E9-FF74):
> Steam Support does not remove VAC bans. Use it at your own risk. Never use it on VAC-secured
> servers, on your main account, or on an account that borrows its library. Use a separate copy of
> the game outside the Steam folder, and keep this project's files out of that folder. This project
> has not been tested against real VAC yet, and not being banned would not prove it safe.

Extensão em C++26 para ampliar o precache do Counter-Strike 1.6 e do HLDS/ReHLDS,
com cliente e servidor trabalhando de forma coordenada.

**Estado: fundação em desenvolvimento. Nenhum artefato publicado amplia o precache do motor.**
O [plugin Metamod](docs/metamodPlugin.md) já carrega no host, mas é inerte: sem um perfil
aprovado, e não há nenhum, não instala callbacks nem hooks. Não há versões do motor declaradas
como suportadas nesta etapa. As [tabelas de modelos](docs/modelTables.md) já passam do limite
nativo em testes locais com perfil sintético; os outros recursos seguem nos limites originais.

O validador de configuração, o codec de controle e a máquina de estados da
negociação 1.0 estão implementados e testados em um núcleo C++26 isolado.
Os codecs de dados 2.0 e os preflights semânticos de aplicação também estão
implementados em um alvo estático isolado, incluindo deltas, consistência,
efeitos, demos e mensagens de módulos. A integração nativa e os adaptadores
continuam pendentes; nenhum perfil de motor foi habilitado.

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
- [Capacidades, configuração e admissão](docs/capabilities.md)
- [Protocolo de negociação 1.0](docs/negotiation.md)
- [Bootstrap, barreira e limites de transporte](docs/transportBootstrap.md)
- [API e ciclo de vida do núcleo de sessão](docs/sessionCore.md)
- [Barreira do sign-on por conexão](docs/signonBarrier.md)
- [Negociação no cliente](docs/clientNegotiator.md)
- [Tabelas de modelos acima do limite nativo](docs/modelTables.md)
- [Entradas e ciclo de vida do plugin Metamod](docs/metamodPlugin.md)
- [Conversão de índices e namespaces](docs/indexMapping.md)
- [Ownership e estabilidade dos endereços](docs/storageOwnership.md)
- [Ciclo de mapa, conexão e reset](docs/mapLifecycle.md)
- [ABI de integração 2.0 planejada](docs/integrationAbi.md)
- [Verificação dos contratos de integração](docs/integrationVerification.md)
- [Plano de dados 2.0 e limites por bytes](docs/dataCodecs.md)
- [API, ownership e limites do núcleo de dados](docs/dataCore.md)
- [Catálogo, assets e consistência](docs/catalogCodec.md)
- [Schemas e frames delta transacionais](docs/deltaCodec.md)
- [Áudio, eventos e efeitos](docs/effectCodecs.md)
- [Container de demos estendidas](docs/demoCodec.md)
- [Contratos de mods e módulos externos](docs/moduleContracts.md)
- [Verificação dos contratos e vetores offline](docs/contractVerification.md)
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

O servidor será distribuído como plugin do Metamod; o cliente, como uma DLL aberta por um
[inicializador próprio](docs/clientNegotiator.md#inicializador), que não é para servidores com VAC.
A instalação final depende dos adaptadores ainda pendentes;
a [instalação de diagnóstico](docs/metamodPlugin.md#instalação-de-diagnóstico) não amplia nada.
Nenhum instalador ou patch dos arquivos originais está incluído.

## Contribuição

Leia [RULES.md](docs/RULES.md) antes de alterar o projeto. Mudanças de código devem
incluir documentação e verificações pertinentes. Não envie binários do jogo,
segredos ou bases de engenharia reversa. A licença do código original é MIT;
referências de pesquisa não são dependências incorporadas ao produto.
Testes offline/locais, seus arquivos e resultados ficam em área local ignorada;
não os publique em commits, pushes ou PRs.
O parser JSON privado e sua licença estão em [thirdPartyNotices.md](docs/thirdPartyNotices.md).
