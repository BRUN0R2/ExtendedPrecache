# Catálogo e consistência — contrato de dados 2

Estado: especificação, sem leitor/escritor nativo implementado. Usa envelope e
limites de [dataCodecs.md](dataCodecs.md), IDs de [indexMapping.md](indexMapping.md)
e ownership de [storageOwnership.md](storageOwnership.md).

## Referências e bytes canônicos

No plano de dados, IDs de recurso são **DomainId u32 one-based em todas as
categorias**. Tipo/kind é explícito ou declarado pelo schema; converter para
`CategoryIndex` apenas na fronteira nativa. Portanto, decal/genérico nativo zero
vira DomainId 1. Zero só significa ausência em um campo declarado nullable;
não é um recurso válido. Mundo é model/1. Não trocar ordinal por ID.

O manifesto é um único stream EPCM, sem compressão, varints ou padding; nomes e
caminhos têm comprimento explícito e não incluem terminador NUL.
SHA-256 dos bytes inteiros é o `catalogIdentity` da negociação. Não há timestamps,
ponteiros ou nomes localizados na identidade. Inteiros little endian; reservados
zero. Bytes de caminhos mantêm a codificação aprovada pelo perfil, não UTF-8
implícito; regras de caminho/tamanho e aliases continuam as do contrato de índices.

### Header EPCM: 72 bytes

```text
magic[4]="EPCM", major:u16=2, minor:u16=0
resourceCount:u32, assetCount:u32, consistencyCount:u32
modelCount:u32, soundCount:u32, genericCount:u32, eventCount:u32, decalCount:u32
schemaSetSha256[32]
```

Contagens devem coincidir com os requisitos congelados de S0; assetCount<=resourceCount.
Seguem resourceCount recursos, assetCount assets e consistencyCount regras, nessa
ordem. Exigir fim exato do stream e soma das cinco categorias igual a resourceCount.
Não há buracos, registros omitidos ou contagem saturada.

### Recurso: 28 + nameBytes

```text
ordinal:u32, kind:u8, type:u8, flags:u16=1
domainId:u32, assetOrdinal:u32
parentKind:u8, reserved[3]=0, parentDomainId:u32
nameBytes:u16, reserved:u16=0, name[nameBytes]
```

Kinds: 1 model, 2 sound, 3 generic, 4 event, 5 decal. Types: 1 model, 2 sprite,
3 world, 4 inlineModel, 5 sound, 6 generic, 7 event, 8 decal. Conferir o par
kind/type; world aparece uma vez em model/1. Flags=1 significa obrigatório para
assetsReady; o primeiro perfil não admite assets opcionais ausentes. Isso não
reinterpreta flags nativos nem `RES_CUSTOM`.

Rows seguem modelos, sons, genéricos, eventos e decals, com DomainId crescente.
Ordinal deve ser exatamente o prefixo da categoria + DomainId−1. Nomes de até
63 bytes, ou 15 para decal, sem vazios/NUL/truncamento e com deduplicação validada.
Repetição de ordinal ou par (kind,ID) é erro. Duas categorias podem compartilhar
arquivo, mas mantêm suas rows. Alias de registro sem entrada real não cria row.

Todo recurso aponta para um asset válido. InlineModel referencia também parent
model/1, compartilha seu asset BSP e tem nome `*n` validado contra o BSP, sem
inferir o DomainId a partir de n. Outros recursos têm parentKind=parentDomainId=0.
Decal aponta para o WAD/asset proprietário da textura e preserva seu nome de
entrada, sem tratar spray como decal base. A resolução local dessas relações
precisa de prova por perfil; o manifesto não autoriza ponteiros nem extração cega.

### Asset: 44 + pathBytes

```text
assetOrdinal:u32, fileBytes:u32, sha256[32]
pathBytes:u16, flags:u16, path[pathBytes]
```

Ordinais densos desde zero, na ordem do primeiro recurso que usa o arquivo;
deduplicar por identidade/caminho canônico do perfil. Mesmo caminho com tamanhos
ou hashes diferentes é conflito. Flags: bit 0 downloadAllowed; outros zero.
Sem downloadAllowed, o arquivo exato precisa existir localmente. Para download,
respeitar também o transporte de arquivo aprovado e fileBytes<=16.777.215;
fragmentação do catálogo não amplia arquivos. WAD compartilhado conta uma vez.
Assets não usados por nenhuma row são recusados.

