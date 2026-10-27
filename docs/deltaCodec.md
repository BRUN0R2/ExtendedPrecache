# Schemas e estado delta — contrato de dados 2

Estado: especificação. Os corpos de SVC 14/15/22/40/41 são novos somente no acordo
estendido; nenhuma descrição delta antiga deve escapar antes do gate. Não mudar
apenas `delta.lst` ou a quantidade de bits: índices passam por tipos inteiros,
sem escalas float ou casts para short. [dataCodecs.md](dataCodecs.md) define o
envelope e [moduleContracts.md](moduleContracts.md) vincula campos ao código local.

## SchemaSet canônico

```text
schemaCount:u16
para schemas em schemaId crescente: schemaBytes:u32, schema[schemaBytes]
```

schemaCount>0; IDs únicos em 1…65535. SHA-256 desses bytes deve coincidir com
schemaSetSha256 da aplicação e do manifesto EPCM. O total fica dentro de
`limits.schemaSetBytes` da aplicação. SVC 14/schema leva um schema inteiro;
schemaSetEnd leva o digest de 32 bytes. A ordem e os bytes não mudam na sessão.
Checar tamanho por registro, soma do conjunto, quantidades e orçamento antes de
alocar. Cada corpo schema, além do conjunto, deve caber no registro com envelope.
O receptor conhece a quantidade exata pela aplicação; schemaSetEnd não permite
parar em um subconjunto. Não confiar em contagens para percorrer bytes ainda não recebidos.

Schema tem header de 8 bytes e descritores:

```text
schemaId:u16, role:u8, reserved:u8=0, fieldCount:u16, nameBytes:u16
name[nameBytes]
fieldCount descritores:
  fieldId:u16, valueKind:u8, resourceKind:u8, flags:u16
  maxElements:u16, maxAtomBytes:u16, nameBytes:u8, reserved:u8=0
  name[nameBytes]
```

Roles: 1 entity, 2 client, 3 weapon, 4 eventArgs, 5 moduleState, 6 modMessage,
7 effect, 8 localReplay. Nomes ASCII de identificadores, 1…63 bytes, sem NUL;
comparação exata contra o schema aprovado. Campos têm IDs densos 0…fieldCount−1,
nomes únicos. fieldCount=0 é permitido somente nos roles 4/5/6/8, para mensagens
ou callbacks sem argumentos; os roles 1/2/3/7 exigem campos. Nenhum offset,
ponteiro, função ou tipo nativo é aceito do peer. Descritor custa 12+nameBytes.

### Átomos de valor

| valueKind | Nome | Bytes por átomo / validação |
| --- | --- | --- |
| 1 / 2 | u8 / i8 | 1; sinal explícito |
| 3 / 4 | u16 / i16 | 2 little endian |
| 5 / 6 | u32 / i32 | 4 little endian |
| 7 | f32 | 4 IEEE-754 binary32, finito e zero canônico positivo |
| 8 | bytes | length:u16 + bytes; length<=maxAtomBytes |
| 9 | resource | DomainId:u32 da categoria estática resourceKind |
| 10 | entity16 | 2, dentro dos limites nativos do objeto/serviço |
| 11 | packedEntity16 | 2; entidade nos 12 bits baixos e attachment nos 4 altos, ambos validados |
| 12 | coord16 | 2, inteiro com sinal em unidades de 1/8; não amplia coordenadas |
| 13 | coords3 | 6, três coord16 |
| 14 | vec3f32 | 12, três f32 canônicos |

`resourceKind` é 1…5 só para kind 9; nos outros, zero. Flags: nullable=1 só para
resource; array=2; nativeCString=4 só para bytes. Outros bits recusados. Scalar
exige maxElements=1. Array codifica count:u16 e count átomos, count<=maxElements;
maxElements>0. maxAtomBytes é exatamente a largura fixa, ou 1…65535 para bytes.
nativeCString recusa NUL embutido e reserva mais um byte para NUL ao converter
ao consumidor local; não confunde tamanho do buffer nativo com bytes na rede.

