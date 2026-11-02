# Tabelas de modelos acima do limite nativo

Estado: **implementado para modelos, inativo sem perfil aprovado**. É o primeiro recurso que passa
do limite nativo. Sons, genéricos, eventos e decals continuam nos limites originais.

Este módulo **altera o código do motor em memória**, ao contrário do restante do adaptador. Nenhum
arquivo da instalação muda, e a alteração só existe com um perfil aprovado e nos hashes de
[engineAudit.md](engineAudit.md).

## Por que dá para mover

O protocolo já comporta esses índices: o `delta.lst` do Counter-Strike usa 10 bits em `modelindex`,
`viewmodel` e `weaponmodel`, a lista de recursos usa 12 bits e os efeitos usam 16. O que limita é o
armazenamento: as tabelas têm 512 células em posições fixas, e o código compara o índice com esse
tamanho.

A tabela de relocação do PE lista cada endereço absoluto do binário, então ela dá o censo completo
das referências, sem depender do alcance de um desmontador. Os modelos têm poucas:

| Motor | Tabelas | Referências | Limites | Outros |
| --- | --- | --- | --- | --- |
| `hw.dll` | `cl.model_precache` | 16 | 5 | |
| `swds.dll` | Nomes, objetos e flags | 47 | 22 | 3 chamadas de limpeza |

`ImageRewrite` troca esses operandos de 32 bits: confere todos antes da primeira escrita e aplica
tudo ou nada. Um valor diferente do esperado recusa a instalação. Os limites são calculados da
capacidade, nas três formas em que o código os escreve: número de células, último índice e tamanho
em bytes da tabela de ponteiros.

## Capacidade

O contrato conta entradas sem a célula zero, que é reservada: 512 células nativas são 511 entradas.

| Limite | Valor |
| --- | --- |
| Verificado de ponta a ponta | 899 entradas, ou 900 células |
| Descritores de modelos conhecidos | 1.024, nos dois motores; este módulo não os move |
| Descritores usados por outras coisas no cliente verificado | 49, inclusive sprites do HUD |

O teto real é o dos descritores. No cliente verificado, 949 entradas funcionam e 999 terminam em
`mod_numknown >= MAX_KNOWN_MODELS`, erro fatal do motor. A margem de 899 cobre mais sprites do que o
jogo verificado carrega, mas não é uma garantia: a ClientDLL pode carregar até 256 sprites no HUD.

A configuração pode pedir menos que o verificado, nunca mais. O servidor recusa com o erro nativo
uma precache além da capacidade, sem escrever fora da tabela. O cliente recusa com
`capacityInsufficient` um mapa que exige mais.

## Servidor

As tabelas são movidas uma vez, ao carregar o plugin, porque um mapa faz precache antes de qualquer
negociação, e ficam assim até o processo terminar. Na saída, o motor pode já ter sido descarregado,
então nada tenta restaurar o código. Uma configuração dentro do limite nativo não altera nada.

O motor zera as tabelas nativas junto com todo o estado do servidor, e as movidas não fazem parte
dele. Por isso, as três chamadas que zeram esse estado passam por uma função que também limpa as
tabelas movidas, no mesmo instante. A API do ReHLDS não tem um ponto entre essa limpeza e as
primeiras precaches do mapa.

A contagem do mapa vem da própria tabela. Ela entra nos requisitos da negociação e decide a
[admissão legada](signonBarrier.md#decisões-por-fase).

## Cliente

A alteração existe só enquanto é necessária, para reduzir o risco com anti-cheat:

| Momento | O que acontece |
| --- | --- |
| Negociação | `SessionPreparer` reserva a tabela e prepara as alterações, antes do ponto sem volta |
| Serverinfo admitido | O código é alterado na thread do motor, antes do parser que começa o mapa |
| Mapa dentro do limite nativo | Nada é alterado |
| Outro mapa na mesma conexão | O código volta ao original e é alterado de novo para o novo mapa |
| Conexão perdida, ou servidor com VAC | O código volta ao original |
| Servidor sem negociação | O código segue original |
| Jogo fechado durante a sessão | Nada é restaurado, porque o processo termina e o motor pode já ter sido descarregado |

A [guarda de VAC](clientNegotiator.md#servidores-com-vac) roda antes da alteração, no mesmo serviço.
Isso reduz o risco, mas não o elimina: a alteração fica ativa durante toda a sessão negociada. Nunca
use o inicializador em servidores com VAC.

A tabela do cliente tem 4.096 células, mesmo em mapas menores. O parser nativo da lista de recursos
grava o índice de 12 bits sem conferir, então nenhum índice do fio consegue sair dessa tabela. Os
limites no código continuam ajustados para o mapa negociado.

## Clientes legados

Um cliente sem o adaptador corromperia a própria memória com um índice acima de 511. Por isso, a
barreira consulta `checkLegacyAdmission` em cada comando de sign-on e o recusa com
`legacyIncompatible`, antes de qualquer recurso. Em um mapa dentro dos limites, esse cliente entra
normalmente, a menos que a política seja `requireExtended`.

## Limites

Verificado localmente com `hw.dll` e `swds.dll` reais, plugin e adaptador de produção e um perfil
sintético, em Counter-Strike 1.6 com `de_dust2`:

- 899 entradas, com nomes idênticos nos dois lados, troca de mapa e volta do código ao original
  depois de desconectar.
- Entidades nos índices 512 e 899 chegam a `HUD_AddEntity` e ao renderizador de modelos.
- Mapa pequeno com adaptador, cliente legado em mapa pequeno e recusa de cliente legado em mapa
  grande.

O que ainda não foi verificado ou não existe:

- **Nunca foi testado contra um servidor com VAC real.** Alterar código do motor em memória é o tipo
  de coisa que um anti-cheat pode punir.
- Demos, sessões longas, outros mapas e outros mods não foram exercitados.
- Módulos do servidor que guardem índices em tabelas de 512 posições não foram auditados.
- Um `delta.lst` com menos de 10 bits em `modelindex` truncaria os índices.
- O hash do catálogo ainda não é calculado do mapa real, e os requisitos dos outros recursos ainda
  vêm do perfil.
- Não há perfil aprovado, então os artefatos publicados não ativam nada disso.
