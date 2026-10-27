# Arquitetura

## Componentes

```text
CS 1.6 x86                          HLDS/ReHLDS x86
  adaptador do cliente                plugin do Metamod
  tabelas e consumidores              adaptador do servidor
           \                         /
            contrato de sessão negociado
                       |
              domínio de recursos
       tipos, catálogo, capacidade, índices
```

O domínio não depende de Windows, Metamod, SDKs do jogo ou offsets. Os adaptadores
traduzirão as regras reais de cada motor para esse domínio. O transporte deverá
aplicar os contratos de capacidade e negociação, provando o bootstrap auditado.

Não há interfaces abstratas especulativas: os adaptadores e o protocolo serão
introduzidos quando existirem consumidores e evidências suficientes.

## Código presente

- `include/extendedPrecache/domain`: tipos fortes e catálogo de caminhos por categoria.
- `src/domain`: validação de caminhos canônicos, independente do motor.
- `include/extendedPrecache/session` e `src/session`: configuração, admissão,
  codec e máquina de estados, sem dependência de APIs do motor.
- `include/extendedPrecache/data` e `src/data`: codecs 2.0, preflights e estado,
  sem SDKs/offsets do motor; APIs e ownership em [dataCore.md](dataCore.md).
- `include/extendedPrecache/abi`: contrato POD com linkage C e versão explícita.
- `src/platform/windows`: DLL diagnóstica sem hooks e provider CNG em alvo estático separado.
- `tests`: domínio, configuração, corpus do protocolo, sessões e carregamento da DLL.
- `cmake`: opções de compilação aplicadas por alvo.

## Catálogo

Cada instância pertence a uma categoria e a um ciclo de vida de mapa. O índice zero
é reservado; a capacidade configurada conta entradas utilizáveis, a partir de um.
Não equivale ao tamanho total de um array legado, que pode incluir slots reservados.

O catálogo armazena uma cópia do nome, compara bytes e diferencia maiúsculas de
minúsculas. Não normaliza nomes silenciosamente. O adaptador deve selecionar o
comportamento exato do motor e tratar aliases antes de registrar caminhos.
Genéricos e decals do motor aceitam índices desde zero; ordinais da lista global
também são um domínio distinto. A auditoria demonstrou que o catálogo atual não
pode ser ligado diretamente a esses campos. As traduções e os resultados de erro
têm [contrato próprio](indexMapping.md), sem mudar a semântica existente. O retorno
zero por alias no registro genérico também é separado de um índice zero válido.

O catálogo oferece registro, busca por nome e congelamento. Após congelar, ainda
resolve entradas existentes, mas recusa novos registros. Uma nova instância deve
ser criada para um novo mapa; índices não são identificadores globais persistentes.
Não existe sincronização interna: a instância tem um único proprietário e acesso
serializado. Falhas de alocação seguem as exceções da biblioteca padrão dentro do
domínio; não são convertidas em sucesso nem podem atravessar a ABI pública.

Os tipos `ResourceIndex<model>` e `ResourceIndex<sound>` são incompatíveis entre si.
`CatalogLimits` valida `ResourceCapacity` e `ResourcePathByteLimit`, impedindo a
troca acidental desses parâmetros. `IndexEncoding` representa
uma largura de campo independente da capacidade do armazenamento.

## Integração futura

A entrada do Metamod é apenas o carregador do lado servidor. Remover uma verificação
de overflow ou interceptar `PrecacheModel` não amplia os demais arrays e consumidores.
O adaptador deverá cobrir leitura, escrita, índices, carregamento e encerramento.

A [negociação 1.0](negotiation.md) define o canal de controle, framing, seleção,
identidades e confirmação antes de liberar o sign-on. As [capacidades](capabilities.md)
separam contagens, IDs, caches e bytes. O [núcleo de sessão](sessionCore.md)
implementa validador, codec e estados por interfaces de digest e preparo durável.
O transporte nativo permanece pendente. Os alvos novos não são ligados à DLL
nem alteram sua ABI de diagnóstico.

O [plano de dados 2.0](dataCodecs.md) especifica referências u32 por categoria,
catálogo/consistência paginados por bytes, schemas aprovados e frames atômicos,
áudio/eventos/efeitos, demos locais e contratos de módulos. Seu descritor é
independente do exemplo fictício ID 1 do corpus. Ambos continuam indisponíveis:
os [codecs de dados](dataCore.md) estão implementados isoladamente, mas nenhum
binding nativo ou perfil foi aprovado. ID/digest não habilitam um perfil sem
cobertura integral e reserva de todos os consumidores.

Uma sessão sem extensão só poderá continuar se todos os recursos necessários
couberem nas capacidades comprovadas do par. Não haverá truncamento, reutilização
de slots, índices fictícios ou descarte silencioso de recursos para contornar limites.

O protocolo original, o HLDS original e o ReHLDS são contratos distintos a verificar.
Uma referência em fonte ReHLDS não prova identidade com qualquer binário Valve.

A [auditoria estática](engineAudit.md) encerra a investigação dos dois hashes
selecionados. O [protocolo](protocolAudit.md) exige resolver contagem saturada,
consistência 5/10/12 bits, campos s16, schemas delta e o buffer de 65.536 bytes.
O [orçamento](memoryBudget.md) separa tabelas de rede, caches locais, HUD e dados
dos assets. Nenhum desses resultados aprova um adaptador por endereço fixo.

Os [vetores offline](contractVerification.md) são a referência externa dos testes
do núcleo C++26, não bytes gerados pelo codec sob teste. O desenho de
[ownership](storageOwnership.md), [ciclo de mapa](mapLifecycle.md) e
[ABI 2.0](integrationAbi.md) define reservas duráveis, caches sem realocação sob
empréstimo, handles vinculados à geração e retirada com quiescência. São contratos
especificados, ainda sem código de storage/adaptador. A ABI nova copia metadados
e não exporta ponteiros do motor; as APIs externas antigas conservam seus limites.
Provar o bootstrap e os bindings exatos continua obrigatório antes de habilitá-los.
