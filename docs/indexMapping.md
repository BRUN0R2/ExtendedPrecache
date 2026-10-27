# Índices e namespaces — contrato de integração 2.0

Estado: **especificação anterior aos adaptadores**. Este contrato resolve a
tradução sem alterar `ResourceCatalog`, a ABI diagnóstica 1 ou a negociação 1.0.
Não autoriza ampliar um consumidor nativo. Base estática: [engineAudit.md](engineAudit.md)
e [protocolAudit.md](protocolAudit.md). As equações têm vetores independentes em
[integrationVectors.json](contracts/integrationVectors.json).

## Identidades que não podem ser intercambiadas

| Nome conceitual | Escopo e significado |
| --- | --- |
| `DomainId<kind>` | ID one-based do catálogo da categoria; tipo atual `ResourceIndex<kind>`; zero inválido |
| `CategoryIndex<kind>` | Índice da categoria preservado pelo perfil, usado para resolver a tabela nativa; não é um ordinal global |
| `DenseOrdinal<kind>` | Posição zero-based dos metadados próprios, sem slot sentinela |
| `ResourceOrdinal` | Posição zero-based no snapshot lógico congelado, vinculada ao mapa |
| `LegacyResourceOrdinal` | Posição efetivamente emitida pela lista legada; ordem e população podem diferir do snapshot lógico |
| `ModelCacheSlot`, `SoundDescriptorSlot` | Posições locais nos caches K/A, não IDs da rede |
| `HudSpriteRef` | Identidade local de HUD; não é um modelo/sprite de rede |
| `NativeHudHandle` | Inteiro nativo `slot + 1`; não carrega geração nem equivale a `DomainId<model>` |

As identidades novas devem incluir `MapKey = runtimeNonce + localGeneration` no
uso persistente; IDs nus só circulam dentro de uma operação já vinculada ao mapa.
Nonce local tem 128 bits, não zero, gerado por CSPRNG a cada instância do serviço.
Geração é u64 não zero, crescente, sem wrap ou reutilização nessa instância.
Map epoch da rede, spawn count, nonce do servidor e tokens de conexão continuam
identidades distintas. Não comparar a geração local do cliente à do servidor.

## Registro explícito de categorias

`kind` da ABI 2 não é cast de `ResourceKind` C++ nem de `resourcetype_t` ReHLDS:

| Categoria | `ResourceKind` atual | `kind` ABI 2 | Tipo ReHLDS | Base B | N máximo do contrato |
| --- | --- | --- | --- | --- | --- |
| Modelo/sprite de rede | `model=0` | 1 | `t_model=2` | 1 | 2.147.483.646 |
| Arquivo de som | `sound=1` | 2 | `t_sound=0` | 1 | 2.147.483.646 |
| Genérico | `generic=2` | 3 | `t_generic=4` | 0 | 2.147.483.647 |
| Evento | `event=3` | 4 | `t_eventscript=5` | 1 | 65.535 |
| Decal base | `decal=4` | 5 | `t_decal=3` | 0 | 2.147.483.647 |

