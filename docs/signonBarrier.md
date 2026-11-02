# Barreira do sign-on

Estado: **núcleo implementado, sem binding**. O alvo `extendedPrecacheTransport` decide, para
uma conexão, o que o sign-on nativo pode fazer. É independente do motor e nada o liga ao
plugin ainda: o [plugin Metamod](metamodPlugin.md) continua inerte. As observações que
motivam estas regras estão em [transportBootstrap.md](transportBootstrap.md).

## Portas

`ClientBarrier` existe por conexão. Quem a cria entrega o resultado do opt-in, o binding, o
ID alocado da mensagem de controle e duas portas, que precisam viver tanto quanto ela:

| Porta | Responsabilidade |
| --- | --- |
| `ReliableChannel` | Informa o estado do canal e enfileira uma mensagem inteira, ou nada |
| `SessionSource` | Abre a sessão com o contrato e o mapa locais, que este módulo não conhece |

`parseClientOptIn` lê o `userinfo` como o motor: chave ausente é legado, valor `1` é opt-in,
chave repetida é `malformedFrame` e outro valor é `unsupportedVersion`. `ControlMessageId` só
aceita um ID alocado entre 64 e 255. `encodeRegistration` e `encodeFrameMessage` produzem os
bytes do SVC 39 e da mensagem variável; o canal não interpreta nada.

## Decisões por fase

| Fase | `new` e `spawn` | `sendres` | `epc1` |
| --- | --- | --- | --- |
| Legado, sem opt-in | Segue | Segue | Conta e ignora |
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

## Limites

Nada aqui foi exercitado no motor: os testes são locais e usam um canal sintético com o papel
de cliente do mesmo núcleo. A admissão legada de `checkLegacyAdmission`, o registro de
`ExtPrecache` no motor e a leitura do estado do canal pertencem ao binding, ainda pendente.
Não amplia limites nem habilita transporte.
