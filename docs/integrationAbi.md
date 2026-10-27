# ABI de integração 2.0 — especificação

Estado: **planejada e indisponível**, sem export novo, implementação de serviço
ou adaptador. O artefato [integrationAbiV2.h](contracts/integrationAbiV2.h) contém
somente declarações C para verificar contratos/layouts. Fica fora de `include/`
e de todos os alvos de produção. A DLL atual mantém apenas
`extendedPrecacheQueryInfo`, ABI diagnóstica 1, 28 bytes, estágio foundation e
`engineExtensionActive=0`. Os testes devem provar a ausência do novo export.

## Versionamento e migração

A futura entrada é `extendedPrecacheGetIntegrationApi`, linkage C e nome exportado
sem decoração por `.def`, `cdecl` no Windows x86. Recebe quatro argumentos:
`requestedMajor:u32`, `requestedMinor:u32`, `outputBytes:u32`, `output:Api*`;
retorna `EpcAbiStatusV2`, representado por u32, não enum de tamanho implícito.
Para esta especificação exigir **major=2, minor=0, ponteiros de 32 bits e Windows
x86**. C++26 é interno; consumidores C não precisam de STL ou de um CRT comum.

A consulta copia a tabela POD de funções, um contexto opaco já criado pelo host
aprovado e um lease de API com serial único. Esse lease prende o serviço até
`releaseApi`; a query só reserva seu registro em storage previamente orçado,
sem inicializar o motor ou executar alocação não cotada. Sem registro disponível,
retorna `budgetExceeded`. Não registra hooks, carrega config, inicia mapa ou ativa precache.
Sem runtime/profile aprovado, retorna `notReady`/`unsupportedProfile`. Um host
sem mapa pode oferecer a tabela, mas suas consultas de mapa retornam `notReady`.
Receber a tabela não comprova prontidão nativa ou compatibilidade de um cliente.

Esta é uma interface nova e incompatível com a consulta diagnóstica: nunca fazer
cast de `ExtensionInfo*` para `EpcIntegrationApiV2*`, aumentar `ExtensionInfo` em
silêncio ou passar major 2 como enum v1 do export antigo. A futura distribuição
funcional deve preservar o símbolo e a assinatura diagnóstica v1, mas responder
`unsupportedAbi`, sem escrita, à consulta v1 quando deixar de ser um artefato
foundation. Não fabricar `foundation/0` para um artefato funcional. Essa retirada
explícita da capacidade diagnóstica v1 é uma migração de major **apenas planejada**;
nenhum consumidor atual foi alterado. Clientes novos descobrem o símbolo da ABI 2
e validam sua tabela; ausência/falha nunca autoriza continuar uma sessão ampliada.

Major diferente é incompatível. Minor adiciona apenas funções no final da tabela,
sem alterar as funções, structs, códigos ou significados existentes. Novos formatos
de registro precisam de novos tipos/operações, não de escrita adicional nos tipos
V2 desta versão. Cada provider futuro deve suportar explicitamente os minors que
anuncia: pedido 2.0 devolve exatamente o prefixo 2.0 e `abiMinor=0`, mesmo que o
buffer seja maior. Minor desconhecido é `unsupportedAbi`, sem downgrade implícito.
Mudança de função/layout/calling convention ou de semântica exige outro major.

ABI 2.0, schema de configuração 1, negociação 1.0, contrato de aplicação, contrato
de dados e API ReHLDS são versionamentos independentes. Este documento não altera
IDs/digests dos exemplos nem marca seus perfis como disponíveis.

## Layout fixo e tamanhos

Inteiros u32 e arrays de bytes, `pack(push,4)` restrito às declarações desta ABI
e `pop` ao final. Não aplicar packing ao SDK/motor nem inferir layout interno.
Geração e serial u64 usam duas palavras `low/high`: valor lógico
`(u64(high) << 32) | low`; tipos distintos impedem confundir serial, geração e
map epoch. Bytes de nonce/digest mantêm sua ordem declarada. Isso é ABI local,
não serialização de rede ou arquivo; não enviar dumps desses structs.

