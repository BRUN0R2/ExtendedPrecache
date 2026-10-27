# Orçamento das estruturas de precache

Base: Windows x86 nos perfis de [engineAudit.md](engineAudit.md). Valores são
bytes de estrutura observados estaticamente ou derivados do layout público;
não são medições de RAM, VRAM, tempo de carga ou pico de alocação.

## Custos separados por domínio

| Estrutura | Fórmula em bytes | Capacidade atual / custo | Custo de mais 1.024 slots |
| --- | --- | --- | --- |
| Modelo de rede, servidor: nome + objeto + flags | `9M` | 512 / 4.608 | 9.216 |
| Modelo de rede, cliente: ponteiros | `4M` | 512 / 2.048 | 4.096 |
| Som de rede, cada lado | `4S` | 512 / 2.048 | 4.096 |
| Hash de sons, servidor | `2(2S - 1)` | 1.023 entradas / 2.046 | 4.096 |
| Evento, cada lado | `16E` | 256 / 4.096 | 16.384 |
| Genérico ativo, servidor | `64G` | 4.096 / 262.144 | 65.536 |
| Decal, servidor / cliente | `17D` / `16D` | 512 / 8.704 / 8.192 | 17.408 / 16.384 |
| Recurso, servidor / nó recebido pelo cliente | `136R` | Servidor: 4.096 / 557.056; cliente: conforme lista | 139.264 por lado |
| Consistência, cada lado | `44C` | 512 / 22.528 | 45.056 |
| Cache de modelos + CRC, cada lado | `(392 + 12)K` | 1.024 / 413.696 | 413.696 |
| Descritor de áudio local, cliente | `72A` | 1.024 / 73.728 | 73.728 |
| Registro de sprite HUD, cliente | `12H` | 256 / 3.072 | 12.288 |
| Mapa de nomes de modelos, servidor | `32M + 520` | 1.024 nós de 16 bytes + buckets/ponteiros / 16.904 | 32.768 se a política continuar com dois nós por slot |

M/S/E/G/D/R/C/K/A/H são **quantidades de slots físicos**, não IDs máximos nem
entradas úteis. K inclui modelos locais e de interface além dos modelos de rede;
A inclui sons locais além do precache recebido. A identidade do tipo não implica
que esses parâmetros possam compartilhar a mesma capacidade.

O mapa deriva de 128 buckets de quatro bytes, dois nós por slot de modelo,
cada nó com dois ponteiros de encadeamento, chave ponteiro e valor de quatro
bytes, mais vptr e raiz da lista livre. É cálculo do layout C++ x86 em fonte,
não medida de uma alocação do processo. Contadores, alinhamento entre campos,
flags de construção, nomes alocados, overhead do allocator, índices de busca do
novo domínio e estruturas temporárias não estão incluídos nos subtotais da tabela.

O layout legado do servidor ainda reserva 174.080 bytes para 1.280 recursos,
2.048 bytes para ponteiros genéricos e 32.768 para seus nomes, mesmo quando o
caminho `REHLDS_FIXES` usa estruturas alternativas. A lista de customizações do
cliente também reserva 174.080 bytes. Não subtrair essas reservas do orçamento
até demonstrar que todos os consumidores e o ABI legado foram substituídos.

## Requisitos para configurar capacidades

- Calcular produtos e somas com overflow verificado antes de alocar; validar
  também a faixa representável pelos contratos de API e protocolo.
- Reservar espaço para o slot zero quando necessário, mundo e inline BSP,
  caches locais adicionais e temporários usados na transição entre mapas.
- Manter estabilidade de ponteiros de modelos/samples durante callbacks,
  efeitos, colisão e renderização; uma realocação de vetor pode invalidá-los.
- Separar memória de descritores do conteúdo de MDL/BSP/SPR/WAV, PCM, texturas,
  WADs, cache de arquivo, listas de download e mensagens fragmentadas.
- Definir teto por processo x86 e orçamento de pico de mapa/conexão. Medir os
  valores reais na instalação de teste antes de anunciar uma capacidade.
- Validar o tamanho serializado conforme [protocolAudit.md](protocolAudit.md);
  mais memória para tabelas não aumenta o buffer de sign-on de 65.536 bytes.

Este orçamento encerra o levantamento estático de custos da etapa 3. Medições
de desempenho, memória residente, estabilidade e uma capacidade padrão aprovada
continuam pendentes. Nenhuma fórmula aqui autoriza uma configuração do motor.

O contrato de [capacidades](capabilities.md) expressa entradas úteis; converter
modelos/sons/eventos em slots físicos com N+1 antes de aplicar esta tabela.
A seleção de sessão e os exemplos configuráveis são especificações, com orçamento
local e preparo obrigatório, sem alegação de medição ou ativação na DLL atual.
O [contrato de ownership](storageOwnership.md) acrescenta a política de arenas
estáveis, pins, orçamento compartilhado e pico durante sobreposição de gerações.
Não modifica os custos estáticos desta tabela nem permite realocação sob leitores.
