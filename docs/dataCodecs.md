# Codecs de dados — contrato 2, versão 2.0

Estado: **especificação indisponível**, sem codecs de produção ou adaptadores.
O ID 2 é distinto do exemplo fictício ID 1 de `docs/protocol`, que permanece
inalterado. [dataContractV2.json](data/dataContractV2.json) identifica este desenho
e os hashes das suas definições. Nenhum perfil fica disponível por possuir esse
arquivo; implementação completa, módulos aprovados e provas nativas são exigidos.

A identidade é SHA-256 dos bytes exatos UTF-8/LF do descritor, sem BOM e com
newline final. Seus normativeArtifacts vinculam por caminho/hash os seis textos
normativos, o registro de efeitos e o schema de aplicação. Não há hash de si
mesmo dentro do arquivo. Editar qualquer definição exige atualizar esse fecho e
o digest de dados nos contratos de aplicação; não editar os exemplos ID 1.

O conjunto é atômico: [catálogo/consistência](catalogCodec.md),
[schemas/deltas](deltaCodec.md), [áudio/eventos/efeitos](effectCodecs.md),
[demos](demoCodec.md) e [módulos](moduleContracts.md). Não ampliar só uma largura
mantendo consumidores antigos. A ABI diagnóstica 1, a ABI 2.0 planejada, o schema
de configuração 1 e a negociação 1.0 não são renumerados por este contrato.

## Seleção e transporte

Selecionar ID **e** SHA-256 correspondentes na negociação existente. O digest de
aplicação deve identificar o conjunto completo de schemas/módulos; cada papel
verifica seus binários locais. Não comparar o hash do cliente ao do servidor.
Sem configuração válida, reserva, decoder implementado e cobertura integral,
recusar antes de S0/C8. Não interpretar bytes novos por detecção de magic ou
fallback depois da oferta. O modo legado mantém seus codecs e suas provas.

O controle continua em `ExtPrecache`/`epc1`, com os 26 frames e seus limites.
O plano de dados substitui **corpos de opcodes existentes** somente nessa sessão
estendida, antes de qualquer handler legado. Não ocupa SVC 60…63, não usa um ID
de user message fixo e não transporta bulk em comandos de console. A futura
integração precisa provar tanto a captura de cada escritor quanto o despacho do
leitor. Um callback tardio da ClientDLL/GameDLL não é essa prova.

| Direção/opcode | Kinds do novo envelope e entrega permitida |
| --- | --- |
| S→C SVC 14 | 1 schema, 2 schemaSetEnd; confiável |
| S→C SVC 43 | 1 catalogBegin, 2 catalogChunk, 3 catalogEnd, 4 gameStart, 5 modMessage, 6 moduleState, 255 fatal; catálogo/gameStart/fatal confiáveis; modMessage conforme contrato; moduleState conforme frame |
| C→S CLC 7 | 1 consistencyPage, 2 consistencyEnd, 3 assetsReady, 4 gameplayAck, 5 frameAck, 6 resyncRequest, 7 modMessage, 255 fatal; confiável |
| S→C SVC 22 | 1 frameBegin, 2 frameObject, 3 frameEnd; snapshot inicial confiável |
| S→C SVC 40/41 | 1 frameBegin, 2 frameObject, 3 frameEnd; frame completo/delta, respectivamente; uma entrega por transação |
| S→C SVC 15 | 2 frameObject de client/weapon; mesma entrega da transação |
| S→C SVC 6/29 | 1 sound; dinâmico conforme origem, estático confiável |
| S→C SVC 3/21 | 1 event; não confiável/confiável, respectivamente |
| S→C SVC 23 | 1 effect; entrega aprovada do emissor |
| S→C SVC 36 | 1 decalName; confiável, conferência do catálogo, sem criar IDs |

Outros SVC/CLC e user messages só passam pelo contrato fechado de aplicação.
SVC 11 inicial continua protocolo 48, prevalidado e com reset autorizado uma vez.
O emissor não pode executar `SV_SendServerinfo` deixando escapar descrições delta,
recursos ou baselines antigos: separar/suprimir esses trechos antes da emissão.
SVC 7 nativo é suprimido: serverTime passa a frameBegin e só atualiza relógios
com o commit atômico do estado. Não permitir um segundo relógio fora do frame.
SVC 46/CLC 5 de customizações mantêm seu namespace/codec nativo quando aprovado;
não ampliam o catálogo global nem compartilham IDs com decals base.

