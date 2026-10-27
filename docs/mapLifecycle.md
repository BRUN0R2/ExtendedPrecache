# Ciclo de mapa e conexão — contrato de integração 2.0

Estado: **especificação**, sem execução de reset, hooks ou unload no motor.
O ciclo de mapa complementa a máquina de negociação existente; seus estados
não são novos tipos de mensagem nem alteram os 26 frames de [negotiation.md](negotiation.md).
Ownership e índices seguem [storageOwnership.md](storageOwnership.md) e
[indexMapping.md](indexMapping.md).

## Identidades e estados

Cada `RuntimeService` tem nonce local novo e emite gerações u64 não zero sem
reuso. No servidor, o mapa compartilhado mantém a geração enquanto novos clientes
conectam/reconectam; cada conexão recebe tokens e nonce de negociação novos.
No cliente, uma nova conexão cria novo owner/geração local mesmo ao mesmo mapa
remoto. Restart cria outra instância/nonce; não persistir handles entre processos.
Spawn count e map epoch recebidos não substituem essas identidades locais.

Estados de `MapGenerationOwner`, com IDs fixos para a ABI 2:

| ID | Estado | Invariante e saída normal |
| --- | --- | --- |
| 0 | `empty` | Nenhum catálogo publicável; iniciar `building` com nova geração |
| 1 | `building` | Registro/recepção privados, capacidades reservadas, IDs estáveis; validar e congelar |
| 2 | `frozen` | Metadados completos, relações e identidades imutáveis; iniciar carga/binding |
| 3 | `loading` | Snapshot imutável; assets e views privados, sem consumidores de gameplay |
| 4 | `active` | Binding completo publicado na geração; nenhuma mutação de catálogo |
| 5 | `retiring` | Admissão fechada, sessões invalidadas, retirada em curso |
| 6 | `retired` | Views retiradas e pins/leases desse owner zerados; sem reutilização da identidade |
| 7 | `failed` | Falha terminal; iniciar retirada, sem voltar a building/frozen/active |

Transições normais: `empty→building→frozen→loading→active→retiring→retired`.
`building/frozen/loading` também podem ir diretamente a `retiring`; qualquer
fase viva pode falhar, e `failed→retiring` faz cleanup. `retiring` pode permanecer
nesse estado enquanto houver pins; não exige sleeps ou retries de aplicação.
Nova geração é outra instância, não `retired→building`.

Não publicar metadados mutáveis pela ABI. `getCurrentMap`/`acquireSnapshot` só
admitem `frozen/loading/active`; uso nativo depende de `active` e dos gates de
conexão. `retiring/failed` recusam novas aquisições e leituras públicas; leases
anteriores só podem ser liberados. Snapshot antigo retido não autoriza emitir
seus IDs em uma nova conexão/mapa.

## Servidor

1. Na troca de mapa, fechar admissão/emissão da geração antiga, invalidar acordos
   e retirar seus consumidores segundo a barreira de ownership. Cleanup de
   eventos e reset de `server_t`/`modelsMap` só podem ocorrer na ordem comprovada
   pelo perfil. Nenhum endereço de hook é aprovado aqui.
2. Criar nova geração, capturar configuração, reservar arenas e registrar mundo,
   inline BSP e entradas reais. Importar/preservar IDs nativos e aliases sem
   fabricar entradas. Validar mundo, densidade, nomes, tipos, contagens, ordinais,
   lookup, consistência, bytes, módulos e orçamento antes de `frozen`.
3. Congelar catálogo/requisitos/identidades. As estruturas nativas e assets do
   servidor podem atravessar `loading→active` antes de uma conexão completar;
   esse estado do host não libera o sign-on de um cliente ainda incompatível.
4. Em cada `new`, criar negociação ligada ao mesmo snapshot. A barreira de
   `SV_SendServerinfo` e o registro/controle precisam existir antes de qualquer
   emissão incompatível. Servidor já ativo continua aceitando novas conexões
   com gates independentes; não altera os limites/IDs do mapa para um cliente.
5. Provider prepara/reconfirma storage antes de S15. C9 válido, reserva atual e
   canal drenado permitem obter `releaseSignon`. O adaptador ainda verifica seu
   binding ativo e consome essa autorização **uma vez** para aquela conexão.
   Repetir a consulta do núcleo não pode duplicar `SV_New_f` ou o sign-on.
6. Catálogo transferido/verificado, assets e baselines seguem o futuro contrato
   de dados. Host `active`, Ready ou ACK isolados não provam que o cliente está
   pronto para receber gameplay. Desconectar um cliente solta seus leases e não
   aposenta o mapa dos demais.

