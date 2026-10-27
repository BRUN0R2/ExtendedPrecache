# Método e proveniência da auditoria

REA 6.7.0, Ghidra 12.1.4, análise x86 little endian com compiler spec Windows.
Os hashes, métricas e regiões estão em [audit/profiles.json](audit/profiles.json).
As operações foram estáticas, em cópias locais verificadas dos dois executáveis.
Nenhum processo do jogo/servidor foi iniciado e nenhum byte original foi alterado.

## Cobertura e recuperação de funções

1. Inventariar hashes, PE, strings e fontes fixadas. Correlacionar fonte e
   executável por comportamento observado, sem tratar a versão textual como assinatura.
2. Consultar strings, referências exatas e pseudocódigo via REA. Resultados vazios
   de xrefs exatos não encerram a investigação.
3. Em projetos Ghidra próprios sob `out/auditProjects`, exportar todas as funções,
   instruções definidas e referências. Os scripts originais estão em
   [ExportBinaryAudit.java](../tools/audit/ExportBinaryAudit.java) e
   [InspectAuditFunctions.java](../tools/audit/InspectAuditFunctions.java).
   Java é a interface de scripting do Ghidra, não uma mudança da stack C++26.
   Os nomes de classe/arquivo são impostos por essa interface.
4. Comparar todos os operandos escalares/endereços e destinos de referências
   com `[base, base + tamanho]` de cada região. Normalizar o valor hexadecimal
   antes de deduplicar região/endereço/valor. Separar o interior do one-past-end.
5. Examinar cada janela de quatro bytes do PE original, mapear offset para VA e
   cruzar com as relocations `HIGHLOW` e o inventário de instruções. Candidatos
   não relocados são preservados como candidatos, não chamados de consumidores.
6. Resolver o dispatcher por uma referência positiva aos bytes da tabela de
   serviços, recuperar suas 56 raízes únicas e inspecionar os parsers. Recuperar
   comandos de diagnóstico por referências positivas de registro. Não atribuir
   funções por proximidade ou reutilizar offsets do NextClient.
7. Inspecionar consumidores, auxiliares de índice/cache, resets de estruturas,
   emissores e leitores correspondentes, deltas locais e fronteiras de ABI.
   Registrar os resultados factuais sem publicar bytes, dumps ou código recuperado.

As raízes recuperadas alteram **metadados do banco de análise**, não os motores.
O REA desta sessão não disponibilizava essa operação; foi usado um projeto
headless separado, sem modificar o banco privado do REA. Achados desse projeto
não recebem IDs de evidência REA inventados. Requests e resultados são separados;
`existedBefore` e falhas de decompilação são registrados explicitamente.

## Métricas e limites da inferência

| Métrica após recuperar as raízes declaradas | Cliente | ReHLDS |
| --- | --- | --- |
| Instruções definidas | 742.074 | 240.999 |
| Funções definidas | 6.920 | 4.187 |
| Referências definidas | 287.340 | 104.505 |
| Bytes executáveis inicializados | 2.819.072 | 793.088 |
| Bytes reconhecidos como instrução | 2.657.735 | 759.134 |
| Bytes reconhecidos como dados na seção executável | 7.154 | 3.162 |
| Bytes executáveis ainda não classificados | 154.183 | 30.792 |
| Instruções sem função proprietária no banco | 49.028 | 7.548 |
| Fluxos computados reconhecidos | 11.643 | 2.410 |

Bytes não classificados podem ser código, padding ou dados. Essas métricas
impedem a alegação de desassemblagem global perfeita. O censo fechado é o das
regiões e contratos declarados; ponteiros calculados dinamicamente, memória
externa, callbacks e módulos não selecionados não viram prova de ausência.

Há quatro sites sem proprietário no censo do cliente: checks de `max_edicts`
nos corpos de efeitos em `0x101ae337`, `0x101ae888`, `0x101aea9f`, `0x101afed4`.
No servidor, dez são checks de `active`/`paused`, e um (`0x0497eacb`) é o campo
vizinho à raiz de WAD, usado pela estatística de deltas. São referências de
contexto / one-past-end, não acessos sem proprietário aos arrays de recursos.

O scan bruto encontrou 427 / 1.907 candidatos região/janela, dos quais 274 / 442
eram relocados. Cinco / quatro combinações relocadas não estavam dentro de
instruções reconhecidas: são **quatro / três endereços únicos**, com duplicação
quando o endereço também é o fim da região anterior. Estão nos fall-throughs
após chamadas tratadas como não retornantes pelo Ghidra, nas rotinas de preparo,
sign-on, `S_FindName` e consistência; não são novos aliases em uma tabela de
dados. Os VAs exatos constam do perfil. Não foram alteradas as flags `noReturn`
para forçar uma aparência de cobertura completa.