## Envelope EPD2: 40 bytes incluindo opcode

Inteiros little endian. Não copiar structs C/C++ ou buffers com padding.

| Offset | Bytes | Campo |
| --- | --- | --- |
| 0 | 1 | Opcode já existente, segundo direção/kind |
| 1 | 1 | Major do plano de dados = 2 |
| 2 | 1 | Kind, restrito pela tabela acima |
| 3 | 1 | Flags: bit 0 confiável; demais bits zero; deve corresponder à fila/rota aprovada |
| 4 | 4 | `bodyBytes`; tamanho exato do corpo |
| 8 | 16 | `serverNonce` do acordo, não zero |
| 24 | 8 | `mapEpoch` do acordo, não zero |
| 32 | 4 | Sequência do fluxo |
| 36 | 4 | Reservado zero |
| 40 | bodyBytes | Corpo do kind; sem cauda ou padding |

Fluxos separados: S→C confiável, S→C não confiável e C→S confiável. Cada fluxo
começa em zero e conta somente envelopes EPD2, não serviços nativeExact. Não faz
wrap; `0xffffffff` é reservado, não um próximo número.
Confiável exige sequência exata; não adicionar retries de aplicação. Em não
confiável, gaps são perda permitida, e sequências antigas/repetidas são ignoradas
após validar framing/contexto. Isso não autoriza aplicar delta com base ausente.
Nova conexão exige nonce novo. Nonce antigo não zero é descartado sem progresso;
epoch divergente com nonce atual é falha. Nenhum deles autentica o peer.

Validar opcode/direção, major, flags, reservado, contexto, sequência, fase, tamanho
e gramática antes de escrever estado. Comprimento calculado em u64; conferir
`40+bodyBytes`, bytes restantes e orçamento antes de alocar/copiar. Um registro
não atravessa a fronteira de uma mensagem lógica do netchan. Vários registros
podem compartilhar um buffer, contanto que sua soma completa caiba nele.

O netchan pode concatenar prefixo confiável e dados não confiáveis no mesmo
pacote. O bit reliable do cabeçalho UDP não classifica cada serviço. O escritor
deve provar a fila de origem de cada envelope; o leitor valida sua flag/rota
contra o contrato e, em reassemblagem confiável, proíbe envelopes não confiáveis.
Uma flag enviada pelo peer não comprova sua fila nem cria confiabilidade. Não
interpretar todo pacote misto como um único fluxo EPD2. Essa prova faz parte do
bootstrap/dispatch pendente, inclusive na captura de demos.

## Limites por bytes

`reliableMessageBytes` limita a **mensagem GoldSrc completa**, até 65.536 bytes,
incluindo opcodes/envelopes e todos os registros agregados. A fragmentação nativa
não aumenta esse teto. Verificar também buffers reais do perfil; a fonte possui
buffers distintos de 3.990/4.000 bytes e caminhos maiores de sign-on. Aprovar
um buffer grande não amplia os outros automaticamente.
Referências: [net.h](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/net.h)
e [net_chan.cpp](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/net_chan.cpp).

O catálogo é dividido em registros independentes: `40 + offset:u32 + count:u32
+ data[count]`. Assim, `maxChunkOverheadBytes=48`; payload não excede `chunkBytes`
selecionado, e `chunkBytes+48` precisa caber em `inFlightBytes` e na mensagem
confiável. O stream pode dividir um registro do catálogo em qualquer byte; não
requer que um nome/row inteiro caiba no chunk. O tamanho total é `manifestBytes`,
não uma única mensagem de sign-on. `fileBytes` continua limitado a 16.777.215.

`inFlightBytes` cobre os registros completos ainda em filas/reassemblagem; não
contar só o payload. Retirar a cobrança somente depois do consumo confirmado.
O manifesto em staging e os snapshots reconstruídos têm suas próprias reservas
no coordenador de metadados, sem dupla contagem nem alocação fora da cotação.
Strings, schemas, estado de módulos, frames pendentes e janela de bases também
entram no pico de [storageOwnership.md](storageOwnership.md).

