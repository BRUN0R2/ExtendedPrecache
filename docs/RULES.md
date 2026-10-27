# ExtendedPrecache — regras do projeto

## Objetivo e honestidade técnica

Construir uma extensão de código aberto para ampliar de forma coordenada o
precache do CS 1.6 e do HLDS. Remover limites fixos não significa memória infinita:
toda capacidade precisa respeitar memória, representação dos índices e protocolo.

Não anunciar suporte por compilar uma DLL, aumentar uma constante ou passar um teste
isolado. Suporte exige cliente e servidor reais, versões identificadas e validação
ponta a ponta. Uma fundação sem integração deve se identificar explicitamente assim.

## Tecnologia e organização

- Usar C++26 e ferramentas estáveis que aceitem esse modo. Verificar os recursos
  efetivamente implementados pelo compilador; não recorrer a um padrão inferior.
- Usar CMake por alvo, sem dependências externas quando a biblioteca padrão resolver.
- Pedir autorização antes de upgrades principais, troca de framework ou mudança de stack.
- A primeira plataforma de DLL é Windows x86, compatível com a arquitetura dos
  motores-alvo. Um núcleo portável não equivale a suporte a outro motor ou sistema.
- Usar camelCase em arquivos, pastas, funções, variáveis e constantes; PascalCase
  somente em classes, tipos e componentes. Não criar nomes com sublinhados.
- Preservar nomes obrigatórios de ferramentas, contratos externos e os arquivos
  `TODO.md` e `RULES.md` solicitados pelo mantenedor. Não renomear código alheio.
- Manter módulos pequenos, com uma responsabilidade e fronteiras explícitas entre
  domínio, protocolo, adaptadores do motor e plataforma.
- Aplicar abstrações somente quando houver necessidade concreta; evitar hierarquias
  especulativas, estado global mutável e números ou strings mágicos.

## Contratos e implementação

- Usar tipos fortes para categorias, índices, capacidades, versões e erros.
- Validar entradas na fronteira, preservar invariantes e tornar falhas explícitas.
- Não expor STL, exceções, ownership C++ ou layouts internos pela ABI da DLL.
- Versionar contratos públicos; documentar quebras antes de implementá-las.
- Não fazer trabalho complexo sob o loader lock, em `DllMain`.
- Não ativar integração em binários desconhecidos, em correspondências ambíguas ou
  em pares cliente/servidor incompatíveis.
- Não ampliar um índice sem auditar todas as tabelas, comparações, mensagens,
  deltas, serializers, deserializers e consumidores correspondentes.
- Preservar deduplicação, validade dos índices e descarregamento por mapa.
- Toda capacidade deve ser configurável e validada contra os limites comprovados
  da implementação e da sessão; não vender um valor fixo maior como ilimitado.

## Investigação e correções

- Reproduzir o problema e investigar código, logs e evidências antes de corrigir.
- Corrigir a causa raiz. Não mascarar falhas, desativar checks, introduzir sleeps,
  retries arbitrários ou `catch` genérico para forçar sucesso.
- Se um contorno temporário for inevitável, identificá-lo e registrar um `TODO`
  que explique a causa e a solução correta. Preferir registrar um impedimento real.
- REA serve à análise de artefatos distribuídos; analisar fontes disponíveis com
  ferramentas normais. Preservar SHA-256, versão, arquitetura e IDs de evidência.
- Distinguir observação estática, observação em execução, inferência e desconhecido.
- Investigar sequencialmente no agente atual, conforme as instruções da skill REA.
- Não publicar executáveis proprietários, bases de análise, dumps ou trechos de
  código de terceiros sem licença compatível. Registrar referências e proveniência.

## Documentação e acompanhamento

- Atualizar documentação junto com cada mudança: README, build, instalação,
  contratos, arquitetura, configuração e changelog quando existir.
- Comentários explicam motivos, não repetem a implementação.
- Manter [TODO.md](TODO.md) com `[ ]` e `[x]`. Nunca marcar trabalho parcial,
  bloqueado, hipotético ou não testado como concluído.
- Registrar verificações executadas, resultados, limitações e próximo passo concreto.

## Git, revisão e publicação

- Buscar o remoto primeiro e criar branches `codex/` a partir de `main` atualizado
  quando houver repositório e remoto. Não inventar histórico em uma pasta vazia.
- Commits, PRs, correções de CI, squash merge e limpeza estão autorizados dentro
  do escopo do projeto; não repetir pedidos de autorização já concedidos.
- Antes do primeiro commit, verificar assinatura SSH por repositório:
  `gpg.format=ssh`, `user.signingkey`, `commit.gpgsign=true`, `tag.gpgsign=true`
  e `gpg.ssh.allowedSignersFile` contendo a chave registrada no GitHub.
- A chave deve ser específica: `ExtendedPrecache commit signing`. Pedir antes
  de gerar ou registrar uma nova chave. Nunca criar commit sem assinatura.
- Escrever mensagens de commit em inglês, imperativas, específicas e coesas.
- Revisar o staged diff e executar checks antes de enviar; não incluir segredos,
  arquivos temporários, binários de análise ou mudanças não relacionadas.
- Limpar histórico ainda não publicado e preservar assinaturas. Não reescrever
  histórico compartilhado sem autorização; quando autorizado, usar `--force-with-lease`.
- Verificar `git log --format='%h %G?' <base>..HEAD`: todo commit enviado deve
  apresentar `G` e aparecer como Verified no GitHub. Respeitar DCO e aprovações exigidas.
- Acompanhar CI do commit exato após cada push, corrigir causas e repetir os checks.
- Antes do merge, revisar diff completo e head atualizado, resolver achados e
  respeitar proteção de branches. Nunca contornar proteções.
- Após o merge, verificar conteúdo integrado, assinatura e CI de `main`.
- Só limpar branches após provar integração e verificar PRs, worktrees e trabalho
  ativo. Exigir o head esperado ao remover referência remota; preservar trabalho alheio.

## Verificação

- Executar build, testes relevantes, linter e verificação de tipos disponíveis.
- Adicionar testes de comportamento e regressão; não espelhar detalhes internos.
- Testar fronteiras de índices, duplicatas, entradas inválidas e pares incompatíveis.
- Não repetir falhas idênticas sem investigar nem enfraquecer checks para obter verde.
- Sem binários-alvo, credenciais ou validação real, documentar o bloqueio e manter
  a etapa incompleta. Não substituir evidência por promessa.
