# Núcleo de dados C++26

O alvo estático `extendedPrecacheData` implementa o contrato de dados 2.0 de
[dataCodecs.md](dataCodecs.md), separado do controle 1.0, do catálogo de caminhos
do domínio e da DLL diagnóstica. Não há adaptador, transporte nativo, plugin
Metamod, recorder/player do motor ou perfil de integração aprovado. O indicador
`ApplicationContract::nativeAvailable()` permanece falso; JSON recebido não pode
alterá-lo.

## Fronteiras da API

Os headers em `include/extendedPrecache/data` usam IDs distintos para recursos,
ordinais, schemas, campos, módulos, frames, entidades, offsets e tempos.
`ResourceReference` combina categoria e ID de domínio u32; o catálogo confere a
categoria, o intervalo e a nulabilidade. Índices nativos zero/um continuam sendo
responsabilidade dos bindings especificados em [indexMapping.md](indexMapping.md).
Nenhum cast para um campo legado é fornecido pelo codec.

Os codecs e preflights retornam `Result<T>` (`std::expected<T, Failure>`), com
código de erro tipado. Cursors privados verificam bytes, overflow e reservados;
falhas de contrato e de alocação são convertidas na fronteira `noexcept`, sem
resultado parcial. Essas APIs são internas C++ e não fazem parte da ABI da DLL.
Construtores C++ comuns continuam sujeitos às regras de alocação da biblioteca
padrão. Não exportar essas classes, containers ou exceções por uma ABI C.

| Header | Responsabilidade |
| --- | --- |
| `wireLayout.hpp`, `types.hpp` | Larguras de wire independentes de `sizeof`, limites e tipos |
| `schema.hpp` | Schemas, schema sets, 14 tipos de valores, arrays e referências |
| `catalog.hpp` | Catálogo EPCM imutável, assets, regras e chunks contíguos |
| `consistency.hpp` | Páginas por bytes, conferência integral das regras e digest do relatório |
| `envelope.hpp`, `codec.hpp` | Framing EPD2, rotas, fases, políticas de serviço e sequências |
| `state.hpp` | Deltas, reconstrução atômica, snapshots, ACKs e recuperação |
| `application.hpp` | JSON fechado, vínculos e preflight semântico por autoridade local |
| `effects.hpp`, `media.hpp` | Efeitos auditados, sons, sentences, eventos e nomes de decals |
| `modMessages.hpp`, `readiness.hpp` | Mensagens tipadas/nativas aprovadas e registros de readiness |
| `demo.hpp` | Container EPCDEMO2 incremental e validação delegada da gramática nativa |

## Caminho de uso

Carregar `ApplicationContract` com um `DigestProvider` local. O leitor rejeita
chaves extras/duplicadas, números JSON fracionários ou em notação exponencial,
IDs/names duplicados, referências sem destino, roles incompatíveis e hashes de
schemas divergentes. A identidade usa os bytes UTF-8/LF exatos, sem BOM e com
newline final. `specificationOnly` e `availability` conservam os valores
indisponíveis definidos pelo schema.

Executar `preflightApplication` antes de oferecer o perfil. Informar a identidade
do descritor instalado, requisitos do mapa, capacidades selecionadas, papel e
orçamento local. `ApplicationBindings` deve aprovar limites, schemas, módulos,
replay, serviços, efeitos e mensagens; identificar os binários e bindings do
papel local; e fornecer faixas reais dos consumidores nativos. O núcleo não
implementa uma autoridade permissiva nem consulta offsets recebidos do par.

O preflight exige cobertura de todos os slots SVC 0–63 e CLC 0–255, proíbe os
opcodes reservados e verifica fases e modos. Registros indivisíveis e o pior
valor declarado precisam caber na mensagem e no limite do serviço. Mensagens
`boundedLegacy` exigem representação e faixa de consumidor para **toda** a
categoria congelada após a conversão zero/um. Um byte já truncado não permite
recuperar o índice original. Mensagens nativeExact exigem um parser local do
corpo completo e continuam sujeitas a 192 bytes.

Congelar o catálogo com `Catalog::create`, ou usar `Catalog::decode` com digest
esperado obrigatório. O decoder verifica tamanho e identidade antes de produzir
a visão estável. Valida ordem densa, primeiro uso dos assets, caminhos, referências,
parentesco de inline models, world model e consistência. O objeto possui dados,
bytes canônicos e digest por `shared_ptr<const Storage>`; cópias compartilham
ownership imutável. `resolve` empresta uma referência válida enquanto uma cópia
do catálogo permanecer viva. Nenhum objeto é uma arena nativa do motor.

Usar `encodeMessage`/`decodeMessage` para compor as verificações de corpo, rota,
fase, serviço e catálogo. Os helpers de corpo são úteis para assemblers e
ferramentas; isoladamente não autorizam uma transição de sessão. A fase em
`CodecContext` deve vir do controlador autorizado. Readiness, gameStart e ACK
codificam e conferem identidades, mas a prova de assets realmente carregados e a
liberação dos gates continuam externas.

`DataReceiver` adiciona sequência e contexto por conexão/mapa. Depois de verificar
o framing, descarta nonce antigo e mensagens não confiáveis obsoletas antes de
interpretar o corpo. Nonce corrente com epoch errado, sequência confiável
inválida ou corpo inválido torna o receptor terminal. `decodeMessage` isolado
não substitui esse controle. `Envelope`, `ChunkView`, mensagens decodificadas e
views de demo podem emprestar os bytes de entrada; não guardar depois de liberar
ou alterar o buffer. DTOs de valores e snapshots possuem suas próprias cópias.

