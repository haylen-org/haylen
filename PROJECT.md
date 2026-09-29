# Haylen + Tiny Island — plano do projeto

Este é o documento central de organização. Ele reúne tudo o que foi pedido, todas as regras, as decisões técnicas, a estrutura de pastas, a lista completa de recursos da engine e do jogo, e o status de cada item.

Legenda de status: `[x]` implementado, testado e documentado. `[~]` em andamento. `[ ]` pendente.

Um item só recebe `[x]` quando está implementado, coberto por testes (quando a lógica roda sem GPU real ou runtime de plataforma) e documentado.

---

## 1. Objetivo

- Uma engine 2D reutilizável em C++20 (Haylen), distribuída como biblioteca CMake, que sirva para qualquer jogo 2D.
- Lua como linguagem principal dos jogos, via Varn, com toda a API também disponível em C++.
- O runtime executa um pacote de jogo (pasta ou `.zip`) com `game.json`, `main.lua` e a pasta `assets/`.
- Jogos demo em `samples/`, começando por Tiny Island com o pacote Tiny Swords, exercitando todos os recursos da engine.
- Build para desktop (macOS, Windows, Linux), iOS, tvOS, Android e Web a partir de um único script `make.py`.
- Resolução de design 1920x1080 com a UI sempre dentro da safe area.
- Alta performance (milhões de sprites), processamento assíncrono e código de produto, sem gambiarras.

## 2. Pedido original, organizado

Cada ponto abaixo veio do pedido e precisa estar coberto por algum item da seção 8 ou 9.

1. Usar a última versão de todas as bibliotecas.
2. CMake com CPM (última versão) para baixar dependências.
3. `make.py` que gera e compila para todas as plataformas: desktop, iOS, tvOS, Android e Web.
4. Assets colocados na pasta certa por plataforma.
5. Jogo usando o Tiny Swords (baixado do itch.io ou do zip local).
6. Resolução 1920x1080 com a UI dentro da safe area.
7. Tudo para 2D: textos, sprites, alta performance para milhões de sprites.
8. Suporte a mouse, teclado, touch e joystick.
9. Camada de física que abstrai o Box2D (última versão).
10. Comunicação com a plataforma via JSON, enviando e recebendo dados (login com Google, dados do device, executar funções e receber respostas por callback), incluindo a plataforma web.
11. Código da engine separado do código do jogo.
12. Sistema de partículas.
13. GUI/UI, inclusive na web com suporte a input.
14. Performance e async.
15. Coisas específicas de cada plataforma em pastas próprias.
16. Foco em organização, modularização e extensibilidade.
17. Suporte a tudo o que existe para jogos 2D, inclusive todos os recursos do Tiled.
18. Testes com cobertura de 100% ou o mais perto possível, principalmente da engine.
19. Pesquisar as melhores referências do mercado e ter todos os recursos equivalentes.
20. Jogo: mapa feito no Tiled, menu com a logo sobre o chão com água em volta (uma grande ilha), árvores aparecendo aleatoriamente na fase.
21. Jogo: cortar árvores para aumentar o fogo da fogueira. A fogueira diminui um pouco a cada dia.
22. Jogo: inimigos só aparecem à noite. Luz, dia e noite implementados.
23. Jogo: em volta da fogueira existe um círculo que impede os inimigos de entrar. O círculo diminui conforme falta madeira e o fogo vai apagando.
24. Jogo: escolha de personagem, cada um é uma classe.
25. Mega lista e mega pesquisa de tudo o que o jogo precisa.
26. Padrão de código e comentários no CLAUDE.md, seguido em todo o código.
27. Pastas em minúsculo.
28. Áudio no jogo: música de fundo, efeitos de ações e menu, com gerenciamento de música e efeitos.
29. Sistema de preload com load e unload, onde o desenvolvedor escolhe o que carregar.
30. GUI baseada em ImGui com a mesma lista de componentes do workpane-new, todos baseados em tema, com suporte a nine-patch e imagens.
31. Tudo o que for de 2D fica na pasta `2d`. Por enquanto o foco é só 2D, mas a organização já prevê o futuro.
32. Depois de tudo pronto, revisar bugs, código legado, código não utilizado, erros, race conditions e riscos de crash que realmente importam.
33. Não parar até tudo estar desenvolvido, testado e documentado.
34. Tudo exportado para Lua usando o Varn (github.com/varn-org/varn). Tudo o que for feito em C++ precisa ser exportado para o Varn e ficar acessível ao jogo. Isso é regra.
35. A engine tem Lua como alvo, mas tudo pode ser usado em C++ também.
36. O CMakeLists da engine é uma biblioteca que pode ser usada em outros projetos.
37. A engine carrega um `.zip` com o jogo inteiro (assets e scripts) e executa o `main.lua`. Também executa a partir de uma pasta, procurando o `main.lua` e a pasta `assets`.
38. Todo caminho de asset é relativo à pasta `assets`, sem repetir `assets/` no caminho, para fontes, imagens, vídeos, áudios e qualquer outro tipo.
39. Testes para tudo ficar 100% testado, sem testes inúteis, sem testes que geram testes e sem excesso de testes.
40. Os jogos demo ficam em `samples/`, com o jogo e os assets do Tiny Swords.
41. Sockets vêm do Varn no nativo. Na web, onde não existe TCP bruto, a engine oferece WebSocket pelo JavaScript do navegador, e o HTTP do Varn usa o fetch.
42. Cuidado com o async do Varn para nunca travar a UI e o loop principal.
43. A melhor arquitetura e organização possível, bem pesquisada e bem estruturada.
44. O CLAUDE.md contém as regras, a descrição do projeto, a estrutura e os padrões de código e de projeto.
45. No futuro haverá um site (em outro repositório) para editar o código Lua e os assets e rodar o jogo como aplicação wasm com WebGL ou WebGPU. A engine precisa funcionar nesse cenário.
46. Tudo deve ser módulo ou plugin para ficar organizado.
47. Arquivos `.h`, `.hpp`, `.c` e `.cpp` em PascalCase e arquivos Lua em dash-case. Isso é regra.
48. Os samples ficam em pastas dash-case (`samples/games/tiny-island`).
49. Os arquivos de módulo CMake são dash-case (`haylen-dependencies.cmake`).
50. Tudo o que for pedido para o editor web e para o resto do projeto fica registrado neste documento para não se perder.

### 2.1 Segundo pedido (28/09/2026)

Cada ponto abaixo veio do segundo pedido e precisa estar coberto por algum item da seção 14.

51. Nada de regra ou mecânica de jogo dentro da engine. O que é do jogo fica no jogo. Revisar a engine e levar para o sample tudo o que for mecânica do Tiny Island (por exemplo um ciclo de dia e noite com fases e contagem de dias).
52. Cenas com transições opcionais entre elas: push, pop, replace e voltar até a raiz, com uma biblioteca completa de transições (fade, fade por cor, crossfade, slide, move in, push, zoom, flip, rotozoom, split, tiles, página, radial, wipe, íris, dissolve e pixelate) e transições próprias escritas em Lua ou C++. Usar transição é sempre opcional.
53. Regras de ciclo de vida no mobile: ao ir para o segundo plano e ao voltar, parar a renderização, pausar e retomar o áudio (incluindo interrupções do sistema como ligações, alarmes e Siri no iOS, foco de áudio no Android e desbloqueio e suspensão do AudioContext na web), salvar configurações, proteger relógios, timers e tweens contra saltos de tempo e avisar o app por eventos.
54. Pausar a partida: pausa global, com modos de processamento por cena, timer, tween, animação e áudio (pausável, sempre, só quando pausado), para o menu de pausa continuar funcionando enquanto o mundo fica parado.
55. Entrada de texto e rich text funcionando em todas as plataformas. Entrada de texto com teclado virtual no mobile, IME, autocorreção, colar, seleção e cursor. Na web, com um campo HTML escondido sincronizado com o campo do app, funcionando 100% em navegadores desktop e mobile. Rich text com marcação (negrito, itálico, cor, tamanho, fonte, contorno, sombra, sublinhado, riscado, imagens no meio do texto, links clicáveis, listas, alinhamento e efeitos animados), tanto no desenho 2D quanto como componente de UI.
56. Componente de safe area: a UI pode ser ancorada na safe area (cantos, bordas, centro ou esticada) ou na tela inteira, de forma opcional, mesmo quando o app ocupa a tela toda do aparelho, inclusive embaixo das áreas recortadas (notch, ilha dinâmica, cantos arredondados e barra de gestos).
57. Sistema de câmeras 2D completo: zoom (com limites e zoom em torno de um ponto), posição, deslocamento, rotação, limites com suavização, seguir com suavização, zonas de arrasto, zona morta, antecipação do movimento, enquadrar vários alvos, tremor por trauma com ruído, várias câmeras, viewports e tela dividida, troca suave entre câmeras, parallax, pixel snap, conversão entre tela e mundo e culling.
58. O app roda em todos os aparelhos: macOS, Windows, Linux, iOS, iPadOS, tvOS (Apple TV), Mac Catalyst, Android (celular, tablet e TV) e web desktop e mobile. visionOS e watchOS quando a plataforma permitir, com a análise documentada.
59. Samples focados em código Lua, com as coisas específicas de cada plataforma em pastas próprias.
60. Projetos Apple (iOS, macOS e tvOS) gerados pelo XcodeGen (`project.yml`) para facilitar a criação do projeto do Xcode. O projeto do Xcode já gerado fica sempre junto (no template e nos projetos), com o `project.yml` ao lado para quando for preciso gerar de novo.
61. A engine é compilada uma vez em artefatos prontos: um xcframework (macOS, iOS, tvOS, Mac Catalyst e, se a plataforma permitir, watchOS e visionOS), um AAR para Android e o wasm pronto para a web. Um app Lua é só o pacote com o código e o conteúdo, sem recompilar a engine.
62. Novo formato do pacote: código Lua em `source/` (com `main.lua` como ponto de entrada) e recursos em `content/` no lugar de `assets/`.
63. Pesquisar e pensar na melhor organização para tudo isso.
64. Anotar tudo neste documento com muitos detalhes para nenhum pedido se perder.
65. Camadas, Y-sort e iluminação funcionando perfeitamente, com todos os recursos de luz 2D (luz pontual, spot, direcional, sombras com oclusores, máscaras de luz e o resto).
66. UI/GUI 100%, com todos os tipos de componente em todas as plataformas e suporte a mouse, touch, teclado e joystick, inclusive TV com controle (navegação por foco, Siri Remote na Apple TV e controle na Android TV).
67. Na web, caixas de texto e todo o input funcionando 100%.
68. Estudar código-fonte de referência (cópias locais) para chegar às melhores soluções.
69. Lua com acesso a tudo e com alto desempenho (APIs em lote e nenhuma alocação por chamada nos caminhos quentes).
70. Lua com acesso ao disco e com classes e dados globais que vivem no app entre as cenas (singletons, por exemplo os dados globais do jogador).
71. Conjunto completo de componentes de UI/GUI funcionando em todas as plataformas.
72. Um sample para cada conjunto de recursos, cada um com um menu simples para escolher o teste e um botão para voltar ao menu:
    - GUI com todos os componentes.
    - Física 2D completa (corpos, juntas, cordas, líquidos, destruição de terreno e o resto).
    - Rede (HTTP, HTTPS e WebSocket).
    - Sistema de arquivos.
    - Preferências.
    - Iluminação.
    - Shaders.
    - Partículas de todo tipo.
    - Localização.
    - Input (teclado, touch, joystick, mouse e gestos).
    - Sprites (pool, spritesheet, animação, recursos 2D simples e avançados, tiros, muitos e poucos objetos).
    - Acesso à plataforma funcionando em todas as plataformas.
    - Orientação do aparelho.
    - Safe area.
    - Áudio com efeitos e áudio 2D posicional.
    - Nine-patch.
    - Todo tipo de fonte.
73. Quanto mais separado e organizado, melhor.
74. Templates de plataforma: projetos prontos para cada plataforma que só esperam o pacote (`source/` e `content/`). O comando que roda um sample Lua apaga e recria a pasta daquele sample em `build/`, junta o template e o pacote e roda na plataforma pedida no parâmetro, e no desktop quando nenhuma é passada. O projeto C++ é um caso à parte.
75. Um comando para rodar samples Lua e outro para rodar samples C++.
76. O wasm fica pré-compilado quando possível. Se não for possível, tudo é embarcado junto.
77. Um comando no `make.py` que serve uma pasta com um servidor Python que já suporta tudo o que o wasm precisa (MIME, SharedArrayBuffer, threads e os cabeçalhos COOP, COEP e CORP), recebendo a pasta e, opcionalmente, a porta.
78. Na web, a logo do projeto com uma barra de progresso durante o carregamento, e a logo da engine quando o projeto não tem uma.
79. Fluxo do desenvolvedor: um comando do `make.py` cria um projeto novo na pasta escolhida, com o template de todas as plataformas e um código Lua de exemplo com `source/` e `content/`.
80. Modo debug com estatísticas: FPS, tempo de frame, draw calls, vértices e a contagem de objetos criados, vivos e destruídos por tipo.
81. Nomes genéricos: nada de `game` ou `game.zip` nas coisas finais. Usar `app`, porque a engine serve para jogos, aplicações multimídia e apps. Separar desde já o que é 2D do que é 3D nos nomes, pastas e arquivos, para que o 3D do futuro não conflite nem obrigue a renomear ou mover nada.
82. Uma revisão geral de tudo, organizada de forma profissional e fácil de desenvolver e de usar, com o objetivo de ser a engine mais fácil de usar, mais robusta e mais completa.

### 2.2 Terceiro pedido (28/09/2026): organização do código C++ e tela de erro

Cada ponto abaixo precisa estar coberto pelos grupos A, B, J, N, O, P, Q, R, S, T, U, V, W e X da seção 14.2.

83. Nada do prefixo `m_` nas variáveis. Os membros usam o nome normal, e os acessores usam `get`, `set`, `is` e `has`.
84. Nada de plugin com nome sem sentido, como o plugin chamado `save`. Todo nome precisa dizer o que a coisa é.
85. Os plugins ficam numa pasta e num namespace próprios de plugins.
86. Nada de funções ou métodos soltos fora de classes, nem de arquivos com uma struct e funções isoladas. Tudo pertence a uma classe.
87. Cada arquivo tem a sua própria classe, com o nome da classe.
88. Um sub-namespace para cada contexto, e não um namespace único para tudo.
89. Revisar tudo: não pode sobrar coisa solta, perdida ou fora de classe.
90. As regras gerais continuam valendo e ficam no CLAUDE.md: sem gambiarras, fallbacks, código porco, código legado ou compatível com versões anteriores. Comentários raros e só onde precisam. Código e comentários em inglês. Nenhuma frase dividida por ponto e vírgula. Fazer só o que faz sentido, nunca para mostrar trabalho. Manter esta mega lista detalhada e revisar no fim se tudo foi feito 100%, testado e documentado.
91. A tela de erro precisa ser bem legível e detalhada: hoje aparecem caracteres estranhos (os tabs da pilha do Lua viram quadrados) e pouco detalhe. Melhorar sem gambiarras.
92. Os templates de iOS (e as outras plataformas Apple) e de Android precisam ter a splash funcionando em paisagem e em retrato.
93. Os templates de plataforma ficam em `templates/platform/<plataforma>/` (e não soltos em `templates/`), com organização modular e extensível.
94. A engine traz os algoritmos que os jogos usam (A* e muitos outros), prontos para alto desempenho, incluindo a distribuição de elementos no mapa com base em área, densidade e regiões, e a destruição de elementos e de terreno.
95. Um sistema de tween bem robusto.
96. Sistema de eventos, ciclo de vida e conexão e desconexão de coisas, tudo bem robusto.
97. Raycast.
98. O nome da engine é Haylen (em minúsculas, `haylen`). Trocar em todos os lugares: arquivos, pastas, docs, README, classes, namespaces, alvos e funções do CMake, módulos Lua, pacotes Java, JavaScript, artefatos, templates e logo.
99. Ciclo de vida de cena com carregamento: a transição começa (cobre a cena atual), a engine chama um método de carregamento da nova cena (assíncrono), e depois a transição de saída exibe a nova cena carregada. Isso permite exibir um loading próprio se o desenvolvedor quiser, ou usar a própria transição como loading. A arquitetura precisa dar todas as possibilidades, com eventos assíncronos e ciclo de vida funcionando perfeitamente, pensada como arquitetura de software, com o máximo de desempenho e sem gambiarras, não importa o tamanho do trabalho.
100. Revisar tudo de novo atrás de bugs, código legado, código não usado, erros, race conditions e falhas que podem derrubar o app, corrigindo o que faz sentido (não código que nunca pode acontecer nem coisas aleatórias só para mostrar trabalho), e manter as regras gerais no CLAUDE.md e esta mega lista detalhada.
101. Os samples ficam em subpastas por categoria, e o comando recebe o caminho da categoria, por exemplo `python3 make.py run games/tiny-island`, para ficar mais organizado.
102. Aplicações sem moldura e transparentes, como o Taskbar Hero: janela sem barra de título e sem bordas, fundo transparente, o jogo rodando no rodapé da tela e arrastável, com a GUI/UI do jogo funcionando. É outra modalidade de jogo que a engine precisa suportar.
103. Comunicação fácil com qualquer plataforma (iOS, Android, desktop, web e as outras): enviar e receber a resposta da plataforma de forma assíncrona, para usar qualquer coisa nativa da plataforma.
104. Chamar bibliotecas e SDKs nativos, como a biblioteca da Steam, bibliotecas nativas em geral e SDKs como o P2P da Epic Online Services (NAT P2P). O `ffi` do Varn pode ser parte da solução. Não é preciso usar esses SDKs, eles são só exemplos, mas a capacidade precisa ser testada nas plataformas.
105. Organizar tudo isso na engine, revisado e testado, não importa o tamanho da refatoração, para a engine cobrir todos os casos do desenvolvimento de jogos. E revisar o projeto inteiro de novo atrás de bugs, código legado, código não usado, erros, race conditions e falhas que derrubam o app, com as regras gerais de sempre.
106. A cada bloco de trabalho terminado, fazer commit e push na `main`. A mensagem do commit tem o prefixo do tipo (feature, fix e os outros) e uma frase curta em minúsculas, sem coautor e sem citar Claude ou qualquer outra pessoa. A regra fica no CLAUDE.md, não na documentação.
107. Antes de cada commit, conferir que não entra nada de build, arquivo temporário, chave de ambiente, segredo ou qualquer coisa privada ou temporária que não deveria ser commitada. A regra fica no CLAUDE.md.
108. Nunca citar outras engines, nem em docs, nem em comentários, nem em código, nem mesmo em comparações. A regra fica no CLAUDE.md.
109. Tudo o que o dono pedir entra nesta lista de coisas a fazer, para nada se perder.

## 3. Regras

As regras completas e oficiais estão em `CLAUDE.md`. O resumo abaixo existe para consulta rápida.

### 3.1 Princípios

- Sem gambiarras, fallbacks escondidos, código porco, código morto ou legado.
- Sem compatibilidade com versões anteriores. Nada de checks do tipo "antes era assim, agora é assado". Só a versão nova e final.
- Refatorar ou refazer o que for preciso, sem medo.
- Fazer apenas o que faz sentido. Nada para "mostrar trabalho".
- Código, comentários e documentação do repositório em inglês. Este `PROJECT.md` é a exceção, em português.

### 3.2 Formatação

- Visual compacto, profissional e consistente com o projeto.
- Espaço vertical só para separar contextos de leitura. Blocos de responsabilidades diferentes separados por uma linha vazia.
- Nunca deixar `if`, validações, loops, mutações de estado e retornos grudados visualmente.
- Função com começo, meio e fim identificáveis. Blocos importantes de funções complexas podem ter um comentário curto de intenção.
- Extrair funções quando uma função acumula responsabilidades. Não extrair só para reduzir tamanho.
- Evitar aninhamento desnecessário. Preferir retorno antecipado. Sem `else` depois de `return`.
- Includes limpos, diretos e ordenados.
- Evitar macros, casts inseguros e ponteiros crus com posse. Usar `const`, referências, RAII e smart pointers.
- Sem comentários em headers descrevendo métodos, seções ou membros.
- Lambdas não triviais ficam entre `// clang-format off` e `// clang-format on`, formatadas à mão.

### 3.3 Comentários