| Tipo | Bytes / alinhamento x86 | Campos e offsets principais |
| --- | --- | --- |
| `EpcRuntimeNonceV2` | 16 / 1 | `bytes=0` |
| `EpcGenerationV2`, `EpcLeaseSerialV2`, `EpcMapEpochV2` | 8 / 4 | `low=0`, `high=4` |
| `EpcMapKeyV2` | 24 / 4 | `runtime=0`, `generation=16` |
| `EpcSnapshotHandleV2` | 24 / 4 | `runtime=0`, `serial=16` |
| `EpcApiLeaseV2` | 24 / 4 | `runtime=0`, `serial=16`; tipo e registro distintos da lease de snapshot |
| `EpcResourceRefV2` | 32 / 4 | `map=0`, `kind=24`, `domainId=28` |
| `EpcMapInfoV2` | 104 / 4 | `structSize=0`, `map=4`, `phase=28`, counts=32…52, `mapEpoch=56`, `spawnCount=64`, `reserved=68`, digest=72 |
| `EpcResourceRecordV2` | 52 / 4 | `structSize=0`, `resource=4`, `resourceOrdinal=36`, `categoryIndex=40`, `nameBytes=44`, `reserved=48` |
| `EpcIntegrationApiV2` | 80 / 4 | header u32=0…20, `context=24`, `lease=28`, funções=52,56,60,64,68,72,76 |

`mapEpoch` é a identidade recebida/publicada pelo acordo, não a geração local.
`kind=1…5`, papéis `server=1/client=2` e fases 0…7 são os registros normativos dos
documentos de [índices](indexMapping.md) e [mapa](mapLifecycle.md); não são casts
dos enums C++ existentes. Contagens são entradas efetivamente congeladas, não
capacidades. `categoryIndex` é a tradução do ID da categoria, não slot K/A/H.
`resourceOrdinal` é lógico e não deve ser usado diretamente na consistência legada.

Uma compilação x64 do artefato serve para verificar structs independentes de
ponteiro; não torna a tabela x64 uma ABI suportada. O teste x86 verifica seus
offsets exatos. Pack não converte uma arquitetura em outra.

## Operações somente de metadados

Todas recebem `context` e a lease de API da tabela. O contexto é um identificador opaco do serviço,
nunca `model_t*`, `sfx_t*`, `client_t*` ou endereço de uma região do motor.

| Função | Contrato |
| --- | --- |
| `getCurrentMap(context, api, bytes, output)` | Copia `MapInfo` de metadados completos em frozen/loading/active. Sem mapa publicável: `notReady`/`wrongPhase`. Não cria lease de snapshot |
| `acquireSnapshot(context, api, expectedMap, output)` | Compara nonce/geração com o mapa corrente e sua fase, reserva lease do snapshot pertencente à lease de API e devolve serial novo; impede troca entre consulta e aquisição |
| `findResource(context, api, snapshot, kind, name, nameBytes, output)` | Procura em uma categoria real, com validação/canonicalização do perfil. Retorna `ResourceRef` completo ou `notFound`, sem registrar recursos ou criar genéricos para aliases |
| `readResource(context, api, snapshot, resource, bytes, output)` | Exige referência do mesmo mapa/lease, kind e ID válidos; copia `ResourceRecord`, sem ponteiro para nome/asset |
| `copyResourceName(context, api, snapshot, resource, output, bytes)` | Copia nome imutável e NUL; exigir capacidade `record.nameBytes+1`; copiar exatamente esse comprimento e preservar a cauda |
| `releaseSnapshot(context, api, snapshot)` | Consome a lease e zera o handle do chamador no sucesso. Permitido após retirada do mapa; repetição/cópia de handle já liberado retorna `staleHandle` |
| `releaseApi(context, api)` | Consome e zera a lease de API somente sem snapshots seus retidos nem chamadas concorrentes; caso contrário `busy`. Depois do sucesso nenhuma cópia dessa tabela/contexto pode voltar a ser chamada |

Para enumerar, obter um lease e percorrer IDs 1…count da categoria no `MapInfo`
correspondente, construindo referências com seu `MapKey`. Nunca guardar um ID
nu e associá-lo depois ao mapa novo. Snapshot antigo retido recusa leituras após
retirada, embora continue existindo até a liberação; não mantém assets nativos
vivos. Uma conexão encerrada não pode liberar o mapa compartilhado.

Nomes são bytes segundo o perfil de caminhos, não conversão UTF-8 implícita do
filesystem. Entrada tem comprimento explícito, sem NUL embutido, vazios ou
truncamento; validar antes de copiar. Máximo 63 bytes, ou 15 para decal, sem NUL.
Não ler `name[nameBytes]`. Normalização/deduplicação não podem trocar a categoria
de uma saída `ResourceRef`. Nomes e metadados de saída pertencem ao buffer do
chamador; nenhum `free` atravessa CRTs. O serviço não retém esses buffers.

## Validação, erros e execução

Pré-condição do chamador: ponteiros não nulos devem referir memória válida,
acessível e alinhada durante a chamada; ranges de entrada/saída não se sobrepõem.
Não existe validação geral de ponteiro arbitrário por `IsBadReadPtr`, SEH ou
tentativa de leitura. Após isso, validar na ordem:

