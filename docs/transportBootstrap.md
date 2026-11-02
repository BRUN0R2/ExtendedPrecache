# Bootstrap e barreira de transporte

Estado: **investigação parcial** dos dois hashes de [engineAudit.md](engineAudit.md):
estática nos dois e observada em execução nos dois, sob instrumentação local. Não há
transporte, hook, perfil ou adaptador aprovado. Os endereços identificam observações nesses
artefatos e não são uma interface de integração nem autorizam instalar patches.

## Servidor: entrada e retomada

No `swds.dll` selecionado, a função em `0x049aa800` corresponde ao fluxo de
`SV_New_f`: depois dos checks de entrada, ela limpa buffers e o netchan, chama
o wrapper de serverinfo e então serializa os registros de user messages. Após
isso, prossegue com callbacks da GameDLL e preparação dos fragmentos do sign-on.
O wrapper em `0x049a8f10` conduz à implementação que escreve o SVC 11; a
implementação em `0x049a9000` escreve protocolo 48. A correspondência é estática,
apoiada também pela [fonte ReHLDS fixada](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/sv_main.cpp#L1509).

A barreira precisa impedir a **continuação inteira**, não apenas omitir a
chamada ao writer de serverinfo. O buffer de sign-on pertence à chamada nativa;
retornar de um hook de `SV_SendServerinfo` sem chamar o writer não suspende
`SV_New_f`, que continua para os registros e callbacks. A existência desse hook
também não comprova uma retomada assíncrona ou a validade de reter seu buffer.

Interceptar somente o comando `new` é insuficiente: o fluxo nativo de `spawn`
em `0x049aafd0` chama `SV_New_f` diretamente quando o spawn count diverge. A
[ativação do servidor em fonte](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/sv_main.cpp#L6214)
também limpa canais e possui outro caminho de serverinfo em modo de um jogador;
essa observação de fonte não comprova a execução desse ramo no perfil selecionado.
O futuro binding deve cobrir todas as entradas relevantes para o perfil ou
recusá-lo, invalidar a geração anterior e liberar a continuação no máximo uma vez.

O inventário integral de referências do hash selecionado fecha as entradas diretas:

| Alvo | Referências estáticas | Consequência para a barreira |
| --- | --- | --- |
| `SV_New_f`, `0x049aa800` | Dados em `0x049b8ef3` (registro do comando `new`); chamada em `0x049ab0f9`, dentro de `spawn` | Interceptar o corpo cobre os dois caminhos, em vez de só o comando |
| Wrapper de serverinfo, `0x049a8f10` | Chamada em `0x049aa922`, dentro de `SV_New_f`; chamada em `0x049b43b3`, em `0x049b4090` (`SV_ActivateServer`) | Fora de `SV_New_f`, só a ativação emite serverinfo |
| `sendres`, `0x049aada0` | Dados em `0x049b8f02` (registro do comando) | Precisa de bloqueio próprio: não passa por `SV_New_f` |

A [fonte fixada](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/sv_main.cpp#L6219)
só alcança a emissão na ativação com `maxclients <= 1`, valor possível também em servidor
dedicado com `-maxplayers 1`. **Decisão:** o perfil recusa `maxclients <= 1`, em vez de
barrar esse ramo, pois o protocolo de negociação pressupõe um cliente remoto. A
correspondência entre a fonte e esse ramo do binário ainda é estática. Chamadas indiretas
continuam fora desse inventário: a tabela não prova ausência de ponteiros calculados.

O serverinfo não é a única emissão da ativação. Em todos os modos, ela
[escreve e fragmenta o registro de user messages](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/sv_main.cpp#L6229)
para os clientes já conectados, sem passar por `SV_New_f`. Na troca de mapa, esse bloco
alcança um cliente parado na barreira. O registro privado do cliente precisa aceitá-lo sem
tratá-lo como colisão, e o predicado de drain precisa esperar seus fragmentos.

O núcleo de sessão concede `releaseSignon` no máximo uma vez por binding;
repetições invalidam a sessão. Essa garantia não suspende nem retoma uma chamada
nativa: o binding ainda precisa associar essa autorização à continuação correta, sem
duplicação ou reentrada.

A continuação executa `SV_New_f` fora do comando `new`, portanto não basta chamá-lo.
Com `src_command`, o corpo [retorna em silêncio](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/sv_main.cpp#L1528),
sem erro e sem sign-on, e fora do pacote do cliente `host_client` não aponta para ele. O
[binding](signonBarrier.md#binding-no-servidor) resolve os dois rodando dentro do pacote do
cliente, onde o motor [já selecionou a conexão](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/sv_user.cpp#L1864),
e ajustando apenas `cmd_source`, que volta ao valor anterior depois da chamada.

## Registro e despacho no cliente

A tabela do cliente associa SVC 39 ao wrapper `0x101aa090`; a inspeção do corpo
`0x101a6130` mostra leitura do ID, tamanho e nome, e uma lista nativa de
registros. O parser pode zerar uma associação com o mesmo ID e atualizar uma
associação com o mesmo nome. Por isso, uma associação preexistente não comprova
ownership nem torna uma colisão segura.

No dispatcher `0x101a7cb0`, mensagens de usuário são procuradas nessa lista;
para uma mensagem de tamanho variável, o tamanho precede o payload. O corpo é
consumido mesmo quando não existe callback registrado para aquele nome. O
bootstrap precisa, portanto, de registro e despacho privados anteriores à
ClientDLL, com validação explícita de ID, nome, tamanho e limite, conforme
[negotiation.md](negotiation.md). Essas observações não demonstram que uma
interceptação específica seja segura em execução.

`RegUserMsg` não é privado: toda entrada nova
[segue aos clientes conectados](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/sv_main.cpp#L5112)
no frame seguinte, e um cliente na barreira recebe essa cópia além da enviada pelo bootstrap.
**Requisito:** registrar antes de haver cliente conectado, e não ao tratar `new`. Com o registro
feito depois da primeira ativação, a cópia extra deixou de ser observada. O registro
privado do cliente precisa aceitar a repetição do mesmo ID, nome e tamanho sem abrir outra
sessão; sessão nova é uma oferta com outro nonce. A lista nativa sobrevive à troca de mapa e à
reconexão, logo uma sessão nova pode encontrar a associação já presente e deve validá-la, como
qualquer outra.

O parser de serverinfo `0x101a78c0` chama o caminho de limpeza `0x101a1ae0`
antes de verificar o protocolo. A rotina `0x101a18d0` libera estruturas e zera
a região auditada do estado de cliente. O acordo e a reserva preparada devem
viver fora das regiões e allocators atingidos pelo reset; a sobrevivência da
reserva concreta permanece uma prova pendente.

## Comandos e rate limit

No servidor, a tabela associa `clc_stringcmd` ao parser `0x049c8140`. O parser
contabiliza o comando antes de ler e classificar o texto; o caminho de comandos
não reconhecidos segue para a GameDLL. A [fonte fixada](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/sv_user.cpp#L1085)
lista os comandos nativos e não inclui `epc1`. Os hooks de execução de comandos
do motor, por si só, não demonstram interceptação de `epc1` antes da GameDLL.

`HandleNetCommand` roda antes do parser na API ReHLDS, e seu callback recebe o opcode após
a leitura deste. O [binding](signonBarrier.md#binding-no-servidor) o usa para consumir `epc1`:
só esse texto deixa de passar pelo parser, então o cursor e os demais comandos seguem iguais,
mas a contabilização nativa não o vê e o limite próprio a substitui. A disponibilidade e a
ordem foram observadas em execução no hash selecionado; isso não aprova esse caminho para
outros binários.

O [limitador nativo](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/rehlds/rehlds_security.cpp#L147)
conta os comandos, mas só age sobre clientes `active`. Durante a barreira o cliente ainda não
é ativo, então nada limita a taxa de `epc1`; a sessão também não cobra quota de repetições
idênticas. **Requisito:** o transporte precisa de um limite próprio de comandos `epc1` por
conexão, recusando com `budgetExceeded`, em vez de contar com o motor ou com a sessão. O
[núcleo da barreira](signonBarrier.md#limite-de-comandos) define esse limite.

## Drenagem e reset

`Netchan_Clear` em `0x04993dc0` descarta fragmentos e, quando há payload
confiável pendente, limpa seu comprimento e altera a sequência. Assim, aceitar
um frame na fila não prova que ele sobreviveu ao reset ou chegou ao peer.
Na [fonte fixada](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/net_chan.cpp#L269),
mensagens regulares, fragmentos ativos, filas de espera e o buffer confiável em
trânsito são estados distintos; o reconhecimento remoto libera esse último.
`Netchan_CreateFragments` ainda pode mover uma mensagem regular para a fila de
fragmentos. Uma fila regular vazia, isoladamente, não prova drain.

O adaptador deverá demonstrar que não resta controle sujeito ao reset, que a
confirmação pertence ao canal e à geração atuais e que nenhuma emissão concorrente
reintroduz dados antes da limpeza. Os indicadores nativos exatos, atomicidade,
retransmissão, ordem entre streams e equivalência do binário ainda exigem prova;
não há predicado de drain aprovado por este documento.

## Evidência e limites

| Observação estática | Evidência REA |
| --- | --- |
| Entrada, limpeza, writer, registros e continuação de `SV_New_f` | `ev_7b53c3348ef37f943f21dc5a4d048ac2a48589a83fd9e5baaf0eefb28b162d30` |
| `Netchan_Clear` e descarte de fragmentos | `ev_6a50c38cea91eb2fda04eadcd98a1dd289e364df3bcf4dd99db06712c3266f7e` |
| Wrapper e writer de serverinfo | `ev_d73e760ffb6779f38bbfa3b071007fa7c2348ac4e455f30383ee6c037b0f2dde`, `ev_8646446f36870bd17a7c7d0497cd655f418b94a6e92aaa360d01d572faf4bebe` |
| Chamada interna de `spawn` a `SV_New_f` | `ev_f3929bed84bc5d1810fab3d074d73ef7ed46fa2643dec5542ba164dd3c384e26` |
| Tabela e parser de `clc_stringcmd` | `ev_3fa73f5feabd7b66a8c8a6fb3add4def2fabe7f9385863dc5c0579cadb9d6ab1`, `ev_76ba932d4658c28e98903bf35da6d8c023e62ed33dbc3b3c7fc9e5130567f689` |
| SVC 39 e parser de registro do cliente | `ev_fddd4b42d1f24c4eed2c6079db07f20a8d5be360a013502bcc37b212e8892430`, `ev_f83cadcaeaa295d0742d79f69317794a66c429967a55cd6ecd9f02cffcb82b59` |
| Dispatcher do cliente | `ev_d7e58dd9d09a9cfd86ff7858edb1c8f531c3646048cadc546be224f585256cbb` |
| Serverinfo e reset do cliente | `ev_a2474d2a2b196d5e5c2ca34f265387d31410bf7769fa210a6336eb10a7077d4d`, `ev_cbfdb77a095f7a724799e6971f6c29f648f353139866ba77b4bb36c9383e0b2d`, `ev_ead9916169326dae096261afb29ef114af672180c63b1e8599dad26493c461c7` |

Os bundles completos ficam localmente em `.local/research`. Chamadas indiretas
e limites de análise declarados pelo provedor permanecem desconhecidos; os
resultados de fonte não são tratados como identidade automática com o binário.
A tabela de evidências acima não inclui observação de execução.

## Situação em execução

Em laboratório local, o [núcleo de sessão](sessionCore.md) real rodou sob instrumentação
dentro do servidor selecionado, primeiro com um cliente sintético que fala o protocolo 48 e
depois dentro do `hw.dll` selecionado, nos dois papéis do mesmo núcleo. Os frames são reais,
trafegados pelo netchan do motor. No lado do servidor, os mesmos pontos foram repetidos depois
com o [binding do plugin](signonBarrier.md#binding-no-servidor), sem instrumentação no caminho
da barreira e com um perfil sintético. O cliente segue instrumentado. Isso é observação em
execução, não um transporte habilitado. Os resultados permanecem locais.

| Ponto a provar | Servidor real | Cliente real (`hw.dll`) |
| --- | --- | --- |
| **Continuação:** corpo de `SV_New_f` executado uma única vez a partir do acordo, sem outro `new` do cliente | Observado | Não se aplica |
| **Bloqueio:** `sendres`, `spawn` e `new` repetido retidos até a liberação | Observado | Não se aplica |
| **Bootstrap:** os 26 frames pelos transportes de controle | Observado na emissão | Observado: registro privado anterior ao serverinfo, sem callback nativo, e registro normal com o mesmo ID |
| **Despacho:** `epc1` tratado antes da GameDLL | Observado, inclusive sob flood, com o limite próprio recusando o excesso | Observado na emissão |
| **Drain:** predicado sobre mensagem, fragmentos e filas | Observado com dado pendente no commit e com perda | Não se aplica |
| **Invalidação:** desconexão, conexão substituída e troca de mapa encerram a sessão | Observado | Observado: `retry` e troca de mapa abrem um acordo novo |
| **Reset:** o acordo sobrevive a `CL_ClearState` | Não se aplica | Observado; falta a reserva concreta |

Limites: um único cliente por vez, na mesma máquina; a instrumentação não é o caminho de
produção; a reserva é sintética; a identidade usada para detectar conexão substituída
(`connect_time`) e os endereços são observações destes hashes, não uma interface aprovada. O
endereço de `client_t` se repete entre conexões, logo não identifica a geração. Com o
`hw.dll`, a perda de pacotes não foi exercitada. O `retry` dele desconecta antes de conectar de
novo, então a conexão substituída sem queda só foi exercitada pelo cliente sintético.

Próximo passo: o adaptador de produção do cliente e a reserva concreta, que ainda são
laboratório. Registrar as evidências localmente, sem alterar os originais ou publicar fixtures,
traces e resultados.
