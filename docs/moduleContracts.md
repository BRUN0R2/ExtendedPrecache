# Contrato de aplicação, mods e módulos — versão 2

Estado: especificação indisponível. O schema fechado é
[applicationContractV2.schema.json](data/applicationContractV2.schema.json), com
[rascunho](data/applicationContractV2.example.json) sem módulos aprovados. Não é
uma lista de DLLs que o peer pode mandar carregar. Artefato/implementações são
instalados localmente; comparar SHA-256 dos bytes UTF-8/LF exatos, incluindo newline,
antes de negociar. Não reserializar JSON para comparar identidades.

O contrato de dados 2 define wire; a aplicação identifica produtores, leitores,
schemas, regras de mensagem, limites e comportamento de replay. A disponibilidade
continua `unavailable` até implementação integral e aprovação de cada perfil local.
Hashes de módulos dos papéis podem ser diferentes e são verificados localmente.

## Documento e limites

Root obrigatório, sem defaults/chaves extras: schemaVersion=2, specificationOnly,
availability, dataContractId=2, dataContractSha256, schemaSetSha256, limits,
objectLimits, schemaDefinitions, moduleProfiles, effectPolicies e servicePolicies. Os digests
são 64 hex minúsculos, não zero. O draft não se torna disponível por compilar
ou por o operador trocar uma string. Módulo sem consumidor/produtor auditado,
declaração incompleta ou serviço não classificado impede aprovação.

`limits` contém bytes de schemaSet/artefato/frame de estado, janela de bases,
mensagem não confiável, prazos de carga/consistência e limites de demo (arquivo,
bloco, bootstrap e duração). São limites configuráveis da aplicação, dentro da
representação e da capacidade real de ambos os papéis. Não alteram silenciosamente
o schema de configuração 1. Seu hash muda quando a política muda; cada endpoint
precisa do contrato correspondente e pode recusar antes de S0 se não puder cumpri-lo.

Todos os limites são inteiros exatos; não aceitar float para contagens/bytes,
nem reduzir u64 a size_t/JSON double. demoFileBytes e demoDurationMicros têm teto
INT64_MAX. JSON Schema descreve valores inteiros matemáticos; o parser também
deve recusar tokens numéricos com ponto/expoente, mesmo quando equivalentes a
um inteiro. Essa checagem lexical faz parte do preflight semântico. Usar teto
INT32_MAX nas demais contas de bytes, salvo mensagem não
confiável (65.536) e janela (65.535). schemaSetBytes>=2. Validar relações além
do schema: artefatos cabem em contractArtifactBytes, schema e pior ValueSet com
seus headers cabem na mensagem selecionada, pior estado completo cabe em
stateFrameBytes, e demoBlockBytes comporta reliableMessageBytes+68. A soma das
reservas simultâneas precisa caber no metadataBudgetBytes local de cada papel.
Esses limites de arquivo não autorizam reservar seu tamanho inteiro em memória.

`objectLimits` declara entradas entity/client/weapon/static. Entity fica no teto
nativo selecionado de 2.048, client no de 32; weapon/static ainda precisam do
limite efetivo do perfil. Zero desabilita uma classe permitida, não significa
ilimitado. Objeto module usa o moduleId registrado, não um índice de recurso.
Limites extras não substituem as 12 capacidades negociadas nem as reservas K/A/H.

## Schemas e bindings

schemaDefinitions é a representação declarativa dos schemas de
[deltaCodec.md](deltaCodec.md): IDs, roles, campos, tipos, flags, máximos e nomes.
Os bytes canônicos do SchemaSet são derivados somente desses dados, não da ordem
das chaves JSON. Sort por schemaId e fieldId, IDs de campo densos e validação
semântica precedem a codificação. Conferir o hash declarado e as relações entre
mensagens/roles; não aceitar schema fornecido pelo servidor por ser sintaticamente
válido. Um módulo pode exigir schemas sem publicar seus offsets nativos no wire.

Cada moduleProfile tem moduleId u16 não zero único, nome estável e bindings server
e client (null apenas se esse papel não participa). Um binding informa o SHA-256
do binário e o SHA-256 do perfil de implementação local aprovado. Este último
identifica layouts, offsets, tipos, funções, calling conventions, lifetimes e
conversões aprovados; o JSON da rede não é interpretado como endereços de código.
Arquitetura, versão, bytes completos e procedência precisam coincidir, não só nome
de arquivo, tamanho ou padrão de instrução. Listar todos os produtores e leitores
indiretos, incluindo GameDLL, ClientDLL, Metamod, estúdio, HUD, áudio e DemoPlayer.