1. Nulos/argumentos estruturais, versão e arquitetura na consulta inicial.
2. Tamanho mínimo dos outputs, antes de qualquer escrita.
3. Contexto live da tabela, thread proprietário e ausência de reentrada.
4. Nonce/serial e tipo das leases de API/snapshot, vínculo entre ambas, mapa/geração
   correntes e fase permitida. Um consumidor não libera o snapshot de outro.
5. Categoria, ID, nome/comprimento e limites específicos da operação.
6. Reservas/cópia/commit da operação; outputs publicados somente no sucesso.

Na query, papel/contexto disponíveis e profile aprovado também são necessários;
não devolver tabela parcial. Todos os sete ponteiros de função são obrigatórios,
reservados zero e `structSize=80`, com lease não zero válida. Um consumidor valida major/minor, pointerBits,
papel, tamanho e campos obrigatórios antes de chamar. Tamanhos maiores não autorizam
ler/escrever além do prefixo conhecido. `getCurrentMap` escreve 104 bytes e
`readResource` escreve 52, independentemente da sobra do buffer.

| Código u32 | Status | Uso |
| --- | --- | --- |
| 0 | `success` | Todos os outputs definidos; índice nativo zero pode ser válido |
| 1 | `invalidArgument` | Nulo, comprimento inválido ou argumento estrutural inválido |
| 2 | `unsupportedAbi` | Major/minor sem implementação explícita |
| 3 | `unsupportedArchitecture` | Alvo diferente de Windows x86 / ponteiros de 32 bits |
| 4 | `outputTooSmall` | Buffer insuficiente, sem escrita parcial |
| 5 | `notReady` | Serviço/mapa ainda não disponível |
| 6 | `unsupportedProfile` | Identidade local ou consumidores não aprovados |
| 7 | `wrongThread` | Thread incorreto ou reentrada proibida |
| 8 | `wrongPhase` | Operação não autorizada no estado do mapa |
| 9 | `staleHandle` | Runtime/serial de lease inválido, liberado ou desconhecido |
| 10 | `staleMap` | Geração/MapKey retirados ou divergentes |
| 11 | `wrongKind` | Categoria desconhecida/fora do contrato |
| 12 | `outOfRange` | ID fora do catálogo efetivo |
| 13 | `notFound` | Nome válido ausente da categoria solicitada |
| 14 | `busy` | Retirada/unload impedido por usos retidos |
| 15 | `budgetExceeded` | Reserva de metadados local insuficiente |
| 16 | `internalFailure` | Falha interna diagnosticada, sem outputs de sucesso |

No erro, nenhum output é modificado, inclusive handles e caudas. Recursos ainda
privados criados durante uma operação falha são desfeitos pelo allocator dono.
Não atravessam a ABI STL, exceções, referências, ownership C++ ou structs nativos.
Uma futura implementação deve converter falhas específicas na fronteira e reportar
falhas de programação, sem `catch (...)` para fabricar sucesso. Os status desta ABI
não são `QueryStatus` v1 nem `RejectCode` da negociação; proibir cast numérico
entre eles. Esgotar RAM não é o `budgetExceeded` de bytes do transcript de rede.

## Vida da tabela e fronteiras externas

Chamador mantém o módulo carregado e a lease de API viva durante todas as chamadas,
leases e function pointers retidos. Cada query bem-sucedida adquire uma lease nova;
copiar a tabela ou o handle não estende sua vida. Primeiro liberar seus snapshots,
depois `releaseApi`, invalidar todas as cópias e só então soltar o módulo. Um lease
liberado não pode ser chamado para testar a validade de um contexto já encerrado.
O host só encerra o serviço/unload após revogar acesso, drenar leitores e callbacks
e obter leases de API/snapshot zero; se isso não for comprovável, retorna/propaga `busy`.
Não criar um mecanismo de refcount que permita chamar código após `FreeLibrary`.

API ReHLDS, GameDLL, ClientDLL, estúdio e Metamod mantêm seus contratos existentes.
Esta ABI nova não aumenta o limite de `IRehldsServerData::GetModel`, não valida
`GetResource` antigo nem torna seus setters seguros. O perfil deve rejeitar
consumidores externos não cobertos antes da ativação. A API 2 não fornece bypass
para emitir bytes/shorts de mods, obter ponteiros internos ou ativar capacidades.

Fontes de plataforma: [cdecl](https://learn.microsoft.com/en-us/cpp/cpp/cdecl?view=msvc-170)
e [packing](https://learn.microsoft.com/en-us/cpp/preprocessor/pack?view=msvc-170).
São regras de compilação, não provas de assinaturas internas dos motores.