O censo da fonte é lexical e inclui cabeçalhos, engine, API pública e
implementação ReHLDS na revisão fixada. É um índice para revisão, não uma prova
automática de fluxo de dados. Helpers que recebem `model_t*`, `sfx_t*` ou
`resource_t*` e o reset de estruturas inteiras foram tratados como fronteiras
de ownership; consumidores externos estão explicitados em `engineAudit.md`.

## Evidências REA preservadas

| ID | Observação |
| --- | --- |
| `ev_1ebbd73b682e82effdd36bf512753960410763918ae6e5d9f3dc4bff26b9c740` | Chamadores de `CL_PrecacheResources` no cliente |
| `ev_2976873944a1ca8f8300efff92ddd5361099a4e4b6aab1e50d2a5f5f800a9484` | Busca binária positiva da referência ao nome `svc_resourcelist`, após xrefs exatos vazios |
| `ev_ea23ff8c43f33e4b19d74b4db0a61898348b3e7d4f98af9e893ab1cae0ecb354` | Linha da tabela com ID 43 e ponteiro do handler |
| `ev_62a8494f820433dee5605806698588606e32a0072a6ccad723bf5ff0c15830b6` | Leitura da tabela de serviços do cliente |
| `ev_b77c3c3dea324bc4ed31648a137d7a107c95a14e36fc612e4d44563ac14857cd` | Referência positiva ao diagnóstico do parser de recursos |
| `ev_8df613369df62d66d4c11bfb3b15fc4fdc582ffdcf767173f54399cb356619ef` | Caminho de download/finalização no cliente |
| `ev_3d2527d8d42aa96123b69df1c99cbedff161a8212d727530b0a40a2d5562bc82` | Genéricos ativos: capacidade 4.096, nomes e semântica zero-based |
| `ev_05e4d7ab81e0b3b7baa6591d892ccf67c82c3dae8e21661bfc796432ce25acab` | Cache de 1.024 modelos e indexação CRC no servidor |
| `ev_03d49ddefa22fb6a3aff7c20bed94fb4bfdc4791fff9be33195c52f741330d89` | Serialização nativa de recursos, saturação de contagem/índices e laço original |

Bundles locais: `.local/research/clientAuditEvidence.json` (14 registros) e
`serverAuditEvidence.json` (27). Snapshots foram exportados e sessões encerradas.
Os arquivos podem conter material proprietário e permanecem fora do Git.
Os IDs identificam observações retidas, não links públicos nem cobertura de runtime.

## Reprodução local

Fornecer legalmente cópias dos executáveis com os hashes exatos. Não procurar
equivalência por nome ou tamanho. Usar Ghidra 12.1.4 e um JDK compatível; esta
execução usou JDK 27.0.1. Criar um projeto em caminho aceito pelo Ghidra, sem
componentes iniciados por ponto. No Windows, importar as cópias por caminhos
simples para evitar a interpretação de parênteses pelo launcher `.bat`.

Exemplo conceitual, com caminhos substituídos pelo operador:

```powershell
analyzeHeadless <projects> clientAudit -import <input>/hw.dll -scriptPath tools/audit `
  -postScript InspectAuditFunctions.java docs/audit/clientServiceRoots.json <privateSvcOutput> `
  -postScript InspectAuditFunctions.java docs/audit/clientConsumerRoots.json <privateConsumerOutput> `
  -postScript ExportBinaryAudit.java <privateInventory> <clientSha256>
```

Repetir com `serverAudit`, `swds.dll`, `serverConsumerRoots.json` e o hash do
servidor. Cada saída deve ser nova; os scripts recusam sobrescrita e hash
incompatível. É necessário manter os outputs de pseudocódigo e inventário fora
do Git. Versões/configurações diferentes de autoanálise podem mudar proprietários
e contagens; comparar os VAs e a cobertura, sem confundir isso com mudança do motor.
Conferir os marcadores `AUDIT_FUNCTION_INSPECTION_COMPLETE` / `AUDIT_EXPORT_COMPLETE`,
os arquivos de resultado e os diagnósticos do script; o exit code do launcher
headless isoladamente não comprova sucesso de todos os scripts.

Os scripts foram compilados pelo Ghidra, executados nos dois hashes e também
validados com `javac -Xlint:all -Werror`. A checagem avulsa usa diretórios das
classes distribuídas pelo Ghidra: manifests de alguns JARs referenciam bibliotecas
antigas ausentes e causam warnings de classpath, sem relação com os scripts.
Esses manifests não foram usados como classpath da checagem, os JARs originais
não foram alterados e nenhuma regra de lint foi desativada.
