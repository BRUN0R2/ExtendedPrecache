# Registro de investigação

Investigação inicial: 26/10/2026. Auditoria ampliada: 27/10/2026, America/Fortaleza.

O relatório vigente é [engineAudit.md](engineAudit.md), com
[protocolo](protocolAudit.md), [memória](memoryBudget.md),
[proveniência](auditMethod.md) e inventários por hash. O registro abaixo preserva
as primeiras evidências; pendências daquela exploração foram atualizadas para
refletir a auditoria estática concluída, sem declarar suporte em execução.

## Método e limites

Análise estática, sem executar nem modificar os motores. Fontes públicas foram
consultadas como referência; o REA/Ghidra foi usado para os artefatos distribuídos.
Pseudocódigo recuperado não é código-fonte original nem observação em execução.
Endereços abaixo são endereços de análise do binário identificado, não offsets de
arquivo ou assinaturas universais. Não podem ser usados diretamente como patches.

REA: 6.7.0; Ghidra: 12.1.4. Resultados completos e snapshots de evidências ficam em
`.local/research/`, fora do Git. Snapshots não são uma distribuição do motor nem
uma promessa de persistência do banco temporário do Ghidra.

## Artefatos selecionados

| Identificação | Arquivo | SHA-256 | Estado |
| --- | --- | --- | --- |
| Servidor indicado pelo mantenedor | `REHLDS-Rezombie/swds.dll` | `f6c552ca46e933c6d02ee4fb888cc99bbe8f8aab04a4afbe9ff45d74f896cd02` | PE x86, analisado estaticamente |
| Cliente Steam instalado | `Half-Life/hw.dll` | `9ba9a2db5e07598fd59afa35507a98c86162e4e15b3835177b78c11842cd2295` | PE x86, auditoria estática; suporte não declarado |
| Servidor Valve instalado | `Half-Life/swds.dll` | `64b6872f9c2875ea7da88eb694a34e6e46f2aac7cb35fb436744f462611c03ae` | Inventariado; ainda não analisado |

