# ExtendedPrecache — plano de trabalho

Atualizado em: 27/10/2026.

Este arquivo é o registro de execução do projeto. Use `[ ]` para trabalho pendente
e `[x]` somente para trabalho concluído e verificado. Evidências e impedimentos
devem acompanhar o item correspondente. As regras estão em [RULES.md](RULES.md).

## 1. Definição e documentação

- [x] Criar `docs/TODO.md` e `docs/RULES.md` antes da implementação.
- [x] Documentar objetivo, recursos abrangidos e critérios de aceitação em `scope.md`.
- [x] Documentar arquitetura, fronteiras de ABI e plano de compatibilidade em `architecture.md` e `abi.md`.
- [x] Levantar limites iniciais de tabelas e protocolo em fontes primárias; auditoria completa permanece na etapa 3.
- [x] Separar observações, inferências e questões abertas em `research.md`.
- [x] Avaliar o NextClient indicado: NitroAPI, precache, tabelas e situação da licença geral.

## 2. Ambiente e fundação

- [x] Inventariar compiladores, ferramentas, binários e estado do Git.
- [x] Configurar CMake e C++26 obrigatório, sem redução silenciosa do padrão.
- [x] Criar uma DLL Windows x86 com contrato de diagnóstico versionado.
- [x] Implementar e testar tipos de domínio e catálogo independente do motor, incluindo índices além de 512.
- [x] Configurar verificações locais e CI com toolchain e actions fixados; os primeiros runs de push e PR passaram.
- [x] Executar build, testes, clang-tidy e clang-format; resultados em `verification.md`.

## 3. Investigação dos motores

- [x] Identificar os artefatos iniciais de CS 1.6 e ReHLDS por arquitetura e SHA-256.
- [x] Executar investigação estática inicial com REA no precache do servidor e do cliente, sem modificar os originais.
- [x] Concluir o censo estático das referências às regiões de modelos, sons, genéricos, eventos, decals e caches dos dois hashes selecionados: 350 sites/78 funções no cliente e 674 sites/119 funções no servidor; CSVs e limites de cobertura em `engineAudit.md` e `audit/profiles.json`.
- [x] Auditar os pares de serialização/leitura, deltas locais, efeitos temporários, carga e reset: dispatcher recuperado, 56 raízes de serviços inspecionadas e restrições registradas em `protocolAudit.md`.
- [x] Registrar fronteiras de integração, ownership e limites por hash, incluindo API ReHLDS, caches de áudio/modelos e handles de HUD; nenhum hook ou ABI interna foi aprovado para uso.
- [x] Calcular os custos das estruturas x86 e o orçamento por bytes do sign-on em `memoryBudget.md` e `protocolAudit.md`; medições reais permanecem na etapa 5.

## 4. Extensão integrada

- [ ] Definir capacidades configuráveis a partir das evidências; não prometer infinitude.
- [ ] Projetar negociação de capacidades antes de transmitir índices estendidos.
- [ ] Especificar conversão zero/um, ownership estável, ciclo de mapa e ABI versionada, resolvendo as restrições de `engineAudit.md` antes dos adaptadores.
- [ ] Projetar deltas, consistência, efeitos, limites por bytes e representação em demos; definir contratos para mensagens de mods e módulos externos do perfil final.
- [ ] Implementar as entradas e o ciclo de vida do plugin Metamod do servidor.
- [ ] Implementar adaptador HLDS com detecção exata de versão e falha segura.
- [ ] Implementar adaptador CS 1.6 com o mesmo contrato e ciclo de vida.
- [ ] Integrar armazenamento e todos os produtores/consumidores de índices.
- [ ] Preservar sessões legadas quando todos os recursos couberem no protocolo original.
- [ ] Recusar pares incompatíveis antes de transmitir estado incompatível.
- [ ] Verificar atualização de mapa, reconexão, demos e mensagens de mods.

## 5. Validação e publicação

- [ ] Criar cenário de integração com recursos reais acima dos limites originais.
- [ ] Validar modelos, sprites, sons, genéricos, eventos e decals ponta a ponta.
- [ ] Medir uso de memória e estabilidade em sessões prolongadas.
- [ ] Documentar instalação e matriz de versões efetivamente suportadas.
- [x] Adotar MIT para o código original e manter fontes de pesquisa fora do produto; incorporação de terceiros exige licença comprovada.
- [x] Configurar remoto GitHub e assinatura SSH específica, com geração e registro autorizados pelo mantenedor.
- [x] Revisar a fundação, criar commit assinado e PR e verificar os primeiros runs de CI.
- [ ] Concluir publicação da extensão funcional: integrar o head revisado, verificar assinatura e CI de `main` e limpar branches com segurança.

## Estado atual

Fundação publicada: núcleo C++26 e DLL x86 de diagnóstico. Auditoria estática
dos dois motores selecionados concluída com parecer contrário a uma ampliação
por constantes/hooks isolados. Relatórios, inventários e escopo de cobertura em
[engineAudit.md](engineAudit.md). Isso não é certificação dinâmica nem prova de
consumidores em módulos externos não selecionados.

A ampliação funcional, a negociação e os adaptadores Metamod/cliente continuam
pendentes. Nenhum arquivo da instalação do jogo ou servidor foi alterado.
Próximo trabalho técnico: desenhar capacidades e contratos da etapa 4 a partir
das restrições encontradas, antes de implementar os adaptadores.

Publicação: repositório público
[`BRUN0R2/ExtendedPrecache`](https://github.com/BRUN0R2/ExtendedPrecache) criado.
A chave `ExtendedPrecache commit signing` foi gerada e registrada após autorização
explícita; assinatura e verificação local foram configuradas por repositório.
O commit de inicialização `8920ac6` foi confirmado como Verified no GitHub.
A fundação foi submetida no [PR #1](https://github.com/BRUN0R2/ExtendedPrecache/pull/1),
com os primeiros runs de [push](https://github.com/BRUN0R2/ExtendedPrecache/actions/runs/40441784269)
e [PR](https://github.com/BRUN0R2/ExtendedPrecache/actions/runs/40441852829) aprovados
para `5a96f33`. O PR foi integrado em `main` no commit Verified `a664f14`, com
[CI de main aprovado](https://github.com/BRUN0R2/ExtendedPrecache/actions/runs/40442390881).
A entrega da extensão funcional continua pendente nas etapas 4 e 5.
