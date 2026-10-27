# ABI de diagnóstico — versão 1

Único símbolo público: `extendedPrecacheQueryInfo`, com linkage C e convenção
`cdecl` no Windows x86. O arquivo `.def` preserva o nome exportado sem decoração.
O cabeçalho é para consumidores C++; a fronteira binária contém somente ponteiros,
inteiros de largura fixa, enums de base `uint8_t` e uma estrutura POD.

O chamador informa a versão desejada como `AbiVersion` (enum de base `uint8_t`),
o tamanho do buffer em bytes e um ponteiro
válido e alinhado para `ExtensionInfo`. A DLL não retém o ponteiro. O chamador é
responsável pela validade da memória; um ponteiro não nulo não pode ser validado
arbitrariamente pela biblioteca.

Ordem de validação: ponteiro nulo, versão de ABI, tamanho do buffer. Na falha,
retorna um `QueryStatus` específico e não escreve no buffer. No sucesso, escreve
somente `sizeof(ExtensionInfo)` bytes; bytes adicionais permanecem intactos.

`QueryStatus` também tem base `uint8_t`. A estrutura de saída tem 28 bytes,
compostos por sete campos `uint32_t`, sem membros enum ou padding entre campos.
Os campos de versão e estágio contêm os valores numéricos dos enums correspondentes.
`AbiVersion::invalid` reserva o valor zero para requisições inválidas; a versão
atualmente aceita é `AbiVersion::v1`.

O resultado contém tamanho da estrutura, versão da ABI, versão do projeto,
`RuntimeStage::foundation` e `engineExtensionActive = 0`.
Esse último campo é um inteiro booleano de 32 bits; zero significa que nenhuma
ampliação de precache está ativa. Não existe API de ativação nesta versão.

Não atravessam essa fronteira exceções, containers STL, strings alocadas, ownership
ou referências C++. A consulta é sem alocação, `noexcept` e não altera o motor.
Ela não é o handshake de rede nem a API do Metamod.

O arquivo de configuração com `schemaVersion=1` e a negociação de rede 1.0 são
contratos separados desta ABI 1. As especificações em `capabilities.md` e
`negotiation.md` não adicionam ativação, estruturas ou exports à DLL atual.

Mudanças incompatíveis de layout, semântica ou convenção exigem nova versão de ABI
e documentação da migração antes da implementação.
