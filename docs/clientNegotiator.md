# Negociação no cliente

Estado: **núcleo e adaptador implementados, sem carregador e sem perfil aprovado**.
`ClientNegotiator`, no alvo `extendedPrecacheTransport`, conduz o lado do cliente da
[negociação](negotiation.md), ao longo dos mapas de uma conexão, e é independente do motor. O alvo
`extendedPrecacheClient` o liga ao `hw.dll`, mas nada carrega essa DLL no jogo ainda e, sem perfil
aprovado, ela não altera nada. A contraparte no servidor é a
[barreira do sign-on](signonBarrier.md), com as mesmas portas.

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

## Adaptador no `hw.dll`

`extendedPrecacheClientAttach` é a única entrada da DLL e devolve um código a quem a carregou,
porque o console do motor pode ainda não existir. Com um perfil aprovado, confere o SHA-256 do
`hw.dll` contra o hash selecionado e instala o adaptador; sem perfil, não faz nada. Precisa rodar
fora do loader lock e antes da primeira conexão.

**Nenhum código do motor é alterado.** O adaptador troca entradas de tabelas de dados do `hw.dll`,
conferindo antes se elas contêm o que este build tem, e usa as funções que o motor entrega à
ClientDLL:

| Ponto | Origem | Papel |
| --- | --- | --- |
| `svc_newusermsg` | Tabela de serviços | Na primeira vez, associa o handler; confere o registro e segue o parser nativo |
| `svc_serverinfo` | Tabela de serviços | Chama `admitServerInfo` antes do parser nativo |
| `HUD_Frame` | Tabela de funções do cliente | Detecta perda de conexão e chama `pump` |
| `pfnHookUserMsg` | API do motor | Entrega os frames de `ExtPrecache` ao adaptador |
| `pfnServerCmd`, `pfnClientCmd`, `pfnConsolePrint` | API do motor | Envia `epc1`, desconecta e informa o motivo |
| `pNetAPI->Status` | API do motor | Informa se há conexão e desde quando |

O registro segue pelo parser nativo, e não por uma tabela privada. A associação é por nome e
[sobrevive](transportBootstrap.md#registro-e-despacho-no-cliente) a `CL_ClearState`, à troca de
mapa e à reconexão. O despacho nativo entrega o frame ao handler também antes do serverinfo, sem
passar pela ClientDLL, que não conhece esse nome. Assim, não é preciso interceptar o dispatcher.

Uma recusa imprime o motivo no console e pede `disconnect`. Se ela acontece num serviço, o cursor
vai ao fim da mensagem: o parser nativo não roda e o motor não lê o restante como comandos. Uma
conexão nova é percebida quando o motor informa que não há conexão, ou quando o tempo de conexão
regride, e nenhuma resposta é enviada à conexão antiga.

As tabelas ficam apontando para a DLL, então ela se fixa no processo antes de alterá-las, e uma
segunda chamada de `extendedPrecacheClientAttach` não muda nada.

## Limites

O adaptador foi exercitado localmente no `hw.dll` real contra o plugin real, com perfis sintéticos
e com o laboratório no lugar do carregador; as observações estão em
[transportBootstrap.md](transportBootstrap.md#situação-em-execução). Falta o carregador, que também
precisa colocar `epc=1` no `userinfo` antes de conectar: sem isso, o servidor trata o cliente como
legado. **O comportamento com VAC não foi testado.** Alterar tabelas do motor em memória é o tipo
de coisa que um anti-cheat pode punir, então o adaptador não deve ser usado em servidores seguros.

Cliente legado em `preferExtended` segue sem a prova de `checkLegacyAdmission`, aceitável enquanto
nada amplia limites. A reserva é sintética. Não amplia limites nem habilita transporte.