- Raros. Só onde o contexto ou a intenção não são óbvios.
- Toda frase completa, começando com maiúscula e terminando com ponto.
- Se a frase começaria com um identificador minúsculo, reescrever a frase.
- Comentário acima de função, classe ou módulo diz o que ela faz para quem chama, nunca como é implementada.
- Uma frase nunca é quebrada em várias linhas.
- Sem frases separadas por ponto e vírgula, nem em comentários nem em documentação.

### 3.4 Nomes e pastas

- Tipos e valores de enum em `PascalCase`. Funções, métodos, variáveis e parâmetros em `camelCase`. Membros privados com prefixo `m_`. Constantes em `kPascalCase`.
- Os nomes em Lua são os mesmos do C++: módulos `haylen.<modulo>`, funções e métodos em `camelCase`, tipos em `PascalCase`. Isso também combina com o padrão do Varn.
- Arquivos C e C++ (`.h`, `.hpp`, `.c`, `.cpp`, `.mm`) em `PascalCase`, com o nome do tipo principal (`FrameClock.hpp`, `Renderer2D.cpp`). Arquivos Lua em `dash-case` (`main-menu.lua`), e todo pacote mantém `main.lua` como entrada. Isso é regra.
- Módulos CMake em `dash-case` (`haylen-dependencies.cmake`). Pastas de samples em `dash-case` (`samples/games/tiny-island`).
- Todas as pastas em minúsculo. Assets em `snake_case` minúsculo.
- Todo o código da engine no namespace `haylen`.
- Tudo o que é específico de 2D fica em `2d/`.

### 3.4.1 Lua e Varn

- Tudo o que existe em C++ é exportado para Lua no mesmo trabalho, com testes em Lua e página de referência em `docs/lua-api/`.
- Os módulos da engine são instalados no mesmo estado Lua do Varn, via `package.preload`, ao lado dos módulos do Varn.
- O loop do Varn avança com `Runtime::poll()` uma vez por frame, sem bloquear.
- Trabalho pesado roda no `taskPool()` do Varn, I/O bloqueante no `ioPool()`, e o resultado volta pelo event loop e, em Lua, por uma `Promise` que a corrotina aguarda com `:await()`.
- Chunks Lua são carregados sempre como texto, nunca como bytecode.

### 3.5 Arquitetura

- A engine é uma biblioteca CMake independente em `engine/`, consumível por `add_subdirectory`, CPM ou `find_package(haylen)`.
- A engine nunca depende dos jogos. Jogos usam só a API Lua e os headers públicos de `engine/include`.
- A biblioteca portável (`haylen_engine`) nunca chama Sokol app nem APIs do sistema. Ela fala com o sistema só pela interface interna `Host`.
- O jogo nunca inclui Sokol, Box2D, miniaudio, backend do ImGui, internals do Varn, JNI, UIKit, AppKit ou Emscripten.
- Serviços de plataforma passam pelo `PlatformBridge` com JSON e callbacks assíncronos entregues na thread do frame.
- Todo input de gameplay passa pelo mapa de ações.
- A UI se posiciona na safe area fornecida pela engine.
- Trabalho pesado de CPU passa pelos pools do Varn através do `JobSystem` da engine. Resultados voltam pelo event loop do Varn.
- Recursos de GPU são criados e destruídos só na thread do frame.
- Física só pelo wrapper de Box2D da engine.
- O runtime do Tiled lê o formato JSON atual, sem código para formatos antigos.

### 3.6 Dependências

- Declaradas com CPM em `engine/cmake/haylen-dependencies.cmake`, com hash SHA-256.
- O Varn entra antes das bibliotecas que ele compartilha com a engine (nlohmann/json e zlib), que são resolvidas uma única vez.
- Sempre a última release. Sem release, o último commit da branch padrão.
- Ao atualizar uma dependência, adotar a API nova em todo lugar.

### 3.7 Testes

- GoogleTest para toda lógica que roda sem GPU real ou runtime de plataforma.
- Host headless (backend dummy do Sokol, áudio sem dispositivo e runtime do Varn) para testar renderer, UI, bindings Lua e loop da engine.
- Bindings Lua testados executando Lua pela engine headless e conferindo o estado e os retornos.
- Testes verificam comportamento que importa. Nada de testes redundantes, gerados ou que só repetem a implementação.
- Cobertura da engine o mais perto possível de 100%.

## 4. Stack e versões

Versões conferidas nas fontes oficiais em 26/09/2026.

| Componente | Versão | Uso |
| --- | --- | --- |
| CMake | 3.28 ou mais novo (testado com 4.4.3) | Build |
| CPM.cmake | v0.43.2 | Gerenciador de dependências |
| Varn | v0.0.1 | Runtime Lua, event loop, pools, promises, http, sockets, fs, json, zip, crypto |
| Lua | 5.5.0 (via Varn, compilado como C++) | Linguagem dos jogos |
| Sokol | master `2e75443` (15/09/2026) | Janela, eventos e GPU (Metal, D3D11, GL, GLES3, WebGPU, Vulkan) |
| sokol-tools-bin | master `11d0cf6` | Compilador de shaders `sokol-shdc` |
| Dear ImGui | v1.92.9b | Base da UI |
| Box2D | v3.1.1 | Física 2D |
| miniaudio | 0.11.25 | Áudio (com stb_vorbis para OGG) |
| nlohmann/json | v3.12.0 | JSON |
| zlib | a do Varn (zlib-cmake) | Descompressão de mapas Tiled e de pacotes `.zip` |
| zstd | v1.5.7 | Descompressão de mapas Tiled |
| stb | master `2c980bb` | Imagens, fontes e empacotamento de retângulos |
| GoogleTest | v1.18.0 | Testes |
| Android Gradle Plugin | 9.4.1 | Android |
| Gradle | 9.8.0 | Android |
| Android NDK | r30 (30.0.16248370) | Android |
| Android SDK | compile e target 37, mínimo 30 | Android |
| Emscripten | 6.0.10 | Web (single-thread, como o Varn) |
| Tiled | 1.12.2 | Editor de mapas (formato JSON) |
| Xcode | versão atual | iOS e tvOS (macOS usa Ninja ou Xcode) |

## 5. Estrutura de pastas

```text
CMakeLists.txt              Projeto raiz: engine, player, samples e testes.
make.py                     Ponto único de build, execução, testes, cobertura, formatação, empacotamento e servidor web.
CLAUDE.md                   Regras oficiais, descrição do projeto e estrutura.
PROJECT.md                  Este documento.
README.md                   Visão geral e início rápido.
docs/                       Guias e referência da API Lua (docs/lua-api/).
tools/                      Scripts Python do sample (importar Tiny Swords e gerar o mapa da ilha).
engine/
  CMakeLists.txt            Projeto CMake independente da engine, usável por outros projetos.
  cmake/                    Módulos CMake da engine (CPM, dependências, plataforma, shaders, haylen_add_game, deploy do conteúdo).
  include/haylen/
    core/                   Aplicação, engine, cenas, plugins, jobs sobre o Varn, log, sinais, timers, tweens, random, UTF-8.
    math/                   Vec2, Rect, Color, Transform2D, easing, ruído, geometria, Poisson disk.
    io/                     Pacote do jogo (pasta, zip ou memória), caminhos e armazenamento do usuário.
    assets/                 Gerenciador de assets e grupos de preload.
    graphics/               Base de GPU sem dimensão: texturas, render targets, blend, viewport e escala.
    input/                  Teclado, mouse, touch, gamepad, mapa de ações, controles virtuais, gestos.
    audio/                  Engine de áudio, sons, música, barramentos.
    ui/                     Backend ImGui, temas e componentes.
    platform/               Bridge JSON e cabeçalho público da bridge Apple.
    localization/           Tabelas de texto por idioma.
    save/                   Save slots e configurações persistentes.
    debug/                  Profiler e overlay de debug.
    lua/                    API C++ pública para escrever bindings Lua (usada pela engine e por projetos que estendem a engine).
    2d/
      graphics/             Renderer2D, sprites, câmera 2D, atlas, nine-slice, primitivas, batches estáticos, pós-processo.
      text/                 Fontes SDF e layout de texto.
      animation/            Animação de sprites e atlas.
      particles/            Sistema de partículas.
      lighting/             Luz 2D e ciclo de dia e noite.
      physics/              Wrapper do Box2D.
      tiled/                Modelo, loader, renderer, colisão e mundos do Tiled.
      navigation/           A* em grade e steering.
      spatial/              Spatial hash.
  src/                      Implementação no mesmo formato de include/, com os bindings Lua ao lado de cada módulo.
    platform/               Serviços por sistema (apple, android, web, windows, linux, desktop), host Sokol e host headless de testes.
  shaders/                  Shaders sokol-shdc.
  platform/
    android/                Módulo Gradle `haylen` (Activity, bridge, insets, controles, transporte Kotlin do Varn) e `haylen-game.gradle`.
    web/                    Shell HTML e `haylen-runtime.js` (bridge, logs, erros, pacotes e API do editor).
    apple/                  Info.plist e launch screens de macOS, iOS e tvOS.
  tests/                    Testes GoogleTest da engine e dos bindings Lua.
samples/
  tiny-island/
    CMakeLists.txt          Chama haylen_add_game.
    game/                   Pacote do jogo: game.json, main.lua, módulos Lua e assets/.
    platform/android/       Projeto Gradle do app, que inclui o módulo `haylen` da engine.
```

## 6. Arquitetura

### 6.1 Produtos e bibliotecas

- `haylen::engine`: biblioteca portável com toda a API C++ e os bindings Lua. Não conhece Sokol app nem o sistema operacional.
- `haylen::runtime`: host real por plataforma (Sokol app, backend gráfico real, serviços nativos, JNI, UIKit, JavaScript).
- `haylen_headless`: host de testes (backend dummy do Sokol, áudio sem dispositivo, pacote em pasta local).
- `haylen` (player): executável de desktop e página web que rodam qualquer pacote de jogo.
- `haylen_add_game(<alvo> PACKAGE <pasta> ...)`: função CMake que gera o app de um pacote para cada plataforma.

### 6.2 Pacote do jogo

- Um pacote é uma pasta ou um `.zip` com `game.json`, `main.lua`, outros módulos Lua e `assets/`.
- `game.json` define nome, identificador, versão, janela, resolução de design, política de escala e orientação. Ele é lido antes do Lua, porque a janela é criada antes do primeiro script.
- `main.lua` é o ponto de entrada. `require("scenes.menu")` procura `scenes/menu.lua` a partir da raiz do pacote, também dentro do zip.
- Todo caminho de asset é relativo a `assets/`: `assets.texture("tiny_swords/units/blue/warrior/idle.png")` lê `assets/tiny_swords/units/blue/warrior/idle.png`.
- O player aceita `haylen caminho/do/jogo` ou `haylen jogo.zip` no desktop e `haylen.html?package=jogo.zip` na web. Um app gerado por `haylen_add_game` leva o pacote como conteúdo.

### 6.3 Fluxo de um frame

1. O host entrega eventos (teclado, mouse, touch, janela, ciclo de vida) e a engine converte posições para coordenadas de design.
2. Gamepads são lidos pelo host e o mapa de ações é atualizado.
3. O loop do Varn avança com `Runtime::poll()`: promises resolvem, corrotinas continuam, timers disparam, callbacks de http e sockets rodam, e uploads de assets prontos acontecem.
4. Os callbacks da bridge de plataforma são entregues.
5. O relógio avança e os passos fixos rodam (física e lógica determinística).
6. Timers de jogo, tweens e cenas (C++ ou Lua) são atualizados.
7. A UI (ImGui) começa o frame, as cenas renderizam o mundo e a UI.
8. O renderer grava todos os buffers transitórios, executa os passes offscreen (luz, render targets) e o passe final na tela.
9. O input fecha o frame (bordas de pressionado e solto).

### 6.4 Threads e async

- Thread do frame: GPU, estado Lua, cenas, UI, física e callbacks. Nunca bloqueia.
- `taskPool()` do Varn: decodificação de imagens e áudio, parsing de mapas e trabalho paralelo de dados (partículas, conversão de sprites).
- `ioPool()` do Varn: leitura de arquivos e I/O bloqueante.
- Em Lua, operações lentas retornam uma `Promise` do Varn e a corrotina usa `:await()` sem travar o jogo.
- A web é single-thread como o Varn. Jobs rodam quando `poll()` drena os pools, e `parallelFor` roda direto na thread do frame.

### 6.5 Renderização 2D

- Um quad estático e instâncias de 48 bytes (posição, tamanho, UV, cor, flash, rotação, flags, pivô).
- Buffers `write_transient` do Sokol gravados uma vez por frame antes de qualquer draw.
- Canvases de mundo (com câmera e luz opcional) e de tela (coordenadas de design).
- Ordenação por camada e profundidade (Y-sort) com batching por textura, blend e pipeline.
- Batches estáticos com buffers imutáveis para tiles e decoração.
- Luz: mapa de luz com ambiente e luzes pontuais aditivas, multiplicado sobre a cena.

### 6.6 API Lua

- Cada módulo C++ tem um módulo Lua com os mesmos nomes (`haylen.graphics`, `haylen.input`, `haylen.audio`, `haylen.physics`, `haylen.tiled` e assim por diante).
- Objetos C++ são userdata com metatables e métodos (`sprite:setPosition(x, y)`).
- Cenas podem ser escritas em C++ ou em Lua (tabelas com `enter`, `update`, `render` e os demais callbacks).
- Os módulos do Varn continuam disponíveis no mesmo estado (`async`, `http`, `socket`, `json`, `fs`, `zip`, `crypto`, `log`, `platform`, `process`, `datetime`, `xml`).

## 7. Plataformas e build

| Plataforma | Backend gráfico | Gerador | Comando |
| --- | --- | --- | --- |
| macOS | Metal | Ninja ou Xcode | `python3 make.py build --platform macos` |
| Windows | D3D11 | Ninja ou Visual Studio | `python make.py build --platform windows` |
| Linux | OpenGL Core | Ninja | `python3 make.py build --platform linux` |
| iOS | Metal | Xcode | `python3 make.py build --platform ios` |
| tvOS | Metal | Xcode | `python3 make.py build --platform tvos` |
| Android | GLES3 | Gradle + Ninja | `python3 make.py build --platform android` |
| Web | WebGPU | Ninja + Emscripten | `python3 make.py build --platform web` |
| Web | WebGL2 | Ninja + Emscripten | `python3 make.py build --platform web-webgl2` |

Rodar um pacote no desktop: `python3 make.py run --game samples/games/tiny-island/game` ou `./build/macos-debug/bin/haylen/haylen samples/games/tiny-island/game`.

Rodar um pacote na web: `python3 make.py run --platform web --game samples/games/tiny-island/game`, que compila o player, gera `game.zip` e serve `haylen.html?package=game.zip`.

O build Android primeiro configura a árvore nativa com o NDK para baixar o Varn e passa ao Gradle a pasta das fontes Kotlin do transporte HTTP (`-PhaylenVarnSourceDir`) e o `sokol-shdc` (`-PhaylenSokolShdc`). O Gradle compila a biblioteca do jogo com o CMake 4.1.2 do SDK e o NDK 30.0.16248370 para arm64-v8a e x86_64.

Deploy do conteúdo, agora com o pacote do jogo:

- Windows e Linux: saída em `build/<plataforma>-<config>/bin/<app>/` com um link `game` apontando para a pasta do pacote, criado pelo alvo `SYNC_PACKAGE-<app>`.
- Apple (macOS, iOS e tvOS): o app é um bundle, e cada arquivo do pacote é marcado com `MACOSX_PACKAGE_LOCATION "Resources/game/<subpasta>"`.
- Web: `--preload-file <pacote>@/game`.
- Android: a task `copyHaylenPackage` de `engine/platform/android/haylen-game.gradle` copia o pacote para `assets/game/`, grava `haylen-package-index.json` (o Android não lista pastas de assets recursivamente) e é registrada como diretório gerado de assets de cada variante.

## 8. Recursos da engine

Regra geral: todo item desta seção que tem API em C++ só está pronto quando também tem binding Lua, teste do binding e página em `docs/lua-api/`.

### 8.0 Runtime Lua (Varn) e pacotes de jogo

- [x] **Integração com o Varn**: `varn_core` via CPM, um `varn::runtime::Runtime` por engine, `poll()` por frame, módulos da engine em `package.preload`.
- [~] **Driver por plataforma do Varn**: cliente HTTP da plataforma no Apple (NSURLSession) e no Android (HttpURLConnection com as classes Kotlin do Varn e `AndroidHttpBridge::publish` no `JNI_OnLoad` da engine), fetch na web. Validado no Android (emulador arm64, API 36, HTTPS com status 200) e na web. Falta validar no Apple, que precisa do Xcode.
- [x] **WebSocket** (`haylen.net`, `WebSocket` e `NetPlugin`): cliente `ws://` e `wss://` com subprotocolos, texto, binário, mensagens fragmentadas, ping e pong, fechamento pelos dois lados e erros. No nativo, cada conexão roda numa thread própria com o Poco e os certificados são verificados com o trust store que o Varn encontra. Na web, usa o `WebSocket` do navegador através da página. Os eventos chegam no começo do frame, e o plugin mantém o socket vivo até fechar. Validado nos testes com um servidor Poco e no Chrome com WebGPU e WebGL2.
- [x] **Pacote em pasta**: leitura de `game.json`, `main.lua`, módulos Lua e `assets/` a partir de uma pasta.
- [x] **Pacote em zip**: leitura de todos os arquivos de dentro de um `.zip` (em disco, embutido no app ou nos assets do Android), com acesso seguro entre threads.
- [x] **game.json**: nome, identificador, versão, janela, resolução de design, escala, orientação e taxa do passo fixo (`fixedRate`, em Hz). `haylen_add_game` lê nome, identificador e versão do mesmo arquivo para o app.
- [x] **Loader de módulos Lua** que resolve `require` dentro do pacote, sempre em modo texto.
- [x] **Aplicação Lua**: carrega o pacote, executa `main.lua` e encaminha o ciclo da engine para cenas Lua.
- [x] **Player** `haylen <pasta|zip>` no desktop (validado com Metal no macOS) e `haylen.html?package=<zip>` na web (validado com WebGPU no Chrome).
- [~] **haylen_add_game** gerando apps para todas as plataformas com o pacote como conteúdo. Desktop, web e Android validados (APK rodando no emulador com bridge, HTTP, pausa e retomada). iOS e tvOS precisam do Xcode para validar.
- [x] **Engine como biblioteca**: três formas de consumo a partir da pasta `engine/`, todas com `haylen::engine`, `haylen::runtime` e `haylen_add_game`. `add_subdirectory` e `CPMAddPackage` compilam a engine dentro do projeto do jogo, com C++20 e o suporte a exceções da web propagados pelo target, e testes, player e benchmarks desligados quando a engine não é o projeto principal. `find_package(haylen)` usa o SDK de `make.py sdk --platform <desktop|web>`: todas as bibliotecas estáticas do fechamento da engine (Varn, Poco, OpenSSL, libuv, Lua, Box2D e as outras) são fundidas em `libhaylen` e o runtime em `libhaylen_runtime`, com os headers públicos, os headers de ImGui, Lua e nlohmann/json que eles usam, o `haylen-config.cmake` e os arquivos de `haylen_add_game` em `share/haylen`, instalados pelo componente `haylen_sdk`. O sample `samples/cpp/embedding` é um projeto CMake próprio compilado nos três modos por `make.py embedding --mode subdirectory|cpm|package` (validados no macOS, e o modo `package` também na web com o SDK WebGPU rodando no Chrome). Android, iOS e tvOS usam a engine por `add_subdirectory` nos projetos Gradle e Xcode.
- [x] **API C++ para bindings** (`haylen/lua/`) para que outros projetos exponham seus próprios módulos, inclusive `lua::LuaPromise` para bindings assíncronos, que o jogo espera com `:await()` e que o código nativo resolve de qualquer thread sem depender dos headers do Varn (o SDK não instala esses headers).
- [x] **Erros de tarefas assíncronas**: um erro que escapa de `async.spawn` ou `async.run` mostra a tela de erro com a pilha da corrotina e chega ao `onError` da página, como um erro de callback de cena. Falhas em `start`, `installLua` e `endFrame` de plugins também viram tela de erro.

### 8.0.1 Plugins

