# Ownership e estabilidade — contrato de integração 2.0

Estado: **desenho normativo, sem storage nativo ou adaptador implementado**.
Aplicar somente a perfis exatos e aprovados. [engineAudit.md](engineAudit.md)
documenta por que endereços, nomes, cache CRC e HUD não podem depender de vetores
realocáveis ou do estado zerado no serverinfo.

## Proprietários e empréstimos

| Proprietário conceitual | Possui | Não possui |
| --- | --- | --- |
| `RuntimeService` | Instância, nonce local, coordenador de orçamento, registros de leases e providers | Módulos/ponteiros entregues por terceiros sem contrato |
| `RuntimeCaches` | Arenas contíguas de descritores K/A, sidecar CRC, identidade dos slots e HUD | Capacidade de rede confundida com tamanho do cache |
| `MapGenerationOwner` | Builder e, após freeze, snapshot imutável, nomes próprios, tabelas e índices do mapa | Ownership das outras conexões do servidor |
| `NativeBindings` | Publicação das views aprovadas e pins necessários aos nomes/objetos usados pelo motor | Permissão para mudar o layout de `server_t`, `client_state_t` ou de APIs antigas |
| `PreparedSession` | Staging/decoder da conexão, reserva orçada e lease do mapa/identidades do acordo | Destruição do mapa compartilhado ao fechar uma única conexão |
| Lease de API 2 | Pin do serviço/contexto e registro dos snapshots adquiridos por esse consumidor | Ownership da DLL sem o pin de módulo mantido pelo chamador |
| Lease pública da ABI 2 | Snapshot de metadados e nomes, enquanto registrada | Ponteiros, texturas, samples ou autorização para emitir IDs nativos |
| Empréstimo nativo interno | Uso restrito de um objeto da geração e pins dos owners necessários | Ownership transferível à GameDLL/ClientDLL por convenção implícita |

Esses são papéis de ownership; não obrigam uma hierarquia C++ ou alocações por
linha. A implementação deve usar ownership exclusivo e leases contados pelo
proprietário, sem cópias de objetos nativos, estado global mutável ou lifetime
deduzido de um IP/slot de cliente. Chamadas e transições são serializadas no
thread proprietário do motor, sem reentrada. Callback aninhado autorizado só
adquire leitura; não congela, publica, retira ou troca arenas.

Um lease público possui serial u64 não zero e nunca reutilizado no mesmo runtime.
Tipos de lease de API e snapshot são distintos; o serviço valida o tipo e qual
lease de API possui cada snapshot. `releaseApi` com snapshots retidos falha como
`busy`; copiar uma tabela não cria outro consumidor ou mais uma referência.
Copiar os bytes de um handle não adquire outra referência. Liberação consome o
registro; cópias remanescentes falham como `staleHandle`. Serial/geração esgotado
recusa nova emissão, sem wrap; só uma nova instância com nonce novo reinicia.
Um ponteiro ou um endereço estável não é, sozinho, prova de identidade viva.

## Política de alocação e publicação

Antes de publicar endereços, reservar capacidade completa e validar somas,
produtos, alinhamento, limites de inteiro/allocator e disponibilidade do espaço
virtual x86. Arenas expostas não crescem, compactam ou mudam de endereço durante
o uso. Strings são cópias próprias com NUL, endereços estáveis e grafia definida
por [indexMapping.md](indexMapping.md). Containers de lookup podem referenciar
essas strings, mas não possuir o único storage de um ponteiro nativo publicado.

Metadados internos podem usar armazenamento denso imutável. Para as views que
exigem contiguidade, usar arenas pré-dimensionadas por geração/runtime, não uma
coleção segmentada fingindo ser array. A publicação é um commit no thread do
motor: todos os objetos necessários válidos, owners presos, índices consistentes,
leitores suspensos na barreira e todas as views trocadas antes de reabrir acesso.
Atualizar apenas uma base e deixar contadores, CRC ou lookup antigos é proibido.

Falha antes da publicação desfaz apenas recursos ainda privados, usando seus
allocators. Depois da publicação, uma falha fecha acessos e inicia retirada;
não restaura ponteiros antigos por suposição nem faz downgrade de sessão.
Destrutores não podem lançar exceção nem chamar código de um módulo já descarregado.

## Restrições concretas dos motores

| Restrição auditada | Decisão de ownership |
| --- | --- |
| `model_t*` escapa para colisão, movimento, desenho e estúdio | Célula de descritor estável durante todo empréstimo; alteração da geração de payload só após quiescência dos consumidores |
| 392 bytes por descritor e CRC por diferença de ponteiros | K células contíguas, stride original e base única por perfil; CRC sidecar com K entradas de 12 bytes, mesma correspondência de slots |
| Helpers CRC podem receber ponteiros antigos | Resolver pertencimento/base/stride/geração antes de derivar slot; não subtrair ponteiros de alocações diferentes em C++ |
| `sfx_t*` de 72 bytes escapa para canais/cache de áudio | A células estáveis; interromper e drenar consumidores antes de reutilizar payload/slot; PCM é ownership separado |
| `modelsMap` tem nós finitos e chaves ponteiro | Índice próprio orçado com chaves estáveis e resultado de inserção conferido; todos os consumidores antigos precisam de tradução/substituição |
| Hash de som usa `short` e flag de construção | Novo lookup usa IDs completos e estado de construção por geração; nunca truncar IDs no hash antigo |
| Eventos têm nome e script com allocators distintos | Nome do snapshot é próprio; view nativa usa cópia/pool aprovado; script nativo mantém o deallocator compatível e é liberado uma vez |
| `SV_ClearCaches` para no primeiro evento sem nome | Eventos densos e sem buracos; não remover uma entrada no meio do mapa; cleanup precisa cobrir todo o storage ampliado |
| `localmodels[][5]` contém `*n` | Arena de nomes inline dimensionada por comprimento real, preservando referências até retirada |
| Cliente apaga 0x1b0e70 bytes e pode liberar hunk/pools | Serviço, negociação, reservas e metadados duráveis fora dessa região e dos pools liberados |
| HUD usa `slot + 1`, e encerramentos liberam texturas de formas diferentes | Owner de HUD separado de modelo de rede, protocolo único de teardown e pins dos consumidores do renderer |
| Getters/setters da API ReHLDS usam layouts/limites antigos | Preservar a ABI existente; não entregar views ampliadas ou contagens incompatíveis a consumidores antigos |

