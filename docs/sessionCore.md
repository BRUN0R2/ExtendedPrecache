# Núcleo de configuração e negociação

Implementação C++26 independente do motor, anterior aos adaptadores. Os alvos
`extendedPrecacheSession` e `extendedPrecacheCrypto` não são ligados à DLL
diagnóstica. Não registram mensagens, interceptam funções ou modificam o jogo.
O contrato da ABI pública continua inalterado e `engineExtensionActive=0`.

## Configuração e admissão

`parseConfiguration` valida os bytes UTF-8, léxico, duplicatas, estrutura fechada,
campos obrigatórios, intervalos, dependências e estimativa base de memória.
`Configuration::create` aplica as mesmas invariantes aos valores tipados criados
em C++; `validateConfiguration` acrescenta reservas e overhead do perfil local.
Os erros têm código, campo estático quando aplicável e offset lexical disponível.
Não são retornados ponteiros para o texto da entrada.

`readConfiguration(path, role)` usa defaults completos somente se o caminho não
existir. Arquivo presente inválido, symlink quebrado, diretório, falha de leitura,
papel divergente e tamanho excessivo são erros. A leitura é limitada a 65.537
bytes para detectar excesso sem carregar o restante. Não procura arquivos nos
diretórios do jogo. Falha de alocação é explícita, sem fallback de configuração.

A biblioteca padrão não fornece parser JSON. A dependência privada
`nlohmann/json` 3.12.0 usa SAX para detectar duplicatas antes de construir uma
representação que as perderia; recusa floats, expoentes e `-0` antes da validação
dos campos. A profundidade de objetos é limitada à estrutura do contrato.
O código implementa este schema específico, sem anunciar um interpretador geral
de JSON Schema. Versão, hash do download e licença estão fixados no repositório.

`CapabilityLimits` representa tetos escalares válidos e `MapRequirements`
representa o snapshot completo, com mundo, soma das categorias e consistência.
`intersectLimits` calcula o mínimo exato; `admitRequirements` identifica o primeiro
campo insuficiente e os dois valores. Nenhum recurso é removido para caber.
Valores por campo têm tipos distintos; conversão para arrays u32 fica nas
fronteiras dos vetores de protocolo.

## Codec

`FramePayload` é uma variante de corpos tipados; o tipo de mensagem deriva do
corpo. `encodeFrame` e `decodeFrame` implementam explicitamente little endian,
sem serializar layouts C++. `EncodedFrame` possui armazenamento fixo para até
60 bytes e só é produzido por uma codificação validada.

O decoder exige tamanho exato, magic, versões conhecidas, enums, sequência,
nonce não zero, reservados zero e páginas de quatro IDs consecutivos. O codec
verifica a forma da página; a sessão verifica sua posição na conversa.
`decodeClientCommand` aceita exclusivamente `epc1 ` e hexadecimal minúsculo,
até 125 caracteres, sem newline, NUL, aspas ou outros comandos. Opcode, NUL e
registro de user message pertencem ao transporte futuro.

## Sessão e envio confiável

Criar uma instância por conexão com `createServer` ou `createClient`, após
capturar configuração e binding imutáveis. O relógio é monotônico, em ms.
O chamador fornece tokens não zero de conexão e peer e uma geração local de mapa;
esses tokens não substituem autenticação. A geração local não é o epoch recebido
na oferta. O servidor também fornece snapshot congelado, spawn, epoch, identidade
de catálogo e nonce novo; o cliente fornece seu nonce novo.

As factories exigem `ContractProfile` disponível, ID e digests correspondentes,
configuração consistente e orçamento suficiente. O perfil é fornecido por código
confiável que verificou os módulos e implementações locais; nunca é inferido do
peer, do JSON de configuração ou da coincidência de um inteiro. Não existe
registro de perfis de motor aprovados nesta entrega. Os exemplos versionados
continuam indisponíveis; somente os testes injetam disponibilidade sintética.

O ciclo de envio é:

1. Chamar `prepareOutbound(context)`; ausência de frame significa aguardar o peer.
2. Enfileirar os bytes retornados no canal confiável, ordenado e pertencente à
   mesma conexão. Preparar novamente retorna o mesmo frame sem avançar estado.
3. Após aceitação da fila, chamar `commitOutbound(frame, contextoAtualizado)`.
   Conferem-se bytes exatos, relógio, binding e reserva antes de avançar.
4. Se a fila falhar, abortar com `transportUnavailable` e encerrar o canal;
   um frame preparado ou recusado não conta como envio concluído.

`receive` aceita somente a direção remota e o próximo tipo/sequência previstos.
Repetição idêntica não avança nem aumenta quota; repetição alterada e gap são
terminais. Nonce antigo não zero e binding de outra conexão/peer são descartados.
Esses descartes não dispensam rate limit no transporte. `poll` deve ser chamado
também em conexões silenciosas. Não há temporizadores, threads ou retries internos.
O acesso à instância é serializado por um único proprietário; providers devem
permanecer vivos e não reentrar na sessão. Não usar uma instância após movê-la.

Os transcripts são fixos, com no máximo 32 frames por direção, e os bytes únicos
das duas direções são limitados pela configuração antes de armazenamento/hash.
SHA-256 recebe os labels com NUL final e os frames completos aceitos, conforme
`negotiation.md`. O provider Windows usa CNG do Windows 10 ou superior, incluindo
CSPRNG para os nonces. O núcleo recebe `DigestProvider` por interface e não
implementa uma primitiva criptográfica própria.