Para resource, zero é permitido somente com nullable; os demais valores são
validados contra a contagem congelada da categoria e o subtipo exigido pelo
binding. Entity/player/attachment, sentença e handle HUD não usam esse átomo.
Não alargar esses campos só por parecerem shorts. coord16 preserva a quantização
do protocolo; f32 não admite NaN, infinito nem bits de −0 no wire. Não avaliar
conteúdo bytes como comandos ou código.

Calcular o pior tamanho com aritmética u64: átomo bytes até 2+maxAtomBytes;
array até 2+maxElements*atomMaxBytes. Um schema cujo pior objeto não caiba em um
registro confiável/reserva permitidos é recusado antes de S0. Não truncar uma
lista de campos, aumentar a reserva depois ou desativar checks no receptor.

## Frame de estado transacional

SVC 22 é o snapshot inicial confiável. SVC 40 é frame completo; SVC 41 é delta.
Em uma transação, todos os registros usam a mesma entrega. SVC 15 pode carregar
objetos client/weapon e SVC 43/moduleState objetos module, sem abrir outra
transação. Um receptor mantém no máximo uma transação pendente por entrega.
Begin/end usam 22 no inicial (base=0), 40 no completo (base=0) e 41 no delta
(base não zero). Objetos entity/static usam o mesmo opcode do begin; client/
weapon usam 15/kind 2, module usa 43/kind 6. Essas rotas canônicas evitam dois
decoders para um mesmo objeto. Não aceitar object/end sem begin correspondente.

frameBegin: `frameId:u32, baseFrameId:u32, changeCount:u32, changeBytes:u32,
serverTime:f32, reserved:u32=0` (24 bytes). serverTime é finito, não negativo,
zero canônico e não decrescente nos frames aplicados; só avança com o commit do
frame. O servidor aloca frameId em 1…0xfffffffe, estritamente crescente; não faz
wrap. Isso não exige chegada em ordem entre os dois fluxos. Uma transação de ID
já aplicado/superado é consumida e descartada após framing/contexto válidos,
sem restaurar estado, relógio, base ou emitir um novo pedido de recuperação.
baseFrameId=0 significa completo; delta exige uma base completa, confirmada e
retida da mesma geração/schema set. changeBytes é a soma dos corpos frameObject,
limitada por `limits.stateFrameBytes`; changeCount também é validado contra os
objetos possíveis declarados na aplicação. O mesmo stateFrameBytes limita a soma
dos fullObjects reconstruídos, inclusive seus headers, mesmo quando o delta é
pequeno. Preflight calcula o pior estado permitido pelos schemas/objetos e a
janela de bases; não basta conferir bytes de diferenças recebidos.

frameObject, header de 16 bytes:

```text
frameId:u32, objectKind:u8, operation:u8, schemaId:u16
objectId:u32, maskBytes:u16, reserved:u16=0
mask[maskBytes], valores de campos presentes por fieldId crescente
```

objectKind: 1 entity, 2 client, 3 weapon, 4 module, 5 static. O profile define
schemaRole e limites nativos exatos por objeto; não aumenta o número de edicts,
armas ou jogadores. IDs de objeto e DomainId de recurso são namespaces distintos.
Roles são entity=1, client=2, weapon=3, module=5 e static=1. IDs entity/static/
weapon começam em zero, client em um; cada um respeita sua contagem declarada.
Module objectId deve ser moduleId registrado, com schema de estado aprovado no
binding. O binding local aprova os schemas possíveis de cada classe nativa.
Ordenar mudanças estritamente por (objectKind,objectId), sem duplicatas.

operation 0 replace exige todos os campos do schema; 1 patch exige base, mesmo
schema e pelo menos um campo; 2 remove exige schema da base, maskBytes=0 e nenhum
valor. World/entity 0 não pode ser removido. Patch de objeto inexistente é erro;
mudança de schema exige replace completo. Para replace/patch, maskBytes é
exatamente ceil(fieldCount/8), bits LSB-first por fieldId, cauda não usada zero.
Ausência em patch copia o valor da base; nunca cria defaults de recurso válidos
a partir de zero sem nullable. Objetos estáticos de SVC 20 só têm binding se
aprovado; a existência desse objectKind não habilita o handler nativo obsoleto.

