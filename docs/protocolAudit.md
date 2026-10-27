# Auditoria do protocolo 48

Escopo: os hashes e a revisão de fonte fixados em [engineAudit.md](engineAudit.md).
Esta é uma descrição estática do tráfego atual, não a especificação de um novo
protocolo. `uN` significa inteiro sem sinal com N bits; `s16` significa leitura
com extensão de sinal de 16 bits. Contagem, índice da categoria, ordinal da
lista global, entidade e handle local são contratos diferentes.

## Pares de escrita e leitura

| Fluxo | Escritor ReHLDS / leitor cliente | Campos relevantes e limites |
| --- | --- | --- |
| SVC 11, server info | `SV_SendServerinfo`; cliente `0x101a78c0` via wrapper `0x101aa2d0` | Protocolo 48; não transmite capacidades ampliadas. O reset do cliente ocorre no início do processamento |
| SVC 45, resource request | `SV_SendResources_internal`; wrapper cliente `0x101aa290` | Spawn count 32 + início 32; solicita a lista de customizações do cliente, não amplia a lista global |
| SVC 56, resource location | `SV_SendResources_internal`; wrapper cliente `0x101aa280` | URL terminada em NUL, envio condicionado a comprimento menor que 129; não altera o codec de recursos |
| SVC 43, resource list | Servidor `0x049a9500`; cliente wrapper `0x101aa270`, corpo `0x101a7690` | Contagem u12; registros e cauda de consistência descritos abaixo |
| SVC 6, dynamic sound | `SV_BuildSoundMsg`; cliente `0x101a5e10` | Flags u9; volume/atenuação opcionais u8; canal u3; entidade u11; som u8 ou u16 conforme flag; coordenadas e pitch opcional u8 |
| SVC 29, static sound | `PF_ambientsound_I`; cliente wrapper `0x101aa3b0`, corpo `0x101a8410` | Posição; som **s16**, volume u8, atenuação u8, entidade s16, pitch u8, flags u8 |
| SVC 3, event | `SV_EmitEvents`; cliente `0x101a9840` | Quantidade u5; índice do evento u10; referência opcional de entidade u11; delta dos argumentos; atraso opcional u16 |
| SVC 21, reliable event | `EV_PlayReliableEvent_internal`; cliente `0x101aa160` | Índice do evento u10; delta dos argumentos; atraso opcional u16. Não usa a mesma contagem da mensagem não confiável |
| SVC 14, delta description | Escrita das descrições delta; cliente `0x101a6000` | Nome + quantidade de campos u16 + metadados delta; largura do modelo depende do schema enviado |
| SVC 22, baseline; SVC 40/41, entities; SVC 15, client data | `DELTA_WriteDelta` e emissores respectivos; baseline cliente `0x101a6c60` e leitores de deltas | `modelindex`, `weaponmodel` e `viewmodel` seguem o schema, tipo e armazenamento da estrutura; não inferir 9 bits da tabela de 512 |
| SVC 23, temporary entity | Escritas de mensagens pela GameDLL/API; cliente wrapper `0x101aa4d0`, corpo `0x101ade70` | Subtipo u8; modelo/sprite frequentemente **s16**, decals variam; inventário abaixo |
| SVC 36, decal name | Mensagem legada de decal; cliente `0x101a9d90` | Índice u8 + nome NUL; só endereça 256 IDs, apesar da tabela local de 512 |
| SVC 46, customization | `SV_CreateCustomization`; cliente `0x101a9d50` | Jogador u8, tipo u8, nome NUL, índice 16, tamanho 32, flags u8 e MD5 condicional; é o caminho de sprays customizados |
| SVC 20, spawn static | `PF_makestatic_I` legado | Fonte emite modelo 16, mas o handler deste cliente (`0x101aa3a0`) retorna sem consumir payload. Não é um transporte disponível para a extensão |

`MSG_ReadShort` em `0x101b9e60` retorna um `short` estendido com sinal. Assim,
um campo de 16 bits não implica faixa positiva até 65.535. Tampouco a largura
de um transporte autoriza escrever fora dos arrays atuais de 512/256 entradas.
Sentenças de áudio usam flags/IDs próprios; canais e IDs de entidades não devem
ser alargados junto com o índice de som.

