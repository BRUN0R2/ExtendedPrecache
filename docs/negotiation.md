# Negociação cliente/servidor — versão 1.0

Estado: especificação e corpus offline, **sem transporte ou adaptadores
implementados**. A negociação seleciona um contrato de dados disponível e limites
compatíveis com o mapa completo. Sua conclusão não carrega assets nem libera
gameplay antes da validação do catálogo e do carregamento previstos nesse contrato.

## Bootstrap e barreira

1. Cliente em `preferExtended` / `requireExtended` inclui exatamente `epc=1`
   no `userinfo` antes de conectar. O acréscimo `\epc\1` custa seis bytes.
   Não remover/truncar dados para caber no buffer de 256 bytes; se não couber,
   recusar localmente. A chave é indicação, não prova de capacidade. O servidor
   captura seu valor na aceitação; mudanças posteriores não renegociam a sessão.
   Com negociação habilitada, chave repetida é malformada e valor presente
   diferente de `1` é versão sem suporte; ausência é o caso legado.
2. No servidor, bloquear `new`, `sendres`, spawn e qualquer emissão de schema ou
   índice dependente de capacidade **antes de `SV_SendServerinfo`**. A fonte
   auditada emite deltas dentro dessa função, antes de `SV_SendUserReg` e de
   `ClientConnect`; um callback tardio do Metamod não basta.
3. Para opt-in aceito, registrar uma única mensagem variável `ExtPrecache`
   (`RegUserMsg(..., -1)`). Enviar seu registro SVC 39 antes do primeiro frame,
   usando o ID alocado, nunca um ID fixo. Colisão de nome/size/ownership ou falta
   de slot recusa o transporte; `RegUserMsg` pode devolver um registro existente,
   portanto resultado não zero sozinho não prova ownership.
4. Cliente intercepta o registro e os frames antes da entrega à ClientDLL,
   inclusive antes de serverinfo. Esse estado fica fora da região zerada por
   `CL_ClearState`. Não depender de callback de HUD ou de ClientDLL já inicializada.
   O registro de bootstrap é consumido em uma tabela privada, sem chamar o
   handler nativo prematuramente. O registro normal posterior ao serverinfo
   precisa manter o mesmo ID/nome/size; mudança nessa associação é erro.
5. Servidor só libera o fluxo nativo depois de `clientCommitAck` válido, com
   mesmo snapshot, canal ainda válido e filas de controle confirmadas/drenadas.
   `SV_New_f` limpa buffers/fragments: o adaptador deve impedir que esse reset
   apague controle pendente, sem duplicar o sign-on ou misturar mensagens antigas.