Mapear por branches/tabela validada, recusando enums desconhecidos. `t_skin`,
`t_world` artificial, sentenças, customizações e sprays não criam categorias
novas. Mundo é um modelo com índice 1. Um `t_decal` de customização não se torna
decal base só por compartilhar o enum: validar também a origem da mensagem.
Esses valores ReHLDS provêm de
[custom.h, revisão fixada](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/public/rehlds/custom.h#L27).

## Conversão total com resultado explícito

Para N entradas úteis e base B, validar categoria, N e mapa antes de aritmética:

```text
domainToCategory(d, N, B): exigir 1 <= d <= N; devolver (d - 1) + B
categoryToDomain(i, N, B): exigir B <= i e i - B < N; devolver (i - B) + 1
domainToDense(d, N):      exigir 1 <= d <= N; devolver d - 1
physicalSlotCount(N, B): validar N; devolver N + B com soma verificada
```

Calcular intermediários em u64, conferir o destino e só então converter. O teste
`i - B < N` vem depois de `i >= B`, para não causar underflow. Não usar clamp,
casts para `short`, saturação ou `value != 0` para classificar sucesso.
Resultados conceituais são `value`, `categoryDisabled`, `invalidCapacity`,
`outOfRange`, `wrongKind` ou `staleMap`; uma falha não devolve índice utilizável.
Um resultado nativo zero de genérico/decal pode ser sucesso; zero de ID de
domínio, referência ou handle continua inválido.

N=0 é permitido para som, genérico, evento e decal: categoria desabilitada,
sem `ResourceCatalog` de capacidade zero e sem ID válido. Uma view nativa de
base 1 mantém apenas a célula sentinela; a de base 0 fica vazia. Arrays legados
compilados continuam sujeitos aos pisos físicos de [memoryBudget.md](memoryBudget.md).
Modelos exigem N>=1; mundo está incluído. Contagens úteis são diferentes de
slots, inclusive no máximo representável: N=2.147.483.646 de modelo exige
2.147.483.647 slots, ainda sujeito ao produto por stride e ao orçamento x86.

Não limitar uma tradução ao valor do CVar anunciado: verificar o número de
entradas congeladas, a capacidade da sessão, o storage, a representação do
consumidor e o tamanho completo da mensagem. `IndexEncoding` sozinho só verifica
largura numérica. Um ID que cabe em u16 pode ser inválido em um caminho s16.

## Importação, registro e aliases

Preservar os números já retornados à GameDLL. Importar cada tabela real na ordem
do índice nativo, aplicar a canonicalização comprovada para a categoria e exigir
que `ResourceCatalog::intern` devolva exatamente `categoryToDomain(i)`. Mundo deve
ser o primeiro modelo. Divergência, buraco, duplicata que produza IDs conflitantes
ou entrada fora da capacidade rejeitam o mapa; nunca renumerar para acomodar.
Depois de congelar, só lookup; registro tardio ou mudança de nome exige novo mapa.

Copiar os nomes enquanto o argumento original ainda é válido. Manter separadas
a chave de deduplicação e a grafia de apresentação: generic normaliza barras e
usa comparação ASCII sem caixa; a primeira grafia normalizada fica estável.
Som e evento também usam comparação sem caixa na revisão selecionada; não
estender isso a modelos/decals sem prova do perfil. A equivalência completa,
incluindo bytes não ASCII, precisa constar do perfil local; sem essa prova,
esses nomes não autorizam uma integração ampliada. O domínio atual compara bytes
e não muda silenciosamente sua regra.

Há um caso de retorno zero que **não é conversão de índice**:
[PF_precache_generic_I_internal](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/pr_cmds.cpp#L1472)
retorna zero quando o nome já corresponde a modelo ou a `sound/` + som, antes
de procurar/inserir no catálogo genérico. A saída interna deve ser uma soma
discriminada: `RegisteredGeneric` (novo ou existente), `AssetAlias` para o recurso
de destino ou erro. Somente `RegisteredGeneric` passa pelas equações de base 0.
Em `AssetAlias`, preservar o retorno legado zero, sem inventar genérico com ID 1
nem aumentar sua contagem. Se um genérico foi registrado antes do modelo, sua
entrada anterior permanece; um alias posterior não apaga nem renumera esse ID.
O wrapper precisa verificar a tabela/ramo correspondente, não interpretar o
inteiro zero isoladamente. Esta política ainda não está implementada.

Nomes começando por `!` não são arquivos de som comuns; o registro auditado os
recusa. Handles de sentenças e flags de reprodução seguem caminho separado.
Nomes `*n` representam inline BSP: n é positivo e validado contra o BSP carregado,
não é um ID de precache deduzido da string. Construir `*` + decimal + NUL com
tamanho verificado e ownership estável; não reutilizar `localmodels[][5]` para
números de mais dígitos. `*1000` requer seis bytes, mesmo que o ID caiba em u16.

## Ordinais e referências cruzadas

O snapshot lógico usa esta ordem estável: modelos, sons, genéricos, eventos e
decals; em cada categoria, `DomainId` crescente. Para categoria k:

```text
prefix[k] = soma verificada das contagens das categorias anteriores
resourceOrdinal = prefix[k] + (domainId - 1)
```

Exigir `resourceCount = soma das cinco contagens` e manter a relação inversa
`ordinal -> (kind, domainId)`. Cada par aparece uma vez, inclusive mundo e inline
BSP; alias de registro que não criou entrada não acrescenta linha. Entradas
distintas que apontem para o mesmo arquivo mantêm suas identidades. Compartilhar
bytes de um asset não autoriza colapsar categorias/IDs. O hash dos bytes do futuro
manifesto continua dependendo do contrato de dados ainda pendente.

A ordem legada auditada é genéricos, sons, modelos, decals e eventos, com regras
próprias para sentenças. Não usar o ordinal lógico como ordinal de SVC 43 ou de
consistência sem uma projeção completa validada. A projeção legada guarda o mapa
entre os registros efetivamente emitidos e os recursos lógicos; qualquer alias
ou marcador tem representação explícita e verificada. Não basta somar um offset
ou reordenar a lista depois de calcular consistência/digest.

Um registro recebido deve validar `(map, kind, categoryIndex)` antes da escrita,
independentemente de um contador já limitado. Repetição do mesmo ordinal ou par
é erro; registros de metadados não herdam a regra de duplicatas do transcript de
controle. Retirar registros ausentes/fatais para caber continua proibido.

## Caches e HUD

`network index -> model/sfx pointer -> local cache slot` é uma resolução explícita,
não uma soma/subtração de bases entre namespaces. Vários consumidores locais podem
reter o mesmo descritor. Modelos locais e HUD entram em K; sons locais em A.
CRC usa o slot do cache de modelos, jamais o índice de rede.

Um handle HUD nativo h válido resolve `slot=h-1`, com `1<=h<=H`; falha nativa zero
não vira slot zero. O wrapper próprio acrescenta runtime/geração, conforme
[storageOwnership.md](storageOwnership.md). Essa geração não existe no inteiro
entregue à ClientDLL: ele não pode ser reaproveitado para outro sprite enquanto
algum consumidor antigo puder usá-lo. A API 2 de metadados não expõe esses handles
nem ponteiros de modelo, sample ou textura.