## Cliente e reset inicial

1. Ao iniciar conexão, criar binding/generation e owner durável fora de
   `client_state_t` e de hunks/pools que serão apagados. O owner fica `building`:
   ainda não recebeu o catálogo completo. `awaitOffer` tem deadline mesmo em
   servidor silencioso; sem prova legada e antes de S0 não há fallback implícito.
2. A negociação prepara capacidade e decoder antes de C8. Essa reserva não é
   uma afirmação de que arquivos/metadados já chegaram. Armar após C9 aceito na
   fila mantém o acordo e a reserva, sem publicar tabelas ou gameplay.
3. Interceptar/prevalidar o primeiro serverinfo, sem consumir ou modificar o
   cursor nativo: protocolo 48, spawn count esperado e binding ainda atual.
   Chamar `confirmServerInfo` antes de permitir efeitos do parser nativo.
4. Executar o reset inicial **autorizado** na barreira comprovada do perfil,
   preservando serviço, acordo e reserva. `CL_ParseServerInfo` auditado inicia
   `CL_ClearState` antes da própria checagem de protocolo; guardar o acordo na
   região zerada ou validar somente depois do reset seria insuficiente.
   Não publicar ponteiros da geração nova antes desse reset.
5. Esse reset inicial, vinculado ao mesmo handoff/nonce e consumido uma vez,
   não incrementa a geração local por si só. Qualquer reset fora desse caminho,
   outro serverinfo, mudança de spawn/epoch, desconexão ou troca de peer fecha
   o acordo e inicia retirada; não reinterpretar como continuação permitida.
6. Receber o manifesto pelo [codec especificado](catalogCodec.md); validar IDs, contagens, duplicatas,
   nomes, bytes e digest contra o acordo antes de `frozen`. Só então a ABI pode
   devolver snapshot. Carregar assets, executar bindings pós-reset e publicar
   todas as views na geração correta antes de `active`.

O deadline atual do núcleo termina no handoff validado; não é um timeout de
download/carga. Prazos e confirmações de catálogo/assets/gameplay têm
[contrato de dados próprio](dataCodecs.md), ainda sem implementação.
Não inventar frames de controle para preencher essa etapa nem aprovar o perfil
exemplo indisponível.

## Retirada, reconexão e descarregamento

Em mudança de mapa ou falha terminal, marcar `failed/retiring`, bloquear novos
acessos e invalidar acordos antes do primeiro free/reset. Fechar cada negociação
e impedir que callbacks/frames enfileirados da geração velha alterem a nova.
O núcleo filtra nonce/binding; o adaptador também deve guardar geração em jobs
de download, callbacks de áudio/render, cache e publicação. Não inferir validade
pela coincidência de nome de mapa, slot, endereço ou spawn count.

Drenar leitores/consumidores nativos, retirar views, soltar pins e executar os
deallocators corretos. Leases públicos antigos recusam leituras, mas a liberação
continua permitida; seu metadata pode permanecer alocado até a última liberação.
`retired` exige referências desse owner zeradas. Caches persistentes de runtime
não são destruídos apenas porque um mapa terminou.

Quiescência desconhecida impede unload/realocação. O futuro plugin Metamod deve
recusar unload (`busy`) enquanto houver callbacks, contextos, leases ou bindings
retidos, inclusive por outros módulos. Remover entradas/hook registrations antes
de liberar seus owners; manter a DLL carregada enquanto existirem chamadas ou
function pointers em uso. Não fazer esse trabalho em `DllMain`.

Uma reconexão ao mesmo mapa tem nonce/tokens novos e não reaproveita um acordo
anterior; o mapa compartilhado do servidor pode permanecer. Não há renegociação
in-place na versão 1. Demo é um ciclo local diferente: não cria negociação de
rede nem emite C0/C8/C9. O [contrato de demo](demoCodec.md) define contexto de replay
local e reset único. Playback ampliado permanece indisponível sem implementação
e provas nativas de todos os produtores/consumidores.

## Critérios antes dos adaptadores

Provar nos hashes selecionados a barreira anterior ao serverinfo, o reset inicial
autorizado, invalidação de jobs/handles, quiescência de cada consumidor indireto,
drain/reset de netchan e a ordem dos deallocators. Demonstrar mapas sucessivos,
reconnect ao mesmo mapa, falha em cada fase e unload recusado com pins vivos.
Os cenários em [integrationVectors.json](contracts/integrationVectors.json) são
requisitos de integração; sua validação estrutural não é execução desses eventos
nos motores. O teste do núcleo continua cobrindo seus próprios gates existentes.