Fontes primárias: [recursos](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/sv_main.cpp#L1205),
[áudio e eventos](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/pr_cmds.cpp#L260),
[escrita de bits](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/common.cpp#L396).

## Lista global e consistência

Cada registro de SVC 43 contém tipo u4, nome NUL, `nIndex` u12, tamanho u24,
flags u3, MD5 de 128 bits condicional, presença u1 e dados reservados de 256 bits
condicionais. O leitor de nomes limita a string a 63 bytes. O cliente cria um
nó de 136 bytes por recurso e chama `CL_ParseConsistencyInfo` antes de encerrar
a leitura de bits. O índice é da categoria; a posição do registro é o ordinal
usado na consistência.

**Saturação confirmada:** `MSG_WriteBits` nesta revisão limita valores ao máximo
do campo. O binário `0x049a9500` transmite `min(num_resources, 4095)`, mas
percorre `num_resources` registros. Portanto, 4.096 registros armazenados geram
contagem 4.095 seguida de 4.096 registros: a fronteira do parser fica incorreta.
Não ocorre wrap para zero. O mesmo cuidado vale para índice u12 e tamanho u24;
validar antes da serialização, sem confiar na saturação do escritor.

Há outra assimetria: o escritor envia somente os bits `RES_WASMISSING` e
`RES_FATALIFMISSING` nas flags, mas testa `RES_CUSTOM` do objeto original para
decidir se escreve MD5. O leitor testa `RES_CUSTOM` nas flags recebidas. Um hook
que introduza esse flag na lista global não produz um round-trip válido com
esse escritor. O caminho normal de customizações é SVC 46 / CLC 5. Os 128 bits
opcionais pertencem à gramática do leitor; não autorizam inserir customizações
arbitrárias em SVC 43 sem corrigir o contrato de ambos os lados.

A cauda de consistência começa com presença u1. Para cada arquivo verificado:
continuação u1, seletor u1 e **delta u5** quando a distância ao ordinal anterior
é até 31; caso contrário, **ordinal absoluto u10**. Termina com continuação zero.
Um primeiro ordinal 1.024 satura para 1.023 no escritor; uma cadeia com gaps de
até 31 pode alcançar ordinais maiores. O problema depende da distribuição da
lista, não apenas do maior índice. O cliente também recusa mais de 512 arquivos
verificados. Confirmado em `SV_SendConsistencyList`, nativo `0x049c57f0`, e no
leitor `0x101a6ef0`.

No sentido contrário, **CLC 7** contém tamanho do payload **s16**, seguido de
registros com continuação u1, ordinal u12 e CRC u32 ou seis floats de limites
(192 bits); termina com bit zero. O cliente (`0x101a8a60`) grava o comprimento
com cast para `short` e aplica munging dependente do spawn count. O servidor
valida ordinal, flag de arquivo verificado e quantidade esperada de respostas.
Ampliar a lista exige também revisar o comprimento de 32.767 bytes e o contador.

**CLC 5**, upload de recursos do cliente, usa contagem s16, nome, tipo u8, índice
16, tamanho 32, flags u8 e MD5 condicional. O servidor com `REHLDS_FIXES` aceita
no máximo um recurso e valida a customização `tempdecal.wad`. Esse limite não é
um teto de precache global. Fontes:
[consistência e upload](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/sv_user.cpp#L334).

## Deltas da instalação selecionada

SHA-256 de `cstrike/delta.lst`:
`22ecb88fa68a748465f5a88d10edf06f3f46d5a9c8375b5c95a3a9310cc43db9`.

| Linha do arquivo | Campo | Bits |
| --- | --- | --- |
| 35 | `viewmodel` | 10 |
| 78, 138 | `modelindex` | 10 |
| 97, 147 | `weaponmodel` | 10 |
| 193 | `modelindex`, bloco de entidade customizada | 16 |

Isso descreve a configuração local, não todos os mods. A descrição delta
transmite a largura, mas o adaptador ainda terá de validar tipo do campo,
offset, tamanho da estrutura, sinal, baseline e mensagens dependentes. Mudar
apenas esse arquivo não amplia cache, mensagens TE ou tabelas locais. Mensagens
de usuário de mods podem codificar o mesmo índice com byte/short próprio;
precisam de contrato específico. Elas são uma fronteira externa da auditoria.

## Efeitos temporários

O dispatcher de `0x101ade70` foi recuperado pela tabela real de serviços.
Os subtipos abaixo têm campos de recurso; leituras de outros shorts (entidade,
cor, vida, quantidade) não devem ser alteradas por associação ao tamanho.
A nomenclatura vem do
[contrato público de efeitos](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/common/const.h#L148),
comparado com o switch do cliente identificado.

| IDs e nomes | Campo de recurso observado |
| --- | --- |
| 0 BEAMPOINTS, 1 BEAMENTPOINT, 8 BEAMENTS | Sprite s16; IDs de entidades do beam são campos separados |
| 3 EXPLOSION, 5 SMOKE, 7 LIGHTNING, 15 SPRITETRAIL, 17 SPRITE | Sprite/modelo s16 |
| 18 BEAMSPRITE | Dois índices s16, beam e sprite final |
| 19 BEAMTORUS, 20 BEAMDISK, 21 BEAMCYLINDER, 22 BEAMFOLLOW, 23 GLOWSPRITE, 24 BEAMRING | Sprite s16 |
| 100 LARGEFUNNEL, 105 FIZZ, 106 MODEL, 107 EXPLODEMODEL, 108 BREAKMODEL, 110 SPRITE_SPRAY | Modelo/sprite s16; flags e contadores separados |
| 113 BUBBLES, 114 BUBBLETRAIL, 119 PROJECTILE, 120 SPRAY, 121 PLAYERSPRITES, 123 FIREFIELD, 124 PLAYERATTACHMENT | Modelo/sprite s16 |
| 115 BLOODSPRITE | Dois índices de sprite s16 |
| 13 BSPDECAL | Decal s16, entidade s16 e modelo s16 somente quando entidade não é zero |
| 104 DECAL, 116 WORLDDECAL | Decal u8 |
| 117 WORLDDECALHIGH, 118 DECALHIGH | Decal u8 mais 256; duas variantes para a faixa de 512, sem extensão automática |
| 109 GUNSHOTDECAL, 126 MULTIGUNSHOT | Decal u8 encaminhado ao callback do efeito |
| 112 PLAYERDECAL | Jogador/customização, entidade s16 e decal u8; representação de spray e resolução do modelo são específicas do caminho |

O comentário opcional de modelo no SDK para PLAYERDECAL não foi tratado como
um campo adicional: esse short não aparece no leitor deste cliente. A origem
nativa prevalece para este perfil. Subtipos sem índice de recurso, inclusive
99 KILLBEAM e 122 PARTICLEBURST, não ampliam o catálogo; seus shorts têm outro
significado. Os casos obsoletos 16/26 não oferecem um transporte de extensão.
Callbacks dos efeitos usam ponteiros de função; a resolução posterior pode
passar por `CL_ModelPrecache` e por APIs entregues à ClientDLL. A ABI e esses
objetos precisam manter validade até o fim do uso do efeito.

## Orçamento do sign-on e fragmentação

`SV_SendRes_f` prepara um buffer de **65.536 bytes** com verificação de overflow,
serializa a mensagem inteira e somente depois cria os fragmentos de netchan.
A fragmentação não aumenta esse buffer nem o buffer de remontagem do receptor.
Fonte: [montagem do envio](https://github.com/rehlds/ReHLDS/blob/196300131858d1e087267fb2fb60cb935f8f8873/rehlds/engine/sv_main.cpp#L1596).

Para nome de L bytes, um registro ordinário custa `52 + 8L` bits, incluindo NUL
e presença de dados reservados. Acrescentar 256 bits se houver dados reservados;
a gramática do leitor também admite mais 128 bits de MD5 quando customizado,
com a ressalva do escritor atual descrita acima. A lista inclui contagem u12,
cauda de consistência e alinhamento; há ainda SVC 45, SVC 43 e URL opcional.

Com 4.095 nomes de 63 bytes, só contagem, registros ordinários e presença zero
da consistência já custam **284.605 bytes**. Com reservados em todos os registros,
o subtotal é 415.645 bytes. Mesmo abaixo de 4.095 recursos, nomes e metadados
podem estourar 65.536. O limite por sessão precisa ser validado por bytes reais,
não apenas por quantidade, maior índice ou tamanho dos fragmentos.

## Vetores que o novo contrato deverá demonstrar

| Fronteira | Resultado atual demonstrado / verificação futura exigida |
| --- | --- |
| Modelos/sons 511, 512 e 513 | 512 é tamanho de array, não 512 IDs úteis; novo storage e todos os leitores devem concordar |
| Eventos 255/256 e wire 1.023/1.024 | Distinguir limite de tabela, retorno ushort, fila e campo u10 |
| Recursos 4.095/4.096 | Evitar saturação da contagem com emissão de registros excedentes |
| Índice 4.095/4.096; arquivo 16.777.215/16.777.216 bytes | Recusar valores antes da saturação de u12/u24 |
| Primeiro arquivo consistente 1.023/1.024; cadeia com gaps 31/32 | Demonstrar os dois ramos, preservando ordinal absoluto |
| 512/513 arquivos verificados | Ampliar ambos os arrays e seus contadores antes do novo tráfego |
| Respostas só com CRC / só com bounds | 512 registros custam 2.881 / 13.121 bytes de payload; 1.280 com bounds custam 32.801, além de s16 |
| Índices s16 32.767/32.768 | Novo contrato não pode pressupor faixa positiva de u16 |
| Decals 255/256/511/512; HUD sprites 255/256/257 | Distinguir byte, variante HIGH, nome, cache, handle e sprite de rede |
| Mesmo catálogo com nomes curtos/longos; 65.535/65.536/65.537 bytes | Orçamento de mensagem e buffers de ambos os lados, incluindo headers e padding |
| Reinício, reconnect, mudança de mapa, demo e mod com índice em byte | Estado da negociação, geração dos handles e codec devem permanecer coerentes |

Esses vetores são requisitos derivados da auditoria, **não testes de integração
executados**. Não há slot SVC livre comprovado: a tabela recuperada ocupa 0–59
(incluindo entradas reservadas); 60–63 não foram aprovados como IDs livres.
Mensagens de usuário começam em 64. Canal inicial, versionamento, recusa de
incompatibilidade e registro da capacidade em demos exigem desenho explícito
antes de qualquer alteração do tráfego.