A implementação nativa do módulo serializa valores sem padding a partir de seus
campos aprovados e só aplica valores recebidos após validação integral. Para
resource, a conversão conhece categoria/base/ausência; não faz cast de enum ou
float. Inteiros auxiliares/entidades continuam com seu significado. Nomes/scripts
copiados sobrevivem ao argumento e unload segue os leases já especificados.

A ABI 2.0 de metadados continua somente de leitura. Ela não registra codecs,
autoriza ponteiros privados nem cobre a emissão/recepção de mensagens por si só.
O primeiro perfil usa bindings compilados/aprovados, identificados no contrato;
não aceita registro dinâmico de código recebido, plugins de um download ou uma
interface binária nova inventada por convenção. Uma futura API de emissão terá
versionamento próprio antes de ser implementada.

## Mensagens de mods

Cada mensagem declara messageId u16 não zero no namespace do módulo, name ASCII
de até 15 bytes, direção, entrega, schemaId, wireMode, nativeSize e legacyFields.
O schema deve ter role modMessage e representar todos os campos/arrays/strings;
nenhum campo opaco pode esconder índices sem uma prova resourceFree completa.
IDs e nomes não são slots numéricos de user message escolhidos antecipadamente.

`wireMode=typed2`: SVC 43/modMessage ou CLC 7/modMessage, corpo
`moduleId:u16, messageId:u16, schemaId:u16, reserved:u16=0`, seguido do ValueSet
completo. Comprimento exato, direção, fase, dono, schema e orçamento são verificados
antes do callback. Mensagens de mods, inclusive nativeExact, só são entregues
após o gate de gameplay. Estado de inicialização vai no snapshot tipado, não
em callbacks de gameplay durante carga. IDs são DomainId u32 tipados; não
passá-los ao handler antigo que espera `READ_BYTE`/`READ_SHORT`. Os dois papéis precisam de implementação
compatível. Entrega é confiável ou não confiável conforme declaração e envelope;
C→S typed2 é sempre confiável neste contrato, pois não existe fluxo EPD2 C→S
não confiável. typed2 usa nativeSize=-1 e resourcePolicy=typedReferences, sem
legacyFields; esse marcador não solicita registro de uma user message nativa.

`wireMode=nativeExact`: mantém nome/size e gramática legados, até 192 bytes de
payload de user message. nativeSize é −1 para variável ou 0…192 para fixo. O
binding precisa validar a mensagem inteira, inclusive comprimento e cauda, antes
de invocar o consumidor.

nativeExact para user messages existe somente em S→C; GoldSrc não possui o
canal simétrico C→S. Comandos nativos C→S exigem servicePolicies e parser próprio,
não um registro fictício de mensagem de usuário. Mensagem typed2 em qualquer
direção exige os dois bindings server/client presentes no moduleProfile.
Há duas políticas nativeExact permitidas:

- `resourceFree`: prova completa de que nenhum campo/array/caminho de callback
  representa ou reconstrói índice do catálogo ampliado; legacyFields vazio.
- `boundedLegacy`: para cada campo resource, declarar fieldId, largura e sinal
  nativos. Validar **antes de S0** a faixa inteira da categoria congelada após
  conversão de base, incluindo ausência se nullable. Ela precisa caber no campo
  e no armazenamento de todos os leitores. Não inferir que os IDs usados ficarão
  pequenos em execução. Nullable genérico/decal com zero ambíguo não é comprovável
  dessa forma: exigir mensagem typed2 ou outro perfil explícito.

Se um módulo guarda o índice em byte/short antes de chamar MessageBegin/Write*,
a informação já foi perdida; o adaptador não pode recuperá-la. Atualizar o módulo
ou recusar a sessão. Interceptar só WriteByte e reconstruir high bits é proibido.
Mesmo com dados pequenos no mapa atual, não usar uma exceção genérica para liberar
um consumidor cuja capacidade/lifetime não foi comprovada.

O registro variável/fixo de cada user message mantém nome, size, owner e ID
alocado. Colisão, mudança no reset inicial ou registro tardio não previsto é
transportUnavailable/unsupportedModule antes de gameplay. `ExtPrecache` e `epc1`
continuam reservados ao controle e não podem pertencer a um mod. C2S typed2 não
é comando de console: nenhuma avaliação de strings, fallback a command buffer
ou encaminhamento genérico a uma GameDLL.