- [x] **Interface de plugin** com nome, ciclo de vida uniforme (`start`, `fixedUpdate`, `update`, `render`, `renderUi`, `stop`, eventos de ciclo de vida do app) e instalação dos próprios módulos Lua.
- [x] **Registro de plugins** na engine, com os subsistemas embutidos (core, input, gráficos com partículas e luz, assets, animação, áudio, física, Tiled, spatial, navegação, localização, save, debug, UI e plataforma) registrados como plugins.
- [x] **Plugins externos**: projetos que usam a engine registram seus plugins em C++ pela aplicação e em Lua pelo mesmo `require`.

### 8.0.2 Cenário do editor web

- [x] **Pacote em memória**: pacote montado a partir de um zip em memória ou de um conjunto de arquivos enviado pela página.
- [x] **API JavaScript do runtime web**: `Module.haylen.loadZip`, `packageUrl`, `setFile`, `removeFile`, `clearFiles`, `run`, `stop` e `reloadAsset`, sem recarregar a página, além de `register`, `emit`, `onLog` e `onError`.
- [x] **Hot reload** de scripts e assets alterados: no desktop o player observa a pasta do pacote, reinicia o jogo quando scripts ou `game.json` mudam (inclusive a partir da tela de erro) e recarrega assets no lugar; na web o editor usa `setFile` com `run` ou `reloadAsset`.
- [x] **Erros e logs para o host**: mensagens de log (`onLog`) e erros Lua com stack trace (`onError`) enviados ao JavaScript, e ao console nas outras plataformas.
- [x] **Tela de erro**: quando um script falha, a engine mostra o erro na tela e continua viva para receber a correção.
- [x] **Reinício limpo**: o jogo pode ser destruído e recriado no mesmo processo, liberando cenas, estado Lua, assets e recursos de GPU.

### 8.1 Build e ferramentas

- [x] **Estrutura em minúsculo** conforme a seção 5, com `2d/` separado e `samples/`.
- [x] **CPM** v0.43.2 com hash e cache compartilhado em `.cache/cpm`.
- [x] **Detecção de plataforma e backend** com override por opção CMake.
- [~] **Deploy do conteúdo** para o pacote do jogo (desktop, Apple, web e Android).
- [x] **Shaders** compilados com sokol-shdc para GLSL 4.30, GLSL 3.00 ES, HLSL 5, Metal (macOS, iOS e simulador), WGSL e SPIR-V.
- [~] **make.py** com `tools`, `configure`, `build`, `run`, `test`, `coverage`, `format`, `assets`, `map`, `web` (WebGPU e WebGL2 numa pasta só), `bench`, `embedding`, `package`, `serve` e `clean` para todas as plataformas. iOS e tvOS precisam do Xcode para validar.
- [x] **clang-format** com o mesmo estilo do workpane-new, aplicado por `make.py format` em todo C, C++ e Objective-C++ da engine e dos samples, com verificação de lambdas multilinha sem `clang-format off/on` e modo `--check` para CI.
- [x] **Avisos e sanitizers**: `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow` (ou `/W4`), ASan e UBSan opcionais.
- [x] **Cobertura** com LLVM source-based coverage e relatório HTML e texto filtrado para a engine.
- [~] **CI** no GitHub Actions (`.github/workflows/ci.yml`): verificação de formatação, testes e SDK com `find_package` no macOS, Linux e Windows, cobertura no macOS, player web WebGPU e WebGL2 e APK do Tiny Island, com cache das fontes do CPM e do emsdk. Falta rodar no GitHub, porque o repositório ainda não tem remoto.

### 8.2 Core

- [x] **Application e AppConfig**: título, tamanho de design, janela, DPI alto, tela cheia, número de workers, passo fixo, organização e identificador.
- [x] **Engine**: dona de todos os subsistemas e do loop de frame da seção 6.2.
- [~] **Host**: interface interna entre engine e plataforma, com host real e host headless.
- [x] **Cenas**: `enter`, `exit`, `pause`, `resume`, `fixed_update`, `update`, `render`, `render_ui`, pilha com push, pop e replace, transições com fade.
- [x] **JobSystem sobre o Varn**: trabalho no `taskPool()` com conclusão no event loop, `parallelFor` com o chamador participando (inline na web).
- [x] **Log** integrado ao log do Varn (um único console por plataforma), com níveis e `std::format`.
- [x] **Signal e Connection**: callbacks tipados com desconexão RAII, seguros durante a emissão.
- [x] **TimerScheduler**: `after`, `every`, cancelamento e pausa.
- [x] **Random**: xoshiro256** determinístico, faixas, chance, peso e embaralhamento.
- [x] **UTF-8**: decodificação e codificação.
- [x] **FrameClock**: tempo limitado, escala de tempo, passos fixos e interpolação.

### 8.3 Matemática

- [x] **Vec2 e Rect** com todas as operações usuais.
- [x] **Color** com parsing `#RRGGBB` e `#AARRGGBB` (formato do Tiled), HSV, lerp e empacotamento RGBA8.
- [x] **Transform2D** afim com composição e inversa.
- [x] **Utilitários**: constantes, lerp, remap, smoothstep, move toward, ângulos, amortecimento.
- [x] **Easing**: todas as famílias de Penner (in, out e in-out) e nomes para JSON.
- [x] **Ruído**: Perlin, simplex e fractal com semente.
- [x] **Geometria**: círculos, segmentos, polígonos, interseções, área, centróide, convexidade, fecho convexo e triangulação.
- [x] **Poisson disk**: pontos espalhados com distância mínima e predicado de aceitação.

### 8.4 Arquivos, assets e preload

- [x] **Sistema de arquivos virtual**: leitura de arquivos do pacote (pasta ou zip) por caminho relativo, sem sair da raiz, com `assets/` implícito para assets.
- [~] **Armazenamento do usuário**: pasta de dados por plataforma (IDBFS na web, com sincronização).
- [x] **Imagens**: PNG, JPG, TGA e BMP via stb_image em workers.
- [x] **AssetManager**: texturas, atlas, fontes, sons, mapas Tiled, mundos Tiled, efeitos de partículas (`.particles`), JSON, texto e bytes. Carga síncrona e assíncrona, cache por caminho, estados, upload de GPU só no frame.
- [x] **Grupos de preload**: declarados em código ou em manifesto JSON, carregados em paralelo com progresso e descarregados explicitamente. Assets compartilhados só saem quando o último grupo os solta.
- [x] **Hot reload** de texturas (substituídas no lugar, todos os handles veem a imagem nova) e JSON e demais assets (saem do cache e são lidos de novo) no desktop, validado no player do macOS.

### 8.5 Gráficos (base e 2D)

- [x] **Texturas**: RGBA8, filtro nearest ou linear, clamp, repeat ou mirror, handles sem tipos do Sokol.
- [x] **Render targets** usáveis como textura.
- [x] **Viewport e escala**: políticas fit (letterbox), fill (corte), stretch, expand e pixel perfect. Conversões entre janela, framebuffer e design.
- [x] **Renderer2D** instanciado, batching, camadas e Y-sort.
- [x] **Canvases** de mundo e de tela, com luz opcional.
- [x] **Câmera 2D**: posição, zoom, rotação, limites, seguir suave, zona morta, tremor e área visível para culling.
- [x] **Sprites**: pivô, rotação, escala, flips (incluindo diagonal do Tiled), tinta, cor de flash para dano, camada e profundidade.
- [x] **Atlas**: regiões com trim, TexturePacker JSON (hash e array), Aseprite JSON e fatiamento em grade.
- [x] **Nine-slice** clássico e em nove peças separadas (formato das peças do Tiny Swords), com centro esticado ou repetido.
- [x] **Primitivas**: linhas com espessura, polilinhas, retângulos, contornos, círculos, anéis, arcos e polígonos côncavos.
- [x] **Meshes** indexadas com textura, cor e recorte (scissor).
- [x] **Batches estáticos** em buffers imutáveis.
- [x] **Caminho de milhões de sprites**: `drawBatch` com conversão paralela no `JobSystem`, batches estáticos na GPU e benchmark `make.py bench` (app `haylen-sprite-benchmark`, vsync desligado, fases de 100 mil, 1 milhão e 2 milhões de sprites dinâmicos e estáticos). Num Apple M5 Pro com Metal em Release: 2 milhões de sprites animados todo frame a 105 fps com 4,5 ms de CPU por frame, e 2 milhões estáticos no limite de 120 Hz da tela com 0,02 ms de CPU.
- [x] **Texto SDF**: fontes TTF em atlas dinâmico, UTF-8, kerning, alinhamento, quebra de linha, contorno e sombra.
- [x] **Pós-processamento**: vinheta, tinta, saturação, brilho e fade.
- [x] **Estatísticas**: sprites, instâncias, draw calls, trocas de textura e bytes enviados.
- [x] **Limites de recursos da GPU**: pools do Sokol dimensionados para jogos reais (4096 texturas e render targets, 8192 views e 4096 buffers), com erro claro quando um pool enche em vez de texturas inválidas silenciosas.

### 8.6 Animação

- [x] **Clipes de sprite**: frames com duração, modos once, loop e ping-pong, velocidade, eventos por frame e fim.
- [x] **Conjuntos de animação** a partir de tiras, atlas ou tags do Aseprite.
- [x] **Player de animação**: play, reinício, parada, velocidade, frame atual, tempo, callbacks de frame e de fim e fila de animações que toca a próxima quando a atual termina (`Animator::queue`, `clearQueue`, `queued`).
- [x] **Tweens**: float, Vec2 e Color em qualquer tabela ou userdata, com easing, atraso, repetição (`repeatCount`, negativo para sempre), yoyo, callbacks, pausa, cancelamento e `wait()` com Promise (`TweenManager` e `haylen.tween`), sequências de passos (tween, `parallel`, `wait` e `call`) e tags para cancelar, pausar e retomar grupos (`cancelTag`, `pauseTag`, `resumeTag`).

### 8.7 Input

- [x] **Teclado** completo com bordas, modificadores e texto UTF-32.
- [x] **Mouse** com botões, posição em janela e design, delta, roda, cursor, visibilidade e captura.
- [x] **Touch** com até dez toques e fases (início, movimento, fim, cancelado).
- [~] **Gamepads**: quatro slots, botões padrão, sticks, gatilhos, zona morta e conexão. GameController (Apple), XInput (Windows), joystick Linux, Gamepad API (web) e eventos Android.
- [x] **Mapa de ações** em JSON: botões e eixos ligados a teclas, mouse, gamepad e controles virtuais, com remapeamento, definição de ações uma a uma, validação estrita das listas de bindings e detecção do último dispositivo. Enquanto a UI captura o ponteiro, os bindings de mouse não disparam ações (um clique no botão de pausa do HUD não vira ataque), e os botões virtuais continuam funcionando.
- [x] **Controles de toque**: `touchStick` (fixo ou flutuante, com zona morta) e `touchButton` como componentes de UI, com vários dedos ao mesmo tempo, alimentando os sticks e botões virtuais do mapa de ações e soltando tudo que deixa de ser desenhado.
- [x] **Gestos**: toque, duplo toque, toque longo, deslizar e pinça (`tap`, `double_tap`, `long_press`, `swipe` e `pinch`), com o mouse valendo como um dedo e limites configuráveis (`GestureRecognizer`, `input.gestures()`, `input.setGestureSettings` e `input.gestureSettings`).

### 8.8 Áudio

- [x] **Engine miniaudio** com barramentos master, música, efeitos, UI e ambiente, com volume e mudo.
- [x] **Sons** WAV, MP3, FLAC e OGG lidos do conteúdo via VFS, decodificados de forma assíncrona.
- [x] **One-shots** com volume, pitch, variação aleatória de pitch (`pitchVariation`, com semente para testes), pan, fade, início deslocado, loop e limite de vozes com roubo da voz mais antiga.
- [x] **Música** em streaming com loop, crossfade, pausa e retomada.
- [x] **Áudio posicional 2D** com ouvinte e atenuação.
- [~] **Ciclo de vida**: pausa ao suspender o app e retoma depois.

### 8.9 Física 2D

- [x] **Mundo Box2D** com pixels por metro, gravidade, substeps e passo fixo. Todas as grandezas entram e saem em unidades de pixel: forças e forças de motor escalam por pixels por metro, e torques e impulsos angulares por pixels por metro ao quadrado (`maxMotorForce` e `maxMotorTorque` separados).
- [x] **Corpos e formas**: estático, cinemático e dinâmico. Círculo, caixa, cápsula, polígono, segmento e cadeia. Densidade, atrito, restituição, sensores, categorias e máscaras.
- [x] **Juntas**: distance, revolute, prismatic, weld, wheel, motor, mouse e filter (desliga a colisão entre dois corpos).
- [x] **Eventos**: contato (início e fim), sensor (início e fim) e impacto, com user data.
- [x] **Consultas**: raycast (mais próximo e todos), AABB e sobreposição de forma.
- [x] **Debug draw** pelas primitivas.

### 8.10 Tiled

- [x] **Mapas** `.tmj`: ortogonal, isométrico, staggered, hexagonal e oblíquo (`skewx` e `skewy` do Tiled 1.12, com conversão de células, objetos, limites e colisão), ordem de render, stagger, lado do hexágono, cor de fundo, origem de parallax e mapas infinitos.
- [x] **Camadas de tiles**: CSV e base64 com zlib, gzip e zstd, dados finitos e chunks, flips incluindo rotação hexagonal de 120 graus.
- [x] **Camadas de objetos**: retângulo, elipse, cápsula (com colisão), ponto, polígono, polilinha, texto (com rotação), objeto de tile, opacidade por objeto, templates `.tj` (JSON, o formato atual) e ordem de desenho.
- [x] **Camadas de imagem** com repetição em x e y.
- [x] **Grupos** aninhados com offset, opacidade, tinta, visibilidade e parallax herdados.
- [x] **Atributos de camada**: opacidade, visibilidade, tinta, offset, parallax, classe e modo de blend (`normal`, `add`, `multiply` e `screen`; os modos que o blend fixo da GPU não reproduz, como `overlay` e `difference`, são rejeitados com erro claro). Travamento é só do editor.
- [x] **Tilesets** embutidos e externos `.tsj`, de imagem e de coleção, margem, espaçamento, offset, alinhamento de objeto, tamanho de render, modo de preenchimento, grade, transformações, terrenos e Wang sets, classe por tile, probabilidade, animações e colisão.
- [x] **Propriedades**: string, int, float, bool, color, file, object, class e list (do Tiled 1.12), incluindo classes e listas aninhadas. Em Lua uma lista vira uma sequência de valores convertidos.
- [x] **Renderização** com culling em todas as orientações, tiles animados, parallax, tinta, opacidade, imagens repetidas, objetos de tile e camadas estáticas em batch.
- [x] **Colisão**: corpos Box2D a partir das formas dos tiles e das camadas de objetos.
- [x] **Spawn de objetos**: `map:objects()` entrega objetos com classe, nome, forma e propriedades, e `map:spawn(fábricas, camada)` chama a fábrica de cada classe com a posição no mundo (offsets de camada e grupo incluídos). Em C++, `TiledObjectFactories` e `TileMap::forEachObject`.
- [x] **Mundos** `.world` com posições e padrões.

### 8.11 Partículas

- [x] **Efeitos**: emissores com formas ponto, círculo, anel, retângulo e cone; taxa, rajadas com tempo, duração com loop, prewarm (a partir da posição do emissor no primeiro update), vida, velocidade, direção e espalhamento, gravidade, amortecimento, acelerações radial e tangencial, giro, tamanho e cores ao longo da vida, animação por frames, blend e espaço local ou mundo. Configurados por tabela Lua, `ParticleConfig` ou arquivo `.particles` (JSON com as mesmas opções, carregado pelo `AssetManager` e aceito por `particles.newEmitter(efeito, ajustes)`).
- [x] **Simulação** em arrays paralelos (SoA) sem alocação depois do aquecimento, com remoção compactando no lugar e atualização em blocos paralelos pelo `JobSystem` (`update(dt, jobs)`, usado pelo Lua). O resultado paralelo é idêntico ao serial.
- [x] **Render** em um batch por emissor.

### 8.12 Luz 2D e dia e noite

- [x] **Mapa de luz** por canvas com luz ambiente e luzes pontuais aditivas.
- [x] **Luzes pontuais** com posição, raio, cor, intensidade, textura, rotação e escala, e cintilação determinística (`lighting.flicker`).
- [x] **Ciclo de dia e noite** com duração configurável, fases (amanhecer, dia, entardecer, noite), gradiente de ambiente, escuridão, contagem de dias e eventos de troca de fase e de dia (`DayNightCycle` e `lighting.newDayNight`).

### 8.13 UI (ImGui + tema, lista do workpane-new)

- [x] **Backend ImGui próprio** (`UiSystem`) que desenha pela pipeline de mesh do `Renderer2D` em coordenadas de design, implementa o protocolo de texturas do 1.92, recebe mouse, touch, teclado, texto, clipboard e navegação por gamepad, mostra o teclado virtual quando um campo pede texto, transforma erros do ImGui em erros de script e se recupera de frames deixados abertos. Validado no Chrome com WebGPU.
- [x] **Temas**: papéis semânticos `ThemeColor`, `ThemeMetric`, `ThemeFont` e `ThemeSurface`, temas `dark` e `light`, temas em JSON sobre uma base com imagens nine-slice e fontes, troca em tempo real e sincronização com o estilo do ImGui. O tema texturizado Tiny Swords fica no sample (`assets/ui/theme.json`).
- [x] **Árvore de componentes retida** (`UiDocument`): propriedades lidas de JSON com validação estrita, patches atômicos validados sobre o estado mesclado, troca de filhos, comandos, medição com cache por frame, propriedades comuns (visível, habilitado, tooltip, crescer, tamanhos, alinhamento), eventos por id de nó entregues antes do update e registro por tipo (`ComponentRegistry`).
- [x] **Containers**: column, row, grid, stack, scroll (com arrasto por toque), card, panel, spacer, divider, tabs, formField, splitter e safeArea.
- [x] **Texto**: label (quebra de linha, alinhamento e contorno para texto sobre o jogo), pageHeader (com faixa), sectionTitle, emptyState e alert.
- [x] **Botões**: button (default, primary, destructive, toolbar, icon e link), imageButton, chip, menuButton e popover.
- [x] **Escolhas**: checkbox, toggle, radioGroup e combo.
- [x] **Entradas**: textField, secretField, textArea, filterField, numberField e slider.
- [x] **Pickers**: colorField.
- [x] **Indicadores**: badge, statusIndicator, busyIndicator, progress (plano e texturizado), icon, image e avatar.
- [x] **Coleções**: list, tree e table.
- [x] **Configurações**: settingsForm, settingsRow e settingsActions.
- [x] **Sobreposições**: dialog (modal, com foco inicial para gamepad e Escape ou B para dispensar), toast e tooltip.
- [x] **Nine-slice e imagens** em qualquer superfície via tema, com bordas esticadas ou repetidas, escala, padding e tinta. Com `colorize`, a imagem é multiplicada pela cor do componente (por exemplo o tom de uma barra), e o preenchimento de barras e sliders fica dentro do padding da superfície `track`. Controles de toque têm superfícies próprias (`stickBase`, `stickKnob`, `touchButton` e `touchButtonPressed`).
- [~] **Texto na web** funcionando (validado no Chrome desktop), com pedido de teclado virtual ao host. Falta validar em navegadores mobile.

### 8.14 Plataforma