O desenho exige substituir/intermediar todos os helpers CRC e usuários do hash,
não validar somente a entrada de precache. Não há aprovação de uma assinatura
interna por correspondência com fonte C++; registradores/calling convention do
binário precisam de prova antes de qualquer bind. Os strides acima descrevem os
dois hashes auditados; não são tipos públicos novos nem autorização para copiar
structs proprietárias ou inicializá-las com um `memset` especulativo.

No caminho ReHLDS observado, nomes de sons/eventos podem vir de `ED_NewString`,
e `SV_ClearCaches` libera `pszScript` com `Mem_Free`, sem liberar `filename`
individualmente. Não aplicar o deallocator do script ao nome, nem expor um nome
emprestado de um plugin que pode ser descarregado. A matriz de allocator/owner
deve ser preenchida pelo perfil antes de publicar qualquer view.

## Reutilização e mudança de capacidade

Descritores expostos não mudam para outro asset enquanto houver consumidores
capazes de guardar o ponteiro. Reuso de célula, se comprovado no perfil, exige
fim de todos os empréstimos, limpeza dos consumidores e incremento da geração
do payload. Cache persistente pode conservar a mesma identidade entre mapas;
isso não torna o conteúdo/hunk/CRC de uma geração anterior automaticamente válido.

No HUD, o inteiro nativo não tem bits de geração. Um sidecar protege os wrappers
novos, mas não detecta uma ClientDLL usando um inteiro antigo igual a um novo.
Por padrão, preservar a associação handle→sprite durante a vida dos consumidores;
reuso para outro sprite só após teardown e reinicialização comprovados de **todos**
eles. Sem essa prova, esgotamento é `storageUnavailable`, nunca reciclagem silenciosa.
Encerra-se primeiro o uso/seleção no renderer, depois o payload/texturas, depois
o slot. O adaptador não pode combinar os dois encerramentos auditados como se
ambos possuíssem as mesmas texturas.

Alterar K, A ou H exige nova arena e quiescência global de seus consumidores,
incluindo módulos externos. Uma geração nova de mapa/conexão é necessária, mas
não suficiente para mover um cache de runtime. Sem uma barreira comprovada,
mudança dessas capacidades exige reiniciar o processo. Não reduzir uma capacidade
em uso, realocar sob ponteiros emprestados ou prometer hot reload de configuração.

## Quiescência e retirada

Fechar admissão de chamadas novas antes de retirar views. Invalidar referências
para uso no mapa atual, parar emissões/callbacks/áudio/renderização dependentes,
drenar controle e filas de trabalho, e só depois obter contadores zero de leitores
nativos. Callbacks que podem reter ponteiros além do retorno precisam de contrato
de lifetime explícito do módulo; contar apenas frames na pilha não prova quiescência.
Uma assinatura/consumer desconhecido mantém o perfil indisponível.

Após essa barreira, retirar as views e liberar recursos nativos na ordem do
deallocator proprietário. Metadados/nomes imutáveis podem sobreviver por leases
públicos; esses leases não mantêm texturas/PCM vivos. Se uma view nativa aponta
para a arena de nomes, ela mantém seu próprio pin até ser retirada. Liberar o
snapshot/arena somente após todos os seus pins; liberar caches de runtime por
último, na sua barreira global. Não usar deadline para forçar free de memória
ainda emprestada. Se o owner não consegue provar retirada, recusar a transição
ou o unload; preservar a memória e reportar `busy` em vez de criar dangling pointers.

`PreparedSession` do núcleo atual continua dono exclusivo do seu lease. Seu
destrutor solta staging e referências; o futuro provider coordena com os owners
acima e não destrói storage compartilhado enquanto outras conexões o usam.
`stillValid(agreement)` reconfirma nonce/binding, geração, catálogo congelado,
contratos, limites e reserva orçada; não retorna true por um ponteiro ser não nulo.

## Orçamento de pico e compartilhamento

Um coordenador local deve contabilizar uma vez os caches/snapshot compartilhados,
mais staging e leases privados de todas as conexões, metadados retidos e a
sobreposição entre gerações. Reservar o pico antes de admitir uma nova sessão;
não aceitar cada cliente isoladamente com o mesmo saldo livre.

O `peakMetadataBytes` de um provider real deve cotar o pico total local após essa
admissão e nunca ficar abaixo de `minimumSessionMetadataBytes`. A reserva no
coordenador dura até ser solta/reconciliada pelo lease, sem desconto prematuro na
troca de mapa. `metadataBudgetBytes` limita esses metadados/preparação; assets,
PCM, arquivos e VRAM continuam com cotação/viabilidade separadas no perfil.
Sem memória para sobrepor mapas, drenar/retirar o anterior antes de reservar o
novo; se pins remanescentes impedirem isso, falhar explicitamente. Não exceder
o teto, expulsar consumidores ou trocar de perfil para recuperar a alocação.
