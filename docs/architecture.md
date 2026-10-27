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
definir capacidades e mensagens a partir da auditoria completa do protocolo.

Não há interfaces abstratas especulativas: os adaptadores e o protocolo serão
introduzidos quando existirem consumidores e evidências suficientes.

## Código presente

- `include/extendedPrecache/domain`: tipos fortes e catálogo de caminhos por categoria.
- `src/domain`: validação de caminhos canônicos, independente do motor.
- `include/extendedPrecache/abi`: contrato POD com linkage C e versão explícita.
- `src/platform/windows`: DLL de diagnóstico sem hooks ou inicialização em `DllMain`.
- `tests`: testes de comportamento do domínio e carregamento dinâmico da DLL.
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
precisam de contratos próprios, sem mudar silenciosamente a semântica existente.

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
separam contagens, IDs, caches e bytes. Esses contratos ainda não têm leitor,
codec, state machine ou transporte implementado. Não alteram a ABI de diagnóstico.

O perfil de dados ampliado proposto usa índices/contagens u32, mas seu formato
completo ainda depende de deltas, consistência, efeitos, áudio, eventos, decals,
demos e consumidores do mod. O perfil fictício do corpus está indisponível;
nenhum acordo pode habilitá-lo no motor apenas por possuir um ID ou digest.

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

O próximo módulo será o validador/codec de sessão independente do motor, em C++26.
Os [vetores offline](contractVerification.md) são a referência externa dos seus
testes, não bytes gerados pelo codec sob teste. Estado do acordo deve ficar fora
do `client_state_t` zerado no serverinfo; ownership, geração de handles e ABI dos
hooks ainda exigem desenho concreto e validação antes dos adaptadores.