- [x] **PlatformBridge**: chamadas JSON do C++ para o nativo com callback assíncrono na thread do frame, e eventos do nativo para assinantes C++.
- [~] **Registros nativos**: `HaylenBridge.register` no Android (handlers Java na main thread), `HaylenBridge` na Apple (blocos), `Module.haylen.register` na web (handlers async em JavaScript) e handlers C++ no desktop. Android, web e desktop validados; Apple precisa do Xcode.
- [~] **Métodos embutidos**: `device.info`, `system.open_url`, `system.locale` (tag BCP 47, como `pt-BR`), `haptics.vibrate` e `app.version` em todas as plataformas. Validados no Android, na web e no desktop; Apple precisa do Xcode.
- [~] **Plugins de exemplo**: `auth.google.signIn` no Tiny Island, fora da engine. No Android, `GoogleSignInPlugin` usa o Credential Manager (`androidx.credentials` 1.6.0 e `googleid` 1.2.1), registrado pelo `TinyIslandApplication` com o client id vindo de `-PgoogleServerClientId`. Na web, o `shell.html` do sample carrega o Google Identity Services sob demanda com o client id da meta tag `google-client-id`. Validados no emulador e no Chrome até a chamada ao Google (erro sem client id e erro do Credential Manager sem conta). Falta validar um login real, que precisa de um client id de um projeto Google Cloud.
- [~] **Safe area**: UIKit, window insets do Android e `env(safe-area-inset-*)` na web, em coordenadas de design.
- [~] **Ciclo de vida**: suspender, retomar, foco, redimensionar, pouca memória (`onTrimMemory` no Android e aviso de memória do UIKit, entregues como evento `low_memory` e sinal `Engine::lowMemory`) e pedido de saída. Android e desktop validados; iOS precisa do Xcode.
- [x] **Projeto Android** com AGP 9.4.1, Gradle 9.8.0, SDK 37, NDK r30, CMake 4.1.2 do SDK e `HaylenActivity` com bridge, insets, remoção de controles, tela cheia imersiva e regras do R8 para as classes chamadas pelo C++.
- [~] **Projetos Apple** para macOS, iOS e tvOS com Info.plist, launch screen e orientação. O app de macOS roda; iOS e tvOS precisam do Xcode.
- [x] **Shell web** com canvas em tela cheia, builds WebGPU e WebGL2 (ambos validados no Chrome), script da bridge com os métodos embutidos, pacote por `?package=` e servidor local do `make.py`.

### 8.15 Utilitários de gameplay

- [x] **Spatial hash** para consultas em mundos grandes: retângulos, círculos, pontos e o mais próximo com filtro, em ordem determinística (`SpatialHash` e `haylen.spatial`, que guarda qualquer valor Lua).
- [x] **A\*** em grade com custos, diagonais sem cortar cantos, linha de visão e suavização de caminho (`NavGrid` e `haylen.navigation`).
- [x] **Steering**: seek, flee, arrive, wander determinístico por semente e separação, com integração limitada por força e velocidade (`SteeringAgent`, `Wanderer` e `navigation.newAgent`).
- [x] **Máquina de estados** genérica com enter, update e exit, trocas pedidas durante uma transição enfileiradas em ordem, argumentos para o enter em Lua e `onChange` (`StateMachine` e `haylen.ai`).
- [x] **Localização**: tabelas JSON por idioma (uma pasta de assets com um arquivo por idioma), chaves aninhadas, idioma de fallback, argumentos `{nome}`, plurais zero, one e other, escolha do idioma mais próximo de uma tag BCP 47 e troca em tempo real (`Localization`, `LocalizationPlugin` e `haylen.localization`).
- [x] **Save**: slots JSON no armazenamento do usuário com escrita atômica, resumo para menus, data e listagem do mais novo para o mais antigo (`SaveSlots` e `haylen.save`).
- [x] **Configurações persistentes**: chaves com caminho pontuado, gravação ao suspender e ao parar, e `capture`/`apply` explícitos para volumes e mudo dos barramentos, tela cheia e mapa de ações (`Settings`, `SavePlugin` e `haylen.settings`).

### 8.16 Debug

- [x] **Profiler** com escopos de CPU aninhados e somados por frame, histórico de tempos de frame e fases da engine medidas (scripts, fixedUpdate, update, render e submit).
- [x] **Overlay** ImGui (F3 por padrão) com FPS, gráfico de frame, escopos do profiler, estatísticas do renderer, assets pendentes, vozes de áudio e as últimas linhas de log de engine e scripts. O debug de física fica com `world:debugDraw` do jogo.

### 8.17 Testes

- [x] **GoogleTest** v1.18.0 via ctest.
- [x] **Runtime headless** com backend dummy do Sokol, host headless, áudio sem dispositivo e runtime do Varn.
- [x] **Testes dos bindings Lua** executando Lua pela engine headless.
- [x] **Cobertura** da engine o mais perto de 100%, excluindo só backends que exigem o SDK da plataforma. Última medição (`make.py coverage`, 345 testes): 95,5% das linhas, 96,7% das funções, 93,9% das regiões e 85,9% dos ramos.
- [x] **ThreadSanitizer**: a suíte inteira roda sem nenhum relato de corrida.

### 8.18 Bindings Lua por módulo

- [x] `haylen` (engine, versão, tempo, sair, configuração do pacote)
- [x] `haylen.math` (Vec2, Rect, Color, Transform2D, easing, ruído, geometria, Poisson disk, Random)
- [x] `haylen.scene` (cenas Lua, pilha e transições)
- [x] `haylen.jobs` e `haylen.timer`: timers no relógio do frame, e `jobs.spawn(fn, ...)` para trabalho longo em Lua, que roda como corrotina dentro de um orçamento de tempo por frame (`jobs.setBudget`), pausa em `jobs.checkpoint()` quando o orçamento acaba e devolve uma Promise do Varn com o resultado. O estado Lua é único, então CPU em Lua não vai para workers, e o async do Varn cobre I/O.
- [x] `haylen.log` e `haylen.signal`: log com níveis e `signal.new()` com `connect` (devolve uma conexão com `disconnect`), `emit`, `clear` e `size`.
- [x] `haylen.assets` (carga síncrona e assíncrona com Promise, grupos de preload, progresso, unload)
- [x] `haylen.storage`, `haylen.save` e `haylen.settings`
- [x] `haylen.window` e `haylen.viewport` (janela, tela cheia, cursor, escala, safe area)
- [x] `haylen.graphics` (texturas, render targets, canvases, câmeras, fontes e texto, sprites, batches, primitivas, nine-slice, pós-processo, estatísticas)
- [x] `haylen.animation` (clipes, conjuntos e player de sprites)
- [x] `haylen.tween`
- [x] `haylen.input` (dispositivos, mapa de ações, controles virtuais e gestos)
- [x] `haylen.net` (WebSocket com eventos `open`, `message`, `close` e `error`)
- [x] `haylen.audio`
- [x] `haylen.physics`
- [x] `haylen.tiled`
- [x] `haylen.particles`
- [x] `haylen.lighting` (ciclo de dia e noite e cintilação; as luzes são desenhadas por `haylen.graphics.drawLight`)
- [x] `haylen.navigation` (grade, A\* e steering) e `haylen.spatial`
- [x] `haylen.ai` (máquina de estados)
- [x] `haylen.ui` (temas, fontes, componentes com `ui.button{...}`, documentos, eventos `onClick` e afins, captura de ponteiro) e `haylen.imgui` (janelas e widgets imediatos)
- [x] `haylen.platform` (bridge JSON, eventos nativos, safe area, ciclo de vida)
- [x] `haylen.localization`
- [x] `haylen.debug` (profiler, escopos, overlay e log recente)

## 9. Jogo: Tiny Island

### 9.1 Conceito

O jogo é escrito em Lua e fica em `samples/games/tiny-island/game/` (`game.json`, `main.lua`, módulos em `scenes/`, `entities/`, `systems/`, `ui/` e os assets em `assets/`). O app de cada plataforma é gerado por `samples/games/tiny-island/CMakeLists.txt` com `haylen_add_game`.

Sobreviver o maior número de noites numa ilha. Durante o dia o jogador corta árvores e alimenta a fogueira. À noite os inimigos aparecem e só a luz da fogueira mantém um círculo seguro. Quanto menos madeira, menor o círculo.

### 9.2 Fluxo de telas

1. Boot: escolhe o idioma do aparelho no primeiro uso (`system.locale` e `localization.bestMatch`) e faz o preload dos grupos `boot` e `menu` com barra de progresso.
2. Menu principal: a ilha ao entardecer ao fundo, com os cinco sobreviventes em volta da fogueira e a câmera passeando devagar, título numa fita (ribbon) do Tiny Swords, botões Jogar, Configurações e Sair (só no desktop), recorde e música do menu.
3. Seleção de classe: a câmera enquadra o sobrevivente escolhido na fogueira, que comemora com a animação de ataque, com avatares clicáveis, folha de papel com a descrição do especial e barras de atributos.
4. Loading: preload do grupo `gameplay` com barra de progresso, com o sobrevivente esperando na fogueira.
5. Gameplay com HUD.
6. Pausa, configurações e tela de fim de jogo com dias sobrevividos, inimigos derrotados e recorde salvo, como cenas transparentes sobre o jogo escurecido e dessaturado.

### 9.3 Classes

| Classe | Vida | Velocidade | Dano | Alcance | Recarga | Corte | Carga de madeira | Especial |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Warrior | 140 | 300 | 30 | 110 (corpo a corpo) | 0,6 s | 1,0 | 6 | Guarda que reduz 70% do dano enquanto segurada |
| Archer | 90 | 330 | 22 | 700 (flechas físicas) | 0,7 s | 0,6 | 5 | Flechas atravessam um inimigo |
| Lancer | 120 | 310 | 34 | 150 (estocada direcional) | 0,8 s | 0,8 | 6 | Estocada com empurrão |
| Monk | 100 | 300 | 14 (pulso em área) | 220 | 1,2 s | 0,5 | 4 | Cura a si mesmo |
| Pawn | 80 | 360 | 12 | 90 | 0,5 s | 2,0 | 10 | Corta árvores muito mais rápido |

### 9.4 Mapa

- Gerado por `tools/generate_island_map.py` e salvo como `samples/games/tiny-island/game/assets/maps/island.tmj`, editável no Tiled.
- Camadas: fundo de água, espuma animada na costa, grama com bordas por autotile (Wang set de bordas), platô elevado com penhascos e sombra, decorações (arbustos, pedras, pedras na água, nuvens em parallax), objetos.
- Objetos: posição da fogueira, posição inicial do jogador, regiões de spawn de árvores, pontos de spawn de inimigos na costa e colisões da borda da ilha e dos penhascos.

### 9.5 Árvores e madeira

- Espalhadas com Poisson disk dentro das regiões do Tiled, longe da fogueira e da água.
- Balançam com o vento (animação de 8 frames).
- Cada golpe faz a árvore tremer, solta lascas (partículas) e toca som. Ao chegar a zero, vira toco e solta de 2 a 3 madeiras.
- Tocos voltam a ser árvores depois de dois dias.
- A madeira é coletada ao passar por cima, até a capacidade da classe.
- Entregar madeira na fogueira adiciona combustível.

### 9.6 Fogueira e círculo seguro

- Combustível máximo 100. Cada madeira adiciona 8.
- Queima 0,1 por segundo de dia e 0,25 por segundo à noite. A cada novo dia perde 15 de uma vez.
- Raio do círculo: de 90 a 520 pixels, proporcional ao combustível.
- O círculo é uma barreira física filtrada só para inimigos e é desenhado no chão, encolhendo junto com o fogo.
- Com combustível zero o fogo apaga e o círculo some.

### 9.7 Dia e noite

- Dia de 150 s e noite de 90 s, com amanhecer e entardecer de 15 s.
- Luz ambiente em gradiente, luz da fogueira com cintilação e luz pequena ao redor do jogador.
- Crossfade de música entre dia e noite e jingles nas trocas de fase.

### 9.8 Inimigos

- Só nascem à noite, nos pontos de spawn do Tiled. A quantidade cresce a cada noite.
- Tipos: guerreiro vermelho (corpo a corpo), arqueiro vermelho (à distância, mantém distância), lanceiro vermelho (resistente). Unidades pretas aparecem a partir da quarta noite.
- Movimento com A* e steering (separação entre inimigos), sem entrar no círculo seguro.
- Ao amanhecer os que sobraram fogem e queimam com efeito de explosão.

### 9.9 Combate

- Ataques corpo a corpo e à distância, flechas como projéteis físicos, flash branco ao tomar dano, empurrão, números de dano e mortes com explosão.

### 9.10 HUD

- Vida, combustível da fogueira e recarga do especial em barras de madeira coloridas por tom, madeira carregada, dia, contagem regressiva até a próxima fase, número de invasores e avisos de entardecer, amanhecer e fogo apagado, tudo dentro da safe area e feito com a UI temática.

### 9.11 Controles

| Ação | Teclado e mouse | Gamepad | Toque |
| --- | --- | --- | --- |
| Mover | WASD ou setas | Stick esquerdo ou direcional | Joystick virtual |
| Atacar ou cortar | Espaço ou clique esquerdo | A (sul) | Botão virtual |
| Especial | Shift ou clique direito | X (oeste) | Botão virtual |
| Interagir | E | B (leste) | Botão virtual |
| Pausa | Esc | Start | Botão de pausa |

Os controles de toque só aparecem depois que a tela é tocada (`touchOnly`) e podem ser desligados nas configurações. O jogo pausa sozinho quando perde o foco ou vai para o segundo plano.

### 9.12 Áudio

Todos os sons são CC0. Os créditos ficam em `samples/games/tiny-island/game/assets/audio/CREDITS.md`.

| Uso | Origem |
| --- | --- |
| Clique, confirmar, voltar e erro | Kenney Interface Sounds |
| Machado | Kenney RPG Audio |
| Árvore caindo | Kenney Impact Sounds |
| Pegar madeira e alimentar a fogueira | OpenGameArt "80 CC0 RPG SFX" |
| Fogo crepitando (loop) | OpenGameArt "Fire Crackling" |
| Espada, impacto e passos | OpenGameArt "RPG Sound Pack" e Kenney Impact Sounds |
| Morte de inimigo e dano no jogador | OpenGameArt "80 CC0 RPG SFX" e "80 CC0 creature SFX" |
| Flecha | OpenGameArt "Battle Sound Effects" |
| Cura | OpenGameArt "Cure Magic" |
| Jingles de dia, noite e fim de jogo | Kenney Music Jingles |
| Música do menu e do dia | RandomMind "The Old Tower Inn" e "Market Day" |
| Ambiente da noite | JaggedStone "Loopable Dungeon Ambience" |

### 9.13 Recursos da engine exercitados pelo jogo

- [x] Tudo pela API Lua: cenas e transições, preload com progresso e `:await()`, UI temática com nine-slice, mapa Tiled com colisão e spawn, Poisson disk, animação de sprites, tweens, partículas, luz 2D e dia e noite, física com filtros e sensores, A\* e steering, áudio com barramentos e crossfade, mapa de ações com teclado, mouse, gamepad e toque, save de recorde e configurações, bridge de plataforma (`device.info` e login Google), localização (inglês e português), overlay de debug.

### 9.14 Itens do jogo

- [x] **Importação do Tiny Swords**: `make.py assets <zip>` copia o pacote para `samples/games/tiny-island/game/assets/tiny_swords/` com nomes em `snake_case`, gera as peças de nine-slice da UI e versões claras das barras para o tema colorir.
- [x] **Jogo em Lua** usando só a API Lua da engine e os módulos do Varn.
- [x] **Pacote em zip**: o mesmo jogo roda a partir de `tiny-island.zip` no player desktop (Metal) e no navegador (WebGPU).
- [x] **Mapa da ilha** gerado e editável no Tiled.
- [x] **Menu principal** com a ilha ao fundo e a logo.
- [x] **Seleção de classe** com cinco classes.
- [x] **Loading** com preload.
- [x] **Árvores** com vento, corte, tocos e rebrota.
- [x] **Madeira e fogueira** com consumo diário e contínuo.
- [x] **Círculo seguro** físico e visual.
- [x] **Dia e noite** com luz e música.
- [x] **Inimigos** noturnos com A\* e steering.
- [x] **Combate** completo.
- [x] **HUD** temático.
- [x] **Pausa, configurações e fim de jogo** com recorde.
- [x] **Controles** em todos os dispositivos. Teclado, mouse e toque foram testados no navegador. O gamepad usa o mesmo mapa de ações e não foi testado com um controle físico.
- [x] **Áudio** completo com créditos.
- [x] **Demo da bridge** nas configurações. O login Google real depende do client id do projeto no Google Cloud.

## 10. Editor web (projeto futuro, em outro repositório)

O editor web não faz parte deste repositório, mas a engine é construída desde já para funcionar dentro dele. Tudo o que o editor vai precisar da engine está listado aqui.

### 10.1 Visão

- Um site onde o usuário edita o código Lua e os assets do jogo no navegador e roda o jogo na hora, como aplicação WebAssembly com WebGL2 ou WebGPU.
- O runtime web da engine é o mesmo usado para publicar o jogo na web, só que controlado pela página.

### 10.2 O que a engine precisa oferecer ao editor

- [x] **Carregar pacote em memória**: montar o jogo a partir de um `.zip` recebido pela página (`loadZip`) ou de um conjunto de arquivos (`MemoryPackage` com `setFile` e `run`), sem precisar de `--preload-file`. A página também pode indicar um zip por `?package=`.
- [x] **API JavaScript** em `Module.haylen`: `loadZip(bytes)`, `clearFiles()`, `setFile(caminho, bytes | texto)`, `removeFile(caminho)`, `run()`, `restart()` (recarrega os scripts), `stop()`, `reloadAsset(caminho)`, `pause()`, `resume()`, `setPaused(bool)`, `paused()`, além de `register`, `unregister` e `emit` da bridge. O canvas é o `Module.canvas` que a página entrega. Validado no Chrome com WebGPU e WebGL2.
- [x] **Eventos para a página**: `onLog(nível, linha)`, `onError({message, file, line, traceback})` (via `lua::LuaError`), `onStarted({name, identifier, version})`, `onStopped()` e `onStats` uma vez por segundo com FPS, tempos de frame, contadores do renderer, assets, vozes e escopos do profiler. Os callbacks rodam logo depois do frame, então podem chamar o runtime (até reiniciar o jogo).
- [x] **Reinício limpo** do jogo sem recarregar a página: cada início cria um `Engine` novo (cenas, estado Lua com novo `Runtime` do Varn, assets, áudio, física e recursos de GPU), e o anterior é destruído entre frames.
- [x] **Hot reload**: no desktop, `HotReloadPlugin` observa a pasta do pacote em desenvolvimento, recarrega texturas no lugar e reinicia o jogo quando um script muda. Na web, o editor usa `setFile` com `reloadAsset` para assets e `restart` para scripts.
- [x] **Tela de erro**: um erro em script não derruba o runtime. A engine mostra o erro na tela, avisa a página e continua pronta para receber a correção.
- [x] **Backend escolhido pela página**: `make.py web --target <alvo>` gera as builds WebGPU e WebGL2 numa pasta com `index.html` que usa WebGPU quando o navegador oferece um adaptador e WebGL2 no resto. `?backend=webgpu` ou `?backend=webgl2` força um deles. A página é o shell do próprio alvo (o `WEB_SHELL` do jogo ou o shell da engine) com o script de escolha do backend (`engine/platform/web/backend-picker.html`) no lugar do `{{{ SCRIPT }}}`, então plugins de página como o login Google do Tiny Island também funcionam no pacote duplo (validado no Chrome).
- [x] **Canvas controlado pela página**: o runtime usa o `Module.canvas` que a página fornecer (com `id`) e acompanha o tamanho do elemento com um `ResizeObserver`, sem assumir a página inteira.
- [x] **Sem requisitos especiais de hospedagem**: o runtime web é single-thread (como o Varn, e o `JobSystem` roda os trabalhos no próprio frame), então não depende de COOP e COEP nem de `SharedArrayBuffer`.
- [x] **Bridge de plataforma na web**: plugins em JavaScript registrados pela página (`Module.haylen.register`, inclusive em `Module.preRun`), com handlers assíncronos. O exemplo de login com Google está no `shell.html` do Tiny Island.
- [x] **Sockets e HTTP** na web através de JavaScript: o cliente HTTP do Varn usa o fetch do navegador, e `haylen.net.websocket` usa o `WebSocket` do navegador (validados no Chrome). Navegadores não têm TCP bruto, então o módulo `socket` do Varn só existe no nativo.
- [x] **Documentação da API Lua** completa e navegável (`docs/lua-api.md` e `docs/lua-api/`), que o editor pode usar para autocomplete e ajuda.

## 11. Documentação

