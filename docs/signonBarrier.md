# Barreira do sign-on

Estado: **núcleo e binding do servidor implementados, inativos sem perfil aprovado**. O alvo
`extendedPrecacheTransport` decide, para uma conexão, o que o sign-on nativo pode fazer, e é
independente do motor. O [plugin Metamod](metamodPlugin.md) o liga ao motor, mas só instala
algo com um perfil aprovado, e não há nenhum: o build publicado continua inerte. As observações
que motivam estas regras estão em [transportBootstrap.md](transportBootstrap.md).

## Portas

`ClientBarrier` existe por conexão. Quem a cria entrega o resultado do opt-in, o binding, o
ID alocado da mensagem de controle e duas portas, que precisam viver tanto quanto ela:

| Porta | Responsabilidade |
| --- | --- |
| `ReliableChannel` | Informa o estado do canal e enfileira uma mensagem inteira, ou nada |
| `SessionSource` | Abre a sessão com o contrato e o mapa locais, que este módulo não conhece |
| `LegacyAdmission` | Diz se um cliente sem opt-in acompanha o mapa atual com o protocolo nativo |

`parseClientOptIn` lê o `userinfo` como o motor: chave ausente é legado, valor `1` é opt-in,
chave repetida é `malformedFrame` e outro valor é `unsupportedVersion`. `ControlMessageId` só
aceita um ID alocado entre 64 e 255. `encodeRegistration` e `encodeFrameMessage` produzem os
bytes do SVC 39 e da mensagem variável; o canal não interpreta nada.

## Decisões por fase

| Fase | `new` e `spawn` | `sendres` | `epc1` |
| --- | --- | --- | --- |
| Legado, sem opt-in | Segue, ou recusa se o mapa exige a extensão | Igual | Conta e ignora |
| `idle` | Retém e abre a sessão | Retém | Conta e ignora |
| `negotiating` | Retém | Retém | Conta e entrega à sessão |
| `released` | Segue | Segue | Conta e ignora |
| `refused` | Retém | Retém | Conta e ignora |

`spawn` abre sessão porque o corpo nativo chama `SV_New_f` quando o spawn count diverge. Os
nomes nativos são comparados sem diferenciar maiúsculas, como o motor os procura. O token
`epc1` é exato: `isControlCommand` aceita o token sozinho ou seguido de espaço.

Um resultado de erro significa que a conexão precisa ser encerrada. A sessão já terminou, a
reserva foi liberada e o frame `reject` foi enfileirado se o registro já tinha seguido e havia
espaço. A causa devolvida é a primeira. `refused` não volta atrás, nem com um novo mapa.

## Envio e liberação

`pump` roda uma vez por frame do servidor: aplica o prazo e envia o que couber, sempre com o
registro antes do primeiro frame. Sem espaço, o frame fica preparado e não é confirmado; a
próxima chamada o retoma, limitada pelo prazo da sessão. Não há retransmissão própria, porque
o netchan já a faz.

`tryRelease` concede `continueSignon` no máximo uma vez por mapa, com a sessão comprometida e
o canal drenado. A chamada deve acontecer onde o comando nativo rodaria: o estado do canal só
passa a drenado ao processar um pacote do cliente, e nesse ponto o motor já selecionou a
conexão. `beginGeneration` encerra o acordo do mapa anterior, e o próximo comando de sign-on
abre outra sessão. Reconexão é outra barreira, criada pelo dono para a nova conexão.

## Limite de comandos

`maxControlCommands` permite 32 `epc1` por janela, o máximo de frames de uma direção. O canal
confiável entrega cada comando uma vez, então uma conversa legítima não precisa de mais. A
janela abre com a conexão e reinicia a cada sessão. O 33º recusa com `budgetExceeded`, em
qualquer fase, inclusive sem sessão. Assim, a repetição idêntica, que a sessão não cobra, e o
limitador nativo, que não atua antes do sign-on, deixam de ser lacunas.

## Binding no servidor

`approvedTransportProfile` devolve o perfil e, por enquanto, nada. Só com um perfil o plugin
confere o SHA-256 do `swds.dll` contra o hash selecionado, abre a API do ReHLDS (`major` 3,
`minor` 15 ou maior) e registra os hooks, sempre à frente dos outros plugins. Qualquer recusa
deixa o plugin carregado e sem hooks, com o motivo no console. Os hooks ficam até o fim do
processo, presos por um `RetainedUse`.

| Hook da API | Papel |
| --- | --- |
| `HandleNetCommand` | Tenta a liberação e consome `epc1` antes do parser nativo |
| `ExecuteServerStringCmd` | Aplica as decisões a `new`, `sendres` e `spawn`, já tokenizados pelo motor |
| `ClientConnected` | Lê o opt-in e cria a barreira da conexão, substituindo a anterior do slot |
| `SV_DropClient` | Encerra a barreira da conexão |
| `SV_ActivateServer` | Inicia a nova geração; depois do corpo, registra `ExtPrecache` na primeira vez |
| `SV_Frame` | Chama `pump` em cada conexão, antes do corpo, para o que couber sair no mesmo frame |

`epc1` não chega a `ExecuteServerStringCmd`, pois o motor só executa ali os comandos da lista
nativa. Por isso, o texto é lido sem consumir no buffer da mensagem e, sendo do token reservado,
o cursor avança e o parser nativo não roda. A contabilização nativa não vê esses comandos, e o
limite acima a substitui, inclusive com o cliente ativo.

A continuação roda dentro de `HandleNetCommand`, no pacote do cliente. Ali o motor já processou
os ACKs e selecionou a conexão, como faria para o comando nativo, então nada escreve em
`host_client`: só `cmd_source` é ajustado e restaurado. O corpo é o de `new`, achado na lista de
comandos da API. Um pacote sem nenhuma mensagem não dispara o hook, e a liberação fica para o
próximo, sempre limitada pelo prazo da sessão.

`ExtPrecache` é registrado logo depois da primeira ativação: os registros do jogo mantêm os IDs
e ainda não há cliente para receber a entrada nova. Se o host já conhece o nome, ou o ID não é
válido, o transporte fica inativo no mapa, assim como com `maxclients` menor que 2.

O estado drenado é a única leitura sem acessor na API: `reliable_length`, `waitlist` e `fragbufs`
do netchan, a partir do buffer de mensagem que a API expõe. Esses deslocamentos só valem para o
hash selecionado, que é o único aceito. Uma recusa vai ao console e ao cliente, com o texto de
`refusalReason`, e encerra a conexão. Memória esgotada numa etapa da barreira tem o mesmo fim,
com `storageUnavailable`.

## Limites

O binding foi exercitado localmente com um perfil sintético injetado na hora de linkar, com o
`hw.dll` real e com um cliente sintético; as observações estão em
[transportBootstrap.md](transportBootstrap.md#situação-em-execução). A reserva real e a admissão
legada existem só para modelos, em [modelTables.md](modelTables.md), e não há perfil aprovado, então
os artefatos publicados não habilitam transporte.
