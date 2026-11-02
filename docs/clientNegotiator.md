# Negociação no cliente

Estado: **núcleo implementado, sem binding**. `ClientNegotiator`, no alvo
`extendedPrecacheTransport`, conduz o lado do cliente da [negociação](negotiation.md), ao longo
dos mapas de uma conexão. É independente do motor e nada o liga ao jogo ainda. A contraparte no
servidor é a [barreira do sign-on](signonBarrier.md), com as mesmas portas.

## Decisões por fase

| Fase | Registro de `ExtPrecache` | Frame | Serverinfo |
| --- | --- | --- | --- |
| `idle` | Confere | Oferta abre a sessão; outro frame é ignorado | Segue, ou recusa em `requireExtended` |
| `negotiating` | Confere | Entrega à sessão | Confirma se a sessão está `armed`; senão recusa |
| `confirmed` | Confere | Oferta com outro nonce abre outra sessão | Recusa |
| `refused` | Recusa | Ignora | Recusa |

Registro nunca abre sessão, porque o motor o repete: no bootstrap, na ativação do mapa e no
sign-on. Sessão nova é sempre uma oferta com outro nonce, que encerra o acordo anterior. O
registro confere o intervalo do ID e o tamanho variável, e o ID precisa se manter durante a
conexão. `admitServerInfo` roda antes do parser nativo, que começa limpando o estado do cliente.

Um resultado de erro significa que a conexão precisa ser encerrada. A sessão já terminou, a
reserva foi liberada e o frame `reject` foi enviado se havia nonce e espaço. A causa devolvida é
a primeira. Um `reject` recebido do servidor não é respondido.

## Envio e conexão

`pump` roda uma vez por frame do cliente: aplica o prazo e envia os comandos `epc1` que couberem.
Sem espaço, o frame não é confirmado e a próxima chamada o retoma. `connectionLost` encerra tudo
sem responder, porque não há mais a quem responder, e devolve a fase a `idle`, mesmo depois de uma
recusa: uma nova conexão recomeça do zero e pode ter outro ID.

## Limites

Nada aqui foi exercitado no motor pelo código publicado: os testes são locais e colocam as duas
pontas em processo, com canais sintéticos. Cliente legado em `preferExtended` segue sem a prova de
`checkLegacyAdmission`, aceitável enquanto nada amplia limites. Não amplia limites nem habilita
transporte.
