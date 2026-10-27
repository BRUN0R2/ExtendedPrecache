# Escopo e aceitação

## Resultado pretendido

Permitir que servidores e clientes compatíveis utilizem mais recursos do que as
tabelas originais permitem. A capacidade deve ser configurável e validada em
todas as camadas envolvidas. O produto só poderá anunciar essa capacidade depois
de demonstrá-la com o motor real nos dois extremos da conexão.

## Categorias

| Recurso | Tratamento |
| --- | --- |
| Modelos `.mdl` | Catálogo de modelos, carregador, entidades, deltas e efeitos |
| Sprites `.spr` | Mesmo catálogo de modelos; auditar também mensagens temporárias |
| Mapas e submodelos `*n` | Compartilham tabelas de modelos e exigem tratamento do carregador BSP |
| Sons | Catálogo, emissão, mensagens, sentenças e sistema de áudio |
| Arquivos genéricos | Distribuição/download, lista de recursos e consistência |
| Eventos `.sc` | Precache, tabelas e reprodução de eventos |
| Decals | Nomes, índices, recursos e aplicação de decals; não são apenas caminhos |

O catálogo inicial aceita nomes de arquivos canônicos. Não representa ainda
submodelos `*n`, prefixos de som, nomes de decals ou flags de modelos opcionais.
Essas semânticas pertencem aos adaptadores e precisam de testes próprios.

Texturas/WADs, entidades, limites de BSP, lightstyles, visibilidade, mensagens,
memória de renderização e buffers de rede devem entrar na análise de impacto.
Sua ampliação não faz parte automaticamente desta primeira extensão de precache.

## Plataformas e instalação

- Alvo inicial: Windows x86, CS 1.6 e servidor HLDS/ReHLDS.
- Servidor inicial de investigação: instalação `REHLDS-Rezombie` indicada pelo mantenedor.
- Distribuição pretendida do servidor: plugin do Metamod com integração específica
  do motor. Metamod-R requer ReHLDS; não deve ser apresentado como opção para HLDS original.
- Cliente: DLL complementar, com carregamento explícito a definir após auditoria.
- Linux, HLTV e outros motores: sem suporte declarado; avaliar separadamente.

## Critérios de aceitação do produto

1. Identificar os binários por SHA-256 e recusar versões não suportadas sem escrita.
2. Precache e utilização real de recursos acima dos limites originais sem truncamento.
3. Cliente e servidor concordarem sobre capacidades antes de qualquer estado estendido.
4. Índices estáveis durante o mapa, deduplicação correta e liberação no encerramento.
5. Nenhuma regressão em sessões legadas dentro das capacidades demonstradas.
6. Reconexão, troca de mapa, eventos, sons, sprites e efeitos funcionarem em integração.
7. Demos terem comportamento documentado e testado para cada formato suportado.
8. Entradas malformadas e excesso de recursos produzirem erro explícito, sem corrupção.
9. Publicação reproduzível, documentação de instalação e matriz de versões verificadas.

## Aceitação da fundação atual

A fundação deve compilar no modo C++26, testar índices e capacidades além de 512,
preservar a separação de categorias e expor diagnóstico versionado da DLL.
Passar esses checks não satisfaz os critérios de integração acima.
