# Dependências incorporadas

`nlohmann/json` 3.12.0, de Niels Lohmann, licença MIT. O parser header-only é
dependência privada dos núcleos de configuração e aplicação e dos testes já
publicados, obtida pelo CMake. Nenhuma dependência adicional foi introduzida
pelos codecs de dados.
Não é código proveniente das fontes de pesquisa de NextClient ou de um motor.

- Origem: [release oficial](https://github.com/nlohmann/json/releases/tag/v3.12.0).
- Arquivo: `json.tar.xz`.
- SHA-256: `42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa`.
- [Licença preservada](licenses/nlohmannJson.txt).

Distribuições que incorporarem esse núcleo devem incluir este aviso e a licença.
A DLL diagnóstica atual não liga o parser nem os núcleos de sessão e dados. CNG é a API do
sistema Windows; não há implementação de SHA-256 copiada para o repositório.