- [x] **README.md**: visão geral, requisitos, início rápido e links para os guias. O Tiny Island tem o próprio `samples/games/tiny-island/README.md`.
- [x] **docs/architecture.md**: módulos, dependências, frame, threads e posse de recursos.
- [x] **docs/build.md**: `make.py`, plataformas, deploy dos assets e cobertura.
- [x] **docs/platform_bridge.md**: protocolo JSON, handlers nativos e plugins.
- [x] **docs/ui.md**: temas, componentes e JSON de telas.
- [x] **docs/tiled.md**: recursos suportados e convenções do jogo.
- [x] **docs/rendering.md**: renderer, performance e milhões de sprites.
- [x] **docs/audio.md**, **docs/input.md** e **docs/testing.md**.
- [x] **docs/lua.md**: modelo Lua, pacote do jogo, cenas em Lua, async com Varn e como estender a engine com bindings próprios.
- [x] **docs/lua-api.md** e **docs/lua-api/<modulo>.md**: referência completa de cada módulo Lua com exemplos executáveis (os exemplos dos módulos 2D foram executados na engine headless).
- [x] **docs/embedding.md**: como usar a engine como biblioteca em outro projeto CMake.

## 12. Fases de trabalho

| Fase | Conteúdo | Status |
| --- | --- | --- |
| 1 | Estrutura, CMake com CPM, dependências (incluindo Varn), CLAUDE.md e este plano | Concluída |
| 2 | Core e matemática com testes (nomes em camelCase) | Concluída |
| 3 | Integração com o Varn: runtime, jobs, log, pacote do jogo (pasta e zip), loader Lua, toolkit de bindings | Concluída |
| 4 | Host (real e headless), janela, eventos e input | Concluída |
| 5 | Gráficos base e 2D (renderer, texturas, câmera, texto, primitivas, luz) | Concluída |
| 6 | Assets, preload e áudio | Concluída |
| 7 | Animação, tweens, partículas, física, Tiled, navegação | Concluída |
| 8 | UI: backend ImGui, temas e componentes | Concluída |
| 9 | Bindings Lua de todos os módulos com testes | Concluída. Todas as capacidades públicas em C++ levantadas na revisão das páginas da API têm binding, teste e documentação. Registrar tipos novos de componente de UI só é possível em C++, porque o registro recebe fábricas C++ |
| 10 | Plataformas: bridge, safe area, gamepads, Android, Apple, web, player | Concluída, com os itens marcados `[~]` dependentes de hardware ou contas |
| 11 | Ferramentas: importador do Tiny Swords, gerador de mapa, empacotador zip, áudio | Concluída |
| 12 | Jogo Tiny Island em Lua | Concluída |
| 13 | Testes e cobertura até o máximo possível | Concluída (345 testes, 95,5% das linhas) |
| 14 | Documentação e revisão final (bugs, legado, não utilizado, race conditions, crashes) | Concluída. A revisão final corrigiu 30 defeitos (concorrência, tempo de vida, iteração durante callbacks, pilha Lua, crashes e código morto) com testes de regressão |

## 13. Limitações do ambiente atual

- Esta máquina tem o Xcode 27 com os simuladores de iOS e tvOS. Aparelhos físicos precisam do time de desenvolvimento da Apple (`HAYLEN_APPLE_TEAM`).
- O Emscripten não está instalado. O `make.py` instala o emsdk 6.0.10 em `.tools/` quando o build web é pedido.
- O Varn ainda não tem uma API pública para registrar módulos nativos de terceiros. A engine usa o `Runtime` e o estado Lua do núcleo C++ do Varn. Uma API oficial de extensão no Varn (por exemplo `Runtime::addNativeModule`) deixaria essa integração mais limpa.
- O Varn compila o OpenSSL e o Poco a partir do código-fonte, então o primeiro build de cada plataforma é demorado.
- O Varn só escolhe o driver HTTP de um alvo móvel quando o próprio cache já guarda o alvo, o que não acontece na primeira configuração (ele testa `DEFINED CACHE{VARN_TARGET}` antes de criar essa entrada). A engine passa `VARN_HTTP_CLIENT_DRIVER` explicitamente no Android, no iOS e no tvOS.
- No nativo, um WebSocket liberado enquanto ainda conecta (reinício, saída ou coleta de lixo) segura aquele frame até a tentativa terminar, no máximo os 10 segundos do timeout de conexão, porque a thread do socket sempre termina antes dele. Soltar a thread arriscaria falhas na destruição dos estáticos do Poco e do OpenSSL ao sair, e uma conexão interrompível exigiria refazer o handshake do Poco em nível mais baixo, sem poder interromper a resolução de DNS.
- O Varn só escreve no log o erro que escapa de uma tarefa de `async.spawn` ou `async.run`. A engine instala as duas funções por cima das do Varn, rodando cada tarefa num `xpcall` que leva o erro e a pilha da corrotina para a tela de erro e para o `onError` da página. Um gancho oficial de erros de tarefa no Varn deixaria essa integração mais limpa.

## 14. Segundo pedido: decisões e checklist

Esta seção cobre os itens 51 a 82 da seção 2.1. As decisões vieram da pesquisa em código-fonte de referência, no código do Sokol e do miniaudio e no código atual da engine.

### 14.1 Decisões de organização

- **Nome do produto**: a engine passa a se chamar Haylen, sem o "2D", porque o 3D virá depois. O pacote Java passa a ser `dev.haylen`.
- **Nomes genéricos**: o que o desenvolvedor cria é um app (jogo, aplicação multimídia ou app). `game.json` vira `app.json`, `haylen_add_game` vira `haylen_add_app`, o pacote embarcado fica em `app/` e o zip se chama `app.zip`. A palavra "game" só aparece onde é sobre jogos de verdade (gamepad, o sample Tiny Island).
- **Regra 2D e 3D**: um tipo público declarado numa pasta `2d/` cujo conceito também existe em 3D termina com `2D` (`Camera2D`, `Sprite2D`, `PhysicsWorld2D`, `ParticleEmitter2D`). Os tipos 3D do futuro terminam com `3D`. Nomes que só existem em 2D (`Tiled*`, `TileMap`, `NineSlice`, `SpriteAtlas`) ficam como estão. Os módulos Lua dos subsistemas 2D terminam com `2d` (`haylen.graphics2d`, `haylen.physics2d`, `haylen.particles2d`, `haylen.lighting2d`, `haylen.animation2d`, `haylen.navigation2d`, `haylen.spatial2d`). `haylen.graphics` fica só com o que não tem dimensão (texturas, render targets, backend e, depois, shaders). `Font` sai de `2d/` e vai para `text/`, porque uma fonte serve às duas dimensões.
- **Formato do pacote**: `app.json` (nome, identificador, versão, orientação, janela, resolução de design, logo e cor de fundo do carregamento web), `source/` com `main.lua` como ponto de entrada e os outros módulos Lua, e `content/` com os recursos. `require("scenes.menu")` procura `source/scenes/menu.lua`. Todo caminho de recurso é relativo a `content/`. Uma pasta opcional `platform/<plataforma>/` no app guarda personalizações por plataforma.
- **Templates**: a pasta `templates/` na raiz guarda o app inicial em `templates/app` (usado pelo `make.py new`) e os projetos prontos por plataforma em `templates/platform/<plataforma>/`, que só esperam o pacote: `templates/platform/apple` (projeto XcodeGen com o `.xcodeproj` já gerado ao lado do `project.yml`, com alvos iOS, iPadOS, Mac Catalyst, tvOS e macOS), `templates/platform/android` (projeto Gradle que usa o AAR da engine) e `templates/platform/web` (página com a logo, a barra de progresso, a escolha entre WebGPU e WebGL2 e o carregador). É modular e extensível: uma plataforma nova é uma pasta nova em `templates/platform/` e o seu handler de build e execução no `make.py`, que descobre os templates pelas pastas. O `.xcodeproj` e o `build.gradle.kts` nunca mudam por app: nome, identificador, versão e orientação vêm de arquivos gerados pelo `make.py` (`App.xcconfig` e `Info.plist` na Apple, `gradle.properties` no Android e `config.json` na web).
- **Montagem**: rodar um app apaga e recria `build/apps/<app>/<plataforma>/`, copia o template da plataforma, aplica por cima a pasta `platform/<plataforma>/` do app (mesmo caminho substitui, arquivo novo é adicionado), injeta o pacote e roda. O Tiny Island leva para `platform/android` e `platform/web` só o login Google.
- **Artefatos prontos da engine**: `build/artifacts/` com um `manifest.json` (versão da engine e hashes) e reconstrução automática quando a engine muda.
  - `apple/Haylen.xcframework`: biblioteca estática com engine, dependências e player Lua, com os slices macOS (arm64 e x86_64), iOS, simulador iOS, Mac Catalyst, tvOS e simulador tvOS, e os headers públicos (`haylen_main` e `HaylenBridge`). O template leva um `main.mm` mínimo que chama `haylen_main`, que também é o lugar para registrar handlers nativos da bridge.
  - `android/`: o AAR `haylen` (arm64-v8a, armeabi-v7a para TVs Android de 32 bits, x86_64), publicado num repositório Maven local para levar as dependências transitivas (Kotlin do transporte HTTP do Varn).
  - `web/`: `haylen.js` e `haylen.wasm` para WebGPU e para WebGL2. O wasm é mesmo pré-compilado, porque o player web carrega o pacote em tempo de execução: só o `app.zip` muda de um app para outro.
  - `desktop/`: o player `haylen` para rodar apps no macOS, Windows e Linux com hot reload.
- **Comandos do make.py**:
  - `engine [apple|android|web|desktop|all]` gera os artefatos.
  - `new <pasta> [--name --identifier --orientation]` cria um projeto com `app.json`, `source/main.lua`, `content/` de exemplo e a cópia de todos os templates em `platform/`, para o desenvolvedor personalizar.
  - `run <app|sample> [--platform macos|windows|linux|ios|ios-simulator|tvos|tvos-simulator|catalyst|android|web] [--device]` roda um app Lua (desktop quando nenhuma plataforma é passada).
  - `run-cpp <projeto|sample> [--platform ...]` roda um projeto C++, que compila a engine pelo CMake (`add_subdirectory`, CPM ou `find_package`).
  - `package <app>` gera o `app.zip`.
  - `serve <pasta> [--port]` sobe um servidor Python com MIME do wasm, COOP, COEP (com opção `credentialless` ou desligado para páginas que carregam scripts de terceiros, como o login Google), CORP, CORS, sem cache e com arquivos pré-comprimidos.
- **Plataformas Apple além de iOS, tvOS e macOS**:
  - Mac Catalyst compila como iOS (UIKit). Teclado e mouse entram pelo `GCKeyboard` e `GCMouse`, porque o Sokol só entrega toque no Catalyst.
  - visionOS: o Sokol usa `UIScreen` em nove lugares e o `UIScreen` não existe no SDK do visionOS, então um app nativo de visionOS depende de suporte do Sokol. O app iOS roda no Apple Vision Pro como app de iPad compatível.
  - watchOS é impossível: o SDK do watchOS 27 não tem Metal, MetalKit, GameController nem AudioToolbox.

### 14.2 Checklist

#### A. Nomes, pacote e estrutura

- [x] Engine renomeada para Haylen em tudo (namespace `haylen`, headers em `haylen/`, alvos `haylen::engine` e `haylen::runtime`, `haylen_add_app`, módulos Lua `haylen.*`, pacote Java `dev.haylen`, `Module.haylen` na web, `Haylen.xcframework`, `haylen_main`, player `haylen`, arquivos `haylen-*.cmake`, docs, README, CLAUDE.md e a logo com um H de peças, inclusive os derivados do Android e da Apple).
- [x] `game` vira `app` em tudo (arquivos, CMake, Gradle, JavaScript, `make.py`, docs e mensagens).
- [x] Pacote com `app.json`, `source/` e `content/`, com hot reload separando scripts (`source/` e `app.json`) de recursos (`content/`). Só essas três entradas são o pacote em todas as plataformas.
- [x] Módulos Lua 2D com sufixo `2d`, `haylen.graphics` separado de `haylen.graphics2d`, `Font` em `text/` e `FloatRange` em `math/`. Os nomes dos tipos 2D saem dos namespaces do grupo P (`physics2d::World`).
- [x] Ciclo de dia e noite fora da engine, reescrito em Lua no Tiny Island (`source/systems/day-night.lua`). `lighting2d.flicker` continua como utilitário genérico (`LightFlicker`).
- [ ] CLAUDE.md, README e guias atualizados com a nova estrutura e a regra 2D e 3D.

#### B. Distribuição, templates e comandos

- [x] AAR da engine com as três ABIs (arm64-v8a e x86_64 alinhadas em 16 KB, armeabi-v7a), repositório Maven local em `build/artifacts/android/maven` e template Android sem C++, com LEANBACK, banner de TV, toque opcional e controle declarado.
- [x] `Haylen.xcframework` com seis slices (macOS, iOS, simulador iOS, Mac Catalyst, tvOS e simulador tvOS), `haylen_main` em `haylen/platform/apple/HaylenMain.h`, template XcodeGen com o `App.xcodeproj` gerado junto e alvos iOS e iPadOS com Mac Catalyst, tvOS e macOS. Rodado no simulador iOS, no simulador tvOS, no Mac Catalyst e no alvo macOS.
- [~] Teclado e mouse no Mac Catalyst (`GCKeyboard` e `GCMouse`). Teclas e posição do ponteiro validadas. Botões direito e do meio e a roda estão implementados, mas eventos sintéticos não chegam ao GameController, então falta validar com mouse de verdade.
- [x] Versões mínimas o mais baixas que a toolchain permite: a leitura de números decimais usa o `fast_float` 8.3.0, e o limite passou a ser o `std::format` com ponto flutuante. iOS e tvOS 16.3, macOS 13.3, Mac Catalyst 16.4 (o 16.3 do Catalyst equivale ao macOS 13.2) e Android API 27 (o miniaudio só usa AAudio a partir da 27, e o OpenSL ES não é compilado). Rodado num emulador Android 8.1.
- [x] Wasm pré-compilado (WebGPU e WebGL2) e template web com logo do app ou da engine, barra de progresso real (wasm e `app.zip`), checagem de recursos do navegador e tela de erro. O pacote chega ao runtime por `Module.haylen.packageData`.
- [x] Logo da engine (um H de peças, `templates/platform/web/haylen-logo.svg`) usada quando o app não tem logo, com os derivados gerados a partir dela (splash e ícones do Android, ícones e splash da Apple, banner de TV e o `logo.png` do app inicial).
- [x] Splash nos templates Apple (LaunchScreen com Auto Layout em iOS, iPadOS, Mac Catalyst e a imagem de abertura do tvOS) e Android (SplashScreen API mantida na tela até o primeiro frame da engine, sem tela preta no meio), funcionando em retrato e paisagem em celulares, tablets e TVs, com logo e cor de fundo do `app.json` (`splash`) e a logo da engine quando o app não tem uma.
- [x] Player desktop como artefato (`make.py engine --platform desktop`).
- [x] `make.py engine`, `new`, `run`, `run-cpp`, `package` e `serve` como descritos em 14.1, com montagem em `build/apps/<app>/<plataforma>/`, personalização por `platform/<plataforma>/` e plataformas descobertas pelas pastas de `templates/platform/` (uma tabela `RUN_TARGETS` no `make.py`). O `serve` entrega isolamento de origem (`crossOriginIsolated` verdadeiro no Chrome) e aceita `--coep off`.
- [x] Modo de desenvolvimento explícito (`--dev`) no player, sem hot reload em apps publicados. A web nunca passa `--dev`.
- [x] Rodar os samples Lua em macOS, simulador iOS, simulador tvOS, Mac Catalyst, emulador Android e web, e o sample C++ em macOS e web. Os 26 samples rodaram sem crash na web (WebGPU e WebGL2, 60 fps), no iPhone, no iPad e no Android, com o menu, testes abertos por toque e o voltar. No tvOS rodaram o Tiny Island, a UI com o controle remoto e um sample por categoria, e no Catalyst o Tiny Island e a UI abriram. Faltam Windows e Linux (não rodam nesta máquina) e aparelhos físicos.

#### C. Cenas e transições

- [x] Pilha completa: `push`, `pop`, `replace`, `popToRoot`, `popTo(nível)`, acesso por índice (`scene.at`) e listagem (`scene.list`), cada troca devolvendo uma Promise.
- [x] Captura da cena inteira num render target (`Renderer::beginCapture` e `endCapture`, inclusive canvases com luz, pós-processamento e UI), com as duas cenas vivas durante a transição, cada uma na sua imagem. A cena que sai só sai no update que chega ao ponto de saída do efeito, antes do frame ser desenhado.
- [x] Transições: 24 efeitos em `graphics2d::SceneTransition` (fade por cor, crossfade, move in, slide in, push, shrink grow, flip X, flip Y, zoom flip, rotozoom, jump zoom, split de colunas e linhas, tiles, fades direcionais, page turn, wipes radiais, horizontais e verticais, íris, dissolve e pixelate), com 8 direções e easing, e o shader `blend.glsl` para os efeitos por máscara.
- [x] Transições próprias em Lua e C++: a interface `TransitionEffect` e a tabela `effect = {switchProgress, exitProgress, render(self, progress, outgoing, incoming)}` em Lua, com as texturas das duas cenas.
- [x] Input bloqueado durante a transição (opcional), tempo sem escala (a transição anda mesmo com o jogo pausado), easing e aviso de fim (callback, Promise e os ganchos `transitionStarted` e `transitionFinished` da cena).

#### D. Ciclo de vida, áudio e segundo plano

- [x] Estados do app `active`, `inactive` e `background` com sinal e evento (`haylen.appState`), vindos de suspender, retomar, foco, aba escondida na web (`visibilitychange`), `pagehide` e interrupções (ligação no Android deixa o app inativo).
- [ ] Em segundo plano: nenhum frame e nenhum trabalho de GPU, áudio suspenso, input solto (teclas, toques e controles virtuais), configurações salvas, `UserStorage` gravado (na web também no `pagehide`) e evento para o app salvar o que quiser.
- [x] Na volta: o primeiro delta é zero (sem salto de tempo em timers, tweens e física) e o áudio volta.
- [x] Contexto de áudio da engine (`audio::Device` dono do contexto e do dispositivo do miniaudio, `audio.iosSession` e `audio.mixWithOthers` no `app.json`, AAudio com uso `game`): categoria da sessão no iOS configurável no `app.json` (`ambient` por padrão, que respeita a chave de silêncio e mistura com outros apps, `soloAmbient` ou `playback`), uso `game` no AAudio do Android e foco de áudio no Android.
- [x] Interrupções de áudio (ligação, alarme, Siri), num caminho só para as notificações do dispositivo e os eventos da plataforma, e com o áudio voltando sempre que o app fica ativo de novo, porque o iOS nem sempre avisa o fim da interrupção (falta validar num aparelho real): pausa no início e retomada no fim só com o app ativo, com eventos `audio_interrupted` e `audio_resumed`, e troca de rota (fone desconectado) com o evento `audio_route_changed`.
- [x] Web: desbloqueio do `AudioContext` em qualquer toque, clique ou tecla, e de novo quando o navegador suspender o áudio.
- [x] Opções no `app.json` (bloco `lifecycle`): congelar no segundo plano (ligado por padrão), congelar na perda de foco e silenciar na perda de foco, também por `haylen.setLifecycle`. O congelamento do ciclo de vida (`haylen.halted`) é separado da pausa do jogo.

#### E. Pausa da partida

- [x] `Engine::setPaused`, `isPaused` e o sinal `pausedChanged`, com `haylen.setPaused`, `haylen.paused`, os eventos `paused` e `unpaused` e os ganchos de cena `paused` e `unpaused`.
- [x] Modos de processamento (`inherit`, `pausable`, `whenPaused`, `always` e `disabled`) para cenas, autoloads, timers, tweens, sons e barramentos de áudio (efeitos e ambiente param, música e UI continuam, configurável), com motivos de pausa separados por voz.
- [x] Timers e tweens com escolha entre tempo com escala e sem escala.
- [x] O Tiny Island passa a usar a pausa da engine, com a cena de pausa em `whenPaused` (feito). Falta abrir o menu de pausa quando o app vai para o segundo plano ou perde o foco, para a partida continuar pausada na volta (o congelamento da engine só para o tempo enquanto o app está fora). Feito: o menu de pausa abre em `app_background` e em `app_inactive` com `pauseOnFocusLoss`.

#### F. Entrada de texto e rich text