Fontes da decisão: [ordem do sign-on](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/sv_main.cpp#L1096),
[registro e conexão](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/sv_main.cpp#L1495),
[limite de user messages](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/common/quakedef.h#L36),
[limpeza do netchan](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/net_chan.cpp#L156).
Essas são evidências em fonte. Viabilidade e ordenação nos binários ainda devem
ser demonstradas na integração; não há hook nativo aprovado por esta definição.

## Transportes de controle

Servidor → cliente: payload binário de `ExtPrecache`, em netchan **confiável e
ordenado**. Registro com nome ASCII `ExtPrecache`, NUL/padding até 16 bytes,
size -1; ID no espaço existente de user messages 64…255. Cada payload terá no
máximo 60 bytes, abaixo do limite auditado de 192. Não usar SVC 60…63, mensagens
de usuário de outro mod ou um ID numérico escolhido antecipadamente.

Cliente → servidor: `clc_stringcmd` confiável contendo exatamente `epc1 ` seguido
do frame em hexadecimal ASCII **minúsculo**, sem aspas, separadores ou newline,
e terminado em NUL pelo framing GoldSrc. No máximo 125 caracteres: `5 + 2*60`.
O frame máximo ocupa 127 bytes contando opcode e NUL; fica abaixo do truncamento
de strings maiores que 127 caracteres no caminho auditado.

O parser dedicado reconhece o token reservado antes de `SV_ParseStringCommand`
despachar para GameDLL, console ou command buffer. Preservar a contabilização
de rate limit; não executar `Cmd_ExecuteString`, `Cbuf_InsertText`, avaliação de
texto ou comandos arbitrários. Texto malformado com esse token é erro, não deve
ser encaminhado ao mod. O contrato de aplicação precisa reservar esse nome.
Fonte: [despacho de string commands](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/sv_user.cpp#L1492).

O controle não usa `svc_stufftext`, arquivos baixados, HTTP ou pacote UDP paralelo.
Netchan fornece retransmissão/ordenação; não adicionar retransmissões de aplicação
ou sleeps. A fila deve respeitar rate/bytes da conexão e o deadline da negociação.
Se isso impedir conclusão, encerrar explicitamente.

## Frame binário canônico

Inteiros little endian, sem padding de C++, varints ou dump de structs. Hashes
e nonces são bytes na ordem declarada, não inteiros sujeitos a endian swap.

| Offset | Bytes | Campo |
| --- | --- | --- |
| 0 | 4 | Magic ASCII `EPCN`, hex `45 50 43 4e` |
| 4 | 1 | Major = 1 |
| 5 | 1 | Tipo, tabela abaixo |
| 6 | 2 | `bodyBytes`, até 32 |
| 8 | 16 | `serverNonce`, não zero, único para esta conexão/acordo |
| 24 | 4 | Sequência desta direção, desde zero |
| 28 | `bodyBytes` | Body; tamanho total deve ser exatamente `28 + bodyBytes` |

Cada direção permite até 32 frames únicos, incluindo eventual rejeição; não há
wrap. A conversa normal tem 16 do servidor e 10 do cliente: 952 + 592 = **1.544
bytes lógicos**, incluindo headers. `negotiation.maxBytes` limita a soma única
de ambas as direções, até 4.096 bytes. Contabilizar antes de armazenar/hash; os
buffers da conversa são limitados. Bytes/headers/retransmissões do netchan têm
orçamento de transporte separado.

Gerar nonces de 128 bits com CSPRNG da plataforma, novos também após restart e
reconexão. Não usar contador, relógio ou RNG de gameplay como nonce. Nonce liga
a conversa à instância de conexão mantida localmente; não autentica um cliente,
mod ou servidor. SHA-256 verifica identidade/integridade do acordo, sem substituir
autenticação do transporte ou verificação dos módulos locais.

## Tipos e payloads

| ID | Tipo | Body |
| --- | --- | --- |
| 1 | `serverOffer` | 24 bytes: minor u16=0, política u8 (1 prefer, 2 require), reservado u8=0, spawn count u32, map epoch u64 não zero, timeout ms u32, data contract ID u32 não zero |
| 2 | `applicationContract` | SHA-256 de 32 bytes do contrato de aplicação aprovado localmente |
| 3 | `dataContract` | SHA-256 de 32 bytes do contrato completo de codecs, junto ao ID da oferta |
| 4 | `requirementsPage` | Quatro registros de limite, 32 bytes |
| 5 | `serverLimitsPage` | Quatro registros de limite, 32 bytes |
| 6 | `offerEnd` | `offerHash`, 32 bytes |
| 7 | `clientHello` | 24 bytes: minor u16=0, política u8 (1/2), reservado u8=0, client nonce 16 bytes não zero, timeout ms u32 |
| 8 | `offerReference` | Echo de `offerHash`, 32 bytes |
| 9 | `clientLimitsPage` | Quatro registros de limite, 32 bytes |
| 10 | `clientCapabilitiesEnd` | `clientHash`, 32 bytes |
| 11 | `selectionPage` | Quatro registros selecionados, 32 bytes |
| 12 | `selectionEnd` | `selectionHash`, 32 bytes |
| 13 | `clientReady` | Echo de `selectionHash`, 32 bytes; memória reservada e decoder preparado |
| 14 | `serverCommit` | Echo de `selectionHash`, 32 bytes; snapshot e preparo do servidor reconfirmados |
| 15 | `clientCommitAck` | Echo de `selectionHash`, 32 bytes; cliente armou o decoder |
| 16 | `reject` | 12 bytes: código u16, field ID u8 (0 geral), reservado u8=0, valor oferecido u32, valor requerido u32 |
| 17 | `catalogIdentity` | SHA-256 de 32 bytes do manifesto canônico congelado do mapa |

Um registro de limite é `fieldId:u8`, três bytes reservados zero, `value:u32`.
IDs 1–12 e unidades estão em [capabilities.md](capabilities.md). Três páginas
obrigatórias, exatamente IDs 1–4, 5–8 e 9–12, nessa ordem. Não omitir zero,
duplicar campos, rearranjar IDs ou ignorar campos desconhecidos.

Major, minor, tipo, tamanho, reservado e enums desconhecidos são recusados.
Versão da negociação, versão do arquivo, ID do contrato de dados, identidade
da aplicação e ABI da DLL são contratos separados. Não aceitar outro contrato
porque seu inteiro de versão coincida.

## Ordem exata e confirmação

S/C são sequências independentes, não timestamps:

| Etapa | Frames e transição |
| --- | --- |
| Oferta completa | S0 `serverOffer`; S1 aplicação; S2 dados; S3 catálogo; S4–6 requisitos; S7–9 limites do servidor; S10 `offerEnd` |
| Capacidade do cliente | C0 `clientHello`; C1 referência à oferta; C2 aplicação; C3 dados; C4–6 limites do cliente; C7 `clientCapabilitiesEnd` |
| Seleção | S11–13 `selectionPage`; S14 `selectionEnd` |
| Preparo | Cliente verifica contrato, mínimos e orçamento, reserva storage e prepara decoder; C8 `clientReady` |
| Commit | Servidor reconfirma mapa, snapshot, limite e preparo; S15 `serverCommit` |
| Confirmação | Cliente arma decoder, preservando estado fora do reset nativo; C9 `clientCommitAck` |
| Liberação | Servidor valida C9 e canal drenado, então libera o sign-on do perfil selecionado |

Nenhuma implementação desta etapa envia esses frames. A ordem é o requisito
para os futuros codecs e adaptadores, não uma afirmação de suporte atual.

O servidor calcula o mínimo por campo entre os dois vetores; o cliente recalcula
e exige **igualdade exata**. Os requisitos do mapa precisam caber em cada campo.
Revalidar também os relacionamentos de framing/filas e os recursos privados de
cada papel; o mínimo isolado não prova preparo. Capacidades não são reduzidas
depois da oferta para recuperar uma falha de alocação. Mudança exige novo acordo.

Estados do servidor: `awaitCapabilities` → `awaitReady` → `awaitCommitAck` →
`committed`, ou `rejected`. Estados do cliente: `awaitOffer` → `awaitSelection`
→ `awaitCommit` → `armed`, ou `rejected`. Preparação local acontece antes de
`clientReady`; sucesso do handshake não equivale a catálogo/assets prontos.
Servidor não libera estado incompatível em `awaitReady` nem em `awaitCommitAck`.

Após `armed`, o primeiro serverinfo deve informar protocolo 48 e o spawn count
esperado. O reset nativo não apaga o acordo. Depois disso, o contrato de dados
deve separar recebimento/verificação do catálogo, carga dos assets e liberação
de baselines/índices de gameplay. ID e digest sem codec implementado são
`unsupportedContract`, mesmo com números que caibam.
As reservas e o decoder preparados também precisam sobreviver ao reset;
alocação em um hunk que será liberado por `Host_ClearMemory` não satisfaz
`clientReady`. O binding de ponteiros nativos só ocorre na geração correta,
depois do reset e das verificações exigidas pelo perfil.

## Hashes do transcript

SHA-256 sobre bytes exatos, sem JSON, whitespace ou framing externo. Labels ASCII
incluem um único byte NUL final. Concatenar apenas os frames únicos aceitos:

```text
offerHash = SHA256("ExtendedPrecache/offer/1\0" || S0 || ... || S9)
clientHash = SHA256("ExtendedPrecache/client/1\0" || offerHash || C0 || ... || C6)
selectionHash = SHA256("ExtendedPrecache/selection/1\0" || offerHash || clientHash
                       || S11 || S12 || S13)
```

O client nonce em C0, o server nonce em todos os headers, mapa, limites,
requisitos e hashes de identidade ficam vinculados. O catálogo não pode mudar
entre a oferta e a liberação. O cliente deverá conferir seu hash novamente ao
receber o manifesto pelo futuro contrato de dados.

O contrato de aplicação identifica módulos/versões e expectativas de mensagens
do mod; cada papel verifica seus módulos locais. Os hashes de `hw.dll` e
`swds.dll` são diferentes e **não devem ser comparados como iguais entre pares**.
Os perfis locais validam esses hashes contra suas implementações aprovadas.
O contrato de dados identifica todos os codecs necessários, de forma atômica:
não negociar metade da extensão deixando deltas, TE ou consistência antigos.
As identidades de aplicação/dados são SHA-256 dos bytes UTF-8 do artefato de
contrato aprovado, com LF, sem BOM, incluindo seu newline final. Não reserializar
JSON para comparar hashes: uma edição produz uma nova identidade. O digest do
catálogo segue o formato canônico definido pelo próprio contrato de dados.

O ID 1 no corpus é apenas um exemplo **indisponível**. Antes de aprová-lo serão
necessários os formatos completos de catálogo, consistência, deltas, áudio,
eventos, efeitos, decals e demos. Nenhum slot SVC novo foi reservado aqui.
A escolha desses codecs permanece no TODO; a negociação pode recusar esse ID
desde já sem introduzir um protocolo de dados parcialmente definido no motor.

## Repetição, falha e deadline

- Exigir a sequência esperada e o tipo previsto para a direção/estado. Uma cópia
  byte a byte de um frame já aceito é ignorada sem rehash, realocação, progresso
  ou extensão do deadline. Mesmo número com bytes diferentes é erro.
- Nonce antigo não zero é descartado sem alterar estado; continua sujeito à
  contabilização de tráfego/rate. Nonce zero é malformado. Um novo acordo só
  aceita S0 quando o endpoint estiver explicitamente em `awaitOffer`.
- A associação também exige instância local do cliente/canal, identidade do
  servidor remoto e geração do mapa; não usar somente IP, slot ou spawn count.
  Troca de mapa, desconexão e restart invalidam o acordo. Mudança de mapa exige
  nova conexão/nonce, sem renegociação in-place na versão 1.
- Timeout efetivo é o mínimo dos valores da oferta e de C0, entre 1.000 e 60.000
  ms, padrão 15.000. Servidor mede desde a entrada na barreira de `new`; cliente
  desde o primeiro `new` enviado, com relógios monotônicos locais. Antes de S0/C0
  cada papel usa seu limite local. Depois, aplicar o mínimo sem reiniciar relógio.
  Isso também limita `awaitOffer` em um servidor silencioso e o preparo local.
  Expirou quando `elapsed >= effectiveTimeout`; progresso/duplicatas não o renovam.
- Rejeição é terminal. Enviar no máximo um `reject` se houver canal e orçamento;
  caso contrário fechar com motivo local. Nunca emitir estado ampliado, fazer
  clamp ou passar mensagens incompletas ao parser nativo.

| Código | Nome | Uso |
| --- | --- | --- |
| 1 | `malformedFrame` | Framing, tamanho, reservado, nonce zero ou payload inválido |
| 2 | `unsupportedVersion` | Major/minor sem suporte |
| 3 | `unexpectedMessage` | Tipo, direção ou fase incorreta |
| 4 | `unexpectedSequence` | Gap, sequência fora da faixa ou repetição diferente |
| 5 | `unsupportedContract` | Identidade, ID, perfil local ou codecs não aprovados |
| 6 | `capacityInsufficient` | Requisito maior que o limite selecionado; informar field ID |
| 7 | `invalidConfiguration` | Contradição na política/limites locais |
| 8 | `storageUnavailable` | Cotação, reserva, alocação ou decoder indisponível |
| 9 | `transcriptMismatch` | Hash ou echo não corresponde ao transcript |
| 10 | `deadlineExceeded` | Prazo fixo encerrado |
| 11 | `mapChanged` | Snapshot ou geração do mapa mudou |
| 12 | `legacyIncompatible` | Par legado não comporta o mapa completo |
| 13 | `transportUnavailable` | Registro, ownership, rate/canal ou barreira inviável |
| 14 | `budgetExceeded` | Bytes/frames de controle excedem a política |

Campo geral e valores sem aplicação usam zero. Motivo legível é derivado
localmente desses códigos; o controle não transporta comandos ou strings de erro
executáveis. O frame `reject` pode encerrar qualquer fase, usando a próxima
sequência da direção. Não responder a uma rejeição com outra rejeição.

## Legado, demos e critério de implementação

Sem opt-in, servidor `requireExtended` recusa com desconexão legada legível.
Os outros modos só liberam o mapa após os testes de compatibilidade de
`capabilities.md`. Cliente `preferExtended` pode aceitar um servidor legado
somente antes de S0 e após validar o caminho legado; cliente `requireExtended`
recusa. Depois de S0 não há downgrade por timeout, erro ou ausência de confirmação.

Uma demo nunca inicia negociação de rede ou emite C0/C8/C9. O contrato de dados
precisará registrar seleção, identidades e catálogo suficientes para validar
playback local sem o cliente remoto. Não supor que o transcript C→S esteja na
demo GoldSrc. Um perfil ampliado sem definição/testes desse registro continua
indisponível; isso não declara compatibilidade com demos existentes.

O [corpus binário](protocol/negotiationVectors.json) contém 26 frames sintéticos,
hashes e casos negativos. Ele verifica o acordo no nível de especificação,
sem afirmar leitura/escrita pelo motor. A integração deverá provar a barreira,
registro anterior ao serverinfo, despacho anterior à ClientDLL/GameDLL, drain do
netchan, reconexão, reserva, recusa de pares legados incompatíveis e preservação
do acordo através do reset, antes de habilitar qualquer adaptador.
