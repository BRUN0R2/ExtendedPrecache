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
- [x] Configurar verificações locais e preparar workflow de CI com toolchain e actions fixados; execução remota pendente.
- [x] Executar build, testes, clang-tidy e clang-format; resultados em `verification.md`.

## 3. Investigação dos motores

- [x] Identificar os artefatos iniciais de CS 1.6 e ReHLDS por arquitetura e SHA-256.
- [x] Executar investigação estática inicial com REA no precache do servidor e do cliente, sem modificar os originais.
- [ ] Mapear todas as referências às tabelas de modelos, sons, genéricos, eventos e decals.
- [ ] Mapear serialização, deltas, mensagens temporárias, carregamento e descarregamento.
- [ ] Registrar pontos de integração e limites demonstrados por versão.
- [ ] Determinar os orçamentos de memória e o custo das tabelas ampliadas.

## 4. Extensão integrada

- [ ] Definir capacidades configuráveis a partir das evidências; não prometer infinitude.
- [ ] Projetar negociação de capacidades antes de transmitir índices estendidos.
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
- [ ] Revisar diff, criar commit assinado e PR, acompanhar CI e revisões.
- [ ] Integrar somente o head revisado, verificar assinatura e CI de `main`, limpar branches com segurança.

## Estado atual

Fundação local implementada e verificada: núcleo C++26 e DLL x86 de diagnóstico.
A ampliação do precache e os adaptadores Metamod/cliente continuam pendentes.
Nenhum arquivo da instalação do jogo ou servidor foi alterado.

Próximo trabalho técnico: completar a auditoria dos consumidores e do parser do
cliente, definir integração e negociação com base nas evidências e só então
implementar a extensão do motor.

Publicação: repositório público
[`BRUN0R2/ExtendedPrecache`](https://github.com/BRUN0R2/ExtendedPrecache) criado.
A chave `ExtendedPrecache commit signing` foi gerada e registrada após autorização
explícita; assinatura e verificação local foram configuradas por repositório.
O commit de inicialização `8920ac6` foi confirmado como Verified no GitHub.
A fundação segue pela branch `codex/projectFoundation`; CI e merge do código
ainda precisam ser concluídos.