- [x] Protocolo de edição de texto (`platform::TextInput`, `ui::TextSession`): a engine publica o campo ativo (texto, cursor, seleção, retângulo do campo e do cursor, tipo de teclado, tecla de retorno, autocorreção, capitalização e tamanho máximo) e um campo nativo escondido em cima do campo edita o texto e devolve texto, cursor, seleção e composição do IME, aplicados por callback do `InputText` do ImGui.
- [x] Web (validado no Chrome com WebGL2 e WebGPU, inclusive a composição do IME): `textarea` e `input` escondidos posicionados sobre o campo, com `inputmode`, `enterkeyhint`, composição do IME, colar, copiar e desfazer nativos, abertura do teclado dentro do gesto no Safari do iOS e o teclado virtual ocupando a área visível (`visualViewport`).
- [x] Android: `EditText` escondido (validado no emulador) na activity com o `InputConnection`, composição, sugestões e ações do teclado, e os insets do teclado.
- [~] iOS e tvOS: `UITextField` e `UITextView` escondidos (validado no simulador iOS por XCUITest. Faltam a composição CJK no iOS e o teclado da Apple TV rodando) com texto marcado (CJK), autocorreção, ditado, emoji e colar, e o teclado de tela cheia na Apple TV.
- [~] macOS: `NSTextView` escondido (`HaylenFieldEditor`) e Windows com a janela do IME no cursor (`WindowsTextInput`). Escritos, mas não exercitados aqui (tela bloqueada e sem Windows) com texto marcado, teclas mortas, emoji e ditado. Windows: posição da janela de composição do IME.
- [x] Tipos de teclado (texto, várias linhas, número, decimal, telefone, email, URL, busca, senha) e rótulo da tecla de retorno nos campos de UI e no Lua.
- [x] A UI sobe o campo com foco para cima do teclado virtual (validado no Android e no iOS).
- [x] Rich text com BBCode (erros de marcação com linha e coluna, layout em cache, efeitos determinísticos): `b`, `i`, `u`, `s`, `code`, `color`, `bgcolor`, `font`, `size`, `outline`, `shadow`, `glow`, `alpha`, `p`, alinhamentos, recuo, listas, `br`, `hr`, `img`, `icon`, `url`, `hint`, efeitos `wave`, `shake`, `tornado`, `fade`, `rainbow` e `pulse`, efeitos próprios em Lua e C++, revelação de texto (máquina de escrever), e depois `table` e `dropcap`.
- [x] Famílias de fontes (normal, negrito, itálico, negrito itálico e mono), fonte de fallback, e negrito e itálico sintéticos pelo SDF quando a família não tem a variante.
- [x] Rich text no desenho 2D (`graphics2d.newRichText`, `drawRichText` e `measureRichText`) e na UI (`ui.richText` com links focáveis e eventos `link`).

#### G. Safe area e âncoras

- [x] Componente de âncora na UI (16 posições, na safe area ou na tela inteira, com margens): qualquer documento ou nó pode ser ancorado na safe area ou na tela inteira, nos cantos, bordas, centro ou esticado, com margens. Tudo opcional.
- [x] O app pode desenhar em tela cheia, inclusive embaixo do notch, da ilha dinâmica, dos cantos arredondados e da barra de gestos, enquanto a UI escolhe onde fica.
- [x] Simulação de safe area no desktop pelo `app.json` (`debug.safeArea` com nome de aparelho ou recuos) ou por Lua, com uma visualização de debug da safe area.

#### H. Câmera 2D

- [x] Deslocamento (`offset`), âncora (centro ou canto), zoom separado em x e y, limites de zoom e `ignoreRotation`.
- [x] Limites com suavização, suavização de posição e de rotação ligáveis, `resetSmoothing`, `snapTo` e alinhamento imediato.
- [x] Zonas de arrasto por lado (drag margins) com deslocamento, além da zona morta.
- [x] Antecipação do movimento (look ahead) pela velocidade do alvo.
- [x] Zoom em torno de um ponto (pinça e roda do mouse) e enquadramento de vários alvos com zoom mínimo e máximo.
- [x] Tremor por trauma com frequência configurável, trauma direto e tremor direcional.
- [x] Viewports (tela dividida e minimapa), inclusive com luz, várias câmeras por frame e mistura suave entre duas câmeras.
- [x] Camadas de parallax genéricas (fator de rolagem, repetição, rolagem automática e limites), fora do Tiled também.
- [x] Desenho de debug da câmera (tela, limites e margens).

#### I. Camadas, Y-sort e luz

- [x] Modo de ordenação `y` automático com deslocamento da origem de ordenação, ordem numérica entre canvases, deslocamento de camada em escopo e máscaras de visibilidade.
- [x] Camadas de tiles com Y-sort por linha, para entidades passarem por trás e pela frente de objetos do mapa.
- [x] Luz: tipos pontual, spot (cone) e direcional, `enabled`, altura, modos de mistura (somar, subtrair e misturar), intensidade acima de 1 (mapa de luz em ponto flutuante), máscara de itens e faixa de camadas.
- [x] Sombras: oclusores (`LightOccluder2D`, polígonos abertos e fechados, culling e máscara), sombra por luz com filtros (nenhum, PCF5 e PCF13), cor e suavidade da sombra.
- [x] Normal maps e especular em sprites, sprites sem sombreamento (`unshaded`) e emissivos, e máscara de luz por sprite.
- [x] Canvases com luz desenhando em render targets.

#### J. UI completa, foco e TV

- [~] Revisão de todos os componentes com mouse, toque, teclado e controle (feita e testada no desktop e no simulador da Apple TV). Falta rodar num aparelho Android ou Android TV.
- [x] Enter, Espaço ou o botão sul ativam o controle focado por código já na primeira vez, mesmo com o anel escondido (a ativação também conta como navegação e acende o anel).
- [x] Anel de foco do tema, concêntrico com o controle (raio do controle mais a folga, largura `FocusWidth`, cor `Focus`), visível só quando o usuário navega com teclado, controle ou controle remoto. Foco dado por código não acende o anel quando o último dispositivo foi mouse ou toque (a borda estranha em volta do botão do app inicial).
- [x] Navegação por foco em documentos retidos (vizinhos automáticos com as regras do FocusFinder do Android, `focusLeft`/`focusRight`/`focusUp`/`focusDown`, `autofocus`, `focusScope`, `focusWrap`, foco devolvido ao fechar diálogos e rolagem até o controle focado, com as ações `ui_accept`, `ui_cancel`, `ui_menu` e as quatro direções no mapa de ações): vizinhos automáticos por direção, vizinhos explícitos, foco inicial, escopos de foco (diálogos prendem o foco), anel de foco pelo tema, confirmar e voltar pelo mapa de ações.
- [~] Apple TV: Siri Remote (validado no simulador por XCUITest com os botões do controle remoto, falta o gesto de deslizar num aparelho real) (toque direcional, clique, menu como voltar e play/pause) e controles. Android TV: D-pad e controle, com o manifesto `LEANBACK_LAUNCHER`, banner e `touchscreen` opcional.
- [x] Componentes que faltam para jogos e apps (`circularProgress`, `stepper`, `segmentedControl`, `rangeSlider`, `window`, `contextMenu`, `accordion`, `carousel`, `slotGrid` com arrastar e soltar também por teclado e controle, `keyCapture` e `scroll` com encaixe; o `richText` vem do grupo F): `richText`, progresso circular (recarga de habilidades), stepper, controle segmentado, range slider, janela arrastável, menu de contexto, acordeão, carrossel de páginas, grade de slots com arrastar e soltar, captura de tecla para remapear controles e rolagem com encaixe.

#### K. Lua: singletons, classes, disco e desempenho

- [x] Autoloads: módulos listados no `app.json` (ou registrados por `haylen.autoload`) que carregam antes da primeira cena, vivem durante o app todo, ficam acessíveis de qualquer cena e recebem os callbacks do ciclo (update, fixedUpdate, render, renderUi, eventos e stop).
- [x] Helper de classes (`haylen.class`) com herança, construtor, `super`, `is` e mixins, para classes globais do app.
- [x] Acesso ao disco documentado e testado (com `storage.root()` para o `fs` assíncrono do Varn): `fs` do Varn e `haylen.storage` (pasta do usuário, leitura e escrita síncronas e assíncronas, listagem, criação e remoção).
- [x] Desempenho (`collections.newFloatBuffer`, `graphics2d.drawBatch` com buffer e campos, `world:readTransforms` e `writeTransforms`, `emitter:readPositions` e o bunnymark em Lua no `make.py bench --suite lua`: 1 milhão de sprites em 72 ms de CPU por frame pelo caminho em lote contra 347 ms com uma tabela por sprite). APIs em lote sem alocação por item (atualização de muitos sprites de uma vez, buffers de números compartilhados com o C++), métodos rápidos nos userdata dos caminhos quentes e medição no benchmark.

#### L. Modo debug com estatísticas

- [x] Estatísticas compactas sempre visíveis (FPS, tempo de frame, draw calls, vértices) e o modo completo no overlay.
- [x] Contagem de objetos por tipo (criados, vivos e destruídos) em todo userdata exportado para Lua e nos recursos C++ (texturas, render targets, buffers, fontes, sons, corpos, emissores, documentos de UI), memória do Lua, memória de GPU estimada e uso dos pools do Sokol.
- [x] Contadores de tweens, timers, cenas, vozes, assets, passos fixos, contatos de física e partículas vivas.
- [x] Monitores próprios (`debug.addMonitor`) e ligação pelo `app.json` ou por Lua.

#### M. Recursos exigidos pelos samples

- [x] Shaders próprios (`make.py shaders`, arquivos `.shader` com reflexão para Metal, HLSL, GLSL 430, GLSL ES 300 e WGSL, `graphics2d.newMaterial`, hot reload no desktop, validado em Metal, WebGL2 e WebGPU, com o guia `docs/shaders.md`): fontes GLSL anotadas no pacote compiladas pelo `make.py` para todos os backends, material com uniforms por nome, aplicados a sprites, canvases e pós-processamento.
- [x] Efeitos de áudio pelo grafo do miniaudio (nós próprios com parâmetros atômicos: `Filter` com passa-baixa, passa-alta, passa-banda, notch, pico e shelves, `Delay` e `Reverb` Freeverb, tweenáveis pelo Lua) (filtros passa-baixa, passa-alta e passa-banda, equalizador, atraso e eco, e reverb se possível) por barramento e por som.
- [x] Física avançada (a renderização dos líquidos em metaballs vem do renderer): cordas (cadeias de juntas), líquidos (partículas com renderização de metaballs), terreno destrutível (bitmap de terreno com marching squares e recriação das cadeias), ragdoll, veículos, pontes, explosões com impulso radial, plataformas de mão única e esteiras.
- [x] Fontes bitmap (BMFont em texto e binário, e fontes em grade) além das TTF e OTF, com `text::Font` como interface (`TrueTypeFont` e `BitmapFont`). Fontes com contornos CFF (PostScript) saem ruins no campo de distância do stb_truetype: a documentação indica fontes TrueType como fallback.
- [~] Orientação do aparelho em tempo de execução (`window.orientation`, `window.lockOrientation` e o evento, feitos em todas as plataformas. A trava ainda não foi exercitada em aparelho) (ler e travar retrato ou paisagem) e evento de mudança.
- [x] Pools de objetos para sprites e projéteis (`haylen.collections`).

#### N. Samples

Os samples ficam em categorias, e os comandos recebem o caminho a partir de `samples/` (`python3 make.py run games/tiny-island`, `python3 make.py run graphics/lighting --platform web`, `python3 make.py run-cpp cpp/embedding`), com um comando que lista todos: `games/` (tiny-island), `graphics/` (sprites, camera, lighting, shaders, particles, nine-patch, fonts e scenes), `gameplay/` (physics, algorithms, tiled, tween, events, input e audio), `interface/` (ui, safe-area e orientation), `system/` (filesystem, preferences, localization, network e platform) e `cpp/` (embedding).

- [x] Samples movidos para as categorias, `make.py run` e `run-cpp` resolvendo o caminho da categoria, um comando `make.py samples` que lista todos, e README, docs, CLAUDE.md e os READMEs dos samples atualizados. Feito para `games/tiny-island`, `gameplay/physics`, `gameplay/algorithms`, `gameplay/tiled` e `cpp/embedding`. Os samples ainda em produção vão para a categoria quando os agentes terminarem.

Todo sample Lua tem um menu simples para escolher o teste, cada teste é uma cena com um botão para voltar ao menu, e roda em todas as plataformas pelos templates.

- [x] `samples/interface/ui` (19 testes): todos os componentes, temas, foco, teclado virtual, rich text e entrada de texto.
- [x] `samples/gameplay/physics` (17 testes): corpos, formas, juntas, cordas, líquidos, terreno destrutível, ragdoll, veículos, pontes, explosões, plataformas de mão única, sensores e consultas.
- [x] `samples/system/network` (7 testes, validados com e sem rede e na web): HTTP, HTTPS e WebSocket.
- [x] `samples/system/filesystem` (6 testes): leitura, escrita, listagem e remoção no disco do usuário e leitura do pacote.
- [x] `samples/system/preferences` (4 testes, com persistência entre execuções): configurações e saves.
- [x] `samples/graphics/lighting` (15 testes): luz ambiente, pontual, spot, direcional, sombras, normal maps e máscaras.
- [x] `samples/graphics/shaders` (6 testes com 13 shaders próprios): shaders próprios em sprites, canvases e pós-processamento.
- [x] `samples/graphics/particles` (18 testes, 20 mil partículas vivas a 60 fps): fogo, fumaça, explosão, chuva, neve, faíscas, rastros, magia, confete, fogos de artifício e efeitos por arquivo.
- [x] `samples/system/localization` (8 testes em inglês, português, espanhol e japonês): idiomas, argumentos, plurais e troca em tempo real.
- [x] `samples/gameplay/input` (9 testes, com o remapeamento salvo nas preferências e toques reais no emulador Android): teclado, mouse, toque, gestos, controles, mapa de ações e remapeamento.
- [x] `samples/graphics/sprites` (11 testes): pools, spritesheets, animação, batches, tiros, milhares de sprites e poucos sprites.
- [x] `samples/system/platform` (4 testes, com o handler próprio respondendo em JavaScript na web e em Java no Android, e o Objective-C compilado para as plataformas Apple): bridge com os métodos embutidos e um handler próprio em cada plataforma.
- [x] `samples/interface/orientation` (4 testes): orientação do aparelho.
- [x] `samples/interface/safe-area` (4 testes): âncoras na safe area e na tela inteira.
- [x] `samples/gameplay/audio` (7 testes): música, efeitos, barramentos, efeitos de áudio e áudio 2D posicional.
- [x] `samples/graphics/nine-patch` (5 testes): nine-slice esticado e repetido, em peças e em UI.
- [x] `samples/graphics/fonts` (11 testes, com fontes OFL e fontes bitmap geradas): TTF, OTF, bitmap, tamanhos, contorno, sombra, rich text e scripts diferentes.
- [x] `samples/graphics/camera` (14 testes): todos os recursos da câmera.
- [x] `samples/graphics/scenes`: todas as transições (os 24 efeitos com direção, easing, duração e cor), efeitos próprios, o ciclo de carregamento (a transição como loading, view de loading, pré-carregamento e erro com `onError`), a pilha, sobreposições e a pausa.
- [x] `samples/gameplay/tiled` (12 testes, com o conteúdo gerado por `tools/generate_content.py`): mapas de todas as orientações do Tiled.
- [x] `samples/games/tiny-island`: o jogo, no formato novo.
- [x] `samples/cpp/embedding`: o sample C++, rodado por `make.py run-cpp cpp/embedding`.

#### O. Documentação, testes e revisão

