# Verificação dos contratos de integração — 27/10/2026

Escopo: especificação de índices, ownership, ciclo de mapa e ABI 2.0.
Não implementa adaptadores, storage nativo, novo export ou codecs de dados.
Os perfis de motor e os exemplos de aplicação/dados continuam indisponíveis.

## Restrições da auditoria e fechamento no desenho

| Restrição de `engineAudit.md` | Decisão especificada | Prova ainda exigida no motor |
| --- | --- | --- |
| Genérico/decal zero-based; domínio one-based | Conversões parciais tipadas, erros separados de zero em `indexMapping.md` | Produtores/consumidores usando a mesma tradução |
| Zero de generic também pode ser alias sem inserção | `RegisteredGeneric` e `AssetAlias` distintos; preservar contagens/IDs reais | Identificar corretamente ambos os ramos e sua ordem de registro |
| Ordinal global difere de `nIndex` | Ordem lógica fixa, relação inversa e projeção legada explícita | Escritor/leitor/consistência sem troca de namespace |
| Mundo, inline BSP e limite de `localmodels[][5]` | Mundo em 1, inline por registro real, nomes dimensionados | Registro/load de BSP e de nomes com mais dígitos |
| `model_t*`/`sfx_t*` escapam de tabelas | Arenas estáveis, pins por consumidor, reuso só com quiescência | Retenção em colisão, estúdio, áudio, efeitos e módulos externos |
| CRC deriva slot por diferença de ponteiros | Cache/CRC contíguos e pareados; validar pertencimento antes de derivar | Todos os helpers CRC e suas calling conventions |
| `modelsMap`, hash `short` e flag de construção | Lookup por geração/IDs completos, orçamento e inserção conferidos | Nenhum consumidor conservando a representação antiga |
| Eventos densos e allocators distintos | Snapshot próprio, views com owner correto, liberação única sem buracos | Cleanup ampliado e ordem exata de nome/script |
| HUD usa `slot+1` sem geração no inteiro | Namespace separado; não reciclar sob consumidores antigos | Encerramentos, texturas, handles e ClientDLL retidos |
| Cliente zera estado/hunk antes de validar serverinfo | Owner durável, preflight e reset inicial autorizado uma vez | Barreira e binding após reset nos hashes selecionados |
| Mapa compartilhado no servidor | `PreparedSession` solta lease da conexão, não o owner comum | Reconnect/falha de um cliente sem invalidar os outros |
| Getters ReHLDS expõem limites/layouts antigos | ABI 2 copia metadados, mantém APIs antigas e recusa consumidores desconhecidos | Perfis de GameDLL/ClientDLL/SDK/Metamod e integrações externas |
| Chamadas internas têm parâmetros em registradores | Novo C ABI não presume protótipos de hooks | Assinatura, ordenação e ownership exatos de cada binding |
| Troca de mapa/unload com usos vivos | Fechar admissão, drenar, retirar e liberar na ordem dos owners | Contadores/contratos de lifetime e unload recusado como busy |
| Memória x86 e simultaneidade de sessões | Coordenador de pico local, compartilhamento sem dupla contagem, sobreposição orçada | Reserva, stress e medidas reais de metadados/assets/VRAM |

São decisões normativas que resolvem a ambiguidade do contrato, não implementação
ou certificação dinâmica das linhas da auditoria. As provas acima permanecem
requisitos para ativar um perfil; não foram substituídas pelos testes matemáticos.

## Artefatos e oráculos

- [indexMapping.md](indexMapping.md): registro de categorias, equações, aliases,
  ordinal lógico, caches e HUD.
- [storageOwnership.md](storageOwnership.md): owners, arenas, pins, allocators,
  compartilhamento, quiescência e orçamento.
- [mapLifecycle.md](mapLifecycle.md): fases, transições, server/client, reset,
  reconexão, retirada e unload.
- [integrationAbi.md](integrationAbi.md) e
  [header C de especificação](contracts/integrationAbiV2.h): versão, funções,
  layouts, validação, buffers e migração futura.
- [integrationVectors.json](contracts/integrationVectors.json): valores fixos e
  requisitos nativos, sem regeneração de expectativas pelo código sob teste.

`integrationContractTests` executa somente as equações de referência sobre esses
valores, confronta os enums atuais com o registro proposto, verifica `sizeof`,
`alignof` e todos os offsets compilados contra a tabela fixa e confere o inventário
de 21 requisitos nativos pendentes. Não é um segundo storage, ABI funcional ou
simulação afirmada como validação de callbacks. `integrationAbiCProbe.c` compila
o header como consumidor C17 separado do produto C++26 e verifica também que
o packing do chamador foi restaurado. A DLL continua sem o export planejado.

## Reprodução

No ambiente Windows x86 usado pelo projeto:

```powershell
cmake --preset windowsX86 -DextendedPrecacheEnableAnalysis=ON
cmake --build --preset windowsX86
clang-cl --target=i686-pc-windows-msvc /TC /std:c17 /W4 /WX /c /I docs/contracts tests/integrationAbiCProbe.c /Foout/windowsX86/integrationAbiCProbe.obj
ctest --preset windowsX86 --verbose
```

A CI executa esses passos e formata/verifica também `.h`/`.c` dos artefatos.
O núcleo continua C++26; C17 é apenas a linguagem do consumidor de ABI de teste.
Em x64, o teste confirma os PODs sem ponteiros e que a tabela não tem o layout
x86, sem declarar x64 como ABI suportada. Sem consumidor/allocator/hook nativo
real, não há prova de estabilidade de endereço ou de retirada por esta compilação.

Os arquivos antigos de `docs/protocol`, a ABI diagnóstica e o codec/state machine
não mudam. Uma futura implementação deve consumir estes vetores além dos testes
de sessão já existentes, acrescentando as verificações nativas ainda pendentes.

## Resultados locais

- Clang 23.1.4 / C++26 / Windows x86: build com clang-tidy e warnings como erros,
  seis executáveis CTest aprovados. Foram 120 checks de especificação, 3.130 de
  catálogo, 112 de configuração, 1.733 de codec/cripto, 2.349 de negociação e
  16 da DLL; total **7.460**, zero falhas.
- Clang, consumidor C17 com target explícito `i686-pc-windows-msvc`, `/W4 /WX`:
  compilação e asserts aprovados; Machine COFF `0x014c` conferido no objeto.
- GCC/GCC C 16.2.0 / UCRT64: cinco executáveis CTest aprovados, **7.444** checks;
  probe C17 compilada com warnings como erros. Isso não aprova a ABI para x64.
- clang-format em `.hpp/.cpp/.h/.c` e verificação de whitespace: aprovados.
- 15 arquivos JSON e 116 links locais: válidos.
- Nenhum arquivo de produção em `include/`/`src/` ou vetor anterior foi alterado.
  A consulta diagnóstica v1 mantém seus 28 bytes; o teste adicional confirma
  ausência de `extendedPrecacheGetIntegrationApi` na DLL produzida.
