# Capacidades configuráveis — contrato 1

Estado: contrato com [validador e núcleo de sessão](sessionCore.md), anterior aos adaptadores. A DLL de diagnóstico não
carrega configuração, não anuncia capacidades e mantém `engineExtensionActive=0`.
Os números abaixo definem políticas e representação; não certificam capacidade
de um motor. A [auditoria](engineAudit.md) continua sendo a base de evidências.

## Unidade e representação

Capacidade é quantidade máxima de **entradas utilizáveis**, não tamanho de array,
maior índice ou quantidade já registrada. Os limites de sessão são imutáveis.

| Campo / ID de negociação | Índices válidos com capacidade N | Padrão | Teto de representação do contrato |
| --- | --- | --- | --- |
| `modelEntries` / 1 | 1…N; mundo e inline BSP estão incluídos | 511 | 2.147.483.646 |
| `soundEntries` / 2 | 1…N; sentenças são outro namespace | 511 | 2.147.483.646 |
| `genericEntries` / 3 | 0…N−1 | 4.095 | 2.147.483.647 |
| `eventEntries` / 4 | 1…N | 255 | 65.535, pelo retorno `unsigned short` da API |
| `decalEntries` / 5 | 0…N−1; somente decals base | 512 | 2.147.483.647 |
| `resourceEntries` / 6 | Ordinais 0…N−1 da lista global | 4.095 | 2.147.483.647 |
| `consistencyEntries` / 7 | Quantidade de arquivos verificados | 512 | 2.147.483.647 |
| `manifestBytes` / 8 | Bytes do manifesto serializado canônico | 4.194.304 | 2.147.483.647 |
| `chunkBytes` / 9 | Payload de um chunk lógico do manifesto | 16.384 | 65.536, ainda sujeito ao overhead |
| `inFlightBytes` / 10 | Bytes de chunks pendentes, incluindo seus headers | 65.536 | 2.147.483.647 |
| `fileBytes` / 11 | Maior arquivo admitido | 16.777.215 | 16.777.215 |
| `reliableMessageBytes` / 12 | Mensagem GoldSrc completa, incluindo framing | 65.536 | 65.536 |

Zero desabilita uma categoria quando permitido pelo schema. Não significa
ilimitado. `modelEntries` e `resourceEntries` são pelo menos um, pois o mundo
precisa existir. Para N=0 não há índice válido. Modelos/sons/eventos precisam
de N+1 slots físicos; a conversão deve usar soma verificada. O catálogo atual
do domínio permanece one-based: genéricos e decals exigirão tradução explícita,
e um resultado zero desses motores não pode ser usado como um booleano de erro.
Categoria desabilitada será um estado explícito; não construir um `CatalogLimits`
inválido com capacidade zero nem alterar silenciosamente seu contrato atual.
Equações, aliases e ordinais estão em [indexMapping.md](indexMapping.md). Elas
validam o número efetivo congelado antes da capacidade/representação do consumidor.

Os tetos são restrições escalares do projeto de contrato: contadores nativos
assinados, slot zero e API de eventos. **Não são máximos implementados ou
aprovados.** O perfil local do adaptador pode autorizar menos. Índices do futuro
perfil ampliado serão representados por u32, sem casts silenciosos para s16,
bytes ou hashes `short`. Todos esses consumidores precisam ser substituídos
antes de um perfil anunciar a ampliação.

Nomes continuam limitados a 63 bytes, e nomes de decal a 15 bytes, excluindo NUL.
Arquivos permanecem no limite de 24 bits auditado. Alterar comprimento de nomes,
formatos de assets ou transporte de arquivos não faz parte desta ampliação de
contagens. Texturas, entidades, partículas e canais de áudio continuam separados.

Os campos de manifesto/chunks não modificam o envio legado: em `legacyOnly` não
há transferência em chunks. As relações com overhead são verificadas ao preparar
um perfil ampliado conhecido. O legado continua exigindo uma mensagem completa
dentro de 65.536 bytes, independentemente de `manifestBytes`.

## Arquivo e políticas

Formato proposto: `extendedPrecache.json`, UTF-8 sem BOM, no máximo 65.536 bytes.
O caminho será informado ao futuro carregador; não há busca em diretórios do
jogo. O leitor independente recebe caminho e papel explícitos. Sem arquivo, utiliza
os valores completos dos exemplos `legacyOnly`, selecionados pelo papel local.