A aplicação declara limites adicionais configuráveis e fechados: bytes máximos
de frame de estado/schema set/artefatos, janela de bases, bytes não confiáveis,
prazos de carga/consistência e limites de gravação de demo. São parte do digest
de aplicação; não são novos campos silenciosos no leitor de configuração 1.
Os dois papéis devem conseguir cumprir esses limites, além dos 12 negociados.
Exceder o teto representável ou a reserva local recusa o perfil, sem clamp.

Não confiável: exigir registro inteiro dentro do limite de datagrama da aplicação,
do buffer real disponível e do limite selecionado; nenhuma fragmentação de um
registro. Se um frame não couber nesse modo, enviar **toda** a transação de estado
confiavelmente, com seus limites conferidos. Perda/expiração de frame incompleto
descarta staging, não recursos do mapa. Catálogo, schemas, prontidão e consistência
nunca são descartáveis. Efeitos descartáveis só seguem essa política se declarados.

## Gate de carga e gameplay

Após C9 válido e drain, o servidor libera apenas o bootstrap inicial aprovado.
Em seguida:

1. S→C schemas completos e schemaSetEnd; conferir hash contra a aplicação.
2. S→C catalogBegin/chunks/end; conferir bytes exatos, relações e hash contra S0.
3. Cliente congela metadados, carrega/verifica todos os assets e calcula as regras
   de consistência. Envia pages/end e assetsReady com os hashes do catálogo,
   schema set e relatório (32 bytes cada).
4. Servidor valida o relatório completo, snapshot/binding e reserva; envia o
   snapshot inicial confiável, incluindo estado de módulos necessário.
5. Após frameEnd validado, envia gameStart: `initialFrameId:u32`,
   `snapshotHash:32`, `catalogHash:32` (68 bytes).
6. Cliente confere o snapshot inicial, publica bindings na geração pós-reset e
   enfileira gameplayAck: `frameId:u32 + snapshotHash:32`. Só após a fila aceitar
   o ACK abre seu gate local. Servidor abre o gate da conexão após receber o ACK
   válido. Estado active do host não substitui esse gate por cliente.

Nenhum delta/effect/evento/audio de gameplay pode anteceder esse gate, exceto os
objetos declarados do snapshot inicial, que ficam em staging sem efeitos externos.
Não repetir o reset inicial. Registro tardio de módulo/schema/catálogo invalida
o acordo, não inicia renegociação. Preservar owners fora do hunk apagado.

Os prazos de carga e de consistência são fixos desde a entrada nas respectivas
fases, medidos monotonicamente; progresso não os renova. A aplicação declara suas
durações. O deadline da negociação continua inalterado e termina no seu handoff.
Load começa ao enfileirar/aceitar o primeiro SVC 11, conforme o papel, e termina
no gate de gameplay. Consistência começa ao enfileirar/validar catalogEnd; termina
no cliente ao enfileirar consistencyEnd e no servidor ao validá-lo. Nenhum desses
marcos renova load.
Sem tempo/memória/canal, fechar antes de emitir estado incompatível.

## Falha e cobertura

Fatal tem corpo de 16 bytes: `code:u16, reserved:u16=0, subjectSequence:u32,
fieldId:u32, detail:u32`. Códigos de dados: 1 malformed, 2 unsupportedContract,
3 byteBudget, 4 staleContext, 5 wrongPhase, 6 digestMismatch, 7 capacity,
8 assetFailure, 9 consistencyFailure, 10 deadline, 11 invalidSchema,
12 unsupportedModule, 13 sequence, 14 transportUnavailable, 15 storageUnavailable.
São distintos de `RejectCode`, ABI status e erros de arquivo. Enviar no máximo um
fatal confiável, se possível, e fechar; não responder fatal com fatal. Nunca
devolver bytes desconhecidos ao parser legado nem executar strings como comandos.
subjectSequence/fieldId usam 0xffffffff quando não aplicáveis; fieldId é ID do
schema, não offset nativo. detail é zero nesta versão. Não enviar endereços,
strings privadas ou códigos de erro do sistema nesses campos.

SVC 20 continua indisponível para spawn static neste cliente. Se um mod exigir
essa semântica, o perfil precisa de binding explícito aprovado para objetos
estáticos do snapshot; sem ele, recusar. Sentenças, sprays, entidades, texturas,
partículas, formatos de arquivo e canais não se tornam ilimitados por esse desenho.
As provas de bootstrap, dispatch, callbacks e lifecycle continuam pendentes.