frameEnd: `frameId:u32, changeCount:u32, snapshotHash[32]` (40 bytes). Exigir todos
os corpos e changeBytes exatos. Reconstruir em staging e validar o estado inteiro
antes de publicar qualquer campo/callback. Falha deixa a base intacta, fecha o
gate se for violação e inicia retirada quando necessária.

Para o hash, serializar objetos completos ordenados, sem masks:

```text
fullObject = objectKind:u8 || reserved:u8=0 || schemaId:u16 || objectId:u32
             || valuesBytes:u32 || todos os valores do schema
snapshotHash = SHA256("ExtendedPrecache/state/2\0" || mapEpoch:u64
                      || catalogHash[32] || schemaSetHash[32]
                      || frameBeginBody[24] || fullObjectCount:u32 || fullObjects)
```

Isso vincula a reconstrução, não só o pacote de diferenças. O snapshot inicial
inclui os estados client/weapon/module necessários antes de gameStart. Todo
frame completo exige entity/0 do mundo; classes obrigatórias do binding não
podem faltar. Base zero permite somente replace. Staging,
bases e cópias retidas são orçados no coordenador, além das filas inFlightBytes.
Liberação de um frame não invalida objetos/pins ainda usados pelo motor.

## ACK, perda e recuperação

CLC 7/frameAck: frameId:u32 + snapshotHash[32], somente depois de aplicar o frame
inteiro. O servidor usa como base apenas um frame confirmado desse cliente;
mantém até `limits.snapshotWindowFrames`, sem referências a memória descartada.
No snapshot inicial, gameplayAck faz também essa confirmação; não enviar frameAck
separado antes de gameStart nem publicar callbacks por ter recebido frameEnd.
O emissor mantém ID/hash das bases retidas e um marcador retiredThrough para
frames já retirados, retirando sempre o prefixo mais antigo em ordem de frameId.
ACK de frame retirado é consumido sem progresso; não
ressuscita base nem exige histórico ilimitado. ACK futuro/não enviado, hash
divergente de frame ainda retido ou outra geração é erro. Se não houver base
confirmada retida, emitir completo; nunca usar um ACK velho como acesso a memória
já liberada. A janela/cotação limita também o ledger de frames retidos.

Em não confiável, frameBegin novo pode substituir staging incompleto antigo;
frameEnd sem todos os registros, base já expulsa ou gaps não produz estado novo.
Object/end sem begin em não confiável também é perda incompleta: descartar e
solicitar recuperação; em confiável é violação de fase. Não declarar um pacote
malformado como perdido para ignorar campos/tamanhos inválidos.
O cliente solicita um snapshot completo confiável via resyncRequest
(`lastAppliedFrameId:u32`); no máximo uma solicitação pendente por conexão.
Completar e confirmar esse snapshot encerra a recuperação. É recuperação de
perda prevista do transporte, não retransmissão por sleeps/retries arbitrários.
O servidor coalesce pedidos enquanto há recuperação pendente. Envia um novo
frame completo confiável e suspende novos frames de estado **dessa conexão** até
seu frameAck exato; envelopes de outras classes continuam segundo suas políticas.
Assim não surge frame mais novo que torne a recuperação obsoleta antes do ACK.
Se a janela expirar sem base confirmada utilizável, o servidor pode iniciar a
mesma recuperação confiável sem aguardar resyncRequest.
O caso inclui perda do frame inteiro, em que o cliente não chegou a produzir
resyncRequest; o pedido C→S, quando produzido, continua confiável.
Não fixar contagens/timeout novos nem repetir full frames sem confirmação; o
timeout/fechamento do canal nativo permanece aplicável durante gameplay.
Mensagens malformadas, hashes incompatíveis, valores inválidos ou contagens
excedidas continuam fatais mesmo em não confiável.

Se uma transação não couber no orçamento não confiável, toda ela vai confiável;
não misturar os dois modos dentro de frameId. Frame confiável mais recente torna
frames antigos obsoletos; dados atrasados não podem restaurar uma base anterior.
Reter IDs, relógios e gerações em jobs; a passagem por um reset não apaga o acordo.