- [ ] Testes GoogleTest e Lua de cada recurso novo, com cobertura da engine perto de 100%.
- [ ] Páginas `docs/lua-api/` e guias atualizados, incluindo um guia de distribuição (templates, artefatos e comandos) e um de ciclo de vida.
- [x] Revisão final de bugs, código morto, race conditions e riscos de crash, em duas frentes (core, Lua, plugins, storage, io, math, IA, debug, net, áudio e input, e gráficos, texto, UI e os contextos 2D): cerca de 60 e dezenas de correções com testes (crashes na saída e no reinício, use-after-free, estouro de pilha Lua, travamentos por entradas do Lua, asserts do Box2D, leituras fora dos limites, alfa pré-multiplicado, lotes de desenho e memória por frame), ThreadSanitizer limpo, 892 testes. As sobras estão nos itens abaixo.
- [ ] O teste `FontTest.FailsAgainForAGlyphNoAtlasHolds` leva cerca de 250 s em Debug: deixar o teste rápido sem perder o que ele verifica.
- [ ] Quatro testes de gráficos falham ou abortam só sob ThreadSanitizer (`MaterialTest.*`, `MaterialLuaTest.*`, `RendererTest.CapturesCanvasesIntoTargets` e `Graphics2DLuaTest.KeepsGpuPoolsSteadyAcrossFrames`, um deles com a validação de tamanho do `apply_uniforms` do sokol): investigar e corrigir.
- [ ] O destrutor do `PocoWebSocket` espera a thread que ainda conecta (até 10 s ou mais sem timeout de DNS) e o laço ocioso acorda 500 vezes por segundo: refazer a conexão para ser interrompível.
- [ ] `lua::Promise::resolve` com JSON aninhado além de 128 níveis ou binário nunca retoma quem espera: rejeitar com erro claro.
- [ ] Chamar o `__gc` na mão ou trocar o `__native` passa pelas checagens de vida: proteger as metatables.
- [ ] Timers e tweens leem o modo de processamento do dono uma vez só, na criação: seguir o dono quando o modo muda.
- [ ] `debug.addMonitor` sem a opção `owner`.
- [ ] Armazenamento síncrono na thread do frame e o `PackageWatcher` varrendo o pacote na thread do frame no modo de desenvolvimento: oferecer as versões assíncronas e tirar a varredura do frame.
- [ ] O `load` global ainda aceita bytecode e o `string.dump` existe: aplicar a regra de só carregar texto também ao código do app.
- [ ] Nomes Lua que não batem com o C++: `translationPart`, `state`, `highest`, `setDeadzone` e `gamepadDeadzone`, `'bspline'`.
- [ ] Os slots de controle na Apple mudam quando um controle desconecta, e ganhos de filtro finitos muito grandes ainda estouram os coeficientes.
- [ ] Decisão de mistura: `multiply` e `screen` agora recebem cores retas, como os outros modos (fora `premultiplied`). Documentado.
- [ ] Tipos do contexto `text` que repetem o namespace (`text::TextLayout`, `TextStyle`, `TextAlign` e `TextEffect`): renomear para nomes precisos sem repetir o contexto.
- [ ] Funções auxiliares livres em cerca de 40 arquivos de teste, criados depois da conversão dos testes: levar para fixtures ou classes de apoio.
- [ ] Nomes Lua que diferem dos nomes C++ (`TileMap` e `TiledMap`, `HierarchicalPath`, `closest` e `smooth`), as opções Lua `scale` e `tint` do texto e o flip diagonal de sprites no `TypeConverter`, e as tabelas de nomes de enums duplicadas em `TypeConverter.cpp` e `AppConfig.cpp`.
- [ ] Cache de imagens do `[img]` do rich text no `TextPlugin` (hoje a imagem é carregada de novo a cada frame em Lua).
- [ ] Atualização parcial de texturas no `graphics::Device`, para os atlas da UI e das fontes não subirem inteiros a cada mudança.
- [ ] `JobSystem::parallelFor` com roubo de trabalho, porque o `crowd:step` pode ficar esperando atrás de jobs longos de fundo.
- [ ] `PoissonDisk::reachOf` converte uma distância enorme para `int`, o zoom NaN da câmera é aceito, handles C++ de física ficam velhos depois que o slot do mundo é reusado, e `GridRay::traverse` pode travar em raios com mais de 2^23 células.
- [ ] `make.py` sem opção de ThreadSanitizer, e o openssl-cmake compila com todos os núcleos se não receber `-DOPENSSL_ENABLE_PARALLEL=OFF`, furando o limite de 6 jobs.
- [ ] Caminhos do navmesh com raio: 0,8% dos caminhos com início ou fim mais perto de uma parede que o raio cruzam a borda da malha perto dessa ponta (documentado), e a triangulação com restrições não convergiu num layout aleatório (semente 97).
- [ ] Encerramento por uma thread de fundo derruba o app: `SokolRuntime::current` é um `unique_ptr` estático, e um `exit()` fora da thread do frame (no simulador iOS, o IOSurface faz isso quando o servidor de render cai) destrói a engine nessa thread enquanto o frame ainda roda. A engine precisa ser destruída só na thread do frame, no fim normal do app.
- [ ] `make.py`: os builds das bibliotecas nativas vão para `build/apps/<nome da pasta>/native/`, e apps com o mesmo nome de pasta colidem (cache do CMake de outra origem). O caminho precisa identificar o app de forma única.
- [ ] `make.py run` nas plataformas Apple não mostra a saída do app no terminal, porque o log do Varn na Apple só escreve no os_log: transmitir o log do app no `run` (simulador, Catalyst e macOS), como a documentação promete.
- [ ] O shell web dos apps C++ não tem ícone e cada página registra um 404 de `/favicon.ico`: usar a logo da engine.
- [ ] Áudio na web usa `ScriptProcessorNode` (obsoleto no Chrome). O backend AudioWorklet do miniaudio exige wasm workers e isolamento de origem (COOP e COEP), o que impede páginas com scripts de terceiros e hospedagens sem esses cabeçalhos. Decidir com o dono e documentar.
- [ ] As janelas do Mac Catalyst abrem com 1024x768 e não com o tamanho do `app.json`.
- [ ] `make.py run-cpp` só roda no desktop e na web: os apps C++ precisam rodar também no iOS, tvOS, Catalyst e Android.
- [x] Uma tecla ainda segurada quando uma troca de cena termina conta como uma pressão nova (durante a troca o input fica bloqueado e o mapa de ações vê a tecla solta): um toque normal em Escape ou Start abre e fecha o menu de pausa do Tiny Island. Uma tecla que já estava segurada antes do desbloqueio só pode disparar de novo depois de ser solta. Repro no scratchpad `sr/repro-held`. Feito no `ActionMap`: uma ligação segurada quando o input volta só dispara de novo depois de solta.
- [x] O exemplo de `docs/input.md` ainda usa uma ação `back`, que os samples trocaram pelo `onCancel` do documento. Feito.
- [x] Os testes da engine (`engine/tests/text/ShapingTests.cpp`) leem fontes de `samples/graphics/fonts`, o que quebra a regra de a engine nunca depender dos samples: mover as fontes de teste (subconjuntos pequenos, com as licenças) para os dados de teste da engine. Feito: subconjuntos em `engine/tests/data/fonts` com as licenças OFL.
- [x] Conferir o tamanho do texto desenhado com `graphics2d.drawText` e rich text nos samples e no Tiny Island depois da troca para o tamanho pelo em (fontes mais altas que o em ficaram até 1,3 vez maiores). Feito na revisão dos 25 samples, que passam nos harnesses headless e Metal.
- [x] Android: ao fechar o app, o sokol chama `exit(0)` quando a activity é destruída e o processo aborta com "pthread_mutex_lock called on a destroyed mutex" nas threads de UI do Android, deixando um relatório de crash a cada saída. O app precisa sair sem crash. Feito com o patch `sokol-android-quit.patch`, validado no emulador API 36 (Back na raiz, recentes, `am force-stop` e `haylen.quit()`).
- [x] `make.py engine --platform android --jobs 6` não limita o build nativo: sobe três ninja sem limite (cerca de 51 compiladores ao mesmo tempo). O `--jobs` precisa valer para cada ABI, e as ABIs devem compilar uma de cada vez. Feito: o `make.py` compila o `libhaylen.so` uma ABI de cada vez com o `--jobs`, e o Gradle só empacota.
- [x] Android 16 (predictive back): o Back sai do app de dentro de qualquer tela e ignora `window.setBackLeavesApp(false)`, porque a `HaylenActivity` não registra um `OnBackInvokedCallback` (ou o `OnBackPressedCallback` do AndroidX) para o alvo SDK 37. Feito: `android:enableOnBackInvokedCallback` no manifesto do AAR e o callback registrado só enquanto o app segura o Back, validado no emulador API 36.
- [x] `audio.playMusic` não devolve um id de voz, então não dá para pausar só a música: dar um handle ou `pauseMusic` e `resumeMusic`. Feito: `playMusic` devolve a voz da música.
- [x] Um `ui.scroll{grow = 1}` dentro de um painel fica da altura do conteúdo e empurra a página para fora da tela: um filho que cresce deve partir da altura que sobra no pai, e não da altura do conteúdo. Feito: filhos que crescem partem do zero e dividem o espaço que sobra.
- [x] Documentação contraditória: o guia da bridge diz que a API mínima do Android é 30 e usa `Map.of`, enquanto o template usa minSdk 27, e dois guias discordam sobre quando os handlers embutidos do Android são registrados. Feito.
- [x] Fontes OpenType CFF saem quebradas em qualquer tamanho: o `stbtt_GetGlyphSDF` só trata linhas e curvas quadráticas, e os contornos CFF são cúbicos. Gerar o campo de distância a partir do contorno completo (curvas cúbicas incluídas). Feito com o msdfgen v1.13 (`text::DistanceField`), que calcula a distância exata até as curvas cúbicas.
- [x] Glifos de fallback de algumas fontes saem pequenos demais: o `TrueTypeFont` usa `stbtt_ScaleForPixelHeight` (ascendente a descendente) e não o tamanho do em. Feito: todas as faces pelo em.
- [x] As setas do carrossel são desenhadas antes das páginas e ficam embaixo de imagens largas. Feito.
- [x] `document:bounds(id)` devolve nil para um nó `window`, mesmo depois de desenhado e arrastado. Feito para janelas, diálogos e toasts.
- [x] Não há API Lua para trocar a política de escala (fit, fill, stretch, expand e pixel perfect) e a resolução de design em tempo de execução. Feito: `viewport.setScaling` e `viewport.setDesignSize`.
- [x] Slider com `step` avisa `change` no primeiro frame quando o valor não cai no passo (0.35 vira 0.35000000000000003), e a tela de configurações mostra alterações não salvas ao abrir. Feito.
- [x] `preferences.capture()` grava volumes com ruído de float (0.6 vira 0.6000000238418579): gravar os floats pela menor representação decimal. Feito com `core::JsonNumber::fromFloat` em todos os pontos fora da UI (os três da UI ficam com o lote de correções de UI).
- [x] Numa linha (`row`), uma coluna que cresce e tem texto quebrado em várias linhas é medida antes de saber a largura, e o nó de baixo cobre a segunda linha do texto: medir em duas passadas com a largura final. Feito: medida em duas passadas.
- [x] Valores `haylen.Font` nunca são iguais em Lua (`family.fallback[1] == family.fallback[1]` é falso), ao contrário do exemplo da documentação: o mesmo recurso deve virar o mesmo userdata ou comparar igual. Feito com o metamétodo `__eq` compartilhado em todos os recursos.
- [x] Os rótulos e botões da UI só usam a face normal de uma família de fontes, então o fallback (por exemplo CJK) só chega ao rich text: a UI precisa das famílias com fallback (fontes mescladas do ImGui) para um app em japonês não precisar trocar o tema. Feito: `ui.addFont(nome, família)` com fallback mesclado no ImGui e as faces negrito e itálico nos papéis do tema.
- [x] Texto da direita para a esquerda e scripts complexos (árabe, hebraico, hindi e outros): shaping com HarfBuzz e reordenação bidirecional, no texto 2D, no rich text e na UI, para a localização cobrir todos os idiomas. Feito com HarfBuzz 14.5.0 (shaping), SheenBidi 3.0.0 (bidirecional), libunibreak 8.0 (quebra de linha) e o modelo tailandês do BudouX, em todo o texto 2D, no rich text e na UI (`ui.setDirection` espelha a interface, e o catálogo de idioma declara a direção). Limites documentados em `docs/text.md`: fontes bitmap sem shaping, laosiano, khmer e birmanês só quebram em espaços e pontuação, e texto vertical e ruby ficam de fora.
- [x] `haylen.net` não tem uma chamada para mandar um frame de ping do WebSocket. Feito: `socket:ping(payload)` e o evento `pong` no nativo (no navegador o JavaScript não manda ping, e a chamada dá um erro claro).
- [x] O `timeoutSeconds = 2` do HTTP do Varn estourou em 4,7 s: conferir se a engine repassa o valor certo ou se é o comportamento do Varn (e levar ao Varn se for). É comportamento do Varn (Poco): no HTTPS o fechamento TLS espera de novo o timeout, o timeout vale por etapa e não pela requisição inteira, e valores não inteiros caem no padrão de 60 s. Documentado em `docs/lua.md`, com `async.timeout` para um prazo rígido, e as correções sugeridas vão para o Varn.
- [x] Os eventos `network_online` e `network_offline` não dispararam no macOS: conferir o `NWPathMonitor` no desktop Apple. Feito: o estado começa desconhecido, o primeiro aviso publica o evento, e `haylen.networkState()` lê o estado.
- [x] Em `events.stats()` e `signal.list()` o contador `stale` nunca passa de 0: depois que o dono é coletado, os listeners ainda contam como vivos até o fim do frame, com `stale = 0`, e a documentação diz que eles contam como pendurados até serem removidos.
- [x] Escape e o botão B estão ligados à ação `back` dos samples e ao `ui_cancel` da engine ao mesmo tempo: com uma lista de combo aberta, Escape provavelmente fecha a lista e também sai do teste. O voltar deve passar pelo `onCancel` dos documentos, que só dispara quando nenhum popup consumiu o cancelamento. Feito: o que a UI responde (cancelar num popup, diálogo ou edição, aceitar num controle focado, teclas durante a edição de texto e a captura de tecla) não dispara as ações do mapa naquela pressão.
- [x] Rótulos de checkbox e toggle recebem reticências mesmo com espaço: `Widgets::measureToggle` mede o rótulo exato, mas o desenho recalcula a largura por subtração (`bounds.getRight() - track.getRight() - kContentSpacing`) e o `Typography::elide` corta quando a medida passa da largura por arredondamento de float ("Spin" vira "Sp…"). Feito: larguras de texto arredondadas para cima e o rótulo com a largura do próprio controle.
- [x] O Lua não consegue ler a geometria de uma forma de física (tipo, pontos e raio), então não dá para desenhar as peças de um ragdoll ou de uma fratura. Expor `shape.kind`, `shape.points`, `shape.radius` e o que mais a forma tiver. Feito: `shape.kind`, `shape.points`, `shape.worldPoints`, `shape.radius`, `shape:outline()` e `body:outlines()`.
- [x] HPA\* sem versão assíncrona (a construção trava uns 60 ms no mapa grande do sample). Feito: `grid:hierarchicalAsync`.
- [x] No Mac Catalyst as teclas ainda chegam ao mapa de ações enquanto um campo nativo edita texto: a navegação por foco deve ignorar setas e Enter enquanto há edição de texto ativa. Feito pela mesma captura de teclas durante a edição de texto.
- [x] Reconstruir o AAR do Android com a correção do teclado que reabria depois do Back e validar no emulador. Feito e validado no emulador.
- [x] Host headless com limite de textura igual ao das GPUs reais (patch do backend dummy do Sokol com os limites de desktop, 16384), para o teste de fumaça sem janela chegar ao gameplay do Tiny Island (hoje o backend dummy do Sokol recusa texturas acima de 1024 pixels e o mapa da ilha não carrega).
- [x] Regras de commit e push na `main` por bloco, com prefixo e frase curta em minúsculas e sem coautor, a conferência de arquivos privados e temporários antes de cada commit e a proibição de citar outras engines, no CLAUDE.md. Commits feitos e publicados em `github.com/haylen-org/haylen`.
- [x] Nenhuma menção a outras engines no repositório (código, comentários, testes, docs, README, CLAUDE.md e este documento).
- [ ] Revisar as seções 1 a 13 deste documento com os nomes novos (app, `source/`, `content/`, namespaces, módulos `2d`, `storage` e `preferences`), sem ponto e vírgula, e com o status real de cada item.

#### P. Organização do código C++

Mapa de namespaces (um por contexto, igual ao nome da pasta, e com o sufixo `2d` para as pastas dentro de `2d/`, igual aos módulos Lua): `haylen::core`, `haylen::math`, `haylen::io`, `haylen::assets`, `haylen::graphics`, `haylen::text`, `haylen::input`, `haylen::audio`, `haylen::ui`, `haylen::platform`, `haylen::localization`, `haylen::storage`, `haylen::debug`, `haylen::net`, `haylen::lua`, `haylen::plugins`, `haylen::graphics2d`, `haylen::animation2d`, `haylen::particles2d`, `haylen::lighting2d`, `haylen::physics2d`, `haylen::navigation2d`, `haylen::spatial2d` e `haylen::tiled`. Dentro de um namespace 2D o tipo não repete o contexto (`haylen::physics2d::World`, `haylen::physics2d::Body`, `haylen::tiled::Map`).

- [x] Todo o código em sub-namespaces por contexto, com pastas e namespaces batendo um com o outro. Feito: `math`, `io`, `storage` e `lua` (passo 1 de 6), `core`, `ai` e `plugins` (passo 2), `graphics`, `text`, `graphics2d`, `animation2d`, `particles2d`, `lighting2d` e `assets` (passo 3). `physics2d`, `navigation2d`, `spatial2d` e `tiled` (passo 4). `input`, `audio` (`audio::Mixer`), `localization` (`localization::Catalog`), `debug` e `net` (passo 5a). `ui` (passo 5b: `ui::Backend`, `ui::Document`, `ui::Context`, `ui::Theme`, e cada componente num arquivo em pastas por família). `platform` (passo 6: `platform::Bridge`, `platform::Window`, `platform::Host` e `platform::Services` como classe com uma implementação por sistema), testes, bench e o sample C++. A varredura final com o AST do clang está limpa no código convertido. Falta converter os testes novos dos agentes de algoritmos e de renderização e os helpers compartilhados de teste (`EngineFixture` e `test::bytes`), o que entra na revisão final.
- [x] Um arquivo por classe, com o nome da classe. Tipos que pertencem a uma classe (opções, eventos, enums usados só por ela) ficam aninhados nela.
- [x] Nenhuma função livre: utilitários viram métodos estáticos de uma classe (`Easing`, `Geometry`, `MathUtils`), helpers de `.cpp` viram métodos privados, e os bindings Lua viram classes de binding. As únicas exceções são os pontos de entrada exigidos pela plataforma (`main`, `sokol_main`, funções JNI e exports do Emscripten), que só repassam para uma classe.
- [x] Membros sem o prefixo `m_`, com acessores `get`, `set`, `is` e `has`. Em Lua, os pares `get` e `set` aparecem como propriedades.
- [x] Plugins na pasta `plugins/` e no namespace `haylen::plugins`, incluindo a interface `Plugin` e o `PluginRegistry`, com os nomes dos plugins iguais aos módulos Lua.
- [x] O plugin `save` vira `StoragePlugin`, no contexto `storage` (`UserStorage`, `Preferences` no lugar de `Settings` e `SaveSlots`), com os módulos Lua `haylen.storage` (arquivos e slots: `writeSlot`, `readSlot`, `slotInfo`, `slotExists`, `removeSlot`, `listSlots`) e `haylen.preferences` (arquivo `preferences.json`).
- [ ] Revisão do código inteiro atrás de coisas soltas, perdidas ou fora de classe.
- [x] CLAUDE.md com todas essas regras.

#### Q. Tela de erro

- [x] A pilha vem estruturada do Lua (fonte, linha, função e tipo de cada nível), sem depender de texto com tabs, e sem os níveis internos da engine (o `xpcall` e o wrapper das tarefas assíncronas).
- [x] A tela mostra o título, a mensagem com quebra de linha, o arquivo e a linha, um trecho do código-fonte com a linha do erro destacada e numerada, a pilha em colunas (local e função), o nome e a versão do app, a plataforma e a versão da engine.
- [x] Rolagem quando o conteúdo passa da tela (roda do mouse, arrastar e setas), copiar o relatório completo (tecla C ou botão) e reiniciar o app (tecla R ou botão), com botões para toque. Conferido e testado, com o toque seguindo só o primeiro dedo.
- [x] O mesmo relatório estruturado chega ao `onError` da página web (com o array `frames`) e ao log.

#### R. Algoritmos de alto desempenho para jogos

Tudo em C++ com binding Lua, sem alocação por chamada nos caminhos quentes, com versões assíncronas pelo `JobSystem` (Promise em Lua) para os cálculos grandes e com resultados determinísticos por semente. A lista cobre o que os jogos 2D costumam precisar.

- [x] **Caminhos em grade**: A* com custos por célula, diagonais sem cortar cantos, heurísticas (Manhattan, octile, Euclidiana e Chebyshev), A* ponderado, Jump Point Search para grades de custo uniforme, grades hexagonais e isométricas (as orientações do Tiled), linha de visão e suavização (já existem o A* básico e a suavização).
- [x] **Caminhos em grafo**: A* e Dijkstra em grafos de waypoints com pesos, pontos habilitados e desabilitados.
- [x] **Flow fields e mapas de Dijkstra**: um campo de direção calculado uma vez para muitas unidades (RTS e tower defense), mapas de distância com várias origens, e fuga (mapa invertido).
- [x] **Pathfinding hierárquico (HPA\*)** para mapas grandes, com atualização local quando o mapa muda.
- [x] **Navmesh**: malha de navegação a partir de polígonos (objetos de colisão do Tiled ou gerados), triangulação de Delaunay com restrições, caminho com o algoritmo do funil (string pulling), raio do agente e reconstrução quando obstáculos mudam.
- [x] **Multidões**: flocking (separação, alinhamento e coesão), desvio de obstáculos e desvio recíproco entre agentes (RVO/ORCA), além do steering que já existe.
- [x] **Espacial**: quadtree, árvore de AABB dinâmica, k-d tree para vizinho mais próximo, além do spatial hash, com consultas por ponto, retângulo, círculo, raio e k vizinhos.
- [x] **Visão e grade**: raycast em grade (DDA), linhas e círculos de Bresenham, campo de visão por shadowcasting (roguelikes e névoa de guerra), polígono de visibilidade (luzes e visão), flood fill, componentes conectados (ilhas e regiões) e union-find.
- [x] **Distribuição de elementos no mapa**: espalhar objetos por área com densidade (quantidade = área × densidade) dentro de retângulos, círculos, anéis e polígonos (inclusive regiões e objetos do Tiled), Poisson disk com distância mínima variável por um mapa de densidade ou de ruído, grade com jitter, zonas de exclusão, camadas de biomas por ruído, pesos por tipo de objeto, e ponto aleatório uniforme em polígono.
- [x] **Geração procedural** (módulo `haylen.procedural2d`, versões assíncronas com Promise, determinístico por semente): autômatos celulares (cavernas), Wave Function Collapse de tiles, BSP e posicionamento de salas para dungeons, drunkard walk, labirintos (backtracker, Prim e Kruskal), diagramas de Voronoi, triangulação de Delaunay e relaxamento de Lloyd, ruídos Worley (celular) e domain warp além de Perlin, simplex e fractal, e autotiling em tempo de execução (máscaras de 4 e 8 vizinhos e Wang).
- [x] **Geometria e destruição** (`math::Polygon` com Clipper2 2.0.1, `math::MarchingSquares`, `math::Spline`, terreno destrutível por bitmap e por polígonos com reconstrução por pedaço, fratura por Voronoi): operações booleanas de polígonos (união, diferença, interseção e offset, com Clipper2), decomposição em polígonos convexos para o Box2D, simplificação (Ramer-Douglas-Peucker), marching squares (contornos a partir de grades e bitmaps), terreno destrutível por bitmap ou por polígonos (cavar, explodir e reconstruir a colisão), fratura de polígonos (Voronoi) para quebrar objetos, e splines (Catmull-Rom, Bézier e B-spline) com amostragem por distância.
- [x] **IA**: behavior trees com blackboard, utility AI e mapas de influência, além da máquina de estados que já existe.
- [x] **Utilitários** (`math::ShuffleBag`, `math::WeightedChoice`, `math::Spring`, `core::ObjectPool`, `core::RingBuffer` e `haylen.collections` em Lua): sacola aleatória (shuffle bag), escolha ponderada, molas criticamente amortecidas, pools de objetos e ring buffers.
- [x] **Benchmarks** dos algoritmos principais (feitos: `make.py bench --suite algorithms`, com A* 512x512 em 2,4 ms, navmesh de 400 obstáculos em 5,4 ms, 2000 agentes ORCA em 0,1 ms e 100 mil raios de física em 2,8 ms pelo JobSystem. e o `samples/gameplay/algorithms` com 26 testes) (A*, JPS, flow field, navmesh, distribuição e marching squares) no `make.py bench`, e um sample `samples/gameplay/algorithms` com um teste por algoritmo, cada um com menu e botão de voltar. O `make.py bench --suite procedural` roda o benchmark procedural, o `Type<spatial2d::CellGrid>` fica num header só, e a direção das plataformas de mão única gira com o corpo.

