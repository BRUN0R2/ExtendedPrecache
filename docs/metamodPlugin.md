# Plugin Metamod do servidor

Estado: **entradas e ciclo de vida implementados; plugin inerte**. O alvo
`extendedPrecacheServer` carrega no host, identifica-se como fundação e não instala
callbacks, hooks, comandos ou cvars. Não lê configuração, não negocia e não altera
o motor: não amplia o precache e não torna nenhuma versão suportada. O adaptador,
a barreira e o transporte continuam pendentes em [TODO.md](TODO.md).

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
| `GiveFnptrsToDll` (`stdcall`) | Avança para `engineBound` se tabela e globais vierem presentes. Não guarda ponteiros que nada usa |
| `Meta_Query` | Confere interface major igual e minor não menor; devolve as informações ou recusa com `*info` nulo |
| `Meta_Attach` | Exige as tabelas do host e a carga em `startup`; mantém a tabela de callbacks zerada |
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
binding, callback ou lease que sobreviva à chamada corrente deve prender um. Nada o
adquire nesta fase, então todo unload é aceito. `abandoned` não volta atrás, mesmo com
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

Verificadores, cópias do motor e resultados permanecem em área local ignorada. Só
os estados acima foram exercitados: sem clientes conectados, com a instalação local do
host e do motor selecionados. Isso não prova barreira, transporte, ampliação de índices
ou coexistência com outros plugins, e não dispensa os itens seguintes de `TODO.md`.