A DLL do servidor contém strings que informam `3.15.0.896-dev` e commit
`1963001`. Isso é uma autodeclaração observada nos bytes, não autenticação da
origem. O commit público correspondente é
[`196300131858d1e087267fb2fb60cb935f8f8873`](https://github.com/rehlds/ReHLDS/commit/196300131858d1e087267fb2fb60cb935f8f8873).

## Fontes de referência

- ReHLDS, revisão
  [`196300131858d1e087267fb2fb60cb935f8f8873`](https://github.com/rehlds/ReHLDS/tree/196300131858d1e087267fb2fb60cb935f8f8873),
  correspondente ao commit autodeclarado pelo servidor. Substitui como referência
  principal a revisão `550f2d6` usada no levantamento inicial.
  Consultados: `rehlds/common/qlimits.h`, `rehlds/public/rehlds/custom.h`,
  `rehlds/engine/server.h`, `pr_cmds.cpp` e `sv_main.cpp`.
- NextClient, revisão
  [`4391300983eff82c752eae179c070abed7b268de`](https://github.com/CS-NextClient/NextClient/tree/4391300983eff82c752eae179c070abed7b268de).
  Consultados: declarações `goldsrc_abi`, provedores da NitroAPI, rotinas
  `CL_PrecacheResources` e recursos privados.
- [Arquitetura do Metamod](https://metamod-p.sourceforge.net/doc/html/about.html)
  e [requisitos do Metamod-R](https://rehlds.dev/docs/metamod-r/install/).
- [Padrão C++ no CMake](https://cmake.org/cmake/help/v4.4/prop_tgt/CXX_STANDARD.html)
  e [suporte de linguagem do Clang](https://clang.llvm.org/cxx_status.html).

As fontes de referência são checkouts locais de pesquisa. Nenhum código ou SDK
desses projetos foi incorporado ao ExtendedPrecache.

## Limites observados em fonte

| Camada na revisão ReHLDS consultada | Valor | Consequência |
| --- | --- | --- |
| Tabelas de modelos e sons | 512 slots cada | Não equivalem a 512 recursos úteis se houver índices reservados |
| Genéricos legados e decals base | 512 slots cada | Genéricos ativos em `REHLDS_FIXES` usam 4096 nomes de 64 bytes |
| Eventos | 256 slots | Não inferir largura de todas as mensagens a partir desse tamanho |
| Lista legada em `custom.h` | 1280 slots | Não confundir com o armazenamento alternativo de `REHLDS_FIXES` |
| `RESOURCE_INDEX_BITS` | 12 bits | Valor máximo codificável: 4095, inclusive para a contagem enviada |
| Som dinâmico em `SV_BuildSoundMsg` | 8 ou 16 bits | A largura da mensagem difere da tabela de sons |
| Nome de caminho `MAX_QPATH` | 64 bytes no buffer | Um nome terminado em NUL precisa reservar espaço para o terminador |

`REHLDS_FIXES` possui armazenamento alternativo com capacidade de 4096 recursos.
O escritor satura a contagem em 4095, mas emite os 4096 registros se o contador
atingir essa capacidade. O defeito de enquadramento foi confirmado no binário.
A extensão precisa validar armazenamento, maior índice, contagem e tamanho total
da mensagem separadamente; ver `protocolAudit.md`.

O enum de recursos também declara `t_skin` e `t_world`; o próprio cabeçalho
descreve `t_world` como um tipo artificial associado a modelos. Ainda não há
evidência para prometer uma ampliação independente dessas categorias.

No arquivo `cstrike/delta.lst` do servidor indicado, foram observados campos de
modelos e armas com 10 bits e um campo de modelo com 16 bits em outro bloco.
Esse arquivo é configuração local: não generalizar para outros mods ou builds.

## Evidências REA do servidor

| Evidência | Observação | Cobertura |
| --- | --- | --- |
| `ev_015bdf51d17f42f151ca57dd9e62428007d617881dcf88b286624b0af6c8efcc` | Strings de versão, commit, API ReHLDS e erros de precache | Inventário de strings definidas pelo Ghidra |
| `ev_a4b7fc3ecf6ea788287c202e6a05819976217744670c11dff079ae166a3d3d79` | Referências ao erro de modelos | Somente o endereço exato da string |
| `ev_ace6f87860b688e7da3b5d9c6ae162e3fdbf48b1d56991a3e9d7e61c1f6496e2` | Referências ao erro de sons | Somente o endereço exato da string |
| `ev_b1d65bf4e92830b8afe49aae790ce2bfabeef26d4311ab7f8bedbbecd4cb1589` | Função em `0x499ea30` compara índices até `0x1ff` e reporta `0x200` no overflow de modelos | Pseudocódigo estático; função identificada por referências e strings |
| `ev_260a71f9f86b973347e2e3ee277d97c4277dcec973f997739392403b8ce9cdf2` | Função em `0x499d950` percorre tabela de sons e reporta `0x200` no overflow | Pseudocódigo estático; mesmo limite confirmado nessa rotina |
| `ev_8e8373df05a40c0064b2f78ccdba75d3f4a26088b7ecff441843fb380b0a68ea` | 14 referências à base da tabela de nomes de modelos | Não inclui necessariamente referências a endereços internos ou indiretos |
| `ev_2ceecb1f065c42ce018713dadfffb1a6136133610627e494e313f08589f8ce2f` | 23 referências à base da tabela de objetos de modelos | Mesma limitação de endereço exato |
| `ev_aac0808c34d8e26a533d050cdd23be9494dfa10e7aa6b5eca4f8cfe6e87636b3` | 11 referências à base da tabela de nomes de sons | Mesma limitação de endereço exato |

A busca exata pela base candidata de flags de modelos não retornou referências
(`ev_13395012f0f2298350e2d8d69ad247c6f962fc778745dbff245a663892df7413`).
O pseudocódigo mostra um acesso indexado à região. O resultado vazio não significa
ausência de consumidores: offsets embutidos e formas indiretas exigem outra busca.

Conclusão sustentada: mudar apenas a condição de overflow deixaria outros acessos
às tabelas sem ampliação demonstrada. Não há patch aprovado por esta investigação.

## Evidências REA do cliente

As strings de diagnóstico de precache foram localizadas em
`ev_7f47aefdff81f56fe26a73dfd2ed2cd4fcda80ea471d696b44d04c9edcd738b7`.
A referência à string inicial de `CL_PrecacheResources` levou à função
`0x101a44c0` (`ev_0ec87bf811f91909852126e82daa4a2ab6a57a4a1999f883d8a6c160d3083018`
e `ev_c23d6292d62975a5d8fba6b7a5e0338f438f40676274134634e1bc021e1fc21f`).

Seu pseudocódigo, em
`ev_353824df703ff84545731cb39c84165aa5cbd1ccb0ac98a7187f3350ae0a61c2`,
limita a contagem de modelos a `0x200`, mas escreve o objeto usando o índice do
recurso na base de tabela recuperada. Também há escritas indexadas para sons e
eventos. Portanto, ampliar apenas a contagem não demonstra que o armazenamento
ou os consumidores estejam preparados. A análise não provou a validação de
índices em todos os caminhos anteriores a essa rotina.

A busca por referências ao endereço exato da string inicial de `CL_ParseResourceList`
não retornou resultados
(`ev_09bac24004ad77e5775acceaf0e227fb283fa215a643e9fa358c05e65df02be7`).
Isso não prova ausência da função. Na auditoria ampliada, uma busca binária
positiva recuperou a tabela de serviços e o wrapper `0x101aa270`; seu corpo
`0x101a7690` foi decompilado em projeto Ghidra próprio, junto com os demais
handlers e consumidores. As sessões REA foram exportadas e encerradas; nenhum
motor foi executado. A proveniência completa está em `auditMethod.md`.

## Avaliação do NextClient

A NitroAPI expõe pontos como `CL_ParseResourceList`, `CL_PrecacheResources` e
`S_PrecacheSound`, e seleciona provedores de endereços por build. Isso fornece
referências de arquitetura e locais para investigar o lado cliente.

Na revisão consultada, `goldsrc_abi` continua declarando 512 ponteiros para modelos
e sons e 256 eventos em `client_state_t`; `CL_PrecacheResources` escreve nessas
tabelas. O precache separado de recursos privados não demonstra remoção desses limites.

O provedor examinado seleciona o build 8684 com base no tamanho do arquivo.
Não reutilizar essa identificação como prova suficiente: o ExtendedPrecache exige
identidade por hash e validação dos pontos de integração.

Não foi localizada uma licença geral do NextClient nos locais examinados; a
consulta de licença do repositório também retornou 404. Licenças de dependências
não resolvem essa questão. Qualquer incorporação fica pendente de comprovação dos
termos aplicáveis. A implementação atual é original e não depende da NitroAPI.

## Questões abertas

- Implementar os contratos de capacidades/negociação já especificados, mantendo
  as fronteiras externas e lacunas de análise declaradas, sem alegar prova universal.
- Definir e validar em execução o primeiro par suportado; hash auditado não é suporte.
- Verificar HLDS original separadamente do ReHLDS.
- Determinar carregamento do cliente, ABI do Metamod e disponibilidade dos hooks.
- Validar em execução serialização, deltas, efeitos, áudio, eventos, decals,
  demos e troca de mapa; a auditoria estática dos codecs está documentada.
- Medir o orçamento real de memória e validar as capacidades/negociação definidas
  a partir dos custos estáticos e limites por bytes levantados.
- Comprovar a integração em uma instalação de teste isolada com recursos reais.

## Fontes que condicionam o bootstrap

Na revisão ReHLDS fixada, `SV_SendServerinfo_internal` emite as descrições delta
antes de `SV_New_f` chamar `SV_SendUserReg` e a GameDLL `ClientConnect`.
`SV_ParseStringCommand` pode encaminhar texto para a GameDLL ou command buffer;
`Netchan_Clear` remove fragments e estado confiável pendente. Por isso, o desenho
da negociação exige uma barreira anterior ao sign-on e parsers dedicados, em vez
de depender de um callback tardio. Referências em [negotiation.md](negotiation.md).
São observações em fonte, não aprovação de hooks ou execução nesses binários.