## Preparação e liberação

`SessionPreparer` devolve ownership exclusivo de uma `PreparedSession`: reserva
durável, decoder preparado e cotação completa do pico de metadados. Deve falhar
explicitamente se não puder garantir tudo, sem reduzir capacidades. A reserva
fica na sessão durante o acordo e o reset nativo; não pode apontar para memória
que `Host_ClearMemory` ou a troca de mapa liberará.

`minimumSessionMetadataBytes` acrescenta à estimativa base os buffers conhecidos
da instância e o workspace de hash, descontando somente staging de transcript já
contado. Seu valor depende do ABI C++ da compilação. Continua sendo um piso:
providers precisam cotar também suas estruturas, cópias, alinhamento e demais
alocações de pico. Uma cotação abaixo desse piso ou acima de `metadataBudgetBytes`
é `storageUnavailable`. Os testes de reserva são sintéticos, sem medição de RAM
de um motor. As quatro estimativas de referência publicadas permanecem iguais.

O cliente prepara antes de C8 e só alcança `armed` após C9 aceito na fila. O
servidor prepara antes de S15 e só alcança `committed` ao receber C9. A validade
da reserva é reconferida no commit, no ACK e no handoff. Reserva inválida,
snapshot/geração divergente, desconexão ou `close` fecham o caminho e liberam
ownership. Apenas `std::bad_alloc` da preparação é convertido em falha de reserva;
outras exceções de implementação do provider não são mascaradas.

`releaseSignon` retorna o acordo somente no servidor comprometido, com canal
explicitamente `drained` e reserva ainda válida. É uma autorização tipada; não
envia serverinfo e pode ser concedida uma única vez. Uma segunda solicitação
do mesmo binding invalida a sessão e libera a reserva; uma consulta de outra
conexão ou peer não invalida a sessão original. Uma solicitação enquanto o
canal ainda não está drenado não consome a autorização. O cliente deve chamar
`confirmServerInfo(48, spawn, context)` antes de
aceitar o primeiro serverinfo. Uma segunda confirmação invalida a sessão e libera
a reserva, mesmo quando protocolo e spawn count coincidem. Isso também não
carrega assets nem ativa gameplay.
O decoder de dados ainda precisará conferir o manifesto contra o digest acordado.

O deadline continua desde a entrada original, inclusive enquanto se espera drain
ou primeiro serverinfo. Aplica-se o menor timeout anunciado, sem reinício. Depois
do handoff validado, o prazo da negociação não se transforma em timeout de carga
de assets. Regressão do relógio, canal indisponível e geração divergente falham
explicitamente. O chamador precisa fornecer relógio/contexto atuais após qualquer
operação externa, inclusive preparação e enfileiramento.

Rejeição preserva a primeira causa, solta a reserva e permite no máximo um frame
terminal, se houver nonce, canal, sequência e orçamento. Valores de estado do canal
desconhecidos são recusados, assim como um canal indisponível. A preparação e a
confirmação desse frame exigem a mesma geração local da conexão original. Depois de
observar uma geração diferente no mesmo binding de conexão e peer, a sessão descarta o frame
pendente e não volta a emitir mesmo que receba um contexto antigo; uma consulta de
outra conexão ou peer não invalida a sessão original. O transporte deve conferir o
binding atual antes de enfileirar: a confirmação posterior não desfaz bytes já
entregues ao canal. Não responde a rejeição recebida. `close` encerra também o envio
terminal. `checkLegacyAdmission` só aceita
antes da negociação, com política permissiva, mapa inteiro dentro dos limites e
`LegacyProof::complete` fornecido por validação externa dos consumidores e bytes.
O núcleo não fabrica essa prova nem faz downgrade após erro ou timeout.

## Verificação e limites

Os executáveis CTest consomem os arquivos de `docs/protocol` diretamente. Os bytes
e hashes esperados não são gerados pela implementação sob teste. Incluem validador,
admissão, framing/truncamentos, diálogo completo dos dois endpoints e todos os 12
cenários de estado descritos no corpus, além de reservas, filas, bindings e quotas.
O harness transforma exceções inesperadas de fixture em saída de falha; não as
transforma em checks aprovados. Análise estática mantém todos os checks ativos.

Os casos de bytes/índices de tráfego legado, comparações de hashes dos binários,
bootstrap nativo e integração de demos continuam exigindo os componentes
correspondentes. Os [codecs de dados isolados](dataCore.md) compõem um alvo próprio.
Nenhum desses testes é uma conexão CS 1.6/ReHLDS ou validação
de instalação Metamod. Próximas etapas continuam em `TODO.md`.

O [desenho de integração](integrationVerification.md) especifica como o futuro
provider deve separar sua reserva privada do mapa compartilhado, preservar o
acordo no reset inicial e retirar bindings. Esses owners e a ABI 2 continuam
somente especificados; não são novas funções deste núcleo nem exports da DLL.

Referências: [SAX](https://json.nlohmann.me/api/json_sax/),
[parser SAX](https://json.nlohmann.me/api/basic_json/sax_parse/),
[release JSON 3.12.0](https://github.com/nlohmann/json/releases/tag/v3.12.0),
[BCryptHash](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcrypthash),
[BCryptGenRandom](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptgenrandom).
