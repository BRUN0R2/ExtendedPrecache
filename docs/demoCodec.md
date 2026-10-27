# Container de demo estendida — versão 2.0

Estado: especificação, sem recorder/player implementados. Extensão planejada
`.epcdem`, magic `EPCDEMO2`. Não escrever dados ampliados fingindo ser uma demo
GoldSrc protocolo 5, nem presumir que a conversa C→S está na demo tradicional.
O formato nativo possui registros próprios de client/view/eventos além de pacotes;
referência pública: [DemoFile.h](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/HLTV/common/DemoFile.h#L43).
Nenhum layout com ponteiros dessa API é serializado aqui.

O primeiro perfil é **gravação no cliente desde o bootstrap e reprodução linear**.
Armar antes da conexão; iniciar gravação no meio do mapa, append e seek são
recusados, sem gerar uma demo aparentemente completa. Checkpoints/HLTV/server
recording exigem outro contrato antes de suporte. Recomeçar playback cria novo
runtime/geração local; identidades de arquivo não são handles persistentes.

## Header: 256 bytes

Inteiros little endian, sem padding. Digests e nonce como arrays de bytes.

| Offset | Bytes | Campo |
| --- | --- | --- |
| 0 | 8 | magic `EPCDEMO2` |
| 8 | 2+2 | major=2, minor=0 |
| 12 | 4 | headerBytes=256 |
| 16 | 4 | dataContractId=2 |
| 20 | 4 | spawnCount gravado |
| 24 | 8 | mapEpoch não zero |
| 32 | 16 | serverNonce gravado não zero |
| 48 | 48 | 12 limites selecionados, u32 na ordem da negociação |
| 96 | 32 | applicationContractSha256 |
| 128 | 32 | dataContractSha256 |
| 160 | 32 | catalogSha256 |
| 192 | 32 | schemaSetSha256 |
| 224 | 32 | SHA-256 exato dos primeiros 224 bytes |

Validar versões, limites e hashes antes de usar tamanhos/contagens. O arquivo
não autentica quem o gravou e um selectionHash isolado não provaria handshake.
Replay verifica localmente codecs, módulos, artefatos, assets e orçamento; não
cria `NegotiationSession` nem emite C0/C8/C9/CLC 7. Acordo offline é um tipo/contexto
próprio, sem transporte de rede e sem autorização de conexão real.

## Blocos: 56 bytes de header + payload

```text
kind:u16, flags:u16=0, payloadBytes:u32
blockSequence:u64, timeMicros:u64, payloadSha256[32], payload[payloadBytes]
```

Sequência contígua desde zero; sem wrap. timeMicros é tempo desde o início, não
decrescente, no máximo limits.demoDurationMicros. Prelúdio usa zero. Conferir
payloadBytes+56, arquivo restante, soma total e limites configuráveis antes de
alocar/avançar o cursor. Offsets/totais de arquivo são u64 mesmo no player x86;
não converter arquivo inteiro para size_t nem mapeá-lo de uma vez.

Tipos conhecidos:

| Kind | Payload e ordem |
| --- | --- |
| 1 dataArtifact | totalBytes:u32, offset:u32, bytes; chunks contíguos dos bytes exatos do descritor de dados |
| 2 applicationArtifact | Mesmo framing, documento de aplicação exato |
| 3 schemaSet | Mesmo framing, SchemaSet binário canônico |
| 4 catalog | Mesmo framing, stream EPCM canônico |
| 5 bootstrap | firstReliableSequence:u32, firstUnreliableSequence:u32, serverInfoBytes:u32, primeiro SVC 11 completo e aprovado |
| 6 packet | source:u8 (0 canal, 1 reassemblagem confiável), reserved[3]=0, nativeSequence:u32, packetBytes:u32, payload S→C validado |
| 7 localRecord | recordKind:u16, moduleId:u16, schemaId:u16, reserved:u16=0, valuesBytes:u32, ValueSet completo |
| 8 end | previousBlockCount:u64, lastTimeMicros:u64, streamSha256[32] |

1…4 são concluídos nessa ordem antes de bootstrap/pacotes. Não intercalar streams,
duplicar chunks ou aceitar trailing bytes; totalBytes deve caber no limite
correspondente: artefatos da aplicação, schemaSetBytes ou manifestBytes selecionado.
Seus SHA-256 finais precisam coincidir com o header. O descritor de dados também
amarra os hashes de suas definições; ele não é código/bytecode a carregar.
Resolver apenas contratos/bindings já instalados e aprovados com esse digest;
não buscar URLs ou carregar definições externas indicadas pelo arquivo.

Depois de bootstrap, packet/localRecord podem alternar conforme captura. End é
único e final; seu payload tem 48 bytes. previousBlockCount é o número de blocos
anteriores, lastTimeMicros o último tempo, e streamSha256 é SHA-256 do header
inteiro e de todos os headers/payloads anteriores ao end. O próprio end não entra
nesse hash, evitando ciclo; seu payloadSha256 normal protege sua forma. Exigir EOF
exato após end. Arquivo interrompido/truncado não é demo completa, sem reparo
silencioso ou reprodução parcial como sucesso.

## Captura e inicialização

Registrar apenas dados S→C aceitos do canal, mantendo origem/ordem/tempo e gaps
nativos de não confiável. source=0 pode conter serviços mistos; classificar cada
envelope pela rota aprovada, sem deduzir entrega do bit reliable do pacote.
source=1 só contém dados confiáveis reassemblados; nativeSequence é a sequência
do pacote que completou a reassemblagem. Não gravar fragmentos individuais nem
headers UDP/netchan: packetBytes começa no primeiro opcode de serviço retido.
Handshake EPCN, C→S, schemas e catálogo transferidos no bootstrap não são
duplicados como packets: seus artefatos canônicos estão no prelúdio. Gravar o
primeiro serverinfo somente em bootstrap. Preservar, na ordem de captura, todos
os demais serviços nativeExact aprovados após esse SVC 11, inclusive registros
de user messages, lightstyles e demais dados de inicialização necessários.

O parser aprovado delimita serviços completos antes de remover apenas os
registros omitidos. Se um pacote mistura metadados omitidos e serviços retidos,
concatenar os serviços retidos sem alterar seus bytes/ordem e recalcular
packetBytes; pacote resultante vazio não gera bloco. Isso exige prova de framing
de cada gramática, não cortes por busca de opcode. O binding consome o controle
reservado sem chamar uma NegotiationSession ou a ClientDLL prematuramente.

firstReliableSequence e firstUnreliableSequence são os próximos números EPD2
esperados no fluxo retido, não o último número aceito nem um valor decrementado.
Como schemas/catálogo são prefixo confiável antes do snapshot, firstReliableSequence
é a sequência de frameBegin inicial; firstUnreliableSequence é zero. Serviços
nativeExact anteriores ao snapshot não consomem esses contadores. As sequências
iniciais explícitas permitem omitir metadados sem relaxar a validação posterior.

Manter em storage orçado os serviços de bootstrap e bytes do estado inicial enquanto o prelúdio
é completado; não passar de limits.demoBootstrapBytes ou escrever metadados de
outra geração. Depois do início retido, registrar todo envelope de dados aceito,
incluindo os frames descartados por perda de base, para que a reprodução preserve
as mesmas decisões. Não gravar respostas de rede nem refazê-las no player.

Player valida artefatos/relacionamentos em privado e prepara ReplayContext. Faz
preflight do SVC 11 armazenado (protocolo/spawn), executa o reset inicial aprovado
uma vez com owners fora do hunk, verifica/carrega assets localmente e instala
schemas/catálogo já validados. Após receber/aplicar snapshot inicial e gameStart
gravados, abre o gate local, **sem enviar gameplayAck ou qualquer outro pacote**.
O primeiro estado deve ser completo; referências a base externa são recusadas.
Frames integralmente validados viram bases locais de replay, sem simular ACKs
de rede. A retenção respeita a janela da aplicação, e uma base ausente preserva
a mesma decisão de descarte do cliente gravador.

ResourceFree native services/user messages nos packets continuam sujeitos ao
contrato exato de aplicação e validação antes de dispatch. Nunca alimentar um
player legado com envelopes 2, encaminhar strings a console ou executar um módulo
indicado pelo arquivo. Colisão/versão/política não suportada recusa o playback.

## Estado local e duplicação de eventos

localRecord kinds: 1 view, 2 prediction, 3 weaponAnimation, 4 localSound,
5 localEvent, 6 moduleState. Os cinco primeiros usam schema role localReplay;
moduleState usa seu role próprio. Todo schema e produtor/consumidor deve estar
declarado em localReplayBindings do moduleId não zero da aplicação, inclusive os
bindings do próprio motor. O header localRecord tem 12 bytes. Resource fields, inclusive viewmodel
local, usam DomainId e a mesma geração do catálogo; posições/ângulos/tempos seguem
seus tipos aprovados. Nenhum raw ref_params/usercmd/event_args ou ponteiro passa.

Somente callbacks cuja origem seja local independente geram localRecord. Eventos
já produzidos pelo pacote capturado carregam a marca de origem e não são gravados
de novo. Replay inibe recorder e todas as rotas de saída para rede. Se a aplicação
usa um buffer Demo_WriteBuffer privado ou estado externo que afete IDs, precisa
de schema/binding explícito; sem isso não há perfil de replay disponível.

O objetivo é preservar valores, referências, ordem e callbacks observáveis no
wire/estado tipado local, não prometer pixels/RNG cosmético iguais entre GPUs.
Um módulo não pode usar essa ressalva para buscar recursos fora do catálogo ou
modificar schemas. Seus lifetimes e políticas de replay continuam obrigatórios.

## Orçamento e encerramento

limits.demoFileBytes, demoBlockBytes, demoBootstrapBytes e demoDurationMicros são
políticas fechadas de aplicação. demoBlockBytes inclui os 56 bytes do header;
para armazenar uma mensagem GoldSrc completa, exigir pelo menos
`reliableMessageBytes + 12 packet + 56 block` no perfil. Artefatos maiores são
chunked, não alocações baseadas em um length remoto sem teto. Bounds e EOF são
conferidos antes de qualquer callback ou write de arquivo.

Ao mudar mapa, terminar um arquivo/generation e iniciar outra gravação completa,
sem reutilizar identidades ou anexar mapas por suposição. Falha de disco/cota,
arquivo ausente, digest inválido, módulo diferente ou reset inesperado fecha o
recorder/player e retira seus owners; não remove o arquivo para ocultar falha.
Dados de arquivo não satisfazem uma negociação de rede e não tornam demos antigas
automaticamente compatíveis com o perfil ampliado.
