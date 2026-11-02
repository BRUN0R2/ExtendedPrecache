# Plugin Metamod do servidor

Estado: **entradas e ciclo de vida implementados; build publicado inerte**. O alvo
`extendedPrecacheServer` carrega no host e identifica-se como fundação. Contém o
[binding da barreira](signonBarrier.md#binding-no-servidor), que só é instalado com um perfil
aprovado, e não há nenhum. Sem perfil, não instala callbacks, hooks, comandos ou cvars, não lê
configuração, não negocia e não altera o motor. Não amplia o precache e não torna nenhuma versão
suportada. O adaptador e o transporte completos continuam pendentes em [TODO.md](TODO.md).

O contrato alvo é a interface `5:13`, como em Metamod-r 1.3.0.149. A referência é a
[fonte fixada](https://github.com/rehlds/Metamod-R/tree/56590c5c70fa95775051888b7fe0a4e285856ebb),
da qual nenhum código foi copiado: as declarações em `pluginAbi.hpp` são próprias e
descrevem somente o layout binário usado. Metamod-P e outros hosts não foram
verificados e a [Metamod-R requer ReHLDS](scope.md).

## Entradas

Exportadas sem decoração por `extendedPrecacheServer.def`. O host não passa contexto,
então o contrato impõe um único proprietário por módulo em `metamodEntry.cpp`.
`DllMain` não existe. As chamadas são serializadas pelo host no thread do motor.

| Entrada | Efeito |
| --- | --- |
| `GiveFnptrsToDll` (`stdcall`) | Avança para `engineBound` se tabela e globais vierem presentes. Guarda só a tabela do motor, para registrar a mensagem de controle |
| `Meta_Query` | Confere interface major igual e minor não menor; devolve as informações ou recusa com `*info` nulo |
| `Meta_Attach` | Exige as tabelas do host e a carga em `startup`; mantém a tabela de callbacks zerada e instala o binding se houver perfil aprovado |
| `Meta_Detach` | Decide conforme a tabela da próxima seção; nunca libera recursos que outro módulo possa estar usando |

`Meta_Init` é opcional e permanece ausente, pois a DLL não funciona como GameDLL.
Resultados e fases são inteiros do tamanho de `int` na fronteira e convertidos para
enums validados; um valor desconhecido nunca vira um enumerador do host por cast.

## Fases e descarregamento

`unbound → engineBound → queried → attached`, com `detach` de volta a `unbound` e
`abandoned` terminal. Um novo vínculo de engine vindo de `queried` é permitido, porque
o host pode fechar e reabrir um plugin que falhou sem chamar `Meta_Detach`.

| Situação no `Meta_Detach` | Motivo | Resultado |
| --- | --- | --- |
| Fora de `attached`, ou sem usos retidos | Qualquer | `released`: aceita e volta a `unbound` |
| Usos retidos | Comando, INI, arquivo novo, adiado, plugin | `busy`: recusa e continua `attached` |
| Usos retidos | Forçado por comando/plugin, reload ou desconhecido | `pinRequired`: fixa o módulo, recusa e passa a `abandoned` |

`RetainedUse` é o lease RAII desse contrato ([ciclo de mapa](mapLifecycle.md)): cada
binding, callback ou lease que sobreviva à chamada corrente deve prender um. Sem perfil
aprovado, nada o adquire, então todo unload é aceito. Com o binding instalado, ele fica preso
até o fim do processo, porque os hooks não são removidos. `abandoned` não volta atrás, mesmo com
os usos encerrados: a imagem fixada conserva o estado até o fim do processo, e novos
`Meta_Query` são recusados até o servidor reiniciar. Se a fixação falhar, o processo
aborta, pois continuar deixaria ponteiros para um módulo liberado.

## Decisões derivadas do host

- **Recusa de carga:** o host [segue adiante](https://github.com/rehlds/Metamod-R/blob/56590c5c70fa95775051888b7fe0a4e285856ebb/metamod/src/mplugin.cpp#L812)
  quando `Meta_Query` retorna falso; somente informações nulas
  [encerram a carga](https://github.com/rehlds/Metamod-R/blob/56590c5c70fa95775051888b7fe0a4e285856ebb/metamod/src/mplugin.cpp#L828).
  Por isso toda recusa zera `*info`, em vez de depender do retorno.
- **Recusa de descarga:** em descarga forçada ou reload, o host
  [ignora um detach recusado](https://github.com/rehlds/Metamod-R/blob/56590c5c70fa95775051888b7fe0a4e285856ebb/metamod/src/mplugin.cpp#L507)
  e libera a DLL. `busy` sozinho não protegeria os usos vivos, daí a fixação.
- **Detach sem attach:** o host pode
  [chamar `Meta_Detach`](https://github.com/rehlds/Metamod-R/blob/56590c5c70fa95775051888b7fe0a4e285856ebb/metamod/src/mplugin.cpp#L1008)
  em um plugin que nunca concluiu attach; é aceito, sem exigir uma ordem que o host não garante.
- **Encerramento:** em servidor dedicado, o host
  [não descarrega plugins](https://github.com/rehlds/Metamod-R/blob/56590c5c70fa95775051888b7fe0a4e285856ebb/metamod/src/dllapi.cpp#L19)
  ao terminar. Nenhuma limpeza obrigatória pode depender de `Meta_Detach`.
- **Carga:** `loadable=startup`, porque o catálogo de um mapa precisa ser observado desde
  o primeiro precache. Um `meta load` posterior
  [abre o arquivo sem attach](https://github.com/rehlds/Metamod-R/blob/56590c5c70fa95775051888b7fe0a4e285856ebb/metamod/src/mplugin.cpp#L417).
- **Pausa:** `unloadable=anyTime`, abaixo de `anyPause`, faz o host
  [recusar a pausa](https://github.com/rehlds/Metamod-R/blob/56590c5c70fa95775051888b7fe0a4e285856ebb/metamod/src/mplugin.cpp#L606).
  Pausar interromperia callbacks futuros com bindings ainda vivos.
- **Mensagens:** o [log em arquivo](https://github.com/rehlds/Metamod-R/blob/56590c5c70fa95775051888b7fe0a4e285856ebb/metamod/src/mutil.cpp#L92)
  fica mudo sem `log on`. Estado e recusas vão ao console; recusas também ao log.
- **`GetEntityAPI`:** nada aqui pode chamá-la no `mp.dll`; quem pede essas entradas é o host, ao
  carregar. Com um mapa já carregado, a função zera uma tabela global do jogo, que deixa de achar
  câmeras de introdução e pontos de nascimento até o próximo mapa, e os jogadores entram em
  (0, 0, 1), dentro do chão em `de_dust2`. Isso foi observado no `mp.dll` usado nos testes locais;
  que a tabela seja o índice de entidades por nome é inferência.

## Instalação de diagnóstico

Apenas em ambiente de teste: copiar a DLL para `cstrike/addons/extendedPrecache/` e
acrescentar a `addons/metamod/plugins.ini`:

```text
win32 addons/extendedPrecache/extendedPrecacheServer.dll
```

`meta list` deve mostrar `ExtendedPrecache foundation` em `RUN`, e o console, a linha
`[EPC] foundation build attached`. O preset atual gera build Debug, dependente do CRT
de depuração; ainda não há pacote de distribuição.

## Limites

Verificadores, cópias do motor e resultados permanecem em área local ignorada. O build
publicado foi exercitado nos estados acima, sem clientes conectados, com a instalação local do
host e do motor selecionados. O binding foi exercitado localmente, em outro build, com perfil
sintético. Isso não prova transporte habilitado, ampliação de índices ou coexistência com outros
plugins, e não dispensa os itens seguintes de `TODO.md`.