Hash de conteúdo e comprimento são obrigatórios, inclusive para arquivos locais.
Carregar/verificar o mesmo arquivo estável, impedindo substituição entre check
e uso. Artefatos de contrato e módulos executáveis são instalados/verificados
fora do download de recursos; o peer não escolhe código para carregar. O SHA
verifica identidade/integridade e não autentica um servidor ou atesta um cliente.

## Transferência e freeze

SVC 43 catalogBegin tem 16 bytes: totalBytes, resourceCount, assetCount e
consistencyCount, todos u32. totalBytes precisa ser igual ao requisito anunciado
de manifestBytes. Chunks: offset:u32, count:u32 e exatamente count bytes; count>0,
offset contíguo, sem overlap, duplicata ou gap. Verificar soma em u64 e todos os
limites antes de copiar. catalogEnd contém o SHA-256 de 32 bytes, igual a S0.

Checar header, contagens, tipos, IDs, relações, nomes, assets e regras antes de
congelar/publicar. Pode-se fazer parse incremental privado, mas nada dessa geração
fica observável em tabelas ou callbacks antes de validar o stream e seu hash.
Configuração/mapa mudado, arquivo ausente ou falha de alocação inicia retirada.

## Regras de consistência

Cada regra começa com `resourceOrdinal:u32, mode:u8, reserved[3]=0` (8 bytes).
Mode 1 exactDigest não tem cauda; modes 2 exactBounds e 3 withinBounds têm
`mins:f32[3], maxs:f32[3]` (24 bytes). Floats são finitos, −0 normalizado a +0
no emissor e recusado como não canônico no receptor; exigir mins<=maxs.

Regras em ordinal estritamente crescente, sem duplicatas; C<=R e dentro da
capacidade de consistência. Mode 1 exige o digest do asset da row. Modes 2/3
também exigem esse digest e limites calculados pelo loader aprovado: igualdade
binária canônica em exactBounds, e minRecebido>=minPermitido/maxRecebido<=maxPermitido
em withinBounds. Classe de modelo/loader deve permitir bounds no perfil.
Não executar o loader usando structs/offsets fornecidos pela rede.

Os modos da API `ForceUnmodified` precisam de mapeamento explícito por módulo,
inclusive suas condições de disponibilidade. Este perfil exige todos os assets
presentes; modo nativo sem correspondência aprovada recusa o perfil. Não preservar
um CRC32/munging antigo fingindo que é SHA-256 nem reutilizar ordinais legados.

## Resposta CLC 7

consistencyPage: `ruleOffset:u32, entryCount:u32`, depois entryCount entradas.
ruleOffset é posição no vetor de regras, **não** ordinal de recurso. Cada entrada:

```text
resourceOrdinal:u32, mode:u8, reserved[3]=0, actualAssetSha256[32]
[mins:f32[3], maxs:f32[3]] somente modes 2/3
```

Entrada tem 40 ou 64 bytes. Paginar por bytes: para N entries de bounds o total
é `40 envelope + 8 page + 64N`. Com teto 65.536, cabem 1.023 entries, não 1.024.
512 custam 32.816 bytes; 1.280 não cabem juntos e devem ser paginados. Não usar
tamanho s16, delta de ordinal u5 ou absoluto u10/u12 neste codec.

Exigir a sequência completa de regras, ordem, mode e asset esperado. Respostas
extras, repetidas, desconhecidas ou faltantes não completam a fase. consistencyEnd
tem ruleCount:u32 e reportSha256[32]; exigir C exato, inclusive C=0. O digest é:

```text
SHA256("ExtendedPrecache/consistency/2\0" || catalogHash[32] || C:u32
       || entradas canônicas na ordem das regras, sem headers de página)
```

Só após o relatório inteiro e os assets verificados o cliente envia assetsReady.
O servidor compara as regras e o digest, e só depois prepara o snapshot inicial.
O relatório não é atestação contra um cliente modificado. Prazo/bytes/cotações
continuam obrigatórios; falta de prova não se converte em compatibilidade legada.