O [schema fechado](configuration.schema.json) usa
[JSON Schema 2020-12](https://json-schema.org/draft/2020-12).
Não injeta defaults. Em um arquivo presente, todos os campos obrigatórios devem
ser informados; chaves desconhecidas, duplicadas, versão desconhecida, valores
negativos, strings numéricas, floats e notação exponencial são recusados. A
restrição lexical de inteiros e a detecção de duplicatas precedem o schema;
JSON Schema, isoladamente, não detecta essas duas condições.

| Política | Servidor | Cliente |
| --- | --- | --- |
| `legacyOnly` — padrão | Só admite mapa e tráfego comprovadamente legados; não inicia negociação | Não anuncia `epc`; só aceita o contrato legado validado |
| `preferExtended` | Negocia com opt-in; sem opt-in, só libera o mapa se inteiro compatível com o legado | Anuncia opt-in; pode aceitar servidor legado somente antes de receber uma oferta |
| `requireExtended` | Recusa cliente sem opt-in antes do sign-on | Recusa sign-on legado ou ausência de acordo ampliado |

Depois de uma oferta começar, qualquer erro ou timeout encerra a conexão.
Não há fallback nessa conexão. Um manifesto incompleto, descarte de recursos,
remapeamento diferente por jogador ou saturação de índices nunca é fallback.
`preferExtended` permite convivência com pares legados; `requireExtended` exige
o acordo. Nenhuma política transforma a indicação em `userinfo` em confirmação.

Exemplos completos:

- [Servidor conservador](examples/serverLegacy.json) e
  [cliente conservador](examples/clientLegacy.json).
- [Servidor ampliado de projeto](examples/serverExtendedDraft.json) e
  [cliente ampliado de projeto](examples/clientExtendedDraft.json).

Os exemplos ampliados referenciam um
[contrato de aplicação fictício](protocol/applicationContractExample.json).
Esse contrato e o [perfil de dados fictício](protocol/dataContractExample.json)
estão explicitamente **indisponíveis para ativação**. Servem aos vetores offline;
copiar os exemplos não habilita suporte. Um perfil real precisa identificar os
módulos locais e todos os codecs aprovados. Não existe perfil coringa.

O [contrato de dados 2.0](dataCodecs.md) usa ID 2 e possui um
[núcleo isolado](dataCore.md); o perfil de integração permanece indisponível.
Define overhead máximo de chunk de 48 bytes, referências
DomainId u32 e limites adicionais fechados de aplicação para schemas, estado,
datagramas, prazos e demos. Esses limites entram no digest de aplicação e no
preparo de ambos os papéis; não são campos novos no schema de configuração 1.
O exemplo ID 1 e os frames de controle existentes não são alterados.

## Configuração local de armazenamento

`storage` não é uma capacidade compartilhada pelo servidor e cliente. Seus
campos são validados localmente e entram na preparação antes de `clientReady`.

| Campo | Papel | Padrão | Restrição |
| --- | --- | --- | --- |
| `knownModelEntries` | Ambos | 1.024 | Quantidade de descritores físicos; até `floor(INT32_MAX / 392)` |
| `localModelReserve` | Ambos | 64 | Reserva mínima para modelos locais além dos de rede; o perfil pode exigir mais |
| `soundDescriptorEntries` | Cliente | 1.024 | Descritores físicos; até `floor(INT32_MAX / 72)` |
| `localSoundReserve` | Cliente | 64 | Sons locais adicionais; o perfil pode exigir mais |
| `hudSpriteEntries` | Cliente | 256 | Handles próprios do HUD; até `floor(INT32_MAX / 12)` |

Dependências mínimas, com somas verificadas:

```text
servidor: knownModelEntries >= modelEntries + localModelReserve
cliente:  knownModelEntries >= modelEntries + hudSpriteEntries + localModelReserve
cliente:  soundDescriptorEntries >= soundEntries + localSoundReserve
ambos:    consistencyEntries <= resourceEntries
ambos:    inFlightBytes >= chunkBytes + perfil.maxChunkOverheadBytes
ambos:    reliableMessageBytes >= chunkBytes + perfil.maxChunkOverheadBytes
```

O contrato de aplicação exige reservas mínimas locais e quantidade de handles
HUD para os seus consumidores. Esses requisitos não podem ser reduzidos por
negociação. Reservar um descritor por handle HUD é uma estimativa conservadora;
deduplicação posterior não autoriza prometer capacidade sem a reserva.
O perfil deve cobrir também o tamanho de `modelsMap`, CRCs, hash de sons,
buffers de inline BSP e os consumidores externos. Aumentar essas capacidades
em `legacyOnly` acima de 1.024/1.024/256 é erro de configuração.

`metadataBudgetBytes` é teto local de memória de metadados e preparação, padrão
16 MiB, limitado a `INT32_MAX` nesta plataforma. Não é reserva de RAM do processo,
memória de assets ou VRAM. Antes de anunciar qualquer perfil, seu planejador deve
produzir uma cotação de pico que inclua tabelas, nomes, containers, alinhamento,
allocator, manifesto em staging, filas e cópias retidas. As estruturas legadas
continuam cobradas enquanto existirem consumidores. Somente o limite do par é
negociado; comparar números brutos de RAM entre papéis não produz uma capacidade.

O [orçamento auditado](memoryBudget.md) fornece um **piso**, não essa cotação
completa. Para os vetores offline, a reserva de referência usa `P=M+1`, `Q=S+1`, `T=E+1`, com
M/S/E em entradas utilizáveis e K/A/H em descritores físicos:

```text
compartilhado = 16T + 136R + 44C + 404K
servidor = compartilhado + 9P + 4Q + 2(2Q-1) + 64G + 17D
           + 32P + 520 + 174080 + 2048 + 32768
cliente  = compartilhado + 4P + 4Q + 16D + 72A + 12H + 174080
reserva ampliada = subtotal + manifestBytes + inFlightBytes + negotiation.maxBytes
piso legado = subtotal nativo + reliableMessageBytes
```

A estratégia ampliada de referência reserva o manifesto em staging e a fila
em memória; um futuro perfil com streaming limitado pode definir outra cotação
comprovada. Valores que já excedem o orçamento são recusados. Valores abaixo
ainda precisam da cotação completa e de alocação bem-sucedida. No legado, usar
pelo menos os tamanhos físicos nativos ao calcular o piso: P/Q=512, T=256,
D/C=512, K=1.024, G/R=4.096 no servidor e A/H=1.024/256 no cliente. Reduzir um
limite de admissão não reduz um array compilado no motor.

## Admissão do mapa e seleção

O servidor congela um snapshot com IDs, nomes, flags, tamanhos, entradas de
consistência, identidades dos contratos e bytes do manifesto. Inclui mundo,
inline BSP, todos os decals enumerados e genéricos: não apenas arquivos pedidos
diretamente pelo mod. A soma dos cinco catálogos deve coincidir com a quantidade
de registros transmitidos pelo perfil; outras categorias não são admitidas sem
novo contrato. A consistência não pode exceder essa lista.

O snapshot declara um vetor `requirements` nos mesmos 12 campos: quantidades
reais, bytes reais do manifesto, menor chunk atômico necessário, pico mínimo em
voo, maior arquivo e maior mensagem indivisível. O perfil fornece as regras para
essas medições; um valor ausente ou não demonstrado impede o anúncio.

Para um contrato de dados idêntico e disponível nos dois pares:

```text
selected[field] = min(serverLimits[field], clientLimits[field])
requirements[field] <= selected[field], para todos os 12 campos
selected deve satisfazer também as dependências de framing, filas e storage
```

Essa operação é a seleção explícita de limites do acordo, não um truncamento de
dados. O servidor não diminui o catálogo congelado. Se qualquer dimensão não
couber, rejeitar. Se couber, o cliente verifica exatamente o mesmo mínimo e
reserva a memória necessária antes de responder. Falha de alocação não pode ser
convertida em sucesso ou em capacidade menor. O servidor conserva a cotação e
a geração do mapa até terminar a negociação.

Os IDs 1–12 são estáveis e tipados pelo campo; contagens, bytes, hashes,
gerações e durações não podem compartilhar wrappers de domínio por terem o
mesmo inteiro subjacente. Valores recebidos só entram no modelo após validação.
Esses DTOs futuros são internos C++26; não alteram a [ABI pública](abi.md).

## Sessão legada

Cabem nas tabelas não significa cabem no protocolo. Antes de liberar um par
legado, validar o snapshot completo usando os limites efetivos dos caminhos:

- Contagem global até 4.095; nenhum índice/tamanho excede os respectivos campos.
- Cada ramo absoluto da consistência cabe em u10; gaps de até 31 usam u5.
  Até 512 verificações e resposta inteira dentro do comprimento s16.
- Medir a mensagem completa de recursos, com SVC 45, URL opcional, SVC 43,
  strings, flags, reservados, cauda e padding: até 65.536 bytes e até a política
  local. Não enviar `RES_CUSTOM` na lista global com o escritor assimétrico atual.
- Deltas e mensagens do contrato de aplicação precisam representar seus IDs.
  SVC 36 só aceita decal até 255; variantes HIGH de TE não ampliam SVC 36.
  Campos s16, sentenças e sprays preservam seus namespaces.
- Respeitar caches e reservas do perfil legado. Consumidor externo desconhecido
  não é compatibilidade comprovada.

Uma falha recusa a conexão antes do primeiro estado incompatível. Não esconder
recursos para permitir um cliente legado. Os números padrão são limites de
admissão conservadores por dimensão, não garantia para qualquer combinação ou mod.
Uma sessão legada usa o conjunto completo de serializadores/schemas legados do
perfil; não reutilizar automaticamente deltas ou mensagens ampliadas porque os
IDs atuais do mapa são pequenos. O catálogo/IDs do servidor permanece o mesmo.

## Aplicação e mudanças

Validar arquivo, campos, dependências, perfil, orçamento e preparo nessa ordem,
antes de alterar o motor. Erros informam código estável, campo e limites; não
há coerção, clamp ou ignoração de erro. O leitor e os validadores independentes
estão implementados; cotação real, módulos e consumidores do motor permanecem
responsabilidade dos perfis e adaptadores futuros.

Configuração carregada vira um snapshot imutável. Mudanças entram somente em
nova conexão e geração de mapa; não alteram capacidades em uso nem o significado
de um handle. Uma mudança de mapa encerra o acordo ampliado e exige nova conexão
e negociação. O mecanismo nativo de reconexão e o descarregamento seguro serão
verificados nos adaptadores. Não há renegociação in-place no contrato 1.
Mover caches K/A/H também exige quiescência global dos consumidores; sem essa
prova, alterar suas capacidades exige reinício do processo. Ownership e orçamento
compartilhado estão em [storageOwnership.md](storageOwnership.md).