## Serviços e callbacks externos

servicePolicies cobre **todos** os slots do dispatcher e todos os caminhos
produtores do perfil, por direção/opcode e phases explícitas: negotiation,
bootstrap, loading e gameplay. Não inferir fase de um comprimento pequeno.
Modo forbidden tem phases vazio; nos demais, lista única não vazia e compatível
com os kinds/gates deste contrato. Modos: data2, nativeExact ou forbidden.
nativeExact exige grammarSha256 de um parser local aprovado, maxBytes e owner
conhecido; não basta marcar um blob como seguro. Data2 deve corresponder exatamente
à tabela de [dataCodecs.md](dataCodecs.md). Forbidden tem maxBytes=0 e não permite
emissão. Não há modo implícito para slots desconhecidos, nem autorização de SVC
60…63 por estarem fora da tabela auditada. A lista vazia do draft não cobre CS.
Em S→C, servicePolicies classifica os slots 0…63; 64…255 são resolvidos pelo
registro dinâmico nome/size/owner de user messages e pelos contratos acima,
incluindo o controle reservado. Em C→S, classificar 0…255; slots não implementados
são forbidden. Não congelar IDs alocados de user messages no contrato de serviços.

Customização é um namespace nativo explícito com limites antigos de SVC 46/CLC 5;
sentenças têm tabela de IDs aprovada e identidade no binding de áudio. Nenhum dos
dois vira resourceFree por não ser um modelo. Mensagens de comandos/movimento,
callbacks, dados de estúdio e buffers personalizados de demo também precisam de
classificação; recurso embutido em evento, campo user ou buffer privado exige
schema/consumer correspondente. Desconhecido impede o perfil, não ignora bytes.

`maxBytes` é uma política menor ou igual ao buffer real e ao orçamento aplicável.
Após conversão para typed2, medir novamente envelope/arrays/strings: caber em
192 bytes antes não garante caber depois. Se typed2 exceder não confiável, a
declaração precisa permitir confiável; nunca trocar de entrega sem contrato nem
truncar campos. Os codecs não aumentam taxas, canais de áudio, edicts ou formatos.

## Replay e estado local

Cada módulo declara `replayPolicy=wireStateOnly` ou `typedLocalState`. O primeiro
não depende de estado externo não registrado para resolver IDs/callbacks; o
segundo usa localReplayBindings tipados e completos. Nativos de demo com buffers
arbitrários, viewmodel/client prediction, animações, sons locais e estado próprio
não são capturados apenas gravando o tráfego S→C.

Cada localReplayBinding declara recordKind, schemaId e originPolicy=localOnly.
O binding de implementação identifica produtor/consumidor e marca a origem dos
callbacks: o que já deriva de um pacote gravado não é gravado de novo como local.
Sem conseguir distinguir essas origens, recusar replay, em vez de duplicar eventos.
Não persistir ponteiros/hunks, handles HUD nus ou snapshots de structs C++.

Replay não envia rede, comandos ao servidor nem controla gravação recursivamente.
As rotas de engine commands/callbacks que poderiam fazê-lo são inibidas pelo
profile aprovado. Externos que não respeitem isso impedem disponibilidade.
Preservar efeitos/callbacks observáveis no wire não promete pixels, partículas
ou áudio idênticos entre hardwares/implementações nativas.

## Aprovação conjunta

Antes de negociar, conferir fechamentos, unicidade, schemas/hashes, ambos os
bindings das mensagens, registries de serviços, faixas legacy e reservas de pico.
`effectPolicies` aprova cada effectId usado, entrega e ownerModuleId. Para um
schema base do registro normativo, wireSchema=builtin, schemaId=0 e
resourcePolicy=typedReferences. Outros IDs exigem wireSchema=application,
schemaId role effect e resourcePolicy=resourceFree com prova no binding; não
podem substituir um builtin, alias HIGH ou ID obsoleto. Toda ausência recusa o
efeito. NativeExact não autoriza contornar essa lista por um SVC 23 opaco.
Mudança de módulo/schema depois de S0 invalida o acordo. Unload respeita leases;
config diferente exige novo contrato/conexão e, quando necessário, restart de
caches. Nenhum item deste arquivo transforma os hashes estáticos dos dois motores
em aprovação de GameDLL/ClientDLL, HLTV, listen server ou leitores externos.
