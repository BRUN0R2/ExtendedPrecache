# Áudio, eventos e efeitos — contrato de dados 2

Estado: especificação, sem wrappers/callbacks nativos implementados. Usar envelope
EPD2 e gate de gameplay de [dataCodecs.md](dataCodecs.md). A lista normativa
[effectSchemasV2.json](data/effectSchemasV2.json) define a ordem e o tipo de todos
os campos dos efeitos base. Ela é um formato novo, não um dump do switch nativo.
O binding deve provar as conversões de cada perfil antes de disponibilidade.

## Som

SVC 6/29 sound tem corpo fixo de 20 bytes:

```text
entityId:u16, channel:u8, action:u8
sourceKind:u8, volume:u8, attenuationQ6:u8, pitch:u8
sourceId:u32, origin:coord16[3], behaviorFlags:u16
```

action: 0 start, 1 stop, 2 update. sourceKind: 0 arquivo (DomainId de sound),
1 sentença (ID da tabela de sentenças aprovada na aplicação). Um arquivo de som
não é o nome `!sentença`. Sentenças não aumentam soundEntries e exigem tabela/hash
local correspondente. SourceId é não zero em start/update; sourceKind/capacidade
da respectiva tabela são validados antes de resolver descritor.

entityId mantém o limite nativo do serviço (nos alvos, 11 bits úteis); channel
fica em 0…7. Volume/pitch mantêm unidades u8, atenuação Q6 e coordenadas a
quantização antiga. behaviorFlags tem somente bit 0 spawning; demais zero.
Converter as flags de origem por semântica: flags antigas de presença,
LARGE_INDEX, STOP e SENTENCE não são repassadas numericamente.

Stop usa apenas entidade/canal; os demais campos precisam ser zero canônico.
Update exige canal/fonte já compatíveis; não começa outro som por inferência.
SVC 29 exige start, channel=0, spawning=1 e entrega confiável. Para flags/ações
nativas sem correspondência aprovada, recusar o perfil/ação, não ignorar bits.
Índice vira CategoryIndex para resolver sfx_t estável; samples/PCM e seus canais
seguem ownership próprio. Não converter o u32 recebido para `short`.

## Eventos

SVC 3/21 event tem header de 16 bytes:

```text
eventDomainId:u32, entityId:u16, behaviorFlags:u16
delayTicks:u16, argsSchemaId:u16, argsBytes:u32, args[argsBytes]
```

SVC 3 não confiável e SVC 21 confiável; flags de entrega não são reinterpretadas
como índice. Delay usa as unidades nativas aprovadas do binding, representadas
em ticks de 1/100 s; validar a conversão antes de escrever. behaviorFlags usa
bits notHost=1, global=2, update=4, hostOnly=8; notHost e hostOnly não coexistem.
Flags produtoras FEV_SERVER/CLIENT/RELIABLE são tratadas na origem/roteamento,
sem cast para esses bits. Um caso não mapeado impede aprovação do módulo.

eventDomainId válido e script/nome/hash carregados são obrigatórios. args é o
ValueSet completo do schema role eventArgs, sem mask e sem valores ausentes.
Comprimento exato e todos os campos são validados antes de chamar hooks por nome.
Se iparam/bparam de um mod representar modelo/som/decal, seu schema deve declarar
resource e converter na fronteira; não presumir que todo inteiro seja índice,
nem esconder um índice em bytes opacos. entityId não ganha largura por associação.

## Efeitos temporários

SVC 23 effect começa com `effectId:u16, reserved:u16=0, valuesBytes:u32`, seguido
de todos os valores ordenados do schema fixo. effectId permanece em 0…255;
a largura u16 dá framing uniforme, não novos efeitos nativos. Toda emissão exige
effectPolicies correspondente da aplicação, inclusive entrega/dono. Efeito zero (BEAMPOINTS) é válido;
zero não é ausência de efeito. Átomos resource são DomainId u32; demais campos
mantêm suas unidades e faixas. Sprite usa kind model e também verifica type=sprite
no catálogo, conforme o campo. Campos modelSprite aceitam os tipos model/sprite
permitidos pelo efeito, e não um genérico com número igual.

O registro cobre os efeitos com recursos enumerados na auditoria, incluindo dois
sprites em BEAMSPRITE/BLOODSPRITE e os casos com decals. HIGH 117/118 são aliases
somente de entrada nativa: somar 256 ao decal nativo e depois converter para
DomainId, emitindo os efeitos canônicos 116/104. No wire 2, 117/118 são recusados
como não canônicos; não há necessidade de variante HIGH com referência u32.

BSPDECAL tem model nullable explícito: entityId=0 exige model=0; entidade não zero
exige modelo BSP válido dessa entidade. Não usar model=0 como referência ao mundo.
PLAYERDECAL mantém player/entity/spray como namespaces próprios, sem um campo
modelo opcional que o leitor auditado não consome. MULTIGUNSHOT inclui seu decal.
Entidades de beams usam packedEntity16; preservar attachment e validar a entidade
mascarada, sem tratar o short inteiro como índice de precache.

Efeitos sem recurso só são aceitos com gramática resourceFree completa aprovada
na aplicação (schema role effect, effectId não conflitante). Unknown/obsoleto
16/26, tamanho errado, flags ilegais ou sufixos desconhecidos não passam ao
parser nativo. Nenhum registro opaco pode esconder shorts de recurso. As flags
de EXPLOSION e FIREFIELD possuem máscaras fixas no registro normativo.

SVC 36 decalName contém `decalDomainId:u32, nameBytes:u16, name[nameBytes]`.
Nome e ID precisam coincidir com o manifesto, até 15 bytes. Não cria, substitui
ou renumera decals durante gameplay e não transporta uma customização.

## Fonte e fronteira de cobertura

A auditoria de [protocolAudit.md](protocolAudit.md) prevalece sobre comentários
incompletos do SDK, como PLAYERDECAL. A referência pública de nomes/campos é
[const.h na revisão selecionada](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/common/const.h#L148).
As estruturas, flags de origem e callbacks reais ainda exigem perfis de GameDLL,
ClientDLL, estúdio, renderer e áudio. O formato escrito aqui não aprova suas
calling conventions, lifetimes ou performance; nenhum índice pode escapar a um
consumidor antigo sem tradução/validação integral.
