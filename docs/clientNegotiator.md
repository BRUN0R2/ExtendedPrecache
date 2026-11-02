# Negociação no cliente

Estado: **núcleo, adaptador e inicializador implementados, inativos sem perfil aprovado**.
`ClientNegotiator`, no alvo `extendedPrecacheTransport`, conduz o lado do cliente da
[negociação](negotiation.md), ao longo dos mapas de uma conexão, e é independente do motor. O alvo
`extendedPrecacheClient` o liga ao `hw.dll`, e `extendedPrecacheLauncher` carrega essa DLL no jogo.
Sem perfil aprovado, e não há nenhum, o inicializador se recusa a abrir o jogo. A contraparte no
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

## Adaptador no `hw.dll`

A DLL não exporta nada. `DllMain` só agenda uma chamada na própria thread, a principal do jogo, que
roda antes do ponto de entrada e sem o loader lock. Essa chamada confere o SHA-256 do `hw.dll`,
registra a notificação do loader e cria a thread que instala o adaptador quando o motor é mapeado.
Como tudo isso termina antes de o jogo começar, a notificação nunca perde o carregamento do motor.

Para conduzir a negociação, o adaptador não altera código do motor. Ele troca entradas de tabelas de
dados do `hw.dll`, depois de conferir que todas contêm o que este build tem, e usa as funções que o
motor entrega à ClientDLL. O código só muda para ampliar os modelos: durante um mapa negociado acima
do limite nativo e, se a configuração pedir mais descritores, durante todo o processo, como descrito
em [modelTables.md](modelTables.md).

| Ponto | Origem | Papel |
| --- | --- | --- |
| `svc_newusermsg` | Tabela de serviços | Na primeira vez, associa o handler; confere o registro e segue o parser nativo |
| `svc_serverinfo` | Tabela de serviços | Chama `admitServerInfo` antes do parser nativo |
| `pfnRegisterVariable` | API do motor | Na primeira chamada da ClientDLL, registra o opt-in e começa o `pump` |
| `HUD_Frame` | Tabela de funções do cliente | Detecta perda de conexão e chama `pump` |
| `pfnHookUserMsg` | API do motor | Entrega os frames de `ExtPrecache` ao adaptador |
| `pfnServerCmd`, `pfnClientCmd`, `pfnConsolePrint` | API do motor | Envia `epc1`, desconecta e informa o motivo |
| `pNetAPI->Status` | API do motor | Informa se há conexão e desde quando |

O opt-in é a variável `epc`, com `FCVAR_USERINFO`, e não `setinfo`. O motor grava em `config.cfg`
as chaves de `setinfo`, então o opt-in sobreviveria à sessão e o jogo aberto pela Steam, sem o
adaptador, pediria uma negociação que não conseguiria conduzir. Uma variável não arquivada não é
gravada, e a chave que tem o nome de uma variável também não. Assim, o opt-in só existe com o
adaptador instalado, e uma falha ao instalar deixa um cliente legado comum.

O registro segue pelo parser nativo, e não por uma tabela privada. A associação é por nome e
[sobrevive](transportBootstrap.md#registro-e-despacho-no-cliente) a `CL_ClearState`, à troca de
mapa e à reconexão. O despacho nativo entrega o frame ao handler também antes do serverinfo, sem
passar pela ClientDLL, que não conhece esse nome. Assim, não é preciso interceptar o dispatcher.

Uma recusa imprime o motivo no console e pede `disconnect`. Se ela acontece num serviço, o cursor
vai ao fim da mensagem: o parser nativo não roda e o motor não lê o restante como comandos. Uma
conexão nova é percebida quando o motor informa que não há conexão, ou quando o tempo de conexão
regride, e nenhuma resposta é enviada à conexão antiga.

## Servidores com VAC

O adaptador recusa servidores seguros. O servidor declara isso na
[resposta de challenge](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/sv_main.cpp#L2636),
e o `hw.dll` guarda o valor em `cls.isVAC2Secure`. O adaptador lê esse campo a cada frame e antes
dos parsers, porque o parser de serverinfo o sobrescreve. Se estiver ligado, imprime o motivo e
pede `disconnect`, sem negociar nem chegar ao serverinfo.

**Isso reduz o risco, mas não o elimina.** O `connect` sai na mesma chamada em que a resposta de
challenge é tratada, antes do próximo frame, e nenhuma tabela permite agir antes disso. O
adaptador só recusa depois, então a conexão a um servidor seguro chega a começar. Por isso, não
use o inicializador para abrir servidores com VAC, como diz o
[aviso de risco](../README.md#extendedprecache), que ele também imprime a cada execução.

`-insecure` não ajuda aqui: no `hw.dll`, seus dois usos ficam na ativação do servidor Steam, como
em [`CSteam3Server::Activate`](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/sv_steam3.cpp#L260).
Ele afeta um servidor hospedado pelo próprio jogo, não o cliente que entra em outro.

## Builds conhecidos

O que o adaptador sabe de cada build do jogo fica em uma entrada de dados, em
[`clientBuild.cpp`](../src/platform/windows/clientBuild.cpp): o executável, os hashes, os endereços
do `hw.dll` e as referências às tabelas de modelos. O build é escolhido pelo SHA-256 exato do
`hw.dll`. Hash desconhecido é recusado, e nenhum endereço é tentado em um build parecido. Hoje há uma
entrada: o jogo da Steam depois da atualização de 25 anos.

## Inicializador

```text
extendedPrecacheLauncher --game-dir <pasta> [-- <argumentos do jogo>]
```

`--game-dir` é obrigatório e deve apontar para uma cópia separada do jogo, fora da pasta da Steam:
o inicializador nunca escolhe a pasta da Steam por conta própria. Ele identifica o build pelo SHA-256
do `hw.dll`, confere o do executável desse build e abre o jogo suspenso, com a pasta dele como
diretório atual e `-game cstrike`. A carga
de `extendedPrecacheClient.dll`, que precisa estar ao lado dele, é a primeira coisa que a thread
principal faz, antes do ponto de entrada do jogo. Nenhum arquivo da instalação é alterado, e fechar
o jogo desfaz tudo.

O resultado volta por um mapeamento e um evento nomeados pelo processo do jogo, porque o console
do motor ainda não existe nesse momento. Uma falha ao instalar é informada e o jogo continua aberto,
como cliente legado. O código de saída diz o que aconteceu.

## Limites

O conjunto foi exercitado localmente com o `hw.dll` real contra o plugin real e com perfis
sintéticos, inclusive sem nenhuma instrumentação no cliente; as observações estão em
[transportBootstrap.md](transportBootstrap.md#situação-em-execução). **Nunca foi testado contra um
servidor com VAC real:** a guarda foi exercitada ligando o campo em memória, sem VAC envolvido, e a
origem do valor é observação estática. Alterar tabelas ou código do motor em memória é o tipo de
coisa que um anti-cheat pode punir.

O build em Debug depende do CRT de depuração, então ainda não há pacote de distribuição. A reserva
real existe só para modelos, e os outros recursos continuam nos limites nativos.
