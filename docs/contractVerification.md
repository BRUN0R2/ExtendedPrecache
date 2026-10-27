# Verificação dos contratos — 27/10/2026

Escopo: definições de [capacidades](capabilities.md),
[negociação 1.0](negotiation.md), schema, exemplos e corpus sintético. Não há
leitor de configuração, codec de negociação C++ ou adaptador implementado nesta
mudança. O resultado abaixo verifica a especificação, não um motor em execução.

## Resultados offline

| Verificação | Resultado |
| --- | --- |
| Metaschema JSON Schema 2020-12 | Válido em `jsonschema` 4.26.0 |
| Configuração: estrutura e dependências de referência | 33 casos conferidos, incluindo quatro exemplos completos, erros e categoria desabilitada |
| Léxico JSON antes do schema | 6 casos: duplicata, float, expoente, NaN, BOM e JSON inválido recusados |
| Frame binário | 19 casos de framing conferidos, com magic, major/minor, tamanho, nonce, sequência, IDs, reservados e rejeição |
| Conversa de referência | 26 frames, campos little endian, páginas, limites e ordem conferidos |
| Transcript | SHA-256 da oferta, cliente, seleção e ecos finais recalculados independentemente |
| Admissão por dimensão | 27 casos: mínimo do par, bordas dos 12 campos e snapshots contraditórios |
| Perfis fictícios | Ambos mantidos `availability=unavailable`; nenhuma ativação autorizada |

Os bytes dos frames foram montados com Node.js e conferidos separadamente com
`struct`/`hashlib` do Python 3.14.8. O validador JSON Schema foi instalado apenas
em `.local/contractValidation`; não é dependência da DLL, do CMake ou do runtime.
O projeto não mudou de linguagem, stack ou ABI. O corpus é a entrada pública
para os próximos testes do codec C++26.

Arquivos de referência:

- [Casos de configuração](protocol/configurationVectors.json).
- [Admissão e condições legadas](protocol/capacityVectors.json).
- [Frames, hashes e cenários de ordenação](protocol/negotiationVectors.json).

Os 12 cenários de estado e 14 cenários de compatibilidade adicionais do corpus
são critérios de aceitação para a implementação. Não foram executados por um
state machine C++ ou pelo motor; a ordem normal e a aritmética foram conferidas
offline. Não converter essa distinção em uma afirmação de integração testada.

## Valores de referência

| Conversa normal | Valor |
| --- | --- |
| Servidor | 16 frames / 952 bytes lógicos |
| Cliente | 10 frames / 592 bytes lógicos |
| Soma lógica | 1.544 bytes |
| Servidor, com registro SVC 39 e framing de user messages | 1.003 bytes |
| Cliente, com `epc1 `, hex, clc e NUL | 1.254 bytes |

Não incluem headers/retransmissões do netchan nem os seis bytes de indicação
em `userinfo`. A política de 4.096 bytes é para os frames lógicos únicos da
negociação, não uma medida de tráfego total da conexão.

As estimativas base de metadados/preparação dos exemplos são:

| Exemplo | Bytes de referência |
| --- | --- |
| `serverLegacy.json` | 1.568.262 |
| `clientLegacy.json` | 1.325.944 |
| `serverExtendedDraft.json` | 10.860.103 |
| `clientExtendedDraft.json` | 5.452.824 |

Usam a fórmula publicada em `capabilities.md`, com floors físicos no legado e
reservas de staging no ampliado. Excluem os itens ainda não cotados pelo futuro
perfil e não medem memória residente, assets ou VRAM. Estar abaixo do orçamento
configurado não autoriza anunciar um perfil.

## Reprodução básica

Um validador compatível com JSON Schema 2020-12 pode conferir o schema e os quatro
exemplos. Exemplo opcional em PowerShell, usando uma instalação isolada:

```powershell
python -m pip install --target .local/contractValidation jsonschema==4.26.0
$env:PYTHONPATH = Join-Path $PWD '.local/contractValidation'
@'
import json
from pathlib import Path
from jsonschema import Draft202012Validator
schema = json.loads(Path('docs/configuration.schema.json').read_text(encoding='utf-8'))
Draft202012Validator.check_schema(schema)
validator = Draft202012Validator(schema)
for path in Path('docs/examples').glob('*.json'):
    validator.validate(json.loads(path.read_text(encoding='utf-8')))
    print(path.name, 'schema valid')
'@ | python
```

Esse comando verifica estrutura, não duplicatas já perdidas pelo parser nem
relações entre campos. Para essas condições usar o texto lexical e os resultados
esperados do corpus; o próximo validador C++ deverá consumi-los em testes.

Os hashes podem ser recalculados diretamente dos frames públicos:

```python
import hashlib
import json
from pathlib import Path
v = json.loads(Path('docs/protocol/negotiationVectors.json').read_text(encoding='utf-8'))
frames = {(f['direction'], f['sequence']): bytes.fromhex(f['frameHex']) for f in v['frames']}
offer = hashlib.sha256(b'ExtendedPrecache/offer/1\0' + b''.join(frames['server', i] for i in range(10))).digest()
client = hashlib.sha256(b'ExtendedPrecache/client/1\0' + offer + b''.join(frames['client', i] for i in range(7))).digest()
selection = hashlib.sha256(b'ExtendedPrecache/selection/1\0' + offer + client + b''.join(frames['server', i] for i in range(11, 14))).digest()
for name, value in [('offer', offer), ('client', client), ('selection', selection)]:
    if value.hex() != v['digests'][name + 'Sha256']:
        raise ValueError(name + ' digest mismatch')
    print(name, value.hex())
```

Para avaliar o futuro codec, comparar seus bytes aos frames fixos, verificar os
erros esperados e assegurar que nenhuma transição anterior a C9 libere sign-on.
Não gerar a saída esperada pelo próprio codec sob teste. A integração deverá
validar também os eventos reais de reset/reconexão e o canal legado.