Receivers, ledgers, assemblers e streams têm um único owner e acesso serializado.
Schemas emprestados, providers e bindings precisam viver por todo o uso de um
contexto ou objeto que os referencia. Manter uma cópia do `ApplicationContract`
preserva seu storage imutável. Destruir um buffer de entrada invalida as views
desse buffer, mesmo quando o catálogo e a aplicação continuam vivos.

## Bytes, memória e estado

`RecordBudget` separa tamanho de mensagem e custo dos dados possuídos. O tamanho
do corpo não exclui o envelope do orçamento de transporte. `DataMessageBuilder`
acumula envelopes de uma única direção/entrega/nonce/epoch e aplica o limite ao
total da mensagem lógica; uma inclusão recusada preserva o prefixo anterior.
Não calcula sequências nem confirma envio. Quem combinar serviços nativeExact
com EPD2 também precisa contabilizar **todos** os bytes no mesmo limite antes
de enfileirar. O núcleo não classifica um pacote misto pelo bit UDP confiável.

Chunks e páginas respeitam os overheads declarados; não há quantidade fixa de
regras por página. Assemblers exigem offsets contíguos, totais e hashes exatos.
Schema/value decoders e reconstrução limitam os grafos possuídos além dos bytes
de wire. Esses custos usam elementos, strings e `sizeof` da arquitetura atual;
não são medição do heap nem teto do processo, pois o allocator e consumidores
nativos têm custos adicionais.

`ApplicationPlan::core` fornece um piso calculado para estado reconstruído,
staging, janela de snapshots, catálogo, schemas, artefatos, fluxo e demos.
`quotePeak` deve incluir esse piso e o pico real de parsing, índices auxiliares,
containers, allocator, referências ainda emprestadas, arenas e consumidores
nativos. Uma cotação abaixo do piso ou acima do orçamento é recusada. O plano
não reserva memória: reserva durável, ownership de leases, limites de owners
externos e liberação com quiescência continuam sob os contratos de integração.

`FrameReceiver` mantém staging separado por entrega e só publica após reconstruir
o estado completo, validar objetos obrigatórios e conferir o hash de frameEnd.
O merge de objetos é linear e o custo é verificado antes da cópia. Valores f32
finitos são canônicos, incluindo +0. Perda de partes/base não confiáveis produz
um pedido de recuperação; dados inválidos continuam fatais. `FrameLedger` aceita
base retida e confirmada, ignora ACK de prefixo retirado e exige o hash de bases
retidas. Uma recuperação confiável bloqueia novos frames até seu ACK exato.
O controlador deve tratar falhas como encerramento e aplicar prazos; não há
timer, fila de rede, retransmissão ou mudança do relógio nativo neste núcleo.

## Efeitos e demos

O registro de efeitos em produção deriva das larguras e restrições normativas
de `effectSchemasV2.json`. Componentes RGB/beamStyle são campos primitivos nomeados
na API; os bytes e a ordem permanecem os do contrato. Aliases HIGH e efeitos
obsoletos são recusados. Subtipo de modelo/sprite, flags, dono BSP e namespaces
auxiliares têm verificações distintas. Sentence, atualização de canal, schema de
eventArgs e campos auxiliares exigem provas pelos bindings locais correspondentes.

`DemoReader`/`DemoWriter` processam header e blocos limitados, sem mapear o arquivo
inteiro. Offsets, tamanho total, sequência e tempo são u64. A identidade do
arquivo é incremental, exclui o bloco end e requer ordem, hashes e EOF exatos;
o timestamp do end repete o último tempo anterior. `DigestStream` e
`StreamingDigestProvider` são contratos separados do digest de uma só chamada.
No Windows, CNG fornece SHA-256 incremental por RAII, encerrado após finish.
Referências: [BCryptCreateHash](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptcreatehash)
e [BCryptFinishHash](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptfinishhash).

`ContractDemoValidator` confere artefatos, aplicação, schemas e catálogo contra
as identidades instaladas. `DemoRecordValidator` é obrigatório para provar o
bootstrap SVC 11, dividir o pacote misto por gramática, validar os registros
aceitos e a proveniência do estado local. Nenhuma implementação para um motor
é fornecida. Um callback que aceita tudo não comprova uma demo reproduzível.
`DemoReader::finish` exige o tamanho real do arquivo para confirmar EOF. Uma
falha invalida o fluxo; iniciar outro mapa exige outra instância/arquivo.

## Integração pendente

O núcleo não registra hooks, carrega assets, resolve índices nativos, modifica
arquivos do jogo ou aprova módulos externos. O próximo trabalho é comprovar
bootstrap, dispatch, drain/reset e reconexão dos hashes selecionados. Continuam
pendentes o armazenamento nativo, ABI de integração 2, medição de memória e
validação ponta a ponta. A DLL conserva somente a ABI diagnóstica 1.

Verificadores, vetores independentes, harnesses e relatórios adicionados nesta
etapa ficam em área local ignorada. Os checks públicos existentes são preservados.
Este documento descreve contratos e limitações; não é relatório de teste local.