#### S. Tween robusto

A base atual (`TweenManager` e `haylen.tween`: float, `Vec2` e `Color`, easing, atraso, repetição, yoyo, callbacks, pausa, cancelamento, `wait()` com Promise, sequências e tags) cresce até cobrir tudo o que um sistema de tween completo oferece.

- [x] **Alvos e valores**: qualquer campo de tabela ou propriedade de userdata, caminhos aninhados (`position.x`), números, `Vec2`, `Color` (em RGB ou HSV), ângulos pelo caminho mais curto, inteiros (contadores de pontos), texto (máquina de escrever) e vários campos no mesmo tween.
- [x] **Modos**: `to`, `from`, `by` (relativo) e `fromTo`, valor inicial lido na hora de começar, e tweens por velocidade (a duração sai da distância).
- [x] **Easing completo**: todas as famílias de Penner, parâmetros de `back` (overshoot) e `elastic` (amplitude e período), `steps`, curva Bézier cúbica (como a do CSS), curvas por pontos e funções próprias em Lua e C++.
- [x] **Timelines**: `append`, `join` (em paralelo com o anterior), `insert` num tempo, rótulos, atrasos, callbacks num tempo, sequências e timelines aninhadas, repetição e yoyo da timeline inteira.
- [x] **Controle**: `play`, `pause`, `resume`, `restart`, `reverse` (tocar para trás), `seek` (ir para um tempo ou progresso), `complete` (pular para o fim com ou sem callbacks), `kill`, escala de tempo por tween e por grupo, progresso lido e escrito, e estado (`isPlaying`, `isComplete`).
- [x] **Repetição**: número de vezes ou infinita, modos `restart`, `yoyo` e `incremental`, atraso entre repetições.
- [x] **Eventos**: `onStart`, `onUpdate`, `onStep`, `onLoop`, `onComplete` e `onKill`, e `wait()` com Promise para `await`.
- [x] **Tweens prontos**: mover, escalar, girar, desbotar, tingir, pular (arco), seguir um caminho (spline com orientação), Bézier, piscar, tremer (shake com força, vibração e aleatoriedade) e soco (punch).
- [x] **Escalonamento (stagger)**: o mesmo tween em uma lista de alvos com atraso crescente, a partir do começo, do fim ou do centro.
- [x] **Conflitos e vida útil**: modo de sobrescrita (um tween novo no mesmo alvo e campo mata o antigo), tweens ligados à cena (morrem quando a cena sai) e ao alvo (morrem quando o alvo é coletado ou destruído), sem nunca escrever num alvo morto.
- [x] **Tempo**: modos de processamento da pausa (grupo E), tempo com ou sem escala, e atualização no passo fixo quando pedido.
- [x] **Desempenho** (inclui o tween nativo das propriedades de nós de UI por `doc:transform(id)`): milhares de tweens sem alocação por frame, tweens nativos em propriedades C++ (posição, escala, rotação e cor de sprites, câmera e UI) sem chamar Lua por frame, e contagem de tweens nas estatísticas de debug.
- [x] **Testes, página `docs/lua-api/tween.md` completa e o sample `samples/gameplay/tween` (19 testes)** com cada recurso numa cena, menu e botão de voltar.

#### T. Eventos, ciclo de vida e conexões

Hoje existem o `Signal` com `Connection` (C++ e `haylen.signal`), o callback `event` das cenas e os eventos da plataforma. Tudo isso vira um sistema único e robusto.

- [x] **Sinais**: tipados em C++, conexão com RAII, seguros durante a emissão (conectar e desconectar dentro do callback), prioridade, conexão de uma vez só (`once`), conexão adiada (entregue no fim do frame), bloqueio temporário, `isConnected`, e desconexão automática quando o dono morre (objeto C++, userdata ou tabela Lua coletada, cena que saiu, documento de UI desmontado).
- [x] **Barramento de eventos**: publicar e assinar por nome ou tipo, com canais, filtros, prioridade, consumo (parar a propagação), entrega imediata ou na fila do frame, e emissão a partir de outras threads entregue sempre na thread do frame.
- [x] **Ciclo de vida de tudo, com os mesmos nomes em C++ e Lua** (app, cenas, plugins, autoloads, documentos de UI, janela com tela cheia, orientação e safe area, controles, objetos criados e destruídos com contagem e eventos opcionais, e assets carregados, descarregados e recarregados):
  - app: início, ativo, inativo, segundo plano, pouca memória e saída.
  - cenas: entrar, sair, pausar, retomar, começo e fim de transição, pausado e despausado.
  - plugins e autoloads: início, parada e eventos do app.
  - objetos: criado e destruído, com contagem nas estatísticas.
  - documentos de UI: montado e desmontado.
  - assets: carregado, descarregado e recarregado.
  - janela: tamanho, foco, tela cheia, orientação e safe area.
- [x] **Conexão e desconexão de dispositivos e serviços** (controles com slot e nome, WebSocket com reconexão automática por backoff exponencial, rede online e offline e teclado virtual aberto e fechado feitos. e troca de rota de áudio com o evento `audio_route_changed`): controles conectados e desconectados (com o slot e o nome), troca de dispositivo de áudio, rede com eventos de conectar, desconectar e reconectar (WebSocket com reconexão automática por backoff exponencial, opcional), perda e volta da conexão com a internet quando a plataforma informa, e teclado virtual aberto e fechado.
- [x] **Escopos de assinatura**: uma cena, um autoload ou um objeto pode assinar eventos num escopo que cancela tudo sozinho quando ele sai (`scene:listen(sinal, fn)` e `events.on(nome, fn, {owner = ...})`), sem vazamento de listeners.
- [x] **Diagnóstico** (sinais e listeners com contagem de emissões e aviso de dono morto no overlay): listar sinais e listeners ativos e contar emissões no overlay de debug, com aviso de listener que ficou pendurado num dono morto.
- [x] **Testes e documentação** (com `docs/lua-api/events.md`, `docs/lifecycle.md` e o sample `samples/gameplay/events` com 9 testes) (`docs/lua-api/signal.md`, uma página `docs/lua-api/events.md` e um guia de ciclo de vida), e um sample `samples/gameplay/events` com cada recurso numa cena.

#### U. Raycast

Hoje existe o raycast da física (`world:raycast`, o mais próximo e todos). O raycast vira um recurso completo, com e sem física, sempre em C++ e com binding Lua.

- [x] **Física**: raio mais próximo, todos os acertos ordenados por distância e o primeiro que passa num filtro (categoria, máscara, grupo ou função), com ponto, normal, fração, corpo e forma.
- [x] **Shape casts**: círculo, caixa, cápsula e polígono varridos ao longo de um vetor, para personagens, projéteis grossos e previsão de colisão.
- [x] **Sem física**: raio contra segmentos, retângulos, círculos, polígonos e cadeias (`math::Geometry`), contra camadas de tiles e objetos do Tiled, contra grades (DDA) e contra as estruturas espaciais (spatial hash, quadtree e árvore de AABB).
- [x] **Raios avançados**: atravessar vários alvos (piercing) com limite, refletir e ricochetear (lasers), linha de visão entre dois pontos e leque de raios (cone de visão) numa chamada só.
- [x] **Seleção pela tela**: converter um ponto da tela em raio ou ponto do mundo pela câmera e achar os objetos sob o cursor ou o toque.
- [x] **Desempenho**: muitos raios numa chamada só (em lote e em paralelo pelo `JobSystem`), sem alocação por raio, com resultado em buffers reaproveitados em Lua.
- [x] **Debug**: desenho dos raios, acertos e normais no overlay.
- [x] **Testes, documentação** e os testes de raycast no `samples/gameplay/physics` e no `samples/gameplay/algorithms` (`docs/lua-api/physics2d.md` e uma seção de raycast em `math` e `spatial2d`) e um teste de raycast no `samples/gameplay/physics` e no `samples/gameplay/algorithms`.
- O `sokol_app` usa o tamanho da tela e não o da view para o framebuffer no iOS, o que cortava a imagem no Mac Catalyst e nas janelas divididas do iPad. A engine aplica um patch pelo CPM (`engine/cmake/patches/sokol-ios-view-size.patch`) até o Sokol corrigir isso.
- Com o `minSdk` 27, o `sokol_app` escolhe em tempo de compilação o laço de frames que espera o `eglSwapBuffers`, e não o Choreographer (que pede a API 29). Os frames seguem a taxa da tela, com cadência um pouco menos regular.
- O libuv 1.53.0 tem um erro no tvOS (`QUEUE_INIT`) e compila um caminho de `posix_spawn` que só existe no Android a partir da API 28. A engine compila o libuv com as correções e com símbolos fracos no Android.
- visionOS nativo depende do Sokol (ele usa `UIScreen`, que não existe no visionOS). O app iOS roda no Apple Vision Pro como app de iPad compatível. watchOS é impossível (sem Metal, MetalKit, GameController nem AudioToolbox).
- O backend dummy do Sokol limita texturas a 1024 pixels, o que impedia os testes sem janela de carregar mapas reais. A engine aplica um patch pelo CPM (`engine/cmake/patches/sokol-dummy-limits.patch`) com os limites de desktop.

#### V. Ciclo de vida de cena com carregamento

A troca de cena vira um pipeline com fases, no modelo "cobrir, carregar, revelar", e toda cena passa pelos mesmos estados: criada, carregando, carregada, entrando, ativa, coberta (outra cena por cima na pilha), saindo, saiu e descarregada.

- [x] **Carregamento assíncrono da cena**: um gancho `load` (em C++ e em Lua) que roda antes do `enter`, pode esperar Promises (`:await()` numa corrotina do Varn) e informa o progresso (`context:progress(valor, mensagem)`), com ajuda para carregar grupos de preload do `AssetManager` somando o progresso. Os trabalhos pesados continuam nos pools do Varn e os uploads de GPU são distribuídos entre frames, sem travar o frame.
- [x] **Descarregamento**: um gancho `unload` depois do `exit`, que libera os recursos da cena. Na troca por `replace`, a cena antiga pode sair e descarregar antes da nova carregar (opção ligada por padrão nas transições que cobrem a tela), para o pico de memória ser menor.
- [x] **Fases da transição**: cobrir (a cena atual some no efeito), segurar (a tela fica coberta enquanto a nova cena carrega) e revelar (a nova cena aparece). Os efeitos que mostram as duas cenas ao mesmo tempo (crossfade, slide, push e os outros) carregam a nova cena antes de começar, com a cena atual ainda na tela.
- [x] **Loading opcional** (com `loading`, `loadingDelay`, `minimumLoadingTime` e `loadingFadeOut`, de 0,25 s por padrão): a própria transição serve de loading (a tela coberta, com a cor ou o efeito), ou o desenvolvedor passa uma view de loading (uma cena ou tabela com `update` e `render` que recebe o progresso), exibida só se o carregamento passar de um atraso configurável (sem piscar em cargas rápidas) e por um tempo mínimo configurável quando aparece. Também continua possível usar uma cena de loading comum na pilha.
- [x] **Pré-carregamento**: `scene.preload(cena, params)` começa o `load` em segundo plano sem trocar de cena e devolve uma Promise, e a troca depois fica instantânea ou só espera o que falta.
- [x] **Ganchos de transição**: `enterTransitionFinished` na cena que entra (input liberado) e `exitTransitionStarted` na cena que sai, no lugar dos ganchos atuais, sem manter os nomes antigos.
- [x] **Erros**: um erro no `load` cancela a troca, rejeita a Promise da troca e mantém a cena atual quando ela ainda existe. Quando a cena atual já saiu, o erro vai para a tela de erro ou para um `onError` da troca, que pode levar a outra cena.
- [x] **Concorrência e fila**: pedidos de troca durante uma troca entram na fila em ordem. `push`, `pop`, `replace`, `popTo` e `popToRoot` seguem o mesmo pipeline, e a pausa do jogo, o segundo plano e a perda de foco durante uma carga têm comportamento definido e testado.
- [x] **Escopo de vida**: tudo o que a cena cria (timers, tweens, assinaturas de eventos, tarefas assíncronas iniciadas pela cena com `scene:spawn`, documentos de UI) pertence à cena e é cancelado no `unload`, sem nenhuma corrotina retomando numa cena que já saiu.
- [x] **Eventos do ciclo**: `scene_loading`, `scene_loaded`, `scene_load_failed`, `scene_entered`, `scene_exited`, `scene_unloaded` e os começos e fins de cada fase da transição, pelo barramento de eventos.
- [x] **Tiny Island** usando o novo ciclo (o loading do gameplay como view de loading da transição ou como `load` da cena de gameplay).
- [x] **Testes e documentação** (36 testes das combinações, `docs/lifecycle.md` com os diagramas de sequência, `docs/lua-api/scene.md` e o `samples/graphics/scenes` mostrando o ciclo de carregamento).
- O Varn não tem cancelamento de tarefas assíncronas. Para cancelar as tarefas de uma cena no `unload`, a engine fecha a corrotina e deixa no lugar uma função que retorna na hora, para uma Promise que ainda espere por ela não retomar código. Um cancelamento oficial no Varn (ignorar corrotinas que não estão mais suspensas, ou uma API de cancelamento) deixaria isso mais limpo, e vale levar para o Varn.

#### W. Janelas sem moldura e transparentes

A referência são jogos como o Taskbar Hero, que rodam numa faixa transparente em cima da barra de tarefas.

- [x] **Configuração no `app.json` e em Lua**: janela sem moldura (`decorated = false`), fundo transparente por pixel (`transparent = true`), sempre no topo, sem aparecer na barra de tarefas ou no Dock quando pedido, sem roubar o foco quando pedido, posição e tamanho iniciais (inclusive ancorados na área de trabalho, por exemplo em cima da barra de tarefas), e tudo o que puder mudar em tempo de execução também pelo Lua. (com `position` aceitando `fill`, e `window.place` em Lua)
- [x] **Arrastar a janela** a partir do conteúdo do app (`window.startDrag()` no gesto de arrastar), mover e redimensionar por código, e ler a posição e o tamanho.
- [x] **Cliques atravessando a janela**: a janela inteira ou só as áreas transparentes deixam os cliques passarem para o que está atrás (passthrough por polígonos ou pelo alfa do pixel), com as áreas do jogo e da UI recebendo o mouse normalmente.
- [x] **Monitores**: lista de monitores com a área total e a área de trabalho (sem a barra de tarefas, o Dock e a barra de menus), escala de DPI e o monitor atual, com eventos quando mudam.
- [x] **Renderização transparente**: o framebuffer com alfa e a composição correta com a área de trabalho (alfa pré-multiplicado), com a luz, o pós-processamento e a UI funcionando. (o composite de pós-processamento, as transições e o page turn corrigidos para alfa pré-multiplicado, conferido por leitura dos pixels no Metal)
- [~] **Plataformas**: macOS, Windows e Linux (X11) com o que cada sistema permite, e na web o canvas transparente sobre a página. No mobile e na TV não se aplica, documentado. (macOS e web rodados aqui. Windows e Linux compilam com toolchains cruzadas e o patch do sokol aplica limpo, mas faltam rodar numa máquina Windows e num Linux com compositor)
- [x] Transparência como estado em tempo de execução (`window.setTransparent`): no modo janela comum o app fica opaco de verdade (barra de título, conteúdo e sombra normais) e volta a ser transparente no modo faixa. Hoje, ao trocar o Taskbar Quest para janela, a barra de título e as áreas não pintadas continuam transparentes (bug visto pelo dono no macOS). Feito: `window.setTransparent` e `window.canBeTransparent`, com o alfa forçado em 1 enquanto a janela é opaca.
- [x] **Sample** `samples/games/taskbar-quest`: um jogo pequeno numa faixa transparente em cima da barra de tarefas, arrastável, com a UI do jogo, cliques atravessando as áreas vazias, e o modo janela comum para comparar.
- [x] **Testes e documentação** (`docs/lua-api/window.md` e um guia `docs/desktop.md`). (17 testes novos, `docs/lua-api/window.md` e `docs/desktop.md`)

#### X. Comunicação com a plataforma e código nativo

- [x] **Bridge assíncrona fácil em todas as plataformas** (`platform.call` devolve uma chamada com `await`, `cancel` e `timeout`, erros tipados `{message, code, data}`, exceções Java viram falhas, `HaylenCoroutines` no Kotlin e `HaylenBridgeAsync.swift` no Swift): revisar a `platform::Bridge` para que chamar a plataforma e receber a resposta seja uma linha em Lua (`platform.call('metodo', args):await()` e callbacks), com handlers nativos em Java e Kotlin (Android), Objective-C e Swift (Apple), JavaScript (web) e C++ (desktop e apps C++), eventos da plataforma para o app, erros claros e threads corretas (a resposta sempre chega na thread do frame).
- [x] **Bibliotecas nativas pelo Lua (FFI)** (`haylen.native` com `load`, `symbol` e `callback`, sobre o `ffi` do Varn e closures libffi da engine entregues na thread do frame): carregar bibliotecas nativas e chamar funções C pelo `ffi` do Varn (ou o que for necessário além dele), com tipos, structs, ponteiros, buffers e callbacks do código nativo para o Lua entregues na thread do frame, em macOS, Windows, Linux, iOS (frameworks embarcados) e Android (`.so` do APK). Na web o equivalente é chamar JavaScript pela bridge, documentado.
- [x] **Bibliotecas nativas no app** (seção `native` do `app.json`, com `Contents/Frameworks` no macOS, frameworks embarcados ou bibliotecas estáticas com tabela de símbolos gerada no iOS e tvOS, `jniLibs` no Android, ao lado do exe no Windows e `lib/` com RUNPATH no Linux): os templates e as pastas `platform/<plataforma>/` dos apps levam bibliotecas nativas (`.dylib` e frameworks, `.dll`, `.so`, xcframeworks e `jniLibs`) para dentro do pacote final de cada plataforma, com o caminho de carregamento resolvido pela engine.
- [x] **Plugins nativos em C++** para quem compila a engine (apps C++), registrados pela aplicação, com o mesmo ciclo de vida dos plugins da engine e o próprio módulo Lua.
- [x] **Guia de integração com SDKs** (em `docs/native.md`): como integrar a Steam (API flat em C), o Epic Online Services (API em C, NAT P2P) e SDKs parecidos por FFI ou por plugin nativo, com os callbacks do SDK chegando na thread do frame. Os SDKs não entram no repositório.
- [~] **Testes em todas as plataformas nativas** (15 testes novos com a biblioteca `engine/tests/native/NativeTest.c` e o `samples/system/native` passando no player desktop, no app macOS, no simulador iOS com framework dinâmico e biblioteca estática, no emulador Android arm64 e na web com WebGPU e WebGL2. Faltam tvOS, Mac Catalyst, Windows, Linux e aparelhos reais): uma biblioteca nativa de teste (funções, structs, buffers e um callback) compilada para cada plataforma e chamada pelo Lua nos testes da engine e num sample `samples/system/native`, rodando no macOS, no simulador iOS, no emulador Android e, pela bridge, na web.
- [x] **Documentação**: `docs/platform_bridge.md` revisado, um guia `docs/native.md` e as páginas da API Lua.
- A janela transparente no Windows usa DirectComposition por um patch do sokol (`engine/cmake/patches/sokol-desktop-window.patch`, com as opções `desktop` de criação, `sapp_set_window_focusable`, o fechamento de janelas sem borda no macOS e os visuais ARGB no X11), que deve ser levado ao sokol.
- Propostas para o Varn vindas da integração nativa: chamar cdata de ponteiro de função e aceitar `ffi.cast` de lightuserdata para função, aceitar `T*` e `T[n]` onde se espera `const T*`, descrever arrays dentro de structs para o libffi, reportar erros de callbacks na hora e recusar chamadas de outras threads, `LoadLibraryExW` com caminhos UTF-8 no Windows, um gancho para resolver símbolos estáticos no `ffi.C`, e `enum`, bitfields e `intptr_t`.
- O timeout do HTTP do Varn no desktop (Poco) vale por etapa, dobra no HTTPS por causa do fechamento TLS e ignora valores não inteiros (proposta para o Varn: um prazo único por requisição, sem esperar o fechamento TLS depois do timeout, e aceitar qualquer número).
