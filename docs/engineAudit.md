# Auditoria estática dos motores

Data: 27/10/2026. Parecer: **o protocolo original não comporta uma ampliação
transparente por troca de constantes ou interceptação das funções de precache**.
A auditoria dos artefatos abaixo está encerrada com os limites e as fronteiras
registrados. Nenhum adaptador, hook, patch ou capacidade ampliada foi aprovado.

## Identidade e alcance

| Papel selecionado | Arquivo | SHA-256 |
| --- | --- | --- |
| Cliente remoto Steam, Windows x86 | `hw.dll`, 3.598.176 bytes | `9ba9a2db5e07598fd59afa35507a98c86162e4e15b3835177b78c11842cd2295` |
| Servidor indicado, Windows x86 | `REHLDS-Rezombie/swds.dll`, 1.212.208 bytes | `f6c552ca46e933c6d02ee4fb888cc99bbe8f8aab04a4afbe9ff45d74f896cd02` |

O servidor informa `3.15.0.896-dev` / `1963001` em strings. A fonte primária usada
na auditoria é o commit público
[`196300131858d1e087267fb2fb60cb935f8f8873`](https://github.com/rehlds/ReHLDS/tree/196300131858d1e087267fb2fb60cb935f8f8873).
O texto de versão não autentica a procedência do executável. As observações
nativas confirmam separadamente as capacidades e os caminhos descritos aqui.

Não foi atribuído um build numérico ao cliente por tamanho de arquivo. O provedor
8684 do NextClient examinado seleciona tamanhos diferentes: seus endereços não
são perfis deste cliente. HLDS Valve, Linux, listen server do `hw.dll`, HLTV,
GameDLLs, plugins e leitores externos de demos não estão certificados por esta
auditoria. As fronteiras com esses componentes estão registradas, sem presumir
que os seus consumidores aceitem índices maiores.

Esta é evidência estática. Não houve execução dos motores, conexão de clientes,
teste de mapa ou medição de memória residente. Os originais foram preservados.

## Inventário verificável

- [Perfis, regiões, contagens e cobertura](audit/profiles.json).
- [Referências do cliente](audit/clientReferences.csv) e
  [do servidor](audit/serverReferences.csv): endereço da observação, função
  proprietária quando reconhecida, valor e origem da referência; sem instruções
  ou pseudocódigo proprietário.
- [313 linhas de referência na fonte ReHLDS](audit/serverSourceReferences.csv):
  caminho, linha e símbolo na revisão fixada, sem copiar a implementação.
- [Raízes do dispatcher do cliente](audit/clientServiceRoots.json),
  [consumidores do cliente](audit/clientConsumerRoots.json) e
  [consumidores do servidor](audit/serverConsumerRoots.json).
- [Protocolo e vetores de fronteira](protocolAudit.md),
  [orçamento de memória](memoryBudget.md) e
  [método de reprodução](auditMethod.md).

Os CSVs contêm 350 combinações região/endereço/valor no cliente e 674 no servidor,
agrupadas em 78 e 119 funções reconhecidas, respectivamente. Todas essas funções
foram decompiladas e inspecionadas; as contagens incluem referências de contexto
e ao endereço imediatamente posterior a um campo. **Não são 197 hooks necessários.**
`onePastEnd=true` é um candidato a limite de laço ou ao início do campo vizinho;
não deve ser contado automaticamente como acesso à tabela anterior.

A seleção cobre operandos e referências para todo o interior de cada região,
contadores, hashes, caches e bases de estruturas usadas no reset. A busca binária
complementar examinou cada janela de quatro bytes dos arquivos, incluindo
referências não alinhadas e relocations PE32 `HIGHLOW`. O método e as lacunas do
desassemblador estão quantificados; uma busca vazia por uma base não é prova de
ausência de consumidores.

## Tabelas, produtores e consumidores

Endereços são VAs nas bases de análise `0x10000000` / `0x04970000`, válidos apenas
para os hashes selecionados. Nomes nativos sem símbolos são correspondências
semânticas com a fonte e os diagnósticos; não são protótipos de ABI aprovados.

| Domínio | ReHLDS observado | Cliente observado | Contrato que precisa ser preservado |
| --- | --- | --- | --- |
| Modelos e sprites de rede | Nomes em `0x04d3d940`, objetos em `0x04d3e140`, flags em `0x04d3e940`; 512 slots | Ponteiros em `0x113f6410`; 512 slots; contador em `0x113f6c10` | Zero reservado; mundo em 1; modelos BSP `*n` também consomem slots |
| Sons de rede | 512 ponteiros em `0x04d3fb40`; hash com 1.023 entradas `short` em `0x04d40340`, flag de construção em `0x04d40b40` | 512 ponteiros em `0x113f6c14` | Índice zero e namespace de sentenças não equivalem a um arquivo de som comum |
| Eventos | 256 registros de 16 bytes em `0x04d3eb40` | 256 registros de 16 bytes em `0x113f5410` | Zero reservado; nome e script têm ownership próprio; API usa `unsigned short`, mensagens usam 10 bits |
| Genéricos | Caminho ativo `REHLDS_FIXES`: 4.096 nomes de 64 bytes em `0x05010690`, contador em `0x05050690` | Entradas da lista ligada de download; não há array dedicado no precache final | Primeiro índice zero é válido; normalização de barras e deduplicação sem distinguir caixa no servidor |
| Decals base | 512 nomes de 17 bytes em `0x04d53c10`, contador em `0x04d53c0c`; WAD apontado por `0x04c72ae4` | 512 nomes de 16 bytes em `0x11249f00`; WAD/cache e texturas são outra camada | Nomes de até 15 bytes mais NUL; IDs base iniciam em zero; sprays customizados têm representação própria |
| Lista global de recursos | 4.096 registros de 136 bytes em `0x04f88690`, contador em `0x04d38138` | Nós de 136 bytes alocados e ligados ao estado do cliente | Ordinal da lista global difere de `nIndex` da categoria |
| Consistência | 512 registros de 44 bytes em `0x04d3813c` | 512 registros de 44 bytes em `0x113f7414` | Lista de verificação e resposta possuem limites adicionais aos de precache |

O layout legado mantém genéricos de 512 slots e `resourcelist` de 1.280 registros.
Eles não descrevem o armazenamento ativo alternativo de `REHLDS_FIXES` observado
neste servidor. No cliente, os 1.280 registros em `0x11258074` são a lista de
**customizações enviada pelo cliente**; não limitam a lista ligada recebida do
servidor. `t_skin` é ignorado pelo precache final do cliente, e `t_world` é um
tipo artificial na declaração pública, não um novo domínio com capacidade própria.

### Fechamento dos caminhos do servidor

| Caminho | Evidência nativa principal | Consumidores em fonte e consequência |
| --- | --- | --- |
| Registro de modelos | `0x0499ea30` | `PF_precache_model_I_internal`, `SV_LookupModelIndex`, `SV_ModelIndex`, `PF_setmodel_I`, consistência, baseline/deltas, colisão/PVS/movimento, carregamento BSP e API ReHLDS |
| Registro e procura de sons | `0x0499d950` | `PF_precache_sound_I_internal`, `SV_BuildSoundMsg`, `SV_LookupSoundIndex`, construção/inserção no hash, som ambiente e criação de recursos |
| Eventos | `0x0499dc60` | `EV_Precache_internal`, `EV_Playback`, envio confiável/não confiável, argumentos delta, criação de recursos e liberação dos scripts |
| Genéricos | `0x0499ed60` | `PF_precache_generic_I_internal`, manifesto `.res`, supressão de duplicatas de modelos/sons, criação de recursos e reset do contador |
| Decals | `0x0497c3f0`, `0x0499f4b0` | `Decal_Init`, merge de WADs, `PF_DecalIndex`, `Draw_DecalCount/Name/Size`, criação de recursos, encerramento e getters/setters públicos |
| Lista global | `0x049b2b50`, hook em `0x049b2bc0`, montagem em `0x049b33a0` | `SV_AddResource_internal`, `SV_CreateResourceList`, envio, localização por tipo, download/consistência, estatísticas e API |
| Diagnóstico | `0x0498cee0`, `0x04993b80` | Listagem de recursos e `Mod_Print` também percorrem arrays; não podem conservar bases/capacidades antigas |

Os consumidores de `g_psv.models` aparecem também em `sv_phys.cpp`, `sv_pmove.cpp`,
`world.cpp` e no carregador de modelos. Retornar um `model_t*` transfere o consumo
para carregamento, colisão ou renderização; o objeto precisa permanecer estável
durante esse uso. O CSV de fonte inclui os acessos às tabelas, seus auxiliares e
as declarações públicas; não se limita a procurar mensagens de overflow.

Fontes primárias: [precache e eventos](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/pr_cmds.cpp#L1013),
[lista e ciclo de vida](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/sv_main.cpp#L5375),
[decals](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/decals.cpp#L526).

### Fechamento dos caminhos do cliente

| Caminho | Entradas e observações |
| --- | --- |
| Entrada de recursos | `svc_resourcelist` aponta para wrapper `0x101aa270`, corpo `0x101a7690`; lê 12 bits de contagem, aloca nós, lê metadados, liga a lista e chama consistência em `0x101a6ef0` |
| Download e finalização | `0x101a62c0`, `0x101a4870`, `0x101a44c0`; movimentação/liberação dos nós, arquivo ausente, FastDL e callbacks precedem o preenchimento das tabelas |
| Modelos | `0x101a44c0` limita o contador a 512, mas usa `nIndex` na escrita; `0x101a2fa0` faz resolução/carregamento tardio; baseline em `0x101a6c60` e consumidores de entidades, desenho, sprites e efeitos usam os objetos resultantes |
| Sons | `0x101a44c0` carrega e escreve; som dinâmico em `0x101a5e10`, estático em `0x101a8410`; reprodução/cache local segue o descritor `sfx_t*` |
| Eventos | Registro do script e nome em `0x101a44c0`; eventos confiáveis/não confiáveis são decodificados e encaminhados aos hooks pelo nome; índice da fila não é a capacidade do catálogo |
| Decals | `0x101bf490` escreve nomes de 15 bytes com limite 512; resolução por nome/índice, textura/WAD, sprays customizados e aplicações de TE usam caminhos distintos |
| Diagnóstico | Comando de modelo em `0x101d0920` escreve no precache; `Mod_Print` em `0x102416e0` percorre o cache; ambos foram recuperados por referências positivas de registro de comandos |

A limitação do contador de modelos não valida o índice usado na escrita. O
parser recuperado não demonstra validação por categoria antes de todas as
escritas de sons, modelos e eventos. A futura fronteira deve validar tipo,
índice, contagem, duplicatas, comprimento e bytes antes de alterar estado.

### Caches adicionais e ownership

- **Modelos conhecidos:** 1.024 descritores de 392 bytes em ambos os motores,
  com 1.024 registros CRC de 12 bytes. Bases: cliente `0x10dd5e40` /
  `0x109860f0`; servidor `0x04c75e70` / `0x04c72e38`. Contadores:
  `0x109860e4` e `0x04c75e6c`. `Mod_FindName` usa slots livres/reutilizáveis;
  falha quando não há descritor disponível. Helpers CRC calculam o ordinal por
  diferença de ponteiros, com stride 392. Recursos locais também usam esse cache.
- **Mapa de nomes ReHLDS:** `modelsMap` tem 128 buckets e 1.024 nós, isto é,
  `MAX_MODELS * 2`; `put` retorna falso ao esgotar os nós. Os chamadores de registro
  não transformam esse resultado em uma nova capacidade. Ampliar o array e manter
  esse mapa deixaria a resolução por nome incompleta. Fonte:
  [layout e mapa](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/server.h#L149).
- **Áudio local:** o cliente aloca 73.728 bytes para 1.024 descritores de 72 bytes
  em `S_Init` (`0x101fd6b0`); ponteiro `0x111b1a34`, contador `0x111b1a30`.
  `S_FindName` (`0x101fd540`) deduplica, reutiliza entradas de gerações anteriores
  e falha em 1.024 se não houver slot reutilizável. O nome tem até 63 bytes.
  A carga do sample/PCM e os canais de reprodução não são a tabela de rede.
- **Sprites do HUD:** `SPR_Init` (`0x1019cf40`) aloca 256 registros de 12 bytes;
  ponteiro `0x1032e5bc`, capacidade `0x1032e5c0`. `SPR_Load`
  (`0x1019cf80`) retorna handle `posição + 1`, usa `Mod_ForName` e falha ao
  esgotar a lista. `SPR_Set`, dimensões, frames e crosshair resolvem o handle;
  desenho usa o sprite já selecionado. Encerramentos `0x1019d140` e
  `0x1019d1d0` diferem na liberação das texturas. Ambos dependem de uma flag de
  inicialização. Isso é observação do binário; não foi aplicado o ajuste de
  encerramento existente no NextClient.

Sprites de rede compartilham o precache de modelos. Handles de HUD são outro
namespace, outro ciclo de vida e outro limite. Contagem de decals visíveis,
texturas/GPU, canais de som, entidades, partículas e limites dos formatos BSP,
MDL, SPR e WAV não se tornam ilimitados ao ampliar estes catálogos.

## Ciclo de vida e fronteiras públicas

No servidor, `SV_CreateResourceList` enumera genéricos desde zero, sons/modelos
desde um, decals desde zero e eventos desde um. Não é uma simples cópia do
catálogo de domínio. `SV_ClearCaches` libera nomes/scripts de eventos até a
primeira lacuna; `SV_SpawnServer` zera `server_t`, limpa `modelsMap` e reinicia o
contador de genéricos. Mundo e inline models são repovoados. O buffer
`localmodels[MAX_MODELS][5]` também depende da capacidade e da representação
`*n`; não comporta arbitrariamente mais dígitos. O cache de modelos pode manter
descritores marcados para recarga, em vez de liberar todos os objetos a cada mapa.

No cliente, `CL_PrepFileTransfers` (`0x101a18d0`) libera customizações e scripts
de eventos, zera **0x1b0e70 bytes** de estado a partir de `0x11257f60` e
reinicializa as sentinelas das listas ligadas. `CL_ClearState` (`0x101a1ae0`)
chama esse caminho, pode passar por `Host_ClearMemory` (`0x101d2590`) e reinicia
outros pools. `CL_ParseServerInfo` (`0x101a78c0`) inicia esse reset antes de
verificar o protocolo 48. Uma negociação que guarde estado dentro dessa região
seria apagada. Negociação, mapa e geração dos handles precisam de ownership
explícito; não basta alocar um vetor global maior.

`IRehldsServerData` expõe dados, contagens e ponteiros para consumidores externos.
`GetModel` (`0x049d8e40`), `GetEventPrecache` (`0x049d8e60`) e `GetConsistency`
(`0x049d8e20`) mantêm limites 512/256/512. `GetResource` (`0x049d8d00`) calcula
`base + 136 * index` sem validação; setters de contagem também permitem alterar
o estado sem ampliar storage. Isso exige contrato versionado e validação na
fronteira, sem alterar silenciosamente o significado da ABI atual. Fonte:
[implementação pública](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/rehlds/rehlds_interfaces_impl.cpp#L660).

O binário usa otimizações de chamadas internas, inclusive parâmetros em
registradores que não podem ser inferidos apenas da assinatura C++ da fonte.
Callbacks da GameDLL, ClientDLL, API de estúdio, eventos, plugins, mensagens de
mods e DemoPlayer são fronteiras indiretas, não candidatos automaticamente
seguros para hooks. Estruturas legadas e ponteiros já entregues não podem mudar
de layout ou endereço sem cobrir os consumidores correspondentes.

## Decisão para o próximo marco

A etapa seguinte é **projetar** os contratos, sem ativar adaptadores: capacidades
por categoria e por sessão, conversão entre índices zero/um, armazenamento com
endereços estáveis, orçamento por bytes, negociação anterior ao primeiro estado
incompatível e contratos de deltas/efeitos/consistência. O modo legado precisa
validar o menor limite efetivo de todos os caminhos utilizados pelo par.

O catálogo atual é propositalmente independente e começa em um; não representa
diretamente genéricos, decals ou ordinais de recursos. O projeto deve introduzir
essas traduções explicitamente, sem reutilizar um zero ambíguo como sucesso/erro.
Antes de implementar cada integração, seu desenho deverá resolver as restrições
deste relatório, definir a ABI exata e os testes em instalação isolada. A auditoria
não aprova uma capacidade numérica universal nem suporte a builds adicionais.

O desenho seguinte está registrado em [índices](indexMapping.md),
[ownership](storageOwnership.md), [ciclo de mapa](mapLifecycle.md) e
[ABI de integração](integrationAbi.md). A [matriz de verificação](integrationVerification.md)
relaciona as restrições acima às decisões e às provas nativas ainda pendentes.
Isso encerra a especificação desses contratos, sem aprovar um hook ou adaptador.
