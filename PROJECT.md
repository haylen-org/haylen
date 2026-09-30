# Haylen — plano do projeto

Este é o documento central de organização do dono. Ele reúne os pedidos, as decisões técnicas, a estrutura de pastas, a lista completa de recursos da engine e dos samples e o status de cada item. As regras oficiais estão no `CLAUDE.md`.

Legenda de status: `[x]` implementado, testado e documentado. `[~]` em andamento. `[ ]` pendente.

Um item só recebe `[x]` quando está implementado, coberto por testes (quando a lógica roda sem GPU real ou runtime de plataforma) e documentado.

---

## 1. Objetivo

- Haylen é uma engine reutilizável para jogos, aplicações multimídia e apps. Ela é 2D e organizada para o 3D crescer ao lado sem renomear nada, com núcleo em C++20 distribuído como biblioteca CMake.
- Lua é a linguagem principal dos apps, pelo Varn, e toda a API continua disponível em C++.
- O runtime executa um pacote de app (pasta ou `.zip`) com `app.json`, os módulos Lua em `source/` (com `source/main.lua` como ponto de entrada) e os recursos em `content/`.
- A engine é compilada uma vez em artefatos prontos, e um app Lua é só o pacote, montado pelo `make.py` sobre os templates de plataforma. Apps C++ compilam a engine pelo CMake com `haylen_add_app`.
- Plataformas: macOS, Windows, Linux, iOS e iPadOS, Mac Catalyst, tvOS, Android (celular, tablet e Android TV num só APK) e web desktop e mobile (WebGPU, com WebGL2 como alternativa que a página escolhe). visionOS roda o app de iPad, e watchOS não é possível.
- Todo tipo de app de desktop: janelas comuns, sem moldura, transparentes, sempre no topo e com cliques atravessando, para apps que vivem na área de trabalho ou em cima da barra de tarefas.
- Runtime web pronto para um editor web futuro (em outro repositório), com pacotes entregues em tempo de execução, reinício sem recarregar a página, hot reload e erros com a pilha do Lua enviados ao JavaScript.
- Samples em `samples/<categoria>/`, com o jogo Tiny Island (pacote Tiny Swords) e um sample Lua para cada conjunto de recursos.
- Resolução de design configurável (1920x1080 por padrão) com a UI sempre dentro da safe area.
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
11. Código da engine separado do código do app.
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
30. GUI baseada em Dear ImGui com a mesma lista de componentes do workpane-new, todos baseados em tema, com suporte a nine-patch e imagens.
31. Tudo o que for de 2D fica na pasta `2d`. O foco é 2D, e a organização já prevê o 3D.
32. Depois de tudo pronto, revisar bugs, código legado, código não utilizado, erros, race conditions e riscos de crash que realmente importam.
33. Não parar até tudo estar desenvolvido, testado e documentado.
34. Tudo exportado para Lua usando o Varn (github.com/varn-org/varn). Tudo o que for feito em C++ precisa ser exportado para o Varn e ficar acessível ao app. Isso é regra.
35. A engine tem Lua como alvo, mas tudo pode ser usado em C++ também.
36. O CMakeLists da engine é uma biblioteca que pode ser usada em outros projetos.
37. A engine carrega um `.zip` com o app inteiro (`app.json`, `source/` e `content/`) e executa o `source/main.lua`. Também executa a partir de uma pasta com o mesmo formato.
38. Todo caminho de asset é relativo à pasta `content/`, sem repetir `content/` no caminho, para fontes, imagens, vídeos, áudios e qualquer outro tipo.
39. Testes para tudo ficar 100% testado, sem testes inúteis, sem testes que geram testes e sem excesso de testes.
40. Os samples ficam em `samples/<categoria>/`, com o Tiny Island e os assets do Tiny Swords em `samples/games/tiny-island`.
41. Sockets vêm do Varn no nativo. Na web, onde não existe TCP bruto, a engine oferece WebSocket pelo JavaScript do navegador, e o HTTP do Varn usa o fetch.
42. Cuidado com o async do Varn para nunca travar a UI e o loop principal.
43. A melhor arquitetura e organização possível, bem pesquisada e bem estruturada.
44. O CLAUDE.md contém as regras, a descrição do projeto, a estrutura e os padrões de código e de projeto.
45. No futuro haverá um site (em outro repositório) para editar o código Lua e os assets e rodar o app como aplicação wasm com WebGL2 ou WebGPU. A engine precisa funcionar nesse cenário.
46. Tudo deve ser módulo ou plugin para ficar organizado.
47. Arquivos `.h`, `.hpp`, `.c`, `.cpp` e `.mm` em PascalCase e arquivos Lua em dash-case. Isso é regra.
48. Os samples ficam em pastas dash-case (`samples/games/tiny-island`).
49. Os arquivos de módulo CMake são dash-case (`haylen-dependencies.cmake`).
50. Tudo o que for pedido para o editor web e para o resto do projeto fica registrado neste documento para não se perder.

### 2.1 Segundo pedido

Cada ponto abaixo veio do segundo pedido e precisa estar coberto por algum item da seção 14.

51. Nada de regra ou mecânica de jogo dentro da engine. O que é do jogo fica no jogo, como o ciclo de dia e noite com fases e contagem de dias do Tiny Island.
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
62. Formato do pacote: `app.json`, código Lua em `source/` (com `source/main.lua` como ponto de entrada) e recursos em `content/`.
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
74. Templates de plataforma: projetos prontos para cada plataforma que só esperam o pacote (`app.json`, `source/` e `content/`). O comando que roda um app Lua apaga e recria a pasta daquele app em `build/`, junta o template, as personalizações do app e o pacote, e roda na plataforma pedida no parâmetro, ou no desktop quando nenhuma é passada. O projeto C++ é um caso à parte.
75. Um comando para rodar samples Lua e outro para rodar samples C++.
76. O wasm da engine é pré-compilado, e só o pacote do app muda de um app para outro.
77. Um comando no `make.py` que serve uma pasta com um servidor Python que já suporta tudo o que o wasm precisa (MIME, SharedArrayBuffer, threads e os cabeçalhos COOP, COEP e CORP), recebendo a pasta e, opcionalmente, a porta.
78. Na web, a logo do projeto com uma barra de progresso durante o carregamento, e a logo da engine quando o projeto não tem uma.
79. Fluxo do desenvolvedor: um comando do `make.py` cria um projeto novo na pasta escolhida, com o template de todas as plataformas e um código Lua de exemplo com `source/` e `content/`.
80. Modo debug com estatísticas: FPS, tempo de frame, draw calls, vértices e a contagem de objetos criados, vivos e destruídos por tipo.
81. Nomes genéricos: o que o desenvolvedor cria é um app (`app.json`, `app.zip`, `haylen_add_app`), porque a engine serve para jogos, aplicações multimídia e apps. O que é 2D fica separado do que é 3D nos nomes, pastas e arquivos, para que o 3D do futuro não conflite nem obrigue a mover nada.
82. Uma revisão geral de tudo, organizada de forma profissional e fácil de desenvolver e de usar, com o objetivo de ser a engine mais fácil de usar, mais robusta e mais completa.

### 2.2 Terceiro pedido: organização do código C++ e tela de erro

Cada ponto abaixo precisa estar coberto pelos grupos A, B, J, N, O, P, Q, R, S, T, U, V, W e X da seção 14.2.

83. Membros com o nome normal, sem prefixo nem sufixo (nunca `m_`), e acessores com `get`, `set`, `is` e `has`.
84. Todo nome diz o que a coisa é: nada de plugin com nome de verbo ou de palavra vaga, como um plugin chamado `save`.
85. Os plugins ficam numa pasta e num namespace próprios de plugins.
86. Nada de funções ou métodos soltos fora de classes, nem de arquivos com uma struct e funções isoladas. Tudo pertence a uma classe.
87. Cada arquivo tem a sua própria classe, com o nome da classe.
88. Um sub-namespace para cada contexto, e não um namespace único para tudo.
89. Revisar tudo: não pode sobrar coisa solta, perdida ou fora de classe.
90. As regras gerais valem sempre e ficam no CLAUDE.md: sem gambiarras, fallbacks, código porco, código legado ou compatível com versões anteriores. Comentários raros e só onde precisam. Código e comentários em inglês. Nenhuma frase dividida por ponto e vírgula. Fazer só o que faz sentido, nunca para mostrar trabalho. Manter esta mega lista detalhada e revisar no fim se tudo foi feito 100%, testado e documentado.
91. A tela de erro é bem legível e detalhada, sem caracteres estranhos (como os tabs da pilha do Lua desenhados como quadrados), e sem gambiarras.
92. Os templates de iOS (e as outras plataformas Apple) e de Android têm a splash funcionando em paisagem e em retrato.
93. Os templates de plataforma ficam em `templates/platform/<plataforma>/`, com organização modular e extensível.
94. A engine traz os algoritmos que os jogos usam (A* e muitos outros), prontos para alto desempenho, incluindo a distribuição de elementos no mapa com base em área, densidade e regiões, e a destruição de elementos e de terreno.
95. Um sistema de tween bem robusto.
96. Sistema de eventos, ciclo de vida e conexão e desconexão de coisas, tudo bem robusto.
97. Raycast.
98. O nome da engine é Haylen (em minúsculas, `haylen`) em todos os lugares: arquivos, pastas, docs, README, classes, namespaces, alvos e funções do CMake, módulos Lua, pacotes Java, JavaScript, artefatos, templates e logo.
99. Ciclo de vida de cena com carregamento: a transição começa (cobre a cena atual), a engine chama um método de carregamento da nova cena (assíncrono), e depois a transição de saída exibe a nova cena carregada. Isso permite exibir um loading próprio se o desenvolvedor quiser, ou usar a própria transição como loading. A arquitetura precisa dar todas as possibilidades, com eventos assíncronos e ciclo de vida funcionando perfeitamente, pensada como arquitetura de software, com o máximo de desempenho e sem gambiarras, não importa o tamanho do trabalho.
100. Revisar tudo de novo atrás de bugs, código legado, código não usado, erros, race conditions e falhas que podem derrubar o app, corrigindo o que faz sentido (não código que nunca pode acontecer nem coisas aleatórias só para mostrar trabalho), e manter as regras gerais no CLAUDE.md e esta mega lista detalhada.
101. Os samples ficam em subpastas por categoria, e o comando recebe o caminho da categoria, por exemplo `python3 make.py run games/tiny-island`, para ficar mais organizado.
102. Aplicações sem moldura e transparentes, como o Taskbar Hero: janela sem barra de título e sem bordas, fundo transparente, o jogo rodando no rodapé da tela e arrastável, com a GUI/UI do jogo funcionando. É outra modalidade de jogo que a engine precisa suportar.
103. Comunicação fácil com qualquer plataforma (iOS, Android, desktop, web e as outras): enviar e receber a resposta da plataforma de forma assíncrona, para usar qualquer coisa nativa da plataforma.
104. Chamar bibliotecas e SDKs nativos, como a biblioteca da Steam, bibliotecas nativas em geral e SDKs como o P2P da Epic Online Services (NAT P2P). O `ffi` do Varn pode ser parte da solução. Não é preciso usar esses SDKs, eles são só exemplos, mas a capacidade precisa ser testada nas plataformas.
105. Organizar tudo isso na engine, revisado e testado, não importa o tamanho da refatoração, para a engine cobrir todos os casos do desenvolvimento de jogos. E revisar o projeto inteiro de novo atrás de bugs, código legado, código não usado, erros, race conditions e falhas que derrubam o app, com as regras gerais de sempre.
106. A cada bloco de trabalho terminado, fazer commit e push na `main`. A mensagem do commit tem o prefixo do tipo (feature, fix e os outros) e uma frase curta que começa com maiúscula, sem coautor e sem citar Claude ou qualquer outra pessoa. A regra fica no CLAUDE.md, não na documentação.
107. Antes de cada commit, conferir que não entra nada de build, arquivo temporário, chave de ambiente, segredo ou qualquer coisa privada ou temporária que não deveria ser commitada. A regra fica no CLAUDE.md.
108. Nunca citar outras engines, nem em docs, nem em comentários, nem em código, nem mesmo em comparações. A regra fica no CLAUDE.md.
109. Tudo o que o dono pedir entra nesta lista de coisas a fazer, para nada se perder.

### 2.3 Quarto pedido: CLAUDE.md completo e repositório sem histórico

Cada ponto abaixo precisa estar coberto pelo grupo O da seção 14.2.

110. O CLAUDE.md descreve o projeto sem histórico e sem versões de bibliotecas, e contém tudo sobre padrões de código, arquitetura, organização e regras, para que cada execução de um agente tenha o que precisa para novos recursos e correções. Histórico e informações de versões anteriores do projeto não podem existir em nenhum lugar do repositório.

### 2.4 Quinto pedido: plugins nativos

Cada ponto abaixo precisa estar coberto pelo grupo Z da seção 14.2.

111. O app fala com a plataforma de forma assíncrona: a chamada do Lua cai no handler em Swift ou Objective-C na Apple e em Kotlin ou Java no Android, e a resposta volta para o app sem travar o jogo. Quem desenvolve um plugin escreve a parte em Swift ou Objective-C e a parte em Kotlin ou Java (e em JavaScript na web).
112. O sistema de plugins é parte central da engine: precisa ficar fácil usar SDKs como Firebase e AdMob, inclusive os que mostram views nativas por cima do jogo, como o banner.
113. Tudo isso funciona 100% e otimizado em todas as plataformas, com pesquisa e planejamento de como fazer cada parte.
114. A cada bloco terminado, fazer commit e push.
115. Revisar o projeto inteiro de novo atrás de bugs, código legado, código não usado, erros, race conditions e falhas que derrubam o app, com as regras gerais de sempre, e manter tudo no CLAUDE.md e nesta mega lista.
116. Os plugins de terceiros citados (Firebase, AdMob e os outros) ficam fora deste repositório, em repositórios próprios em https://github.com/haylen-org, feitos pelo dono depois e separado. Este repositório tem só a arquitetura, a engine e a capacidade, com testes simples e equivalentes, sem bibliotecas de terceiros além das da engine.

### 2.5 Sexto pedido: diálogos, notificações, informações do sistema e webview

Cada ponto abaixo precisa estar coberto pelo grupo AA da seção 14.2.

117. Diálogos nativos, notificações do sistema e informações do sistema como recursos da engine: no desktop com bibliotecas C++ boas do GitHub (dependências da engine, como as outras), e no mobile e na web com as APIs de cada plataforma, cada plataforma com o que der.
118. Um navegador embutido (webview) como view nativa por cima do app: no desktop com a biblioteca C++ `webview`, e no mobile e na web com os equivalentes de cada plataforma.
119. Revisar bem tudo isso, com as regras gerais de sempre, tudo no CLAUDE.md e nesta mega lista, desenvolvido, testado e documentado 100%.
120. O CI do GitHub precisa passar em todos os jobs: o job do Windows e o do Android falham desde o primeiro push (o MSVC recusa uma comparação de `core::Json` com `std::string_view` no `PropertyReader.hpp`, e o `sdkmanager` não acha `platforms;android-37`, que se chama `platforms;android-37.0`).
121. Decisão: a engine fica só com o básico, que são as informações do sistema, a caixa de mensagem e os diálogos de arquivos e pastas. Notificações e o webview viram plugins, feitos pelo dono depois em repositórios próprios em https://github.com/haylen-org, e a biblioteca `webview` não entra, porque troca a view do Sokol no macOS, só aceita uma `GtkWindow` no Linux e trava a thread do frame no Windows.
122. Os plugins precisam ter a capacidade de enviar e receber de forma assíncrona tudo o que os recursos básicos de cada sistema pedem: áudio, câmera, foto, localização, notificação local e os dados de uma notificação push recebida, na web, no Android, no iOS, nos desktops e nas outras plataformas. A engine entrega a capacidade, e o plugin de demonstração testa cada mecanismo sem SDK de terceiros.
123. Plugins que abrem outra tela (uma activity no Android, um view controller na Apple, como o paywall do RevenueCat, telas de login, de compra, de câmera e de anúncios) precisam de uma arquitetura que suporte isso em todas as plataformas, pesquisada e pensada para não criar incompatibilidades no futuro: abrir a tela, pausar e cobrir o app enquanto ela aparece, receber o resultado e voltar ao jogo sem perder estado.
124. As imagens da engine usam a marca nova de `extras/images/` (símbolo, `logo-h` e `logo-v`) no lugar da logo antiga: nos ícones e nos espaços pequenos só o símbolo, sobre o fundo azul escuro da fonte (`#07112f`), e a logo onde há espaço.
125. Nenhuma frase começa com letra minúscula, em comentários, documentação, mensagens de erro e de log e na saída do `make.py`: quando a frase começaria com um comando, identificador ou caminho em minúsculo, vai uma palavra antes, como "O comando" ou "Rode". E toda expressão reservada (comando, código, identificador, caminho, chave e valor) fica marcada para não se misturar com a frase: crases nos documentos Markdown e nos comentários, e aspas duplas nas mensagens, nos logs e na saída dos comandos, como no exemplo do dono e como o Varn e o Workpane fazem. A regra vale para qualquer texto, inclusive as mensagens de commit, cuja frase depois do prefixo do tipo começa com maiúscula.
126. A engine não impõe nada aos projetos finais do Android e do Xcode. Cada plugin define o que o projeto precisa (frameworks, dependências, permissões, chaves do Info.plist, entitlements e configurações), e o projeto é do desenvolvedor, que o altera como quiser (por exemplo editando o `project.yml` do XcodeGen e gerando de novo), porque cada empresa tem o seu padrão. A engine não carrega nem compila coisas particulares que o app não usa. Quando um requisito de um plugin falta, o plugin registra no log e ignora a chamada até o desenvolvedor cumprir o requisito, em vez de quebrar. Analisar, pesquisar, planejar e revisar a melhor forma de organizar isso.
127. Regra: o `CLAUDE.md` só cita uma versão ou um número quando uma regra depende dele, como o C++20 da linguagem ou os 100% de cobertura que os testes buscam. Valores secundários que mudam o tempo todo ficam fora: versões de bibliotecas, de ferramentas, de SDKs e da própria engine, e limites, tamanhos, contagens e durações que o código declara. Para esses, a regra diz o tipo de limite e aponta o arquivo que guarda o valor.

## 3. Regras

As regras oficiais do projeto estão no `CLAUDE.md`, que é obrigatório e precisa ser lido por inteiro antes de qualquer trabalho. Ele reúne os princípios, as regras de trabalho e de commit, os nomes, a organização do código, os bindings Lua, a arquitetura, as dependências, os samples, a formatação, os comentários, os testes e a documentação. Este documento não repete essas regras.

## 4. Stack

As versões fixadas de cada componente ficam em `engine/cmake/haylen-dependencies.cmake`, nos templates de plataforma e no workflow de CI (`.github/workflows/ci.yml`).

| Componente | Uso |
| --- | --- |
| CMake e CPM.cmake | Build e download das dependências, fixadas por hash SHA-256 e com cache compartilhado em `.cache/cpm`. |
| Python | `make.py` e as ferramentas de `tools/`. |
| Varn | Runtime Lua (Lua compilado como C++), event loop, pools de workers, promises e os módulos `async`, `http`, `socket`, `json`, `fs`, `zip`, `crypto`, `log`, `platform`, `process`, `datetime`, `xml` e `ffi`. Traz Lua, Poco, OpenSSL, libzip, zlib e libffi, que a engine reaproveita. |
| nlohmann/json | JSON, declarado antes do Varn para existir uma cópia só no build. |
| Sokol | Janela, eventos e GPU (Metal, Direct3D 11, OpenGL, OpenGL ES 3 e WebGPU). |
| sokol-shdc | Compilador dos shaders da engine e dos shaders dos apps para todos os backends. |
| Dear ImGui | Base da UI e das janelas de debug. |
| Box2D | Física 2D. |
| Clipper2 | Operações booleanas e offset de polígonos. |
| miniaudio | Áudio, com os decodificadores de WAV, MP3, FLAC e OGG. |
| zstd | Camadas comprimidas dos mapas Tiled. |
| stb | Imagens, contornos TrueType e empacotamento de retângulos. |
| msdfgen | Campos de distância dos glifos a partir do contorno completo. |
| HarfBuzz, SheenBidi, libunibreak e o modelo tailandês do BudouX | Shaping, texto bidirecional e quebra de linha de todos os scripts. |
| fast_float | Leitura de números decimais em todas as plataformas. |
| GoogleTest | Testes da engine. |
| Emscripten | Build web (single-thread, como o Varn). |
| Android SDK, NDK, Gradle e Android Gradle Plugin | Biblioteca Android e apps Android. |
| AndroidX (core e core-splashscreen) e kotlinx-coroutines | Dependências da biblioteca Android: barras do sistema, splash e handlers suspensos da bridge. |
| Xcode e XcodeGen | Apps Apple e o template Apple. |
| clang-format | Formatação de C, C++, Objective-C e Objective-C++. |
| Tiled | Editor de mapas (formato JSON). |

## 5. Estrutura de pastas

```text
CMakeLists.txt              Projeto raiz: engine, player, testes e benchmarks.
make.py                     Ponto único de build para todas as plataformas e tarefas: artefatos da engine, criação, execução e empacotamento de apps, samples, shaders, servidor web, testes, cobertura, sanitizers, formatação e benchmarks.
CLAUDE.md                   Regras oficiais, descrição do projeto e estrutura.
AGENTS.md                   Aponta para o CLAUDE.md.
PROJECT.md                  Este documento.
README.md                   Apresentação e início rápido.
.github/workflows/ci.yml    CI: formatação, testes, cobertura, SDK e os artefatos web, Android e Apple.
docs/                       Guias e referência da API Lua (docs/lua-api/).
tools/                      Ferramentas Python (importador do Tiny Swords, gerador do mapa da ilha, leitura e escrita de PNG).
engine/
  CMakeLists.txt            Projeto CMake independente da engine, usável por outros projetos.
  cmake/                    Módulos CMake: CPM, dependências e patches, haylen_add_app, deploy do conteúdo, shaders, avisos, cobertura, instalação do SDK, fusão das slices do xcframework e toolchain do Mac Catalyst.
  include/haylen/           API C++ pública dos contextos sem dimensão: core, ai, math, io, assets, graphics, text, input, audio, ui, platform, localization, storage, debug, net, lua e plugins.
  include/haylen/2d/        API 2D: graphics, animation, particles, lighting, physics, tiled, navigation, spatial e procedural.
  src/                      Implementação no mesmo formato de include/, com a classe de binding Lua ao lado de cada contexto. src/lua/ guarda o toolkit de bindings, src/plugins/ os plugins embutidos e src/platform/ a fronteira com o sistema, a interop nativa e uma pasta por plataforma (sokol, headless, apple, android, web, windows, linux e desktop).
  shaders/                  Fontes sokol-shdc e a biblioteca de shaders que os shaders dos apps também incluem.
  platform/android/         Projeto Gradle da biblioteca Android haylen (activity com splash, bridge, controles, insets, entrada de texto e o player).
  platform/web/             Runtime JavaScript (haylen-runtime.js), processador de áudio AudioWorklet, shell HTML e seletor de backend dos apps C++.
  platform/apple/           Info.plist e launch screens dos apps Apple montados pelo CMake.
  bench/                    Benchmarks de sprites, algoritmos, geração procedural e Lua.
  tests/                    Suíte GoogleTest com os helpers de suporte, as fontes de teste e a biblioteca nativa de teste.
samples/
  <categoria>/<sample>/     Samples por categoria (games, graphics, gameplay, interface, system e cpp).
templates/
  app/                      Pacote do app inicial do make.py new.
  platform/apple/           Projeto XcodeGen (project.yml) com o App.xcodeproj gerado ao lado: iOS e iPadOS com Mac Catalyst, tvOS e macOS.
  platform/android/         Projeto Gradle do app, sem C++, que depende do AAR haylen.
  platform/web/             Página de carregamento com a logo, a barra de progresso, a escolha do backend e a tela de erro.
```

Um sample Lua tem `app.json`, `README.md`, `source/` e `content/`, e `platform/<template>/` só com o que acrescenta ao template. Só `app.json`, `source/` e `content/` são o pacote. O Tiny Island fica em `samples/games/tiny-island` e o sample C++ em `samples/cpp/embedding`.

Tudo o que o build gera fica em `build/`: as árvores `build/<plataforma>-<config>`, os artefatos da engine em `build/artifacts/`, os apps montados em `build/apps/<app>-<hash>/<plataforma>/` e os projetos C++ em `build/cpp/<projeto>-<hash>/`.

## 6. Arquitetura

O guia `docs/architecture.md` descreve a arquitetura em detalhe. Esta seção é o resumo.

### 6.1 Produtos e bibliotecas

| Alvo | O que é |
| --- | --- |
| `haylen::engine` | Biblioteca estática portável com toda a API C++ e todos os bindings Lua. Fala com a GPU pelo Sokol gfx e nunca chama o Sokol app nem APIs do sistema. |
| `haylen::platform` | Partes portáveis do runtime: tradução dos eventos do Sokol, handlers da bridge no desktop e o aviso de memória. |
| `haylen::runtime` | Host real de cada plataforma: ponto de entrada (`sokol_main`, ou `haylen_main` na Apple), `SokolHost` e os serviços da pasta da plataforma. |
| `haylen::headless` | Host dos testes: backend dummy do Sokol, mixer sem dispositivo e chamadas da bridge gravadas. |
| `haylen` | Player que roda qualquer pacote (`engine/src/platform/sokol/LuaPlayer.cpp`): executável no desktop e na web e `libhaylen.so` no Android. |
| `haylen_add_app` | Função CMake que monta o app C++ de um pacote para a plataforma do build. |

O executável sempre define `core::Application::create()`: o player devolve um `lua::Application`, que roda `source/main.lua`, e um app C++ devolve a própria aplicação. `make.py sdk` instala o SDK para `find_package(haylen)`, com as bibliotecas fundidas em `libhaylen` e `libhaylen_runtime`, e `make.py engine` gera os artefatos prontos dos apps Lua (seção 7).

Todo subsistema é um plugin (`plugins::Plugin`) com nome e os ganchos `start`, `installLua`, `event`, `beginFrame`, `fixedUpdate`, `update`, `render`, `renderUi`, `endFrame` e `stop`. `plugins::BuiltInPlugins` registra os plugins embutidos em ordem de dependência, e um projeto que usa a engine registra os próprios com `Engine::addPlugin`, pela mesma interface. A engine só alcança o sistema pela interface interna `platform::Host`.

### 6.2 Pacote do app

- Um pacote é uma pasta ou um `.zip` com `app.json`, os módulos Lua em `source/` e os recursos em `content/`. Nada mais na pasta faz parte do pacote, então projetos de plataforma, notas e arquivos de build podem ficar ao lado.
- `app.json` define identidade (`name`, `identifier`, `version`), janela (inclusive as opções de desktop), resolução de design e política de escala, orientação, passo fixo, tempo máximo de frame, cor de fundo, splash, ciclo de vida, sessão de áudio, debug, autoloads e bibliotecas nativas. Ele é lido antes do Lua, porque a janela é criada antes do primeiro script, e chaves desconhecidas e valores inválidos são erros claros.
- `source/main.lua` é o ponto de entrada. `require("scenes.menu")` carrega `source/scenes/menu.lua` (ou `source/scenes/menu/init.lua`), também dentro do zip e sempre como texto.
- Todo caminho de asset é relativo a `content/`: `assets.texture("tiny_swords/units/blue/warrior/idle.png")` lê `content/tiny_swords/units/blue/warrior/idle.png`.
- Onde o pacote fica: no desktop, a pasta ou o zip passado ao player (`haylen --dev samples/games/tiny-island` ou `haylen app.zip`). Nos apps Windows e Linux, `app/` ou `app.zip` ao lado do executável. Nos apps Apple, `Resources/app` do bundle. No Android, `app/` nos assets do APK, com `haylen-package-index.json`. Na web, o `app.zip` que a página entrega (`Module.haylen.packageData` ou `packageUrl`), ou `/app` no sistema de arquivos virtual dos apps C++.

### 6.3 Fluxo de um frame

1. O viewport se atualiza com o tamanho do framebuffer, a resolução de design, a política de escala e a safe area.
2. O host preenche os gamepads e o mapa de ações é atualizado, com o input segurado enquanto o app está parado ou uma troca de cena o bloqueia.
3. O relógio avança com o tempo do frame, limitado por `maxFrameTime` e multiplicado pela escala de tempo, e toques e gestos são atualizados.
4. A engine publica no `EventBus` as mudanças de tela cheia, orientação, safe area e controles, cria os recursos de GPU dos assets decodificados dentro do orçamento de upload do frame, avança o loop do Varn com `Runtime::poll()` (promises, corrotinas, timers, HTTP, sockets e conclusões dos workers) e entrega as respostas e os eventos da bridge.
5. Os plugins rodam `beginFrame`.
6. Os passos fixos rodam: `fixedUpdate` das cenas, tweens de passo fixo e `fixedUpdate` dos plugins.
7. Timers, tweens, `update` dos plugins e o update das cenas, onde a troca de cena pendente avança pelas fases, a view de loading é atualizada e a cena do topo recebe `update`.
8. O mixer de áudio é atualizado.
9. As cenas desenham o mundo e a UI, com o `render` e o `renderUi` dos plugins. Durante uma transição, as cenas de antes e de depois desenham cada uma na própria captura, e o efeito desenha as duas imagens na tela.
10. O renderer envia o frame inteiro para o swapchain do host.
11. Os plugins rodam `endFrame`, a `FrameQueue` roda os eventos enfileirados e os sinais adiados, o device destrói os objetos de GPU liberados e o input fecha o frame.

Um app parado pelas opções de ciclo de vida pula os passos 6 e 7 com tempo zero, e um app em segundo plano também não desenha. Eventos da plataforma chegam entre frames por `Engine::handleEvent`. Uma exceção do código do app mostra a tela de erro, e o loop do Varn, a bridge, `beginFrame` e `endFrame` continuam rodando para o hot reload e a página web poderem reiniciar o app.

### 6.4 Threads e async

- Thread do frame: GPU, estado Lua, cenas, UI, controle de áudio e física. Nunca bloqueia.
- `core::JobSystem` roda trabalho no `taskPool()` do Varn (`post`), I/O bloqueante no `ioPool()` (`postIo`), trabalho com conclusão na thread do frame (`run`) e laços paralelos (`parallelFor`, em que o chamador roda todo pedaço que nenhum worker começou).
- Imagens, áudio e mapas são decodificados no `taskPool()`, e os objetos de GPU são criados na thread do frame dentro do orçamento de upload. O renderer e as partículas usam `parallelFor`.
- Lua nunca roda em workers. Em Lua, operações lentas devolvem uma `Promise` do Varn que a corrotina aguarda com `:await()`, e computações longas rodam como corrotinas de `haylen.jobs` com orçamento de tempo por frame.
- Handlers nativos da bridge e as conexões WebSocket nativas (uma thread cada) entregam os resultados por filas que a thread do frame drena no começo do frame.
- Nenhum job deixa exceção escapar: a falha vira erro da engine na thread do frame.
- Estáticos que outras threads ainda usam na saída do processo são criados com `new` e nunca destruídos.
- A web é single-thread como o Varn: `poll()` roda os jobs dos pools, `parallelFor` roda na thread do frame, e a página não precisa de `SharedArrayBuffer` nem de cabeçalhos de isolamento.

### 6.5 Renderização 2D

- Canvases de mundo (câmera, luz e pós-processamento opcionais) e de tela (coordenadas de design), capturas em render targets e viewports.
- Ordem de desenho por camada, profundidade e Y-sort, com máscaras de visibilidade.
- Sprites, glifos, peças de nine-slice e splats de metaballs são instâncias de 48 bytes de um quad compartilhado, e o renderer junta desenhos vizinhos numa draw call quando eles têm o mesmo programa, blend, recorte, textura e sombreamento.
- Batches de sprites sem chamada Lua por sprite, e batches estáticos em buffers imutáveis para camadas de tiles e decoração.
- Luz: mapa de luz por canvas com luz ambiente, luzes pontuais, spot e direcionais, sombras de oclusores, normal maps e especular.
- Materiais com shaders próprios, pós-processamento, metaballs e texto por campo de distância.
- Recursos de GPU criados na thread do frame e destruídos depois do envio do frame, com os pools do Sokol dimensionados para jogos reais.
- Detalhes em `docs/rendering.md`.

### 6.6 API Lua

- Cada capacidade é um módulo `haylen.<modulo>` instalado em `package.preload`, no mesmo estado Lua dos módulos do Varn (`async`, `http`, `socket`, `json`, `fs`, `zip`, `crypto`, `log`, `platform`, `process`, `datetime`, `xml` e `ffi`).
- Os módulos dos subsistemas 2D terminam com `2d` (`haylen.graphics2d`, `haylen.animation2d`, `haylen.particles2d`, `haylen.lighting2d`, `haylen.physics2d`, `haylen.navigation2d`, `haylen.spatial2d` e `haylen.procedural2d`). Os módulos sem dimensão mantêm o nome (`haylen.graphics`, `haylen.input`, `haylen.audio`, `haylen.ui` e os outros), e `haylen.tiled` também, porque mapas do Tiled são 2D por natureza.
- Objetos C++ são userdata com métodos e propriedades: um par `getX` e `setX` é a propriedade `x` (`sprite.x`), e um getter só de leitura é propriedade ou método sem o prefixo (`world:bodyCount()`).
- Opções vão em tabelas, e uma chave desconhecida dá `Unknown option "<key>".`. Enums vão pelo nome em string.
- Cenas e autoloads são tabelas Lua com ganchos, `haylen.class` monta classes com herança e mixins, e tudo o que uma cena ou outro dono cria termina com ele.
- A referência completa está em `docs/lua-api.md` e `docs/lua-api/`, e o modelo de programação em `docs/lua.md`.

## 7. Plataformas e build

Os guias `docs/distribution.md` (comandos, artefatos, templates e montagem dos apps), `docs/build.md` (build da engine, dependências, apps C++ e web) e `docs/embedding.md` (engine dentro de outro projeto CMake) têm os detalhes.

### 7.1 Plataformas

| Plataforma | Backend gráfico | Como um app Lua roda |
| --- | --- | --- |
| macOS | Metal | Player de desktop com hot reload (`make.py run <app>`) ou o alvo macOS do template Apple (`--platform macos`). |
| Windows | Direct3D 11 | Player de desktop ou o player ao lado do pacote (`--platform windows`). |
| Linux | OpenGL core | Player de desktop ou o player ao lado do pacote (`--platform linux`). |
| iOS e iPadOS | Metal | Alvo iOS do template Apple (`--platform ios` ou `ios-simulator`). |
| Mac Catalyst | Metal | Alvo iOS no Mac (`--platform catalyst`). |
| tvOS | Metal | Alvo tvOS do template Apple (`--platform tvos` ou `tvos-simulator`). |
| Android | OpenGL ES 3 | Template Android com o AAR `haylen` (`--platform android`). |
| Web | WebGPU ou WebGL2, escolhido pela página | Template web com o player pré-compilado e o `app.zip` (`--platform web`). |

As versões mínimas são iOS e tvOS 16.3, macOS 13.3, Mac Catalyst 16.4 e Android 8.1 (API 27), com os motivos em `docs/distribution.md`.

### 7.2 Comandos do make.py

| Comando | Para que serve |
| --- | --- |
| `tools` | Baixa as ferramentas fixadas (`sokol-shdc`, emsdk e Gradle) em `.tools/`. |
| `configure` e `build` | Gera e compila uma árvore da engine em `build/<plataforma>-<config>`. |
| `test` | Compila e roda os testes da engine nesta máquina, com `--sanitizers address` ou `thread` quando pedido. |
| `engine` | Gera os artefatos prontos em `build/artifacts/`: `apple/Haylen.xcframework`, o AAR `haylen` num repositório Maven local em `android/maven`, os players web WebGPU e WebGL2 em `web/` e o player de desktop em `desktop/`, com um `manifest.json` que marca os artefatos velhos. |
| `new` | Cria um app a partir de `templates/app`, com a cópia de todos os templates de `templates/platform` em `platform/`. |
| `samples` | Lista os samples por categoria, com o comando que roda cada um. |
| `run` | Roda um app ou sample: no player desta máquina com hot reload, ou montado a partir dos templates para `--platform macos`, `windows`, `linux`, `ios`, `ios-simulator`, `tvos`, `tvos-simulator`, `catalyst`, `android` ou `web`. |
| `run-cpp` | Compila e roda um projeto C++ que usa `haylen_add_app`, no desktop, na web, no iOS, no tvOS, nos simuladores, no Mac Catalyst ou no Android. |
| `package` | Gera o zip com `app.json`, `source/` e `content/` de um app. |
| `shaders` | Compila os shaders de `content/shaders/` de um app em arquivos `.shader`. |
| `serve` | Serve uma pasta com MIME do wasm, COOP, COEP (com `--coep credentialless` ou `off`), CORP, CORS, sem cache e com arquivos pré-comprimidos. |
| `coverage` | Mede a cobertura da engine com LLVM source-based coverage. |
| `format` | Aplica o `.clang-format` e confere os lambdas multilinha, com `--check` no CI. |
| `bench` | Roda os benchmarks de sprites, algoritmos (`--suite algorithms`), geração procedural (`--suite procedural`) e Lua (`--suite lua`). |
| `sdk` | Compila e instala o SDK para `find_package(haylen)`. |
| `embedding` | Compila o sample C++ por `add_subdirectory`, CPM ou o SDK instalado. |
| `assets` e `map` | Importam o Tiny Swords e geram o mapa da ilha do Tiny Island. |
| `clean` | Apaga `build/`. |

Os comandos recebem o caminho do sample a partir de `samples/` (`python3 make.py run games/tiny-island`), e `run` sem app roda o Tiny Island.

### 7.3 Como um app é montado

Cada app tem a própria pasta de build, `build/apps/<app>-<hash>/`, e `run` recria `<plataforma>/` nela a cada execução:

1. Copia o template da plataforma (`apple` para as plataformas Apple, `android` ou `web`). Windows e Linux começam de uma pasta vazia.
2. Aplica por cima o `platform/<template>/` do app: o mesmo caminho substitui, e arquivo novo é adicionado.
3. Injeta o pacote: `app/` ao lado do projeto Xcode, `app/src/main/assets/app/` com o índice no Android e `app.zip` na web.
4. Compila ou copia as bibliotecas nativas da seção `native` do `app.json` para o lugar de cada plataforma.
5. Grava as configurações geradas a partir do `app.json` (nome, identificador, versão, orientação e splash): `App.xcconfig`, os `Info.plist` e os assets de splash na Apple, as chaves `haylen` do `gradle.properties` e os recursos de splash no Android, e o `config.json` na web.

Depois disso, `run` compila o projeto, instala e abre o app no simulador, emulador, aparelho, Mac ou servidor local, e transmite a saída do app no terminal. Os artefatos da engine são gerados de novo quando o hash das fontes da engine muda.

### 7.4 Apps C++

Um app C++ é um projeto CMake que chama `haylen_add_app(<alvo> PACKAGE <pasta> [SOURCES ...] [CPP] [APPLE_PROJECT ...] [WEB_SHELL ...])` e compila a engine por `add_subdirectory`, CPM ou `find_package(haylen)`. A função leva o pacote para o app de cada plataforma: links para a pasta do pacote no Windows e no Linux (alvo `SYNC_PACKAGE-<alvo>`), recursos em `Resources/app` do bundle na Apple, `--preload-file` em `/app` na web e, no Android, a biblioteca do app e o caminho do pacote, que `make.py run-cpp --platform android` junta ao template Android. O sample `samples/cpp/embedding` é o exemplo.

## 8. Recursos da engine

Regra geral: todo item desta seção que tem API em C++ só está pronto quando também tem binding Lua, teste do binding e página em `docs/lua-api/`.

### 8.0 Runtime Lua (Varn) e pacotes de app

- [x] **Integração com o Varn**: `varn_core` via CPM, um `varn::runtime::Runtime` por engine, `poll()` por frame e módulos da engine em `package.preload`.
- [~] **Driver por plataforma do Varn**: cliente HTTP da plataforma na Apple (NSURLSession) e no Android (HttpURLConnection com as classes Kotlin do Varn e `AndroidHttpBridge::publish` no `JNI_OnLoad` da engine), e fetch na web. Validado no Android (emulador arm64, HTTPS com status 200) e na web. Falta validar na Apple.
- [x] **WebSocket** (`haylen.net`, `net::WebSocket` e `plugins::NetPlugin`): cliente `ws://` e `wss://` com subprotocolos, texto, binário, mensagens fragmentadas, ping e pong, fechamento pelos dois lados e erros. No nativo, cada conexão roda numa thread própria com o Poco, e os certificados são verificados com o trust store que o Varn encontra. Na web, usa o `WebSocket` do navegador através da página. Os eventos chegam no começo do frame, e o plugin mantém o socket vivo até fechar. Validado nos testes com um servidor Poco e no Chrome com WebGPU e WebGL2.
- [x] **Pacote em pasta**: leitura de `app.json`, dos módulos Lua de `source/` e dos assets de `content/` a partir de uma pasta.
- [x] **Pacote em zip**: leitura de todos os arquivos de dentro de um `.zip` (em disco, embutido no app ou nos assets do Android), com acesso seguro entre threads.
- [x] **app.json**: identidade (`name`, `identifier` e `version`), janela com as opções de desktop, resolução de design, escala, orientação, passo fixo (`fixedRate`, em Hz), `maxFrameTime`, `clearColor`, `splash`, `lifecycle`, `audio`, `debug`, `autoload` e `native`, com erro claro para chaves desconhecidas e valores inválidos. `haylen_add_app` e os templates leem nome, identificador e versão do mesmo arquivo.
- [x] **Loader de módulos Lua** que resolve `require` na pasta `source/` do pacote, sempre em modo texto.
- [x] **Aplicação Lua** (`lua::Application`): carrega o pacote, executa `source/main.lua` e encaminha o ciclo da engine para as cenas Lua.
- [x] **Player** `haylen [--dev] <pasta|zip>` no desktop (validado com Metal no macOS) e o player web pré-compilado, que recebe o `app.zip` da página do template web (validado com WebGPU no Chrome).
- [~] **haylen_add_app** gerando apps C++ para todas as plataformas com o pacote como conteúdo. Desktop, web, Android (APK no emulador com bridge, HTTP, pausa e retomada), simulador iOS e Mac Catalyst validados. Falta rodar no tvOS.
- [x] **Engine como biblioteca**: três formas de consumo da pasta `engine/`, todas com `haylen::engine`, `haylen::runtime` e `haylen_add_app`. `add_subdirectory` e `CPMAddPackage` compilam a engine dentro do projeto, com C++20 e o suporte a exceções da web propagados pelo target, e testes, player e benchmarks desligados quando a engine não é o projeto principal. `find_package(haylen)` usa o SDK de `make.py sdk --platform <macos|linux|windows|web|web-webgl2>`: todas as bibliotecas estáticas do fechamento da engine (Varn, Poco, OpenSSL, libuv, Lua, Box2D e as outras) são fundidas em `libhaylen` e o runtime em `libhaylen_runtime`, com os headers públicos, os headers de Dear ImGui, Lua e nlohmann/json que eles usam, o `haylen-config.cmake` e os arquivos de `haylen_add_app` em `share/haylen`, instalados pelo componente `haylen_sdk`. O sample `samples/cpp/embedding` é um projeto CMake próprio compilado nos três modos por `make.py embedding --mode subdirectory|cpm|package` (validados no macOS, e o modo `package` também na web com o SDK WebGPU no Chrome). Apps C++ para Android, iOS, tvOS e Mac Catalyst compilam a engine pelo CMake com `make.py run-cpp`.
- [x] **API C++ para bindings** (`haylen/lua/`) para que outros projetos exponham seus próprios módulos, inclusive `lua::Promise` para bindings assíncronos, que o app espera com `:await()` e que o código nativo resolve de qualquer thread sem depender dos headers do Varn (o SDK não instala esses headers).
- [x] **Erros de tarefas assíncronas**: um erro que escapa de `async.spawn` ou `async.run` mostra a tela de erro com a pilha da corrotina e chega ao `onError` da página, como um erro de callback de cena. Falhas em `start`, `installLua` e `endFrame` de plugins também viram tela de erro.

### 8.0.1 Plugins

- [x] **Interface de plugin** (`plugins::Plugin`) com nome, ciclo de vida uniforme (`start`, `installLua`, `event`, `beginFrame`, `fixedUpdate`, `update`, `render`, `renderUi`, `endFrame` e `stop`, com os eventos de ciclo de vida do app) e instalação dos próprios módulos Lua.
- [x] **Registro de plugins** (`plugins::PluginRegistry` e `plugins::BuiltInPlugins`), com os subsistemas embutidos registrados como plugins em ordem de dependência: `core`, `jobs`, `input`, `graphics2d`, `assets`, `text`, `animation2d`, `particles2d`, `audio`, `physics2d`, `tiled`, `spatial2d`, `procedural2d`, `navigation2d`, `localization`, `storage`, `debug`, `hotReload`, `ui`, `net`, `platform` e `native`.
- [x] **Plugins externos**: projetos que usam a engine registram seus plugins em C++ pela aplicação (`Engine::addPlugin`) e usam seus módulos em Lua pelo mesmo `require`.

### 8.0.2 Cenário do editor web

- [x] **Pacote em memória** (`io::MemoryPackage`): pacote montado a partir de um zip em memória ou de um conjunto de arquivos enviado pela página.
- [x] **API JavaScript do runtime web**: `Module.haylen.packageData`, `packageUrl`, `loadZip`, `clearFiles`, `setFile`, `removeFile`, `run`, `restart`, `stop`, `setPaused`, `pause`, `resume`, `paused` e `reloadAsset`, sem recarregar a página, além de `register`, `unregister`, `emit`, `onLog`, `onError`, `onStarted`, `onStopped` e `onStats`.
- [x] **Hot reload** de scripts e assets alterados: no desktop o player com `--dev` observa a pasta do pacote, reinicia o app quando um arquivo de `source/` ou o `app.json` muda (inclusive a partir da tela de erro) e recarrega no lugar os assets de `content/`. Na web o editor usa `setFile` com `run` ou `reloadAsset`.
- [x] **Erros e logs para o host**: mensagens de log (`onLog`) e erros Lua com a pilha (`onError`) enviados ao JavaScript, e ao console nas outras plataformas.
- [x] **Tela de erro**: quando um script falha, a engine mostra o erro na tela e continua viva para receber a correção.
- [x] **Reinício limpo**: o app pode ser destruído e recriado no mesmo processo, liberando cenas, estado Lua, assets e recursos de GPU.

### 8.1 Build e ferramentas

- [x] **Estrutura em minúsculo** conforme a seção 5, com `2d/` separado e `samples/` por categoria.
- [x] **CPM** com hash SHA-256 por dependência e cache compartilhado em `.cache/cpm`.
- [x] **Detecção de plataforma e backend** com override pela opção CMake `HAYLEN_RENDER_BACKEND`.
- [~] **Deploy do conteúdo** do pacote do app (desktop, Apple, web e Android).
- [x] **Shaders** compilados com sokol-shdc para GLSL 4.30, GLSL 3.00 ES, HLSL 5, Metal (macOS, iOS e simulador) e WGSL.
- [~] **make.py** com `tools`, `configure`, `build`, `test`, `engine`, `new`, `samples`, `run`, `run-cpp`, `package`, `shaders`, `serve`, `coverage`, `format`, `bench`, `sdk`, `embedding`, `assets`, `map` e `clean` para todas as plataformas. Faltam rodar os apps numa máquina Windows e Linux e em aparelhos Apple físicos.
- [x] **clang-format** com o `.clang-format` do repositório, aplicado por `make.py format` em todo C, C++, Objective-C e Objective-C++ da engine, dos samples e dos templates, com verificação de lambdas multilinha sem `clang-format off/on` e modo `--check` para o CI.
- [x] **Avisos e sanitizers**: `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow` (ou `/W4`), e AddressSanitizer com UndefinedBehaviorSanitizer ou ThreadSanitizer opcionais (`--sanitizers address|thread`).
- [x] **Cobertura** com LLVM source-based coverage e relatório HTML e texto filtrado para a engine.
- [x] **CI** no GitHub Actions (`.github/workflows/ci.yml`): verificação de formatação, testes e SDK com `find_package` no macOS, Linux e Windows, cobertura no macOS e os artefatos da engine para web (WebGPU e WebGL2), Android (AAR) e Apple (xcframework), com cache das fontes do CPM e do emsdk. Cada teste tem um limite de 120 segundos, então um teste que trava falha sozinho e o resto da suíte roda.

### 8.2 Core

- [x] **Application e AppConfig** (`core::Application` e `core::AppConfig`): identidade, janela, resolução de design, escala, orientação, passo fixo, tempo máximo de frame, cor de fundo, splash, ciclo de vida, sessão de áudio, debug, autoloads e bibliotecas nativas, lidos do `app.json`.
- [x] **Engine** (`core::Engine`): dona de todos os serviços de um app e do loop de frame da seção 6.3.
- [~] **Host**: interface interna entre engine e plataforma (`platform::Host`), com host real e host headless.
- [x] **Cenas** em C++ e Lua com os ganchos `load`, `enter`, `exit`, `unload`, `pause`, `resume`, `paused`, `unpaused`, `event`, `fixedUpdate`, `update`, `render` e `renderUi`, pilha com `push`, `pop`, `replace`, `popTo` e `popToRoot` e transições (grupos C e V da seção 14.2).
- [x] **JobSystem sobre o Varn**: trabalho no `taskPool()` com conclusão no event loop, e `parallelFor` com o chamador participando (inline na web).
- [x] **Log** integrado ao log do Varn (um único console por plataforma), com níveis e `std::format`.
- [x] **Signal e Connection**: callbacks tipados com desconexão RAII, seguros durante a emissão.
- [x] **TimerScheduler**: `after`, `every`, cancelamento e pausa.
- [x] **Random** (`math::Random`): xoshiro256** determinístico, faixas, chance, peso e embaralhamento.
- [x] **UTF-8** (`core::Utf8`): decodificação e codificação.
- [x] **FrameClock**: tempo limitado, escala de tempo, passos fixos e interpolação.

### 8.3 Matemática

- [x] **Vec2 e Rect** com todas as operações usuais.
- [x] **Color** com parsing `#RRGGBB` e `#AARRGGBB` (formato do Tiled), HSV, lerp e empacotamento RGBA8.
- [x] **Transform2D** afim com composição e inversa.
- [x] **Utilitários** (`math::Math`): constantes, lerp, remap, smoothstep, move toward, ângulos e amortecimento.
- [x] **Easing**: todas as famílias de Penner (in, out e in-out) e nomes para JSON.
- [x] **Ruído** (`math::Noise2D`): Perlin, simplex e fractal com semente.
- [x] **Geometria** (`math::Geometry`): círculos, segmentos, polígonos, interseções, área, centróide, convexidade, fecho convexo e triangulação.
- [x] **Poisson disk**: pontos espalhados com distância mínima e predicado de aceitação.

### 8.4 Arquivos, assets e preload

- [x] **Sistema de arquivos do pacote** (`io::Package`): leitura de arquivos do pacote (pasta, zip ou memória) por caminho relativo, sem sair da raiz, com `content/` implícito para assets e `source/` para módulos.
- [~] **Armazenamento do usuário** (`storage::UserStorage`): pasta de dados por plataforma (IDBFS em `/persistent` na web, com sincronização).
- [x] **Imagens**: PNG, JPG, TGA e BMP via stb_image em workers.
- [x] **Gerenciador de assets** (`assets::Manager`): texturas, atlas, fontes TrueType e bitmap, sons, mapas Tiled, mundos Tiled, efeitos de partículas (`.particles`), JSON, texto e bytes. Carga síncrona e assíncrona, cache por caminho, estados e upload de GPU só no frame.
- [x] **Grupos de preload**: declarados em código ou em manifesto JSON, carregados em paralelo com progresso e descarregados explicitamente. Assets compartilhados só saem quando o último grupo os solta.
- [x] **Hot reload** de texturas (substituídas no lugar, todos os handles veem a imagem nova) e de JSON e demais assets (saem do cache e são lidos de novo) no desktop com `--dev`, validado no player do macOS.

### 8.5 Gráficos (base e 2D)

- [x] **Texturas** (`graphics::Texture`): RGBA8, filtro nearest ou linear, clamp, repeat ou mirror, e handles sem tipos do Sokol.
- [x] **Render targets** (`graphics::RenderTarget`) usáveis como textura.
- [x] **Viewport e escala** (`graphics::Viewport`): políticas fit (letterbox), fill (corte), stretch, expand e pixel perfect, e conversões entre janela, framebuffer e design.
- [x] **Renderer** (`graphics2d::Renderer`) instanciado, com batching, camadas e Y-sort.
- [x] **Canvases** de mundo e de tela, com luz opcional.
- [x] **Câmera 2D** (`graphics2d::Camera`): posição, zoom, rotação, limites, seguir suave, zona morta, tremor e área visível para culling.
- [x] **Sprites** (`graphics2d::Sprite`): pivô, rotação, escala, flips (incluindo a diagonal do Tiled), tinta, cor de flash para dano, camada e profundidade.
- [x] **Atlas** (`animation2d::SpriteAtlas`): regiões com trim, TexturePacker JSON (hash e array), Aseprite JSON e fatiamento em grade.
- [x] **Nine-slice** (`graphics2d::NineSlice`) clássico e em nove peças separadas (formato das peças do Tiny Swords), com centro esticado ou repetido.
- [x] **Primitivas**: linhas com espessura, polilinhas, retângulos, contornos, círculos, anéis, arcos e polígonos côncavos.
- [x] **Meshes** indexadas com textura, cor e recorte (scissor).
- [x] **Batches estáticos** (`graphics2d::StaticSpriteBatch`) em buffers imutáveis.
- [x] **Caminho de milhões de sprites**: `drawBatch` com conversão paralela no `JobSystem`, batches estáticos na GPU e o benchmark `make.py bench` (app `haylen-sprite-benchmark`, vsync desligado, fases de 100 mil, 1 milhão e 2 milhões de sprites dinâmicos e estáticos). Num Apple M5 Pro com Metal em Release: 2 milhões de sprites animados todo frame a 105 fps com 4,5 ms de CPU por frame, e 2 milhões estáticos no limite de 120 Hz da tela com 0,02 ms de CPU.
- [x] **Texto SDF** (`text::TrueTypeFont`): fontes TrueType e OpenType em atlas dinâmico, UTF-8, kerning, alinhamento, quebra de linha, contorno e sombra.
- [x] **Pós-processamento** (`graphics2d::PostProcess`): vinheta, tinta, saturação, brilho, contraste, fade e materiais.
- [x] **Estatísticas**: sprites, instâncias, draw calls, trocas de textura e bytes enviados.
- [x] **Limites de recursos da GPU**: pools do Sokol dimensionados para jogos reais (4096 texturas e render targets, 8192 views e 4096 buffers), com erro claro quando um pool enche, sem texturas inválidas silenciosas.

### 8.6 Animação

- [x] **Clipes de sprite** (`animation2d::Animation`): frames com duração, modos once, loop e ping-pong, velocidade, eventos por frame e fim.
- [x] **Conjuntos de animação** a partir de tiras, atlas ou tags do Aseprite.
- [x] **Player de animação** (`animation2d::Animator`): play, reinício, parada, velocidade, frame atual, tempo, callbacks de frame e de fim e fila de animações que toca a próxima quando a atual termina (`animator:queue` e `animator:clearQueue`).
- [x] **Tweens** (`core::TweenManager` e `haylen.tween`): float, Vec2 e Color em qualquer tabela ou userdata, com easing, atraso, repetição (`repeatCount`, negativo para sempre), yoyo, callbacks, pausa, `kill` e `wait()` com Promise, timelines e tags para matar, completar, pausar e retomar grupos (`killTag`, `completeTag`, `pauseTag` e `resumeTag`). O grupo S da seção 14.2 lista o sistema completo.

### 8.7 Input

- [x] **Teclado** completo com bordas, modificadores e texto UTF-32.
- [x] **Mouse** com botões, posição em janela e design, delta, roda, cursor, visibilidade e captura.
- [x] **Touch** com até dez toques e fases (início, movimento, fim e cancelado).
- [~] **Gamepads**: quatro slots, botões padrão, sticks, gatilhos, zona morta e conexão. GameController (Apple), XInput (Windows), joystick Linux, Gamepad API (web) e eventos Android.
- [x] **Mapa de ações** (`input::ActionMap`) em JSON: botões e eixos ligados a teclas, mouse, gamepad e controles virtuais, com remapeamento, definição de ações uma a uma, validação estrita das listas de bindings e detecção do último dispositivo. Enquanto a UI captura o ponteiro, os bindings de mouse não disparam ações (um clique no botão de pausa do HUD não vira ataque), e os botões virtuais continuam funcionando.
- [x] **Controles de toque**: `touchStick` (fixo ou flutuante, com zona morta) e `touchButton` como componentes de UI, com vários dedos ao mesmo tempo, alimentando os sticks e botões virtuais do mapa de ações e soltando tudo o que deixa de ser desenhado.
- [x] **Gestos** (`input::GestureRecognizer`): toque, duplo toque, toque longo, deslizar e pinça (`tap`, `double_tap`, `long_press`, `swipe` e `pinch`), com o mouse valendo como um dedo e limites configuráveis (`input.gestures()`, `input.setGestureSettings` e `input.gestureSettings`).

### 8.8 Áudio

- [x] **Mixer miniaudio** (`audio::Mixer`) com barramentos master, música, efeitos, UI e ambiente, com volume e mudo.
- [x] **Sons** WAV, MP3, FLAC e OGG lidos do pacote, decodificados de forma assíncrona.
- [x] **One-shots** com volume, pitch, variação aleatória de pitch (`pitchVariation`, com semente para testes), pan, fade, início deslocado, loop e limite de vozes com roubo da voz mais antiga.
- [x] **Música** em streaming com loop, crossfade, pausa e retomada.
- [x] **Áudio posicional 2D** com ouvinte e atenuação.
- [~] **Ciclo de vida**: pausa ao suspender o app e retoma depois.

### 8.9 Física 2D

- [x] **Mundo Box2D** (`physics2d::World`) com pixels por metro, gravidade, substeps e passo fixo. Todas as grandezas entram e saem em unidades de pixel: forças e forças de motor escalam por pixels por metro, e torques e impulsos angulares por pixels por metro ao quadrado (`maxMotorForce` e `maxMotorTorque` separados).
- [x] **Corpos e formas** (`physics2d::Body` e `physics2d::Shape`): estático, cinemático e dinâmico. Círculo, caixa, cápsula, polígono, segmento e cadeia. Densidade, atrito, restituição, sensores, categorias e máscaras.
- [x] **Juntas** (`physics2d::Joint`): distance, revolute, prismatic, weld, wheel, motor, mouse e filter (desliga a colisão entre dois corpos).
- [x] **Eventos**: contato (início e fim), sensor (início e fim) e impacto, com user data.
- [x] **Consultas**: raycast (mais próximo e todos), AABB e sobreposição de forma.
- [x] **Debug draw** pelas primitivas.

### 8.10 Tiled

- [x] **Mapas** `.tmj` (`tiled::Map`): ortogonal, isométrico, staggered, hexagonal e oblíquo (`skewx` e `skewy`, com conversão de células, objetos, limites e colisão), ordem de render, stagger, lado do hexágono, cor de fundo, origem de parallax e mapas infinitos.
- [x] **Camadas de tiles**: CSV e base64 com zlib, gzip e zstd, dados finitos e chunks, e flips incluindo a rotação hexagonal de 120 graus.
- [x] **Camadas de objetos**: retângulo, elipse, cápsula (com colisão), ponto, polígono, polilinha, texto (com rotação), objeto de tile, opacidade por objeto, templates `.tj` (JSON) e ordem de desenho.
- [x] **Camadas de imagem** com repetição em x e y.
- [x] **Grupos** aninhados com offset, opacidade, tinta, visibilidade e parallax herdados.
- [x] **Atributos de camada**: opacidade, visibilidade, tinta, offset, parallax, classe e modo de blend (`normal`, `add`, `multiply` e `screen`). Os modos que o blend fixo da GPU não reproduz, como `overlay` e `difference`, são rejeitados com erro claro. Travamento é só do editor.
- [x] **Tilesets** embutidos e externos `.tsj`, de imagem e de coleção, margem, espaçamento, offset, alinhamento de objeto, tamanho de render, modo de preenchimento, grade, transformações, terrenos e Wang sets, classe por tile, probabilidade, animações e colisão.
- [x] **Propriedades**: string, int, float, bool, color, file, object, class e list, incluindo classes e listas aninhadas. Em Lua uma lista vira uma sequência de valores convertidos.
- [x] **Renderização** (`tiled::MapRenderer`) com culling em todas as orientações, tiles animados, parallax, tinta, opacidade, imagens repetidas, objetos de tile e camadas estáticas em batch.
- [x] **Colisão**: corpos Box2D a partir das formas dos tiles e das camadas de objetos.
- [x] **Spawn de objetos**: `map:objects()` entrega objetos com classe, nome, forma e propriedades, e `map:spawn(fábricas, camada)` chama a fábrica de cada classe com a posição no mundo (offsets de camada e grupo incluídos). Em C++, `tiled::ObjectFactories` e o `forEachObject` de `tiled::MapQuery` e `tiled::MapRenderer`.
- [x] **Mundos** `.world` (`tiled::World`) com posições e padrões.

### 8.11 Partículas

- [x] **Efeitos**: emissores com formas ponto, círculo, anel, retângulo e cone. Taxa, rajadas com tempo, duração com loop, prewarm (a partir da posição do emissor no primeiro update), vida, velocidade, direção e espalhamento, gravidade, amortecimento, acelerações radial e tangencial, giro, tamanho e cores ao longo da vida, animação por frames, blend e espaço local ou mundo. Configurados por tabela Lua, `particles2d::EmitterConfig` ou arquivo `.particles` (JSON com as mesmas opções, carregado pelo `assets::Manager` e aceito por `particles2d.newEmitter(efeito, ajustes)`).
- [x] **Simulação** (`particles2d::Emitter`) em arrays paralelos (SoA) sem alocação depois do aquecimento, com remoção compactando no lugar e atualização em blocos paralelos pelo `JobSystem` (`update(dt, jobs)`, usado pelo Lua). O resultado paralelo é idêntico ao serial.
- [x] **Render** em um batch por emissor.

### 8.12 Luz 2D

O ciclo de dia e noite é mecânica de jogo e fica no Tiny Island. O grupo I da seção 14.2 lista os tipos de luz, as sombras e as máscaras.

- [x] **Mapa de luz** por canvas com luz ambiente e luzes aditivas.
- [x] **Luzes** (`lighting2d::Light`) com posição, raio, cor, intensidade, textura, rotação e escala, e cintilação determinística (`lighting2d::LightFlicker` e `lighting2d.flicker`).

### 8.13 UI (Dear ImGui, temas e componentes)

- [x] **Backend Dear ImGui próprio** (`ui::Backend`) que desenha pela pipeline de mesh do `graphics2d::Renderer` em coordenadas de design, implementa o protocolo de texturas dinâmicas do Dear ImGui, recebe mouse, toque, teclado, texto, clipboard e navegação por gamepad, mostra o teclado virtual quando um campo pede texto, transforma erros do Dear ImGui em erros de script e se recupera de frames deixados abertos. Validado no Chrome com WebGPU.
- [x] **Temas** (`ui::Theme`): papéis semânticos `Theme::Color`, `Theme::Metric`, `Theme::Font` e `Theme::Surface`, temas `dark` e `light`, temas em JSON sobre uma base com imagens nine-slice e fontes, troca em tempo real e sincronização com o estilo do Dear ImGui. O tema texturizado Tiny Swords fica no sample (`content/ui/theme.json`).
- [x] **Árvore de componentes retida** (`ui::Document`, `UiDocument` em Lua): propriedades lidas de JSON com validação estrita, patches atômicos validados sobre o estado mesclado, troca de filhos, comandos, medição com cache por frame, propriedades comuns (visível, habilitado, tooltip, crescer, tamanhos e alinhamento), eventos por id de nó entregues antes do update e registro por tipo (`ui::ComponentRegistry`).
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

- [x] **Bridge de plataforma** (`platform::Bridge`): chamadas JSON do C++ e do Lua para o nativo com resposta assíncrona na thread do frame, e eventos do nativo para assinantes C++ e Lua.
- [~] **Registros nativos**: `HaylenBridge.register` no Android (handlers Java na main thread), `HaylenBridge` na Apple (blocos), `Module.haylen.register` na web (handlers async em JavaScript) e handlers C++ no desktop. Android, web e desktop validados. Falta validar na Apple.
- [~] **Métodos embutidos**: `engine.info`, `app.version`, `device.info`, `system.open_url`, `system.locale` (tag BCP 47, como `pt-BR`) e `haptics.vibrate` em todas as plataformas. Validados no Android, na web e no desktop. Falta validar na Apple.
- [~] **Plugins de exemplo**: `auth.google.signIn` no Tiny Island, fora da engine. No Android, `GoogleSignInPlugin` usa o Credential Manager (`androidx.credentials` e `googleid`), registrado pelo `TinyIslandApplication` com o client id da propriedade Gradle `googleServerClientId`. Na web, o `platform/web/app.js` do sample carrega o Google Identity Services sob demanda com o client id da constante `googleClientId`. Validados no emulador e no Chrome até a chamada ao Google (erro sem client id e erro do Credential Manager sem conta). Falta validar um login real, que precisa de um client id de um projeto Google Cloud.
- [~] **Safe area**: UIKit, window insets do Android e `env(safe-area-inset-*)` na web, em coordenadas de design.
- [~] **Ciclo de vida**: suspender, retomar, foco, redimensionar, pouca memória (`onTrimMemory` no Android e aviso de memória do UIKit, entregues como evento `low_memory` das cenas, evento `app_low_memory` e sinal `lowMemory` de `core::Engine`) e pedido de saída. Android e desktop validados. Falta validar no iOS.
- [x] **Biblioteca Android** `haylen` (AAR, namespace `dev.haylen`) com `HaylenActivity` (splash, bridge, insets, teclado virtual, orientação, controles, tela cheia imersiva e Back), regras do R8 para as classes chamadas pelo C++, e o template Android sem C++.
- [x] **Projetos Apple**: template XcodeGen com alvos iOS e iPadOS com Mac Catalyst, tvOS e macOS, com Info.plist, launch screen e orientação vindos do `app.json`, rodados nos simuladores de iOS e tvOS, no Mac Catalyst e no macOS.
- [x] **Web**: template com canvas, builds WebGPU e WebGL2 (ambos validados no Chrome), script da bridge com os métodos embutidos, pacote entregue pela página e servidor local do `make.py`.

### 8.15 Utilitários de gameplay

- [x] **Spatial hash** para consultas em mundos grandes: retângulos, círculos, pontos e o mais próximo com filtro, em ordem determinística (`spatial2d::HashGrid` e `haylen.spatial2d`, que guarda qualquer valor Lua).
- [x] **A\*** em grade com custos, diagonais sem cortar cantos, linha de visão e suavização de caminho (`navigation2d::Grid` e `haylen.navigation2d`).
- [x] **Steering**: seek, flee, arrive, wander determinístico por semente e separação, com integração limitada por força e velocidade (`navigation2d::SteeringAgent`, `navigation2d::Wanderer` e `navigation2d.newAgent`).
- [x] **Máquina de estados** genérica com enter, update e exit, trocas pedidas durante uma transição enfileiradas em ordem, argumentos para o enter em Lua e `onChange` (`ai::StateMachine` e `haylen.ai`).
- [x] **Localização**: tabelas JSON por idioma (uma pasta de `content/` com um arquivo por idioma), chaves aninhadas, idioma de fallback, argumentos `{nome}`, plurais zero, one e other, direção do idioma, escolha do idioma mais próximo de uma tag BCP 47 e troca em tempo real (`localization::Catalog`, `plugins::LocalizationPlugin` e `haylen.localization`).
- [x] **Save slots**: slots JSON no armazenamento do usuário com escrita atômica, resumo para menus, data e listagem do mais novo para o mais antigo (`storage::SaveSlots` e as funções de slot de `haylen.storage`).
- [x] **Preferências persistentes**: chaves com caminho pontuado, gravação ao suspender e ao parar, e `capture` e `apply` explícitos para volumes e mudo dos barramentos, tela cheia e mapa de ações (`storage::Preferences`, `plugins::StoragePlugin` e `haylen.preferences`).

### 8.16 Debug

- [x] **Profiler** (`debug::Profiler`) com escopos de CPU aninhados e somados por frame, histórico de tempos de frame e fases da engine medidas (scripts, fixedUpdate, update, render e submit).
- [x] **Overlay** Dear ImGui (F3 por padrão) com FPS, gráfico de frame, escopos do profiler, estatísticas do renderer, assets pendentes, vozes de áudio e as últimas linhas de log da engine e dos scripts. O debug de física fica com `world:debugDraw` do app.

### 8.17 Testes

- [x] **GoogleTest** via CTest, num executável só (`haylen_tests`).
- [x] **Runtime headless** com backend dummy do Sokol, host headless, mixer sem dispositivo e runtime do Varn, e `test::EngineFixture` rodando uma engine real sobre um pacote em memória.
- [x] **Testes dos bindings Lua** executando Lua pela engine headless.
- [x] **Cobertura** da engine o mais perto de 100%, excluindo só os backends que exigem o SDK da plataforma, medida por `make.py coverage`.
- [x] **ThreadSanitizer**: a suíte inteira roda sem nenhum relato de corrida.

### 8.18 Bindings Lua por módulo

- [x] `haylen` (versão, plataforma, backend gráfico, `app.json` resolvido, relógio, escala de tempo, pausa, estados do app, opções de ciclo de vida, autoloads, classes e saída)
- [x] `haylen.math` (Vec2, Rect, Color, Transform, Random, ruído, easing, geometria, raycast contra formas, polígonos, marching squares, splines, molas, shuffle bag, escolha ponderada e Poisson disk)
- [x] `haylen.scene` (cenas Lua, pilha, carregamento, transições, tarefas e listeners da cena)
- [x] `haylen.events` e `haylen.signal`: barramento de eventos com canais, prioridades, filtros, donos e entrega na fila, e `signal.new()` com `connect` (devolve uma conexão com `disconnect`), `emit`, `clear` e `size`.
- [x] `haylen.timer` e `haylen.tween`
- [x] `haylen.jobs`: `jobs.spawn(fn, ...)` para trabalho longo em Lua, que roda como corrotina dentro de um orçamento de tempo por frame (`jobs.setBudget`), pausa em `jobs.checkpoint()` quando o orçamento acaba e devolve uma Promise do Varn com o resultado. O estado Lua é único, então CPU em Lua não vai para workers, e o async do Varn cobre I/O.
- [x] `haylen.collections` (pools de objetos, ring buffers e float buffers compartilhados com o C++)
- [x] `haylen.log`
- [x] `haylen.assets` (carga síncrona e assíncrona com Promise, grupos de preload, progresso e unload)
- [x] `haylen.storage` e `haylen.preferences`
- [x] `haylen.window` e `haylen.viewport` (janela, tela cheia, cursor, janelas de desktop, escala e safe area)
- [x] `haylen.graphics` (texturas, render targets, fontes TrueType e bitmap, famílias de fontes, shaders e backend)
- [x] `haylen.graphics2d` (canvases, capturas, câmeras, parallax, sprites, batches, primitivas, meshes, texto e rich text, nine-slice, blends de imagem, metaballs, materiais, luzes com `graphics2d.drawLight`, oclusores, pós-processamento e estatísticas)
- [x] `haylen.animation2d` (animações, animators e atlas)
- [x] `haylen.particles2d`
- [x] `haylen.lighting2d` (luzes, oclusores, sombras, consultas de luz e cintilação)
- [x] `haylen.physics2d`
- [x] `haylen.tiled`
- [x] `haylen.navigation2d` (grades, A\*, grafos, flow fields, navmesh, steering e multidões) e `haylen.spatial2d`
- [x] `haylen.procedural2d`
- [x] `haylen.ai` (máquinas de estados, behavior trees, utility AI e mapas de influência)
- [x] `haylen.input` (dispositivos, mapa de ações, controles virtuais e gestos)
- [x] `haylen.ui` (temas, fontes, componentes com `ui.button{...}`, documentos, eventos `onClick` e afins e captura de ponteiro) e `haylen.imgui` (janelas e widgets imediatos)
- [x] `haylen.audio`
- [x] `haylen.localization`
- [x] `haylen.platform` (bridge JSON, eventos nativos, erros tipados, timeouts e cancelamento) e `haylen.native` (bibliotecas nativas para o `ffi` do Varn)
- [x] `haylen.net` (WebSocket com eventos `open`, `message`, `close`, `error` e `pong`)
- [x] `haylen.debug` (estatísticas, contagem de objetos, monitores, profiler, escopos, overlay e log recente)

## 9. Jogo: Tiny Island

### 9.1 Conceito

O jogo é escrito em Lua e fica em `samples/games/tiny-island/`: `app.json`, `source/` (`main.lua`, `config.lua` com os valores de ajuste, e as pastas `data/`, `scenes/`, `systems/`, `entities/` e `ui/`) e `content/` com os assets. O app de cada plataforma é montado a partir dos templates por `python3 make.py run games/tiny-island [--platform ...]`, e `platform/` guarda só o login Google do Android e da web.

Sobreviver o maior número de noites numa ilha. Durante o dia o jogador corta árvores e alimenta a fogueira. À noite os inimigos aparecem e só a luz da fogueira mantém um círculo seguro. Quanto menos madeira, menor o círculo.

### 9.2 Fluxo de telas

1. Boot: escolhe o idioma do aparelho no primeiro uso (`system.locale` e `localization.bestMatch`) e pré-carrega a cena do menu (`scene.preload`), cujo `load` carrega os grupos `boot` e `menu`, com barra de progresso.
2. Menu principal: a ilha ao entardecer ao fundo, com os cinco sobreviventes em volta da fogueira e a câmera passeando devagar, título numa fita (ribbon) do Tiny Swords, botões Jogar, Configurações e Sair (só no desktop), recorde e música do menu.
3. Seleção de classe: a câmera enquadra o sobrevivente escolhido na fogueira, que comemora com a animação de ataque, com avatares clicáveis, folha de papel com a descrição do especial e barras de atributos.
4. Loading: o `load` da cena de gameplay carrega o grupo `gameplay` atrás da view de loading, com o sobrevivente esperando na fogueira e barra de progresso.
5. Gameplay com HUD.
6. Pausa, configurações e tela de fim de jogo com dias sobrevividos, inimigos derrotados e recorde salvo, como cenas transparentes sobre o jogo escurecido e dessaturado.

### 9.3 Classes

| Classe | Vida | Velocidade | Dano | Alcance | Recarga | Corte | Carga de madeira | Especial |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Warrior | 140 | 300 | 30 | 110 (corpo a corpo) | 0,6 s | 1,0 | 6 | Guarda que bloqueia quase todo o dano enquanto segurada |
| Archer | 90 | 330 | 22 | 700 (flechas físicas que atravessam um inimigo) | 0,7 s | 0,6 | 5 | Rajada de três flechas |
| Lancer | 120 | 310 | 34 | 150 (estocada que empurra) | 0,8 s | 0,8 | 6 | Investida que fere tudo no caminho |
| Monk | 100 | 300 | 14 (pulso em área) | 220 | 1,2 s | 0,5 | 4 | Cura um terço da vida |
| Pawn | 80 | 360 | 12 | 90 | 0,5 s | 2,0 | 10 | Corrida curta |

### 9.4 Mapa

- Gerado por `tools/generate_island_map.py` (`make.py map`) e salvo como `samples/games/tiny-island/content/maps/island.tmj`, editável no Tiled.
- Camadas: fundo de água, espuma animada na costa, grama com bordas por autotile (Wang set de bordas), platô elevado com penhascos e sombra, decorações (arbustos, pedras, pedras na água e nuvens em parallax) e objetos.
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

- Ciclo em Lua (`source/systems/day-night.lua`): amanhecer de 15 s, dia de 150 s, entardecer de 15 s e noite de 90 s, com contagem de dias e aviso de cada fase para a partida.
- Luz ambiente em gradiente, luz da fogueira com cintilação e luz pequena ao redor do jogador.
- Crossfade de música entre dia e noite e jingles nas trocas de fase.

### 9.8 Inimigos

- Só nascem à noite, nos pontos de spawn do Tiled. A quantidade cresce a cada noite.
- Tipos: guerreiro vermelho (corpo a corpo), arqueiro vermelho (à distância, mantém distância) e lanceiro vermelho (resistente). Unidades pretas aparecem a partir da quarta noite.
- Movimento com A* e steering (separação entre inimigos), sem entrar no círculo seguro.
- Ao amanhecer os que sobraram fogem e queimam com efeito de explosão.

### 9.9 Combate

- Ataques corpo a corpo e à distância, flechas como projéteis físicos, flash branco ao tomar dano, empurrão, números de dano e mortes com explosão.

### 9.10 HUD

- Vida, combustível da fogueira e recarga do especial em barras de madeira coloridas por tom, madeira carregada, dia, contagem regressiva até a próxima fase, número de invasores e avisos de entardecer, amanhecer e fogo apagado, tudo dentro da safe area e feito com a UI temática.

### 9.11 Controles

| Ação | Teclado e mouse | Gamepad | Toque |
| --- | --- | --- | --- |
| Mover | WASD ou setas | Stick esquerdo ou direcional | Joystick virtual flutuante |
| Atacar ou cortar | Espaço ou clique esquerdo | A (sul) | Botão virtual |
| Especial | Shift esquerdo ou clique direito | X (oeste) | Botão virtual |
| Interagir | E | B (leste) | Botão virtual |
| Pausa | Esc | Start | Botão de pausa |

Os controles de toque só aparecem depois que a tela é tocada (`touchOnly`) e podem ser desligados nas configurações. O jogo pausa sozinho quando perde o foco ou vai para o segundo plano.

### 9.12 Áudio

Todos os sons são CC0. Os créditos ficam em `samples/games/tiny-island/content/audio/CREDITS.md`, e os das fontes Kenney Future em `content/fonts/CREDITS.md`.

| Uso | Origem |
| --- | --- |
| Clique, confirmar, voltar e erro | Kenney Interface Sounds |
| Machado | Kenney RPG Audio |
| Madeira atingida, árvore caindo, golpes, guarda e passos | Kenney Impact Sounds |
| Jingles de amanhecer e entardecer | Kenney Music Jingles |
| Pegar madeira, alimentar a fogueira, explosão e dano e morte de inimigo | OpenGameArt "80 CC0 RPG SFX" |
| Dano no jogador | OpenGameArt "80 CC0 creature SFX" |
| Espada e flecha | OpenGameArt "Swishes Sound Pack" |
| Cura | OpenGameArt "Cure Magic" |
| Fogo crepitando (loop) | OpenGameArt "Fire Crackling" |
| Ambiente da noite | JaggedStone "Loopable Dungeon Ambience" |
| Música do menu, do dia e do fim de jogo | RandomMind "The Old Tower Inn", "Market Day" e "Defeat Theme" |

### 9.13 Recursos da engine exercitados pelo jogo

- [x] Tudo pela API Lua: cenas com `load` e transições, preload com progresso e `:await()`, UI temática com nine-slice, mapa Tiled com colisão e spawn, Poisson disk, animação de sprites, tweens, partículas, luz 2D, física com filtros e sensores, A\* e steering, áudio com barramentos e crossfade, mapa de ações com teclado, mouse, gamepad e toque, recorde e configurações em `haylen.preferences`, bridge de plataforma (`device.info` e login Google), localização (inglês e português) e overlay de debug.

### 9.14 Itens do jogo

- [x] **Importação do Tiny Swords**: `make.py assets <zip>` copia o pacote para `samples/games/tiny-island/content/tiny_swords/` com nomes em `snake_case`, gera as peças de nine-slice da UI e versões claras das barras para o tema colorir.
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
- [x] **Controles** em todos os dispositivos. Teclado, mouse e toque validados no navegador. O gamepad usa o mesmo mapa de ações e falta testar com um controle físico.
- [x] **Áudio** completo com créditos.
- [x] **Demo da bridge** nas configurações. O login Google real depende do client id do projeto no Google Cloud.

## 10. Editor web (projeto futuro, em outro repositório)

O editor web não faz parte deste repositório, mas a engine é construída para funcionar dentro dele. Tudo o que o editor precisa da engine está listado aqui.

### 10.1 Visão

- Um site onde o usuário edita o código Lua e os assets do app no navegador e roda o app na hora, como aplicação WebAssembly com WebGPU ou WebGL2.
- O runtime web da engine é o mesmo usado para publicar o app na web, controlado pela página.

### 10.2 O que a engine oferece ao editor

- [x] **Carregar pacote em memória**: montar o app a partir de um `.zip` recebido pela página (`packageData` antes do início ou `loadZip` depois) ou de um conjunto de arquivos (`io::MemoryPackage` com `setFile` e `run`), sem `--preload-file`. A página também pode indicar um zip por URL com `packageUrl`, e o shell dos apps C++ lê o parâmetro `?package=`.
- [x] **API JavaScript** em `Module.haylen`: `loadZip(bytes)`, `clearFiles()`, `setFile(caminho, bytes | texto)`, `removeFile(caminho)`, `run()`, `restart()` (recarrega os scripts), `stop()`, `reloadAsset(caminho)`, `pause()`, `resume()`, `setPaused(bool)`, `paused()`, além de `register`, `unregister` e `emit` da bridge. O canvas é o `Module.canvas` que a página entrega. Validado no Chrome com WebGPU e WebGL2.
- [x] **Eventos para a página**: `onLog(nível, linha)`, `onError({message, file, line, traceback, frames})` (via `lua::Error`), `onStarted({name, identifier, version})`, `onStopped()` e `onStats` uma vez por segundo com FPS, tempos de frame, contadores do renderer, assets, vozes, escopos do profiler e o estado da saída de áudio. Os callbacks rodam logo depois do frame, então podem chamar o runtime (até reiniciar o app).
- [x] **Reinício limpo** do app sem recarregar a página: cada início cria um `core::Engine` novo (cenas, estado Lua com um `Runtime` do Varn próprio, assets, áudio, física e recursos de GPU), e o anterior é destruído entre frames.
- [x] **Hot reload**: no desktop, `plugins::HotReloadPlugin` observa a pasta do pacote no modo de desenvolvimento (`--dev`), recarrega os assets no lugar e reinicia o app quando um script muda. Na web, o editor usa `setFile` com `reloadAsset` para assets e `restart` para scripts.
- [x] **Tela de erro**: um erro em script não derruba o runtime. A engine mostra o erro na tela, avisa a página e continua pronta para receber a correção.
- [x] **Backend escolhido pela página**: `make.py engine --platform web` gera o player para WebGPU e para WebGL2, e a página do template web usa WebGPU quando o navegador oferece um adaptador e WebGL2 no resto, com `?backend=webgpu` ou `?backend=webgl2` para forçar um deles. Os apps C++ (`make.py run-cpp --platform web`) usam o shell do próprio alvo com o seletor de backend (`engine/platform/web/backend-picker.html`) no lugar do `{{{ SCRIPT }}}`, então plugins de página também funcionam no pacote duplo (validado no Chrome).
- [x] **Canvas controlado pela página**: o runtime usa o `Module.canvas` que a página fornecer (com `id`) e acompanha o tamanho do elemento com um `ResizeObserver`, sem assumir a página inteira.
- [x] **Sem requisitos especiais de hospedagem**: o runtime web é single-thread (como o Varn, e o `JobSystem` roda os trabalhos no próprio frame), então não depende de COOP e COEP nem de `SharedArrayBuffer`. A página só precisa ser servida por https ou por localhost, onde o navegador oferece o `AudioWorklet` da saída de áudio.
- [x] **Bridge de plataforma na web**: plugins em JavaScript registrados pela página (`Module.haylen.register`, inclusive em `Module.preRun`), com handlers assíncronos. O exemplo de login com Google está no `platform/web/app.js` do Tiny Island.
- [x] **Sockets e HTTP** na web através de JavaScript: o cliente HTTP do Varn usa o fetch do navegador, e `haylen.net` usa o `WebSocket` do navegador (validados no Chrome). Navegadores não têm TCP bruto, então o módulo `socket` do Varn só existe no nativo.
- [x] **Documentação da API Lua** completa e navegável (`docs/lua-api.md` e `docs/lua-api/`), que o editor pode usar para autocomplete e ajuda.

## 11. Documentação

- [x] **README.md**: apresentação e início rápido. Cada sample tem o próprio `README.md`, como `samples/games/tiny-island/README.md`.
- [x] **docs/architecture.md**: bibliotecas e produtos, organização das fontes, plugins, fronteira com o host, frame, tela de erro, threads, posse de recursos, pacotes e runtime web.
- [x] **docs/lua.md**: pacote do app, `app.json`, módulos e `require`, caminhos de assets, cenas, autoloads, classes, acesso ao disco, async com o Varn, erros, hot reload, desempenho e como estender a engine com bindings próprios.
- [x] **docs/lifecycle.md**: início, frames e parada, estados do app, pausa, modos de processamento, autoloads, ciclo de vida das cenas e donos.
- [x] **docs/build.md**: `make.py`, requisitos por plataforma, árvores de build, opções CMake, dependências, apps C++ com `haylen_add_app`, player de desktop, builds web, Android, Apple, assets do Tiny Island e CI.
- [x] **docs/distribution.md**: comandos de apps, artefatos da engine, templates, montagem, personalizações por plataforma, bibliotecas nativas, splash, modo de desenvolvimento, carregador web e suporte a plataformas.
- [x] **docs/embedding.md**: a engine como biblioteca em outro projeto CMake (`add_subdirectory`, CPM e SDK).
- [x] **docs/testing.md**: organização da suíte, host headless, `test::EngineFixture`, testes de bindings, sanitizers, cobertura, formatação e CI.
- [x] **docs/rendering.md** e **docs/shaders.md**: renderer, performance, milhões de sprites e shaders próprios.
- [x] **docs/text.md** e **docs/text-input.md**: fontes, shaping, rich text e entrada de texto nativa.
- [x] **docs/ui.md** e **docs/input.md**: temas, componentes, foco, JSON de telas, dispositivos, mapa de ações e gestos.
- [x] **docs/audio.md**: mixer, barramentos, efeitos, sessões e interrupções.
- [x] **docs/tiled.md**: recursos suportados e convenções do Tiny Island.
- [x] **docs/desktop.md**: janelas sem moldura, transparentes, sempre no topo e com cliques atravessando, e monitores.
- [x] **docs/platform_bridge.md** e **docs/native.md**: protocolo JSON, handlers nativos, bibliotecas nativas pelo FFI e integração com SDKs.
- [x] **docs/lua-api.md** e **docs/lua-api/<modulo>.md**: referência completa de cada módulo Lua com exemplos executáveis, com uma página para `haylen`, `scene`, `events`, `signal`, `timer`, `tween`, `jobs`, `collections`, `log`, `graphics`, `graphics2d`, `animation2d`, `particles2d`, `lighting2d`, `viewport`, `window`, `tiled`, `physics2d`, `navigation2d`, `spatial2d`, `procedural2d`, `ai`, `math`, `input`, `ui`, `imgui`, `audio`, `localization`, `assets`, `storage`, `preferences`, `platform`, `native`, `net` e `debug`.

## 12. Fases de trabalho

| Fase | Conteúdo | Status |
| --- | --- | --- |
| 1 | Estrutura, CMake com CPM, dependências (incluindo Varn), CLAUDE.md e este plano | Concluída |
| 2 | Core e matemática com testes | Concluída |
| 3 | Integração com o Varn: runtime, jobs, log, pacote do app (pasta e zip), loader Lua e toolkit de bindings | Concluída |
| 4 | Host (real e headless), janela, eventos e input | Concluída |
| 5 | Gráficos base e 2D (renderer, texturas, câmera, texto, primitivas e luz) | Concluída |
| 6 | Assets, preload e áudio | Concluída |
| 7 | Animação, tweens, partículas, física, Tiled e navegação | Concluída |
| 8 | UI: backend Dear ImGui, temas e componentes | Concluída |
| 9 | Bindings Lua de todos os módulos com testes | Concluída. Todas as capacidades públicas em C++ têm binding, teste e documentação. Registrar tipos novos de componente de UI só é possível em C++, porque o registro recebe fábricas C++ |
| 10 | Plataformas: bridge, safe area, gamepads, Android, Apple, web e player | Concluída, com os itens `[~]` que dependem de hardware ou contas |
| 11 | Ferramentas: importador do Tiny Swords, gerador de mapa, empacotador zip e áudio | Concluída |
| 12 | Jogo Tiny Island em Lua | Concluída |
| 13 | Testes e cobertura até o máximo possível | Concluída |
| 14 | Documentação e revisão final (bugs, legado, não utilizado, race conditions e crashes) | Concluída, com testes de regressão para as correções |
| 15 | Pedidos 51 a 127 (seção 14) | Em andamento, com os itens abertos da seção 14.2 |

## 13. Limitações conhecidas

- A máquina de desenvolvimento é um Mac. Windows e Linux têm os jobs de teste do CI, e os recursos de janela, texto e plataforma desses sistemas faltam rodar numa máquina real. Aparelhos Apple físicos precisam do time de desenvolvimento da Apple (`HAYLEN_APPLE_TEAM`).
- O Varn compila o OpenSSL e o Poco a partir do código-fonte, então o primeiro build de cada plataforma é demorado.
- Correções para o NetSSL_Win do Poco: `SecureStreamSocket::attach` faz o handshake inteiro num laço ocupado mesmo com o socket não bloqueante, e `completeHandshake` devolve 0 quando o handshake termina, e não o 1 que a documentação e o NetSSL do OpenSSL devolvem. A engine conecta o socket seguro antes do handshake e reconhece o fim dele pelo certificado do servidor, o que funciona com as duas bibliotecas.
- A engine aplica patches ao Sokol pelo CPM (`engine/cmake/patches/`), que devem ser levados ao Sokol: `sokol-ios-view-size.patch` (o `sokol_app` usa o tamanho da tela e não o da view para o framebuffer no iOS, o que corta a imagem no Mac Catalyst e nas janelas divididas do iPad), `sokol-android-quit.patch` (o `sokol_app` encerra a activity destruída com `exit()`, o que aborta o processo, e ignora `sapp_quit()` no Android), `sokol-dummy-limits.patch` (o backend dummy limita texturas a 1024 pixels, o que impediria os testes sem janela de carregar mapas reais) e `sokol-desktop-window.patch` (janela transparente por DirectComposition no Windows, as opções `desktop` de criação, `sapp_set_window_focusable`, o fechamento de janelas sem borda no macOS e os visuais ARGB no X11).
- visionOS nativo depende do Sokol, que usa o `UIScreen`, indisponível no SDK do visionOS. O app iOS roda no Apple Vision Pro como app de iPad compatível. watchOS é impossível, porque o SDK do watchOS não tem Metal, MetalKit, GameController nem AudioToolbox.
- O Sokol só aceita imagens inteiras em texturas que vivem entre frames, então uma textura dinâmica alterada (os atlas das fontes e da UI) sobe todos os pixels, uma vez por frame.

## 14. Pedidos 51 a 127: decisões e checklist

Esta seção cobre os pedidos 51 a 127 das seções 2.1, 2.2, 2.3, 2.4 e 2.5. As decisões vêm da pesquisa em código-fonte de referência, no código do Sokol e do miniaudio e no código da engine.

### 14.1 Decisões de organização

- **Nome do produto**: Haylen, sem "2D" no nome, porque o 3D cresce ao lado. O pacote Java é `dev.haylen`.
- **Nomes genéricos**: o que o desenvolvedor cria é um app (jogo, aplicação multimídia ou app): `app.json`, `haylen_add_app`, `core::Application`, o pacote embarcado em `app/` e o zip `app.zip`. A palavra "game" só aparece onde é sobre jogos de verdade (gamepad e os samples de jogos).
- **Regra 2D e 3D**: tudo o que é específico de 2D fica em pastas `2d/` (`engine/include/haylen/2d/`, `engine/src/2d/` e `engine/tests/2d/`), e cada pasta dentro de `2d/` tem um namespace com o sufixo `2d` igual ao nome do módulo Lua (`haylen::physics2d` e `haylen.physics2d`). Os tipos não repetem o namespace (`physics2d::World`, `graphics2d::Camera`), e os tipos do namespace compartilhado `haylen::plugins` dizem a dimensão (`Physics2DPlugin`). Assim os módulos 3D do futuro não conflitam com nada. `haylen.graphics` fica com o que não tem dimensão (texturas, render targets, fontes, shaders e o backend), e `Font` fica em `text/`, porque uma fonte serve às duas dimensões. O Tiled mantém `haylen::tiled` e `haylen.tiled`, porque mapas do Tiled são 2D por natureza.
- **Formato do pacote**: `app.json` (identidade, orientação, janela, resolução de design, splash, ciclo de vida, sessão de áudio, debug, autoloads e bibliotecas nativas), `source/` com `main.lua` como ponto de entrada e os outros módulos Lua, e `content/` com os recursos. `require("scenes.menu")` procura `source/scenes/menu.lua`. Todo caminho de recurso é relativo a `content/`. Uma pasta opcional `platform/<template>/` no app guarda personalizações por plataforma e não faz parte do pacote.
- **Templates**: a pasta `templates/` na raiz guarda o app inicial em `templates/app` (usado pelo `make.py new`) e os projetos prontos por plataforma em `templates/platform/<plataforma>/`, que só esperam o pacote: `templates/platform/apple` (projeto XcodeGen com o `App.xcodeproj` gerado ao lado do `project.yml`, com alvos iOS, iPadOS, Mac Catalyst, tvOS e macOS), `templates/platform/android` (projeto Gradle que usa o AAR da engine) e `templates/platform/web` (página com a logo, a barra de progresso, a escolha entre WebGPU e WebGL2 e o carregador). É modular e extensível: uma plataforma nova é uma pasta nova em `templates/platform/` e o seu alvo de execução no `make.py` (a tabela `RUN_TARGETS`), que descobre os templates pelas pastas. O `.xcodeproj` e o `build.gradle.kts` nunca mudam por app: nome, identificador, versão, orientação e splash vêm de arquivos gerados pelo `make.py` (`App.xcconfig` e `Info.plist` na Apple, `gradle.properties` no Android e `config.json` na web).
- **Montagem**: rodar um app apaga e recria `build/apps/<app>-<hash>/<plataforma>/`, copia o template da plataforma, aplica por cima a pasta `platform/<template>/` do app (mesmo caminho substitui, arquivo novo é adicionado), injeta o pacote, coloca as bibliotecas nativas, grava as configurações geradas e roda. O Tiny Island leva para `platform/android` e `platform/web` só o login Google.
- **Artefatos prontos da engine**: `build/artifacts/` com um `manifest.json` (versão da engine e, por plataforma, a configuração e o hash das fontes do último build) e reconstrução automática quando a engine muda.
  - `apple/Haylen.xcframework`: biblioteca estática com engine, dependências e player Lua, com os slices macOS (arm64 e x86_64), iOS, simulador iOS, Mac Catalyst, tvOS e simulador tvOS, e os headers públicos (`haylen_main` e `HaylenBridge`). O template leva um `main.mm` mínimo que chama `haylen_main`, que também é o lugar para registrar handlers nativos da bridge.
  - `android/maven/`: o AAR `haylen` (arm64-v8a, armeabi-v7a para TVs Android de 32 bits e x86_64), publicado num repositório Maven local para levar as dependências transitivas (Kotlin, AndroidX e kotlinx-coroutines).
  - `web/`: `haylen.js`, `haylen.wasm` e `haylen-audio-worklet.js` para WebGPU e para WebGL2. O wasm é pré-compilado, porque o player web carrega o pacote em tempo de execução: só o `app.zip` muda de um app para outro.
  - `desktop/`: o player `haylen` para rodar apps no macOS, Windows e Linux com hot reload.
- **Comandos do make.py**:
  - `engine [--platform apple|android|web|desktop|all]` gera os artefatos.
  - `new <pasta> [--name --identifier --orientation]` cria um projeto com `app.json`, `source/main.lua`, `content/` de exemplo e a cópia de todos os templates em `platform/`, para o desenvolvedor personalizar.
  - `run [app|sample] [--platform macos|windows|linux|ios|ios-simulator|tvos|tvos-simulator|catalyst|android|web] [--device]` roda um app Lua (no player de desktop com hot reload quando nenhuma plataforma é passada).
  - `run-cpp <projeto|sample> [--platform ...]` roda um projeto C++, que compila a engine pelo CMake (`add_subdirectory`, CPM ou `find_package`).
  - `package <app>` gera o `app.zip`.
  - `shaders <app>` compila os shaders do app.
  - `samples` lista os samples.
  - `serve <pasta> [--port]` sobe um servidor Python com MIME do wasm, COOP, COEP (com opção `credentialless` ou desligado para páginas que carregam scripts de terceiros, como o login Google), CORP, CORS, sem cache e com arquivos pré-comprimidos.
- **Plataformas Apple além de iOS, tvOS e macOS**:
  - Mac Catalyst compila como iOS (UIKit). Teclado e mouse entram pelo `GCKeyboard` e `GCMouse`, porque o Sokol só entrega toque no Catalyst.
  - visionOS: o Sokol usa o `UIScreen`, que o SDK do visionOS marca como indisponível, então um app nativo de visionOS depende de suporte do Sokol. O app iOS roda no Apple Vision Pro como app de iPad compatível.
  - watchOS é impossível: o SDK do watchOS não tem Metal, MetalKit, GameController nem AudioToolbox.

### 14.2 Checklist

#### A. Nomes, pacote e estrutura

- [x] Nome Haylen em tudo (namespace `haylen` com os sub-namespaces por contexto, headers em `haylen/`, alvos `haylen::engine` e `haylen::runtime`, `haylen_add_app`, módulos Lua `haylen.*`, pacote Java `dev.haylen`, `Module.haylen` na web, `Haylen.xcframework`, `haylen_main`, player `haylen`, arquivos `haylen-*.cmake`, docs, README e CLAUDE.md).
- [x] `app` em tudo o que o desenvolvedor cria (arquivos, CMake, Gradle, JavaScript, `make.py`, docs e mensagens).
- [x] Pacote com `app.json`, `source/` e `content/`, com hot reload separando scripts (`source/` e `app.json`) de recursos (`content/`). Só essas três entradas são o pacote em todas as plataformas.
- [x] Módulos Lua 2D com sufixo `2d`, `haylen.graphics` separado de `haylen.graphics2d`, `Font` em `text/` e `FloatRange` em `math/`. Os tipos 2D levam o contexto no namespace (`physics2d::World`), como o grupo P descreve.
- [x] Ciclo de dia e noite fora da engine, em Lua no Tiny Island (`source/systems/day-night.lua`), e `lighting2d.flicker` (`lighting2d::LightFlicker`) como utilitário genérico.
- [x] CLAUDE.md, README e guias com a estrutura atual e a regra 2D e 3D.

#### B. Distribuição, templates e comandos

- [x] AAR da engine com as três ABIs (arm64-v8a e x86_64 alinhadas em 16 KB, armeabi-v7a), repositório Maven local em `build/artifacts/android/maven` e template Android sem C++, com LEANBACK, banner de TV, toque opcional e controle declarado.
- [x] `Haylen.xcframework` com seis slices (macOS, iOS, simulador iOS, Mac Catalyst, tvOS e simulador tvOS), `haylen_main` em `haylen/platform/apple/HaylenMain.h`, template XcodeGen com o `App.xcodeproj` gerado junto e alvos iOS e iPadOS com Mac Catalyst, tvOS e macOS. Rodado no simulador iOS, no simulador tvOS, no Mac Catalyst e no alvo macOS.
- [~] Teclado e mouse no Mac Catalyst (`GCKeyboard` e `GCMouse`). Teclas e posição do ponteiro validadas. Botões direito e do meio e a roda estão implementados, mas eventos sintéticos não chegam ao GameController, então falta validar com um mouse de verdade.
- [x] Versões mínimas o mais baixas que a toolchain permite: a leitura de números decimais usa o fast_float, e o limite é o `std::format` com ponto flutuante. iOS e tvOS 16.3, macOS 13.3, Mac Catalyst 16.4 (o 16.3 do Catalyst equivale ao macOS 13.2) e Android API 27 (o miniaudio só usa AAudio a partir da 27, e o OpenSL ES não é compilado). Rodado num emulador Android 8.1.
- [x] Wasm pré-compilado (WebGPU e WebGL2) e template web com logo do app ou da engine, barra de progresso real (wasm e `app.zip`), checagem de recursos do navegador e tela de erro. O pacote chega ao runtime por `Module.haylen.packageData`.
- [x] Logo da engine usada quando o app não tem logo, com os derivados para o splash e os ícones do Android, da Apple e da web, o banner de TV e o `logo.png` do app inicial, com a marca do grupo AD.
- [x] Splash nos templates Apple (LaunchScreen com Auto Layout em iOS, iPadOS, Mac Catalyst e a imagem de abertura do tvOS) e Android (SplashScreen API mantida na tela até o primeiro frame da engine, sem tela preta no meio), funcionando em retrato e paisagem em celulares, tablets e TVs, com logo e cor de fundo do `app.json` (`splash`) e a logo da engine quando o app não tem uma.
- [x] Player desktop como artefato (`make.py engine --platform desktop`).
- [x] `make.py engine`, `new`, `samples`, `run`, `run-cpp`, `package`, `shaders` e `serve` como descritos em 14.1, com montagem em `build/apps/<app>-<hash>/<plataforma>/`, personalização por `platform/<template>/` e plataformas descobertas pelas pastas de `templates/platform/` (a tabela `RUN_TARGETS` no `make.py`). O `serve` entrega isolamento de origem (`crossOriginIsolated` verdadeiro no Chrome) e aceita `--coep off`.
- [x] Modo de desenvolvimento explícito (`--dev`) no player, sem hot reload em apps publicados. A web nunca passa `--dev`.
- [x] Samples Lua rodando em macOS, simulador iOS, simulador tvOS, Mac Catalyst, emulador Android e web, e o sample C++ em macOS e web. Os 26 samples Lua rodam sem crash na web (WebGPU e WebGL2, 60 fps), no iPhone, no iPad e no Android, com o menu, testes abertos por toque e o voltar. No tvOS foram validados o Tiny Island, a UI com o controle remoto e um sample por categoria, e no Catalyst o Tiny Island e a UI. Faltam Windows e Linux (fora da máquina de desenvolvimento) e aparelhos físicos.

#### C. Cenas e transições

- [x] Pilha completa: `push`, `pop`, `replace`, `popToRoot`, `popTo(nível)`, acesso por índice (`scene.at`) e listagem (`scene.list`), cada troca devolvendo uma Promise.
- [x] Captura da cena inteira num render target (`Renderer::beginCapture` e `endCapture`, inclusive canvases com luz, pós-processamento e UI), com as duas cenas vivas durante a transição, cada uma na sua imagem. A cena que sai só sai no update que chega ao ponto de saída do efeito, antes do frame ser desenhado.
- [x] Transições: 24 efeitos em `graphics2d::SceneTransition` (fade por cor, crossfade, move in, slide in, push, shrink grow, flip X, flip Y, zoom flip, rotozoom, jump zoom, split de colunas e linhas, tiles, fades direcionais, page turn, wipes radiais, horizontais e verticais, íris, dissolve e pixelate), com 8 direções e easing, e o shader `blend.glsl` para os efeitos por máscara.
- [x] Transições próprias em Lua e C++: a interface `core::TransitionEffect` e a tabela `effect = {switchProgress, exitProgress, render(self, progress, outgoing, incoming)}` em Lua, com as texturas das duas cenas.
- [x] Input bloqueado durante a transição (opcional), tempo sem escala (a transição anda mesmo com o jogo pausado), easing e aviso de fim (callback, Promise e os ganchos `exitTransitionStarted` e `enterTransitionFinished` da cena).

#### D. Ciclo de vida, áudio e segundo plano

- [x] Estados do app `active`, `inactive` e `background` com sinal e evento (`haylen.appState`), vindos de suspender, retomar, foco, aba escondida na web (`visibilitychange`), `pagehide` e interrupções (uma ligação no Android deixa o app inativo).
- [x] Em segundo plano: nenhum frame e nenhum trabalho de GPU, áudio suspenso, input solto (teclas, toques e controles virtuais), preferências salvas, `storage::UserStorage` gravado (na web também no `pagehide`) e o evento `app_background` para o app salvar o que quiser.
- [x] Na volta: o primeiro delta é zero (sem salto de tempo em timers, tweens e física) e o áudio volta.
- [x] Contexto de áudio da engine (`audio::Device`, dono do contexto e do dispositivo do miniaudio): categoria da sessão no iOS e no tvOS configurável no `app.json` (`audio.iosSession`, com `ambient` por padrão, que respeita a chave de silêncio e mistura com outros apps, `soloAmbient` ou `playback`, e `audio.mixWithOthers`), uso `game` no AAudio do Android e foco de áudio no Android.
- [x] Interrupções de áudio (ligação, alarme, Siri e outro app Android tomando o foco de áudio) num caminho só para as notificações do dispositivo e os eventos da plataforma: pausa no início e retomada no fim só com o app ativo, com os eventos `audio_interrupted` e `audio_resumed`, e o áudio voltando sempre que o app fica ativo de novo, porque o iOS nem sempre avisa o fim da interrupção (falta validar num aparelho real). A troca de rota (fone desconectado) publica `audio_route_changed`.
- [x] Web: desbloqueio do `AudioContext` em qualquer toque, clique ou tecla, e de novo quando o navegador suspender o áudio.
- [x] Opções no `app.json` (bloco `lifecycle`): congelar no segundo plano (ligado por padrão), congelar na perda de foco e silenciar na perda de foco, também por `haylen.setLifecycle`. O congelamento do ciclo de vida (`haylen.halted`) é separado da pausa do jogo.

#### E. Pausa da partida

- [x] `Engine::setPaused`, `isPaused` e o sinal `pausedChanged`, com `haylen.setPaused`, `haylen.paused`, os eventos `paused` e `unpaused` e os ganchos de cena `paused` e `unpaused`.
- [x] Modos de processamento (`inherit`, `pausable`, `whenPaused`, `always` e `disabled`) para cenas, autoloads, timers, tweens, sons e barramentos de áudio (efeitos e ambiente param, música e UI continuam, configurável), com motivos de pausa separados por voz.
- [x] Timers e tweens com escolha entre tempo com escala e sem escala.
- [x] O Tiny Island usa a pausa da engine, com a cena de pausa em `whenPaused`, e abre o menu de pausa em `app_background` e em `app_inactive` com `pauseOnFocusLoss`, para a partida continuar pausada na volta.

#### F. Entrada de texto e rich text

- [x] Protocolo de edição de texto (`platform::TextInput` e `ui::TextSession`): a engine publica o campo ativo (texto, cursor, seleção, retângulo do campo e do cursor, tipo de teclado, tecla de retorno, autocorreção, capitalização e tamanho máximo), e um campo nativo escondido em cima do campo edita o texto e devolve texto, cursor, seleção e composição do IME, aplicados por callback do `InputText` do Dear ImGui.
- [x] Web (validado no Chrome com WebGL2 e WebGPU, inclusive a composição do IME): `textarea` e `input` escondidos posicionados sobre o campo, com `inputmode`, `enterkeyhint`, composição do IME, colar, copiar e desfazer nativos, abertura do teclado dentro do gesto no Safari do iOS e o teclado virtual ocupando a área visível (`visualViewport`).
- [x] Android: `EditText` escondido (validado no emulador) na activity com o `InputConnection`, composição, sugestões e ações do teclado, e os insets do teclado.
- [~] iOS e tvOS: `UITextField` e `UITextView` escondidos com texto marcado (CJK), autocorreção, ditado, emoji e colar, e o teclado de tela cheia na Apple TV. Validado no simulador iOS por XCUITest. Faltam a composição CJK no iOS e o teclado da Apple TV rodando.
- [~] macOS: `NSTextView` escondido (`HaylenFieldEditor`) com texto marcado, teclas mortas, emoji e ditado, e Windows com a janela de composição do IME posicionada no cursor (`WindowsTextInput`). Implementados, e faltam exercitar no macOS e numa máquina Windows.
- [x] Tipos de teclado (texto, várias linhas, número, decimal, telefone, email, URL, busca e senha) e rótulo da tecla de retorno nos campos de UI e no Lua.
- [x] A UI sobe o campo com foco para cima do teclado virtual (validado no Android e no iOS).
- [x] Rich text com BBCode (erros de marcação com linha e coluna, layout em cache, efeitos determinísticos): `b`, `i`, `u`, `s`, `code`, `color`, `bgcolor`, `font`, `size`, `outline`, `shadow`, `glow`, `alpha`, `p`, alinhamentos, recuo, listas, `br`, `hr`, `img`, `icon`, `url`, `hint`, `table` e `dropcap`, efeitos `wave`, `shake`, `tornado`, `fade`, `rainbow` e `pulse`, efeitos próprios em Lua e C++ e revelação de texto (máquina de escrever).
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
- [x] Camadas de parallax genéricas (`graphics2d::Parallax`: fator de rolagem, repetição, rolagem automática e limites), fora do Tiled também.
- [x] Desenho de debug da câmera (tela, limites e margens).

#### I. Camadas, Y-sort e luz

- [x] Modo de ordenação `y` automático com deslocamento da origem de ordenação, ordem numérica entre canvases, deslocamento de camada em escopo e máscaras de visibilidade.
- [x] Camadas de tiles com Y-sort por linha, para entidades passarem por trás e pela frente de objetos do mapa.
- [x] Luz: tipos pontual, spot (cone) e direcional, `enabled`, altura, modos de mistura (somar, subtrair e misturar), intensidade acima de 1 (mapa de luz em ponto flutuante), máscara de itens e faixa de camadas.
- [x] Sombras: oclusores (`lighting2d::Occluder`, polígonos abertos e fechados, culling e máscara), sombra por luz com filtros (nenhum, PCF5 e PCF13), cor e suavidade da sombra.
- [x] Normal maps e especular em sprites, sprites sem sombreamento (`unshaded`) e emissivos, e máscara de luz por sprite.
- [x] Canvases com luz desenhando em render targets.

#### J. UI completa, foco e TV

- [~] Revisão de todos os componentes com mouse, toque, teclado e controle (feita e testada no desktop e no simulador da Apple TV). Falta rodar num aparelho Android ou Android TV.
- [x] Enter, Espaço ou o botão sul ativam o controle focado por código já na primeira vez, mesmo com o anel escondido (a ativação também conta como navegação e acende o anel).
- [x] Anel de foco do tema, concêntrico com o controle (raio do controle mais a folga, largura `FocusWidth`, cor `Focus`), visível só quando o usuário navega com teclado, controle ou controle remoto. Foco dado por código não acende o anel quando o último dispositivo foi mouse ou toque.
- [x] Navegação por foco em documentos retidos: vizinhos automáticos por direção com as regras do FocusFinder do Android, vizinhos explícitos (`focusLeft`, `focusRight`, `focusUp` e `focusDown`), foco inicial (`autofocus`), escopos de foco (`focusScope`, com diálogos prendendo o foco), `focusWrap`, foco devolvido ao fechar diálogos, rolagem até o controle focado, anel de foco pelo tema, e confirmar e voltar pelas ações `ui_accept`, `ui_cancel`, `ui_menu` e as quatro direções no mapa de ações.
- [~] Apple TV: Siri Remote (toque direcional, clique, menu como voltar e play/pause) e controles, validados no simulador por XCUITest com os botões do controle remoto. Falta o gesto de deslizar num aparelho real. Android TV: D-pad e controle, com o manifesto `LEANBACK_LAUNCHER`, banner e `touchscreen` opcional.
- [x] Componentes para jogos e apps: `richText` (grupo F), `circularProgress` (recarga de habilidades), `stepper`, `segmentedControl`, `rangeSlider`, `window` (janela arrastável), `contextMenu`, `accordion`, `carousel` (carrossel de páginas), `slotGrid` (grade de slots com arrastar e soltar, também por teclado e controle), `keyCapture` (captura de tecla para remapear controles) e `scroll` com encaixe.

#### K. Lua: singletons, classes, disco e desempenho

- [x] Autoloads: módulos listados no `app.json` (ou registrados por `haylen.autoload`) que carregam antes da primeira cena, vivem durante o app todo, ficam acessíveis de qualquer cena e recebem os callbacks do ciclo (update, fixedUpdate, render, renderUi, eventos e stop).
- [x] Helper de classes (`haylen.class`) com herança, construtor, `super`, `is` e mixins, para classes globais do app.
- [x] Acesso ao disco documentado e testado: `fs` do Varn (com `storage.root()` para montar os caminhos) e `haylen.storage` (pasta do usuário, leitura e escrita síncronas e assíncronas, listagem, criação e remoção).
- [x] Desempenho: APIs em lote sem alocação por item (`collections.newFloatBuffer`, `graphics2d.drawBatch` com buffer e campos, `world:readTransforms` e `writeTransforms` e `emitter:readPositions`), buffers de números compartilhados com o C++, métodos rápidos nos userdata dos caminhos quentes e o bunnymark em Lua no `make.py bench --suite lua`: 1 milhão de sprites em 72 ms de CPU por frame pelo caminho em lote contra 347 ms com uma tabela por sprite.

#### L. Modo debug com estatísticas

- [x] Estatísticas compactas sempre visíveis (FPS, tempo de frame, draw calls e vértices) e o modo completo no overlay.
- [x] Contagem de objetos por tipo (criados, vivos e destruídos) em todo userdata exportado para Lua e nos recursos C++ (texturas, render targets, buffers, fontes, sons, corpos, emissores e documentos de UI), memória do Lua, memória de GPU estimada e uso dos pools do Sokol.
- [x] Contadores de tweens, timers, cenas, vozes, assets, passos fixos, contatos de física e partículas vivas.
- [x] Monitores próprios (`debug.addMonitor`) e ligação pelo `app.json` ou por Lua.

#### M. Recursos exigidos pelos samples

- [x] Shaders próprios: fontes GLSL anotadas em `content/shaders/` compiladas por `make.py shaders` em arquivos `.shader` com reflexão para Metal, HLSL, GLSL 430, GLSL ES 300 e WGSL, materiais com uniforms por nome (`graphics2d.newMaterial`) aplicados a sprites, canvases e pós-processamento, e hot reload no desktop. Validado em Metal, WebGL2 e WebGPU, com o guia `docs/shaders.md`.
- [x] Efeitos de áudio pelo grafo do miniaudio, por barramento e por som, com nós próprios de parâmetros atômicos, tweenáveis pelo Lua: `Filter` (passa-baixa, passa-alta, passa-banda, notch, pico e shelves, que formam o equalizador), `Delay` (atraso e eco) e `Reverb` (Freeverb).
- [x] Física avançada: cordas (cadeias de juntas), líquidos (partículas com renderização de metaballs pelo renderer), terreno destrutível (bitmap com marching squares e recriação das cadeias), ragdoll, veículos, pontes, explosões com impulso radial, plataformas de mão única (com a direção girando com o corpo) e esteiras.
- [x] Fontes bitmap (BMFont em texto e binário, e fontes em grade) além das TrueType e OpenType, com `text::Font` como interface (`text::TrueTypeFont` e `text::BitmapFont`).
- [~] Orientação do aparelho em tempo de execução, em todas as plataformas: ler e travar retrato ou paisagem (`window.orientation` e `window.lockOrientation`) e o evento `window_orientation_changed`. Falta exercitar a trava num aparelho.
- [x] Pools de objetos para sprites e projéteis (`haylen.collections`).

#### N. Samples

Os samples ficam em categorias, e os comandos recebem o caminho a partir de `samples/` (`python3 make.py run games/tiny-island`, `python3 make.py run graphics/lighting --platform web`, `python3 make.py run-cpp cpp/embedding`), com `python3 make.py samples` listando todos: `games/` (tiny-island e taskbar-quest), `graphics/` (sprites, camera, lighting, shaders, particles, nine-patch, fonts e scenes), `gameplay/` (physics, algorithms, tiled, tween, events, input e audio), `interface/` (ui, safe-area e orientation), `system/` (filesystem, preferences, localization, network, platform e native) e `cpp/` (embedding).

- [x] Samples em categorias, `make.py run` e `run-cpp` resolvendo o caminho a partir de `samples/`, `make.py samples` listando todos, e README, docs, CLAUDE.md e os READMEs dos samples no mesmo formato.

Todo sample de recursos tem um menu simples para escolher o teste, cada teste é uma cena com um botão para voltar ao menu, e roda em todas as plataformas pelos templates.

- [x] `samples/interface/ui` (19 testes): todos os componentes, temas, foco, teclado virtual, rich text e entrada de texto.
- [x] `samples/gameplay/physics` (17 testes): corpos, formas, juntas, cordas, líquidos, terreno destrutível, ragdoll, veículos, pontes, explosões, plataformas de mão única, sensores e consultas.
- [x] `samples/system/network` (7 testes, validados com e sem rede e na web): HTTP, HTTPS e WebSocket.
- [x] `samples/system/filesystem` (6 testes): leitura, escrita, listagem e remoção no disco do usuário e leitura do pacote.
- [x] `samples/system/preferences` (4 testes, com persistência entre execuções): preferências e save slots.
- [x] `samples/graphics/lighting` (15 testes): luz ambiente, pontual, spot, direcional, sombras, normal maps e máscaras.
- [x] `samples/graphics/shaders` (6 testes com 13 shaders próprios): shaders próprios em sprites, canvases e pós-processamento.
- [x] `samples/graphics/particles` (18 testes, 20 mil partículas vivas a 60 fps): fogo, fumaça, explosão, chuva, neve, faíscas, rastros, magia, confete, fogos de artifício e efeitos por arquivo.
- [x] `samples/system/localization` (9 testes em inglês, português, espanhol, japonês, árabe e hindi): idiomas, argumentos, plurais, direção do texto, fontes por idioma e troca em tempo real.
- [x] `samples/gameplay/input` (9 testes, com o remapeamento salvo nas preferências e toques reais no emulador Android): teclado, mouse, toque, gestos, controles, mapa de ações e remapeamento.
- [x] `samples/graphics/sprites` (11 testes): pools, spritesheets, animação, batches, tiros, milhares de sprites e poucos sprites.
- [x] `samples/system/platform` (4 testes, com o handler próprio respondendo em JavaScript na web e em Java no Android, e o Objective-C compilado para as plataformas Apple): bridge com os métodos embutidos e um handler próprio em cada plataforma.
- [x] `samples/interface/orientation` (4 testes): orientação do aparelho.
- [x] `samples/interface/safe-area` (4 testes): âncoras na safe area e na tela inteira.
- [x] `samples/gameplay/audio` (7 testes): música, efeitos, barramentos, efeitos de áudio e áudio 2D posicional.
- [x] `samples/graphics/nine-patch` (5 testes): nine-slice esticado e repetido, em peças e em UI.
- [x] `samples/graphics/fonts` (12 testes, com fontes OFL e fontes bitmap geradas): TrueType, OpenType, bitmap, tamanhos, contorno, sombra, famílias, fallback, scripts complexos, rich text, efeitos e medição.
- [x] `samples/graphics/camera` (14 testes): todos os recursos da câmera.
- [x] `samples/graphics/scenes` (6 testes): todas as transições (os 24 efeitos com direção, easing, duração e cor), efeitos próprios, o ciclo de carregamento (a transição como loading, view de loading, pré-carregamento e erro com `onError`), a pilha, sobreposições e a pausa.
- [x] `samples/gameplay/tiled` (12 testes, com o conteúdo gerado por `samples/gameplay/tiled/tools/generate_content.py`): mapas de todas as orientações do Tiled.
- [x] `samples/games/tiny-island`: o jogo, com `app.json`, `source/` e `content/`.
- [x] `samples/cpp/embedding`: o sample C++, rodado por `make.py run-cpp cpp/embedding`.

#### O. Documentação, testes e revisão

- [ ] Testes GoogleTest e Lua de cada recurso dos pedidos 51 a 110, com cobertura da engine perto de 100%.
- [x] Páginas `docs/lua-api/` e guias com o estado atual, incluindo o guia de distribuição (`docs/distribution.md`: templates, artefatos e comandos) e o de ciclo de vida (`docs/lifecycle.md`).
- [x] Estáticos da engine que outras threads usam nunca são destruídos (listeners do `core::Log`, contadores de objetos e o registro deles, estáticos do `EventBus`, padrões do `WebSocket`, relay da bridge, estado das chamadas nativas, campos de texto, regiões de janela, controles e estado da página web), com o teste de morte `LogTest.KeepsWorkingWhileAnotherThreadExits`. Os estáticos do spdlog do Varn dependem de uma mudança no Varn, descrita na seção 13.
- [x] Estáticos de gráficos, texto, UI e 2D que outras threads usam na saída (contadores de `TextureResource`, `Font`, `FontFamily`, `RichText`, `ui::Document`, `physics2d::World` e `particles2d::Emitter`, e os estáticos de `RenderTarget`, `SceneTransition`, `StaticSpriteBatch`, `particles2d::Effect`, `Raycaster`, `MapRenderer`, `PhraseBreaker` e `ui::Theme`) criados com `new` e nunca destruídos, conferido com o `-Wexit-time-destructors` do clang em todos os fontes da engine e dos testes. Só os workers do WebSocket nativo têm destrutor na saída, de propósito.
- [x] Avisos de compilação da engine: `-Wshadow` no `PocoWebSocket.cpp`. Builds limpos de macOS Debug, macOS com ThreadSanitizer, Catalyst e web.
- [x] As falhas do AddressSanitizer eram dos testes: os jobs do `RuntimeTests.cpp` que seguravam os workers ainda liam uma variável local depois do fim do teste (agora eles avisam a saída e o teste espera), e o `EventRecorder` do `SceneManagerTests.cpp` guardava conexões que não se desligavam na destruição (agora usa um `ConnectionScope`). A suíte inteira passa sob `--sanitizers address`.
- [x] `SceneLuaTest.SpawnedTasksNeverResumeOnceTheirSceneUnloaded` dependia de tempo real: com a máquina lenta, as duas esperas do teste venciam no mesmo frame e o teste esperava para sempre. A tarefa agora espera uma chamada da bridge que o teste só responde depois que a cena sai, sem depender do relógio.
- [~] `SceneLoadTest.FinishesOnceTheHookAndEveryDeferralReleasedIt` falhou uma vez no ctest paralelo com a máquina ocupada. O teste e o carregamento de cenas rodam só na thread do frame, sem tempo, e ele não falhou em 2000 repetições com três suítes rodando ao lado, em 1500 processos separados nem sob o ThreadSanitizer. A causa provável é um binário de testes compilado enquanto outro agente editava o código, sem prova. Registrar a saída da falha se ela aparecer de novo.
- [ ] Safe area e espaço reservado com `highDpi: false`: o Android informa os insets em pixels da janela enquanto o framebuffer tem metade do tamanho, então a safe area e o espaço reservado saem com o dobro. Converter para pixels do framebuffer em todas as plataformas e testar com `highDpi` ligado e desligado.
- [x] Varn fixado pelo arquivo de um commit (`0248f79`), que nunca muda de conteúdo, no lugar do arquivo da tag, que muda quando a tag é recriada.
- [x] Correções do Varn combinadas com a sessão do Workpane e adotadas: o Varn compila o libuv com o ajuste do tvOS e os símbolos fracos do Android, escolhe o driver HTTP pelo alvo, mantém o OpenSSL no limite de jobs, reconhece o Mac Catalyst, deixa o `HAVE_UNISTD_H` privado ao zlib e compila a fonte de debug do libffi com o gerador do Xcode. A engine registra os módulos pelo `Runtime::addModule`, os símbolos das bibliotecas estáticas pelo `Runtime::addSymbol` e as falhas de tarefas e de callbacks do `ffi` pelo `async.onFailure`, sem contornos para esses casos, e o `ffi` chama ponteiros de função, aceita `enum`, bitfields, `intptr_t` e ponteiros `const` onde cabem, e abre bibliotecas por caminhos UTF-8 no Windows.
- [x] Varn `04b710d` adotado: o driver HTTP da Apple vem do próprio Varn, a tela de erro monta a pilha das tarefas e dos callbacks do `ffi` pelos frames que o `async.onFailure` entrega, o cancelamento das tarefas das cenas e dos jobs usa o cancelamento do Varn, que fecha as variáveis to-be-closed, e a configuração com o CMake 4 não mostra avisos.
- [x] Varn `0248f79` adotado: um `__close` que falha numa tarefa cancelada chega ao `async.onFailure` sem patch, com texto ou tabela, e a pilha das tarefas e dos callbacks do `ffi` usa o `namewhat` e o `linedefined` dos frames do Varn e lê igual à das chamadas protegidas (`global 'error'`, `function <arquivo:linha>`), montada pelo mesmo código de `lua::Runtime`, com a mesma linha de níveis pulados nas pilhas profundas.
- [x] O cache de fontes do CPM guarda um pacote com patches numa chave feita do hash do arquivo e do conteúdo dos patches (`haylen_patched_package_key` e o `CUSTOM_CACHE_KEY` do CPM), então um patch alterado chega a todo checkout que já tinha a fonte, e checkouts em pastas diferentes dividem o mesmo pacote.
- [x] CI verde em todos os jobs (formato, desktops macOS, Ubuntu e Windows, cobertura, web, Android e Apple), com o `desktop (windows-latest)` e o `android` passando pela primeira vez, e o workflow cancelando as execuções substituídas de um mesmo branch. No Windows: os testes da FFI travavam numa caixa de diálogo do MSVC (o buffer de retorno do `ffi` do Varn), o `make.py shaders` roda sem shell porque o `cmd.exe` tirava as aspas do comando, o `wss://` conecta antes do handshake para o app poder abandoná-lo e aceita o handshake terminado do Schannel, o armazenamento não troca um arquivo enquanto outra thread o lê ou troca, o teste das bibliotecas nativas compara caminhos no formato do sistema, e o SDK calcula o prefixo do config pelos nomes e compila o Poco sem os pedidos automáticos de bibliotecas do MSVC.
- [x] Build sem avisos no GCC do Linux e no MSVC do Windows: o GCC deixa de avisar sobre os inicializadores designados, como o Clang, e os fontes e testes que escondiam membros (`Tree.cpp`, `KeyCapture.cpp`, `RangeSlider.cpp`, o `Window.cpp` dos overlays, `ScriptedScene.cpp`, `MixerLuaTests.cpp` e `ShapingTests.cpp`), o parâmetro sem uso de `Signal.hpp` e de `Binding.hpp`, as conversões de `Grid.cpp`, `NativeSignature.cpp` e do tempo limite de `PocoWebSocket.cpp`, o `-Wstringop-overflow` de `MapParser.cpp` e a cópia no laço de `NavMeshTests.cpp` foram corrigidos. Os logs do CI do Ubuntu e do Windows não têm avisos da engine. Os avisos do libffi vêm do Varn.
- [x] `HaylenNativeApi` versão 3 com `registerErrorHandler`: os erros que param o app chegam às bibliotecas nativas nos desktops, com o teste `NativeLuaTest.HandsTheErrorsThatStopAppsToLibraries`.
- [x] A tela de erro com foco nos botões, movido pelo direcional ou pelas setas e apertado pelo botão sul ou Enter, para o controle da TV reiniciar o app, com testes.
- [x] Na web, o foco de um elemento de plugin (o botão de um diálogo) não conta mais como perda de foco do app, então o app volta a ficar ativo quando a tela do plugin fecha, e Escape e Enter chegam ao elemento focado.
- [x] `Engine::stop` só para os plugins da engine cujo `start` terminou, em ordem inversa, inclusive os adicionados com o app rodando, então um plugin que falhou no início (ou que nunca começou porque outro falhou antes) não recebe `stop`, com o teste `EngineTest.StopsOnlyThePluginsThatStarted`.
- [x] O build web não tem avisos (o `DestructionLua.cpp` converte o tamanho da tabela onde lê).
- [x] Sem saída de áudio, o app roda sem som: o log avisa uma vez com o motivo, o mixer segue o tempo misturando e descartando as amostras (vozes terminam, músicas fazem crossfade e vozes posicionais atualizam), uma interrupção sempre termina, a engine tenta abrir a saída de novo quando o app volta a ficar ativo, e `Mixer::isOutputAvailable()`, `audio.outputAvailable()` e o `available` do `onStats` informam o estado. Na web, uma página em http fora do localhost abre e roda sem som, conferido pelo IP da rede local no Chrome, e `make.py serve`, `run` e `run-cpp` aceitam `--host`.
- [x] Posição da janela dos apps C++ no Mac Catalyst: `haylen_add_app` assina o bundle ad hoc depois do link, como o Xcode faz, e a janela abre onde o app pede, e não no canto superior esquerdo.
- [x] Revisão final de bugs, código morto, race conditions e riscos de crash em duas frentes (core, Lua, plugins, storage, io, math, IA, debug, net, áudio e input, e gráficos, texto, UI e os contextos 2D), com testes de regressão para as correções (crashes na saída e no reinício, use-after-free, estouro de pilha Lua, travamentos por entradas do Lua, asserts do Box2D, leituras fora dos limites, alfa pré-multiplicado, lotes de desenho e memória por frame) e ThreadSanitizer limpo. As sobras estão nos itens abertos deste grupo.
- [x] O teste `FontTest.FailsAgainForAGlyphNoAtlasHolds` roda em 43 ms em Debug e verifica o mesmo comportamento.
- [x] Os testes de gráficos (`MaterialTest.*`, `MaterialLuaTest.*`, `RendererTest.CapturesCanvasesIntoTargets` e `Graphics2DLuaTest.KeepsGpuPoolsSteadyAcrossFrames`) passam sob ThreadSanitizer, junto com os 923 testes da engine.
- [x] Fechar ou destruir um WebSocket nativo nunca bloqueia, mesmo durante a conexão, e o laço ocioso da conexão dorme até chegar dado.
- [x] `lua::Promise::resolve` com JSON aninhado além de 128 níveis ou com binário rejeita quem espera com um erro claro.
- [x] Metatables da engine protegidas: não dá para chamar o `__gc` na mão nem trocar o `__native` de um valor da engine.
- [x] Timers e tweens seguem o modo de processamento do dono quando ele muda.
- [x] `debug.addMonitor` com a opção `owner`.
- [x] Armazenamento com versões assíncronas no I/O pool (11 funções `*Async`), e a varredura do pacote do `PackageWatcher` fora da thread do frame no modo de desenvolvimento.
- [x] `load`, `loadfile` e `dofile` do app só carregam texto, e `string.dump` não existe.
- [x] Nomes Lua iguais aos do C++: `translation`, `current`, `findHighest` e `findLowest`, `setGamepadDeadzone` e `'bSpline'`.
- [x] Slots de controle estáveis na Apple quando um controle desconecta (`GamepadSlots`), e o ganho dos filtros de áudio limitado a ±96 dB, para os coeficientes nunca estourarem.
- [x] Decisão de mistura: `multiply` e `screen` recebem cores retas, como os outros modos (fora `premultiplied`), como o shader já fazia, documentado e com um teste que refaz a mistura da GPU para cada modo.
- [x] Tipos do contexto `text` com nomes precisos, sem repetir o contexto (`text::Layout`, `text::Style`, `text::Alignment` e `text::Effect`).
- [x] Arquivos de teste sem funções auxiliares livres: os helpers ficam em fixtures e em classes de apoio de `engine/tests/support/` (como `test::TestFiles`).
- [x] Nomes Lua iguais aos nomes C++: o tipo `haylen.Map` do `tiled::Map`, o `scale` do texto dentro de `text::Style`, `flipHorizontal` e `flipVertical` nos sprites, nos lotes e no `map:tileInfo`, e uma tabela de nomes por enum (20 enums tinham duas), usada pelos bindings e pelos leitores de arquivos. O `tint` e o `scale` do rich text continuam como parâmetros de `Renderer::drawRichText`.
- [x] Imagens do `[img]` do rich text em cache no `plugins::TextPlugin`: a imagem fica carregada enquanto os frames a desenham, então texto refeito a cada frame a carrega uma vez só.
- [x] Texturas dinâmicas no `graphics::Device` (`createDynamicTexture` e `updateTexture`, com `dynamic` e `texture:update` em Lua): os atlas da UI e das fontes mudam no lugar e sobem uma vez por frame, antes dos passes, por mais glifos que o frame acrescente. O Sokol só aceita a imagem inteira nessas texturas, como a seção 13 registra.
- [x] `JobSystem::parallelFor` em que o chamador roda todo pedaço que nenhum worker começou, então o `crowd:step` nunca espera atrás de jobs longos de fundo, como a construção assíncrona de um navmesh.
- [x] `PoissonDisk::reachOf` limitado ao tamanho da grade, zoom NaN da câmera recusado com erro, handles C++ de física com a geração do mundo (inválidos depois que o slot do mundo é reusado) e `GridRay::traverse` somando as distâncias em double, sem travar em raios longos.
- [x] `make.py` com `--sanitizers address|thread`, e o OpenSSL compilado com um job só, porque o `make.py` tira o `CMAKE_BUILD_PARALLEL_LEVEL` do ambiente, dentro do limite de jobs.
- [x] A triangulação com restrições do navmesh converge em todo layout: o teste de orientação exato resolve a semente 97, e uma aresta até 0,1% mais longa que a abertura que cruza conta como exata, o que evita milhares de pontos entre paredes quase paralelas (sementes 344 e 715). Varredura de 2000 sementes: 20000 construções sem falha e 154170 caminhos sem sair da malha.
- [x] Encerramento por uma thread de fundo não derruba o app: o runtime vive num `Process` que nunca é destruído fora da thread do frame, e um `exit()` de uma fila de fundo (no simulador iOS, o IOSurface faz isso quando o servidor de render cai) sai limpo (6 de 6 execuções).
- [x] Pasta de build única por app (`build/apps/<pasta>-<hash>/`), sem colisão entre apps com o mesmo nome de pasta.
- [x] `make.py run` nas plataformas Apple mostra o log do app no terminal: `log stream` filtrado para o app no simulador e no Catalyst, e a saída padrão no macOS.
- [x] O shell web dos apps C++ usa a logo da engine como ícone, sem 404 de `/favicon.ico`.
- [x] Áudio na web por um backend próprio do miniaudio (`BrowserAudioOutput`) que manda os blocos de PCM para um `AudioWorkletNode` pela porta de mensagens (`haylen-audio-worklet.js`), sem memória compartilhada, sem cabeçalhos de isolamento e com a mistura na thread da página, em páginas servidas por https ou localhost. O backend AudioWorklet do próprio miniaudio exige isolamento de origem (COOP e COEP), que bloqueia scripts de terceiros e os popups de login, e fica de fora.
- [x] As janelas do Mac Catalyst abrem com o tamanho do `app.json` (1280x720 centralizado nos apps do template).
- [x] `make.py run-cpp` roda os apps C++ no desktop, na web, no iOS, no tvOS, nos simuladores, no Catalyst e no Android: iOS, tvOS e simuladores pelo gerador do Xcode, Catalyst pelo Ninja e Android pelo template, com o `samples/cpp/embedding` rodado no simulador iOS, no emulador Android e no Catalyst.
- [x] Uma ligação do `ActionMap` segurada quando o input volta (por exemplo no fim de uma troca de cena) só dispara de novo depois de solta, então um toque normal em Escape ou Start não abre e fecha o menu de pausa do Tiny Island.
- [x] O exemplo de `docs/input.md` volta pelo `onCancel` do documento.
- [x] Os testes da engine leem fontes de `engine/tests/data/fonts` (subconjuntos com as licenças OFL), nunca dos samples.
- [x] Tamanho do texto de `graphics2d.drawText` e do rich text nos samples e no Tiny Island conferido com o tamanho pelo em das fontes, nos harnesses headless e Metal.
- [x] Android: o app fecha sem crash (patch `sokol-android-quit.patch`), validado no emulador com Back na raiz, recentes, `am force-stop` e `haylen.quit()`.
- [x] `make.py engine --platform android --jobs N` compila o `libhaylen.so` uma ABI de cada vez com o `--jobs`, e o Gradle só empacota.
- [x] Predictive back no Android 13 e posteriores: `android:enableOnBackInvokedCallback` no manifesto do AAR e o callback registrado só enquanto o app segura o Back (`window.setBackLeavesApp(false)`), validado no emulador.
- [x] `audio.playMusic` devolve a voz da música, para pausar só a música.
- [x] Filhos que crescem (`grow`) partem do zero e dividem o espaço que sobra no pai, então um `ui.scroll{grow = 1}` dentro de um painel não empurra a página para fora da tela.
- [x] Documentação coerente sobre a API mínima do Android e sobre o registro dos handlers embutidos.
- [x] Fontes OpenType CFF corretas em qualquer tamanho: o campo de distância sai do contorno completo, curvas cúbicas incluídas, pelo msdfgen (`text::DistanceField`).
- [x] Todas as faces pelo tamanho do em no `TrueTypeFont`, então os glifos de fallback saem no tamanho certo.
- [x] As setas do carrossel ficam por cima das páginas.
- [x] `document:bounds(id)` devolve os limites de janelas, diálogos e toasts.
- [x] `viewport.setScaling` e `viewport.setDesignSize` trocam a política de escala e a resolução de design em tempo de execução.
- [x] Slider com `step` não avisa `change` no primeiro frame quando o valor não cai no passo, e a tela de configurações não mostra alterações não salvas ao abrir.
- [x] Floats gravados pela menor representação decimal (`core::JsonNumber::fromFloat`), então `preferences.capture()` grava 0.6 e não 0.6000000238418579.
- [x] Numa linha (`row`), colunas que crescem com texto quebrado em várias linhas são medidas em duas passadas com a largura final.
- [x] O mesmo recurso compara igual em Lua (`__eq` compartilhado por todos os recursos), como `family.fallback[1] == family.fallback[1]`.
- [x] A UI usa famílias de fontes com fallback (`ui.addFont(nome, família)`, com o fallback mesclado no Dear ImGui) e as faces negrito e itálico nos papéis do tema, para um app em japonês não precisar trocar o tema.
- [x] Texto da direita para a esquerda e scripts complexos (árabe, hebraico, hindi e outros): shaping com HarfBuzz, reordenação bidirecional com SheenBidi, quebra de linha com libunibreak e o modelo tailandês do BudouX, em todo o texto 2D, no rich text e na UI (`ui.setDirection` espelha a interface, e o catálogo de idioma declara a direção). Limites documentados em `docs/text.md`: fontes bitmap sem shaping, laosiano, khmer e birmanês só quebram em espaços e pontuação, e texto vertical e ruby ficam de fora.
- [x] `socket:ping(payload)` e o evento `pong` no nativo (no navegador o JavaScript não manda ping, e a chamada dá um erro claro).
- [x] O `timeoutSeconds` do HTTP do Varn é um prazo único para a requisição inteira, com os redirecionamentos, aceita frações e recusa um valor que não é positivo, como `docs/lua.md` explica.
- [x] Eventos `network_online` e `network_offline` no macOS pelo `NWPathMonitor`: o estado começa desconhecido, o primeiro aviso publica o evento, e `haylen.networkState()` lê o estado.
- [x] Em `events.stats()` e `signal.list()`, o contador `stale` conta os listeners cujo dono já se foi até eles serem removidos.
- [x] O que a UI responde (cancelar num popup, diálogo ou edição, aceitar num controle focado, teclas durante a edição de texto e a captura de tecla) não dispara as ações do mapa naquela pressão, e o voltar dos samples passa pelo `onCancel` dos documentos, que só dispara quando nenhum popup consumiu o cancelamento.
- [x] Rótulos de checkbox e toggle sem reticências quando há espaço: larguras de texto arredondadas para cima e o rótulo com a largura do próprio controle.
- [x] Geometria das formas de física em Lua (`shape.kind`, `shape.points`, `shape.worldPoints`, `shape.radius`, `shape:outline()` e `body:outlines()`), para desenhar as peças de um ragdoll ou de uma fratura.
- [x] HPA\* assíncrono (`grid:hierarchicalPathfinderAsync`).
- [x] No Mac Catalyst, as teclas não chegam ao mapa de ações nem à navegação por foco enquanto um campo nativo edita texto.
- [x] O teclado virtual do Android não reabre depois do Back, validado no emulador com o AAR.
- [x] Host headless com limite de textura igual ao das GPUs reais (patch `sokol-dummy-limits.patch` do backend dummy do Sokol com os limites de desktop, 16384), para o teste de fumaça sem janela chegar ao gameplay do Tiny Island.
- [x] Regras de commit e push na `main` por bloco, com prefixo e frase curta começando com maiúscula e sem coautor, a conferência de arquivos privados e temporários antes de cada commit e a proibição de citar outras engines, no CLAUDE.md. Os commits são publicados em `github.com/haylen-org/haylen`.
- [x] Nenhuma menção a outras engines no repositório (código, comentários, testes, docs, README, CLAUDE.md e este documento).
- [x] Seções 1 a 13 deste documento com os nomes atuais (app, `source/`, `content/`, namespaces, módulos `2d`, `storage` e `preferences`), sem ponto e vírgula, sem histórico e com o status real de cada item.
- [x] CLAUDE.md descreve o projeto como ele é, sem histórico e sem versões de bibliotecas, e contém os padrões de código, a arquitetura, a organização e as regras de que cada execução de um agente precisa para novos recursos e correções.
- [ ] Varredura final do repositório inteiro atrás de frases de histórico e de menções a outras engines.

#### P. Organização do código C++

Mapa de namespaces (um por contexto, igual ao nome da pasta, e com o sufixo `2d` para as pastas dentro de `2d/`, igual aos módulos Lua): `haylen::core`, `haylen::math`, `haylen::io`, `haylen::assets`, `haylen::graphics`, `haylen::text`, `haylen::input`, `haylen::audio`, `haylen::ui`, `haylen::platform`, `haylen::localization`, `haylen::storage`, `haylen::ai`, `haylen::debug`, `haylen::net`, `haylen::lua`, `haylen::plugins`, `haylen::graphics2d`, `haylen::animation2d`, `haylen::particles2d`, `haylen::lighting2d`, `haylen::physics2d`, `haylen::navigation2d`, `haylen::spatial2d`, `haylen::procedural2d` e `haylen::tiled`. Dentro de um namespace o tipo não repete o contexto (`haylen::physics2d::World`, `haylen::physics2d::Body`, `haylen::tiled::Map`).

- [x] Todo o código em sub-namespaces por contexto, com pastas e namespaces batendo um com o outro: a engine, os testes com os helpers compartilhados, os benchmarks e o sample C++. A varredura com o AST do clang está limpa.
- [x] Um arquivo por classe, com o nome da classe. Tipos que pertencem a uma classe (opções, eventos e enums usados só por ela) ficam aninhados nela.
- [x] Nenhuma função livre: utilitários são métodos estáticos de uma classe (`Easing`, `Geometry`, `Math`), helpers de `.cpp` são métodos privados, e os bindings Lua são classes de binding. As únicas exceções são os pontos de entrada exigidos pela plataforma (`main`, `sokol_main`, funções JNI e exports do Emscripten), que só repassam para uma classe.
- [x] Membros sem prefixo nem sufixo, com acessores `get`, `set`, `is` e `has`. Em Lua, os pares `get` e `set` aparecem como propriedades.
- [x] Plugins na pasta `plugins/` e no namespace `haylen::plugins`, incluindo a interface `Plugin` e o `PluginRegistry`, com os nomes dos plugins iguais aos módulos Lua.
- [x] `plugins::StoragePlugin` no contexto `storage` (`UserStorage`, `Preferences` e `SaveSlots`), com os módulos Lua `haylen.storage` (arquivos e slots: `writeSlot`, `readSlot`, `slotInfo`, `slotExists`, `removeSlot` e `listSlots`) e `haylen.preferences` (arquivo `preferences.json`).
- [ ] Revisão do código inteiro atrás de coisas soltas, perdidas ou fora de classe.
- [x] CLAUDE.md com todas essas regras.

#### Q. Tela de erro

- [x] A pilha vem estruturada do Lua (fonte, linha, função e tipo de cada nível), sem depender de texto com tabs, e sem os níveis internos da engine (o `xpcall` e o wrapper das tarefas assíncronas).
- [x] A tela mostra o título, a mensagem com quebra de linha, o arquivo e a linha, um trecho do código-fonte com a linha do erro destacada e numerada, a pilha em colunas (local e função), o nome e a versão do app, a plataforma e a versão da engine.
- [x] Rolagem quando o conteúdo passa da tela (roda do mouse, arrastar e setas), copiar o relatório completo (tecla C ou botão) e reiniciar o app (tecla R ou botão), com botões para toque e o toque seguindo só o primeiro dedo.
- [x] O mesmo relatório estruturado chega ao `onError` da página web (com o array `frames`) e ao log.

#### R. Algoritmos de alto desempenho para jogos

Tudo em C++ com binding Lua, sem alocação por chamada nos caminhos quentes, com versões assíncronas pelo `JobSystem` (Promise em Lua) para os cálculos grandes e com resultados determinísticos por semente. A lista cobre o que os jogos 2D costumam precisar.

- [x] **Caminhos em grade**: A* com custos por célula, diagonais sem cortar cantos, heurísticas (Manhattan, octile, Euclidiana e Chebyshev), A* ponderado, Jump Point Search para grades de custo uniforme, grades hexagonais e isométricas (as orientações do Tiled), linha de visão e suavização.
- [x] **Caminhos em grafo**: A* e Dijkstra em grafos de waypoints com pesos, pontos habilitados e desabilitados.
- [x] **Flow fields e mapas de Dijkstra**: um campo de direção calculado uma vez para muitas unidades (RTS e tower defense), mapas de distância com várias origens, e fuga (mapa invertido).
- [x] **Pathfinding hierárquico (HPA\*)** para mapas grandes, com atualização local quando o mapa muda.
- [x] **Navmesh**: malha de navegação a partir de polígonos (objetos de colisão do Tiled ou gerados), triangulação de Delaunay com restrições, caminho com o algoritmo do funil (string pulling), raio do agente e reconstrução quando obstáculos mudam.
- [x] **Multidões**: flocking (separação, alinhamento e coesão), desvio de obstáculos e desvio recíproco entre agentes (RVO/ORCA), além do steering.
- [x] **Espacial**: quadtree, árvore de AABB dinâmica, k-d tree para vizinho mais próximo e spatial hash, com consultas por ponto, retângulo, círculo, raio e k vizinhos.
- [x] **Visão e grade**: raycast em grade (DDA), linhas e círculos de Bresenham, campo de visão por shadowcasting (roguelikes e névoa de guerra), polígono de visibilidade (luzes e visão), flood fill, componentes conectados (ilhas e regiões) e union-find.
- [x] **Distribuição de elementos no mapa**: espalhar objetos por área com densidade (quantidade = área × densidade) dentro de retângulos, círculos, anéis e polígonos (inclusive regiões e objetos do Tiled), Poisson disk com distância mínima variável por um mapa de densidade ou de ruído, grade com jitter, zonas de exclusão, camadas de biomas por ruído, pesos por tipo de objeto e ponto aleatório uniforme em polígono.
- [x] **Geração procedural** (módulo `haylen.procedural2d`, versões assíncronas com Promise, determinístico por semente): autômatos celulares (cavernas), Wave Function Collapse de tiles, BSP e posicionamento de salas para dungeons, drunkard walk, labirintos (backtracker, Prim e Kruskal), diagramas de Voronoi, triangulação de Delaunay e relaxamento de Lloyd, ruídos Worley (celular) e domain warp além de Perlin, simplex e fractal, e autotiling em tempo de execução (máscaras de 4 e 8 vizinhos e Wang).
- [x] **Geometria e destruição**: operações booleanas de polígonos (união, diferença, interseção e offset, com `math::Polygon` sobre o Clipper2), decomposição em polígonos convexos para o Box2D, simplificação (Ramer-Douglas-Peucker), marching squares (`math::MarchingSquares`, contornos a partir de grades e bitmaps), terreno destrutível por bitmap ou por polígonos (cavar, explodir e reconstruir a colisão por pedaço), fratura de polígonos por Voronoi para quebrar objetos, e splines (`math::Spline`: Catmull-Rom, Bézier e B-spline) com amostragem por distância.
- [x] **IA**: behavior trees com blackboard, utility AI e mapas de influência, além da máquina de estados.
- [x] **Utilitários**: sacola aleatória (`math::ShuffleBag`), escolha ponderada (`math::WeightedChoice`), molas criticamente amortecidas (`math::Spring`), pools de objetos (`core::ObjectPool`) e ring buffers (`core::RingBuffer`), com `haylen.collections` em Lua.
- [x] **Benchmarks** dos algoritmos principais em `make.py bench --suite algorithms` (A* 512x512 em 2,4 ms, navmesh de 400 obstáculos em 5,4 ms, 2000 agentes ORCA em 0,1 ms e 100 mil raios de física em 2,8 ms pelo `JobSystem`) e da geração procedural em `make.py bench --suite procedural`, e o sample `samples/gameplay/algorithms` com 26 testes, um por algoritmo, cada um com menu e botão de voltar.

#### S. Tween robusto

O sistema de tween (`core::TweenManager` e `haylen.tween`) cobre tudo o que um sistema de tween completo oferece.

- [x] **Alvos e valores**: qualquer campo de tabela ou propriedade de userdata, caminhos aninhados (`position.x`), números, `Vec2`, `Color` (em RGB ou HSV), ângulos pelo caminho mais curto, inteiros (contadores de pontos), texto (máquina de escrever) e vários campos no mesmo tween.
- [x] **Modos**: `to`, `from`, `by` (relativo) e `fromTo`, valor inicial lido na hora de começar, e tweens por velocidade (a duração sai da distância).
- [x] **Easing completo**: todas as famílias de Penner, parâmetros de `back` (overshoot) e `elastic` (amplitude e período), `steps`, curva Bézier cúbica (como a do CSS), curvas por pontos e funções próprias em Lua e C++.
- [x] **Timelines**: `append`, `join` (em paralelo com o anterior), `insert` num tempo, rótulos, atrasos, callbacks num tempo, sequências e timelines aninhadas, repetição e yoyo da timeline inteira.
- [x] **Controle**: `play`, `pause`, `resume`, `restart`, `reverse` (tocar para trás), `seek` (ir para um tempo ou progresso), `complete` (pular para o fim com ou sem callbacks), `kill`, escala de tempo por tween e por grupo, progresso lido e escrito, e estado (`isPlaying` e `isComplete`).
- [x] **Repetição**: número de vezes ou infinita, modos `restart`, `yoyo` e `incremental`, e atraso entre repetições.
- [x] **Eventos**: `onStart`, `onUpdate`, `onStep`, `onLoop`, `onComplete` e `onKill`, e `wait()` com Promise para `await`.
- [x] **Tweens prontos**: mover, escalar, girar, desbotar, tingir, pular (arco), seguir um caminho (spline com orientação), Bézier, piscar, tremer (shake com força, vibração e aleatoriedade) e soco (punch).
- [x] **Escalonamento (stagger)**: o mesmo tween em uma lista de alvos com atraso crescente, a partir do começo, do fim ou do centro.
- [x] **Conflitos e vida útil**: modo de sobrescrita (um tween novo no mesmo alvo e campo mata o anterior), tweens ligados à cena (morrem quando a cena sai) e ao alvo (morrem quando o alvo é coletado ou destruído), sem nunca escrever num alvo morto.
- [x] **Tempo**: modos de processamento da pausa (grupo E), tempo com ou sem escala, e atualização no passo fixo quando pedido.
- [x] **Desempenho**: milhares de tweens sem alocação por frame, tweens nativos em propriedades C++ (posição, escala, rotação e cor de sprites, câmera e nós de UI por `doc:transform(id)`) sem chamar Lua por frame, e contagem de tweens nas estatísticas de debug.
- [x] **Testes, página `docs/lua-api/tween.md` completa e o sample `samples/gameplay/tween` (19 testes)**, com cada recurso numa cena, menu e botão de voltar.

#### T. Eventos, ciclo de vida e conexões

Sinais (`core::Signal` com `core::Connection` e `haylen.signal`), o barramento de eventos (`core::EventBus` e `haylen.events`), os eventos das cenas e os eventos da plataforma formam um sistema único e robusto.

- [x] **Sinais**: tipados em C++, conexão com RAII, seguros durante a emissão (conectar e desconectar dentro do callback), prioridade, conexão de uma vez só (`once`), conexão adiada (entregue no fim do frame), bloqueio temporário, `isConnected`, e desconexão automática quando o dono morre (objeto C++, userdata ou tabela Lua coletada, cena que saiu e documento de UI desmontado).
- [x] **Barramento de eventos**: publicar e assinar por nome ou tipo, com canais, filtros, prioridade, consumo (parar a propagação), entrega imediata ou na fila do frame, e emissão a partir de outras threads entregue sempre na thread do frame.
- [x] **Ciclo de vida de tudo, com os mesmos nomes em C++ e Lua**:
  - app: início, ativo, inativo, segundo plano, pouca memória e saída.
  - cenas: entrar, sair, pausar, retomar, começo e fim de transição, pausado e despausado.
  - plugins e autoloads: início, parada e eventos do app.
  - objetos: criado e destruído, com contagem nas estatísticas e eventos opcionais.
  - documentos de UI: montado e desmontado.
  - assets: carregado, descarregado e recarregado.
  - janela: tamanho, foco, tela cheia, orientação e safe area.
- [x] **Conexão e desconexão de dispositivos e serviços**: controles conectados e desconectados (com o slot e o nome), troca de rota de áudio (`audio_route_changed`), WebSocket com reconexão automática opcional por backoff exponencial, rede online e offline quando a plataforma informa, e teclado virtual aberto e fechado.
- [x] **Escopos de assinatura**: uma cena, um autoload ou um objeto assina eventos num escopo que cancela tudo sozinho quando ele sai (`scene.listen(owner, fonte, fn)` e `events.on(nome, fn, {owner = ...})`), sem vazamento de listeners.
- [x] **Diagnóstico**: sinais e listeners ativos com contagem de emissões no overlay de debug, e aviso de listener pendurado num dono morto.
- [x] **Testes e documentação**: `docs/lua-api/signal.md`, `docs/lua-api/events.md`, o guia `docs/lifecycle.md` e o sample `samples/gameplay/events` com 9 testes, cada recurso numa cena.

#### U. Raycast

O raycast é um recurso completo, com e sem física, sempre em C++ e com binding Lua.

- [x] **Física**: raio mais próximo, todos os acertos ordenados por distância e o primeiro que passa num filtro (categoria, máscara, grupo ou função), com ponto, normal, fração, corpo e forma.
- [x] **Shape casts**: círculo, caixa, cápsula e polígono varridos ao longo de um vetor, para personagens, projéteis grossos e previsão de colisão.
- [x] **Sem física**: raio contra segmentos, retângulos, círculos, polígonos e cadeias (`math::Raycast`), contra camadas de tiles e objetos do Tiled, contra grades (DDA) e contra as estruturas espaciais (spatial hash, quadtree e árvore de AABB).
- [x] **Raios avançados**: atravessar vários alvos (piercing) com limite, refletir e ricochetear (lasers), linha de visão entre dois pontos e leque de raios (cone de visão) numa chamada só.
- [x] **Seleção pela tela**: converter um ponto da tela em raio ou ponto do mundo pela câmera e achar os objetos sob o cursor ou o toque.
- [x] **Desempenho**: muitos raios numa chamada só (em lote e em paralelo pelo `JobSystem`), sem alocação por raio, com resultado em buffers reaproveitados em Lua.
- [x] **Debug**: desenho dos raios, acertos e normais no overlay.
- [x] **Testes e documentação**: `docs/lua-api/physics2d.md`, as seções de raycast de `math` e `spatial2d`, e os testes de raycast no `samples/gameplay/physics` e no `samples/gameplay/algorithms`.

#### V. Ciclo de vida de cena com carregamento

A troca de cena é um pipeline com fases, no modelo "cobrir, carregar, revelar", e toda cena passa pelos mesmos estados: criada, carregando, carregada, entrando, ativa, coberta (outra cena por cima na pilha), saindo, saiu e descarregada.

- [x] **Carregamento assíncrono da cena**: um gancho `load` (em C++ e em Lua) que roda antes do `enter`, pode esperar Promises (`:await()` numa corrotina do Varn) e informa o progresso (`context:progress(valor, mensagem)`), com ajuda para carregar grupos de preload do `assets::Manager` somando o progresso. Os trabalhos pesados continuam nos pools do Varn e os uploads de GPU são distribuídos entre frames, sem travar o frame.
- [x] **Descarregamento**: um gancho `unload` depois do `exit`, que libera os recursos da cena. Na troca por `replace`, a cena anterior pode sair e descarregar antes da nova carregar (opção ligada por padrão nas transições que cobrem a tela), para o pico de memória ser menor.
- [x] **Fases da transição**: cobrir (a cena atual some no efeito), segurar (a tela fica coberta enquanto a nova cena carrega) e revelar (a nova cena aparece). Os efeitos que mostram as duas cenas ao mesmo tempo (crossfade, slide, push e os outros) carregam a nova cena antes de começar, com a cena atual ainda na tela.
- [x] **Loading opcional**: a própria transição serve de loading (a tela coberta, com a cor ou o efeito), ou o desenvolvedor passa uma view de loading (`loading`: uma cena ou tabela com `update` e `render` que recebe o progresso), exibida só se o carregamento passar de um atraso configurável (`loadingDelay`, sem piscar em cargas rápidas), por um tempo mínimo configurável quando aparece (`minimumLoadingTime`) e com saída suave (`loadingFadeOut`, de 0,25 s por padrão). Também é possível usar uma cena de loading comum na pilha.
- [x] **Pré-carregamento**: `scene.preload(cena, params)` começa o `load` em segundo plano sem trocar de cena e devolve uma Promise, e a troca depois fica instantânea ou só espera o que falta.
- [x] **Ganchos de transição**: `enterTransitionFinished` na cena que entra (input liberado) e `exitTransitionStarted` na cena que sai.
- [x] **Erros**: um erro no `load` cancela a troca, rejeita a Promise da troca e mantém a cena atual quando ela ainda existe. Quando a cena atual já saiu, o erro vai para a tela de erro ou para um `onError` da troca, que pode levar a outra cena.
- [x] **Concorrência e fila**: pedidos de troca durante uma troca entram na fila em ordem. `push`, `pop`, `replace`, `popTo` e `popToRoot` seguem o mesmo pipeline, e a pausa do jogo, o segundo plano e a perda de foco durante uma carga têm comportamento definido e testado.
- [x] **Escopo de vida**: tudo o que a cena cria (timers, tweens, assinaturas de eventos, tarefas assíncronas iniciadas com `scene.spawn(owner, fn)` e documentos de UI) pertence à cena e é cancelado no `unload`, sem nenhuma corrotina retomando numa cena que já saiu.
- [x] **Eventos do ciclo**: `scene_loading`, `scene_loaded`, `scene_load_failed`, `scene_entered`, `scene_exited`, `scene_unloaded` e os começos e fins de cada fase da transição, pelo barramento de eventos.
- [x] **Tiny Island** usa o ciclo: o menu e a partida carregam os grupos de preload no `load`, e a partida carrega atrás da view de loading.
- [x] **Testes e documentação**: 36 testes das combinações, `docs/lifecycle.md` com os diagramas de sequência, `docs/lua-api/scene.md` e o `samples/graphics/scenes` mostrando o ciclo de carregamento.

#### W. Janelas sem moldura e transparentes

A referência são jogos como o Taskbar Hero, que rodam numa faixa transparente em cima da barra de tarefas.

- [x] **Configuração no `app.json` e em Lua**: janela sem moldura (`decorated = false`), fundo transparente por pixel (`transparent = true`), sempre no topo, sem aparecer na barra de tarefas ou no Dock quando pedido, sem roubar o foco quando pedido, e posição e tamanho iniciais, inclusive ancorados na área de trabalho (por exemplo em cima da barra de tarefas, com `position` aceitando `fill`). Tudo o que pode mudar em tempo de execução também muda pelo Lua (`window.place` e as outras funções de `haylen.window`).
- [x] **Arrastar a janela** a partir do conteúdo do app (`window.startDrag()` no gesto de arrastar), mover e redimensionar por código, e ler a posição e o tamanho.
- [x] **Cliques atravessando a janela**: a janela inteira ou só as áreas transparentes deixam os cliques passarem para o que está atrás (passthrough por polígonos ou pelo alfa do pixel), com as áreas do jogo e da UI recebendo o mouse normalmente.
- [x] **Monitores**: lista de monitores com a área total e a área de trabalho (sem a barra de tarefas, o Dock e a barra de menus), escala de DPI e o monitor atual, com eventos quando mudam.
- [x] **Renderização transparente**: o framebuffer com alfa e a composição correta com a área de trabalho (alfa pré-multiplicado), com luz, pós-processamento, transições (page turn incluído) e UI funcionando, conferida por leitura dos pixels no Metal.
- [~] **Plataformas**: macOS, Windows e Linux (X11) com o que cada sistema permite, e na web o canvas transparente sobre a página. No mobile e na TV não se aplica, documentado. macOS e web validados. Windows e Linux compilam com toolchains cruzadas e o patch do Sokol aplica limpo, mas faltam rodar numa máquina Windows e num Linux com compositor.
- [x] Transparência como estado em tempo de execução (`window.setTransparent` e `window.canBeTransparent`): no modo janela comum o app fica opaco de verdade (barra de título, conteúdo e sombra normais, com o alfa forçado em 1) e volta a ser transparente no modo faixa, como o Taskbar Quest mostra no macOS.
- [x] **Sample** `samples/games/taskbar-quest`: um jogo pequeno numa faixa transparente em cima da barra de tarefas, arrastável, com a UI do jogo, cliques atravessando as áreas vazias e o modo janela comum para comparar.
- [x] **Testes e documentação**: 17 testes, `docs/lua-api/window.md` e o guia `docs/desktop.md`.

#### X. Comunicação com a plataforma e código nativo

- [x] **Bridge assíncrona fácil em todas as plataformas**: chamar a plataforma e receber a resposta é uma linha em Lua (`platform.call('metodo', args):await()` ou callbacks), com a chamada oferecendo `await`, `cancel` e `timeout` e erros tipados `{message, code, data}`. Handlers nativos em Java e Kotlin (`HaylenCoroutines`) no Android, Objective-C e Swift (`HaylenBridgeAsync.swift`) na Apple, JavaScript na web e C++ no desktop e nos apps C++, com exceções Java virando falhas, eventos da plataforma para o app, erros claros e a resposta sempre na thread do frame.
- [x] **Bibliotecas nativas pelo Lua (FFI)**: `haylen.native` com `load`, `symbol` e `callback`, sobre o `ffi` do Varn e closures libffi da engine entregues na thread do frame, para chamar funções C com tipos, structs, ponteiros, buffers e callbacks do código nativo para o Lua, em macOS, Windows, Linux, iOS (frameworks embarcados) e Android (`.so` do APK). Na web o equivalente é chamar JavaScript pela bridge, documentado.
- [x] **Bibliotecas nativas no app**: a seção `native` do `app.json` leva bibliotecas nativas prontas ou compiladas de um projeto CMake (`.dylib` e frameworks, `.dll`, `.so`, xcframeworks e `jniLibs`) para dentro do pacote final de cada plataforma, com o caminho de carregamento resolvido pela engine: `Contents/Frameworks` no macOS, frameworks embarcados ou bibliotecas estáticas com tabela de símbolos gerada no iOS e tvOS, `jniLibs` no Android, ao lado do executável no Windows e `lib/` com RUNPATH no Linux.
- [x] **Plugins nativos em C++** para quem compila a engine (apps C++), registrados pela aplicação, com o mesmo ciclo de vida dos plugins da engine e o próprio módulo Lua.
- [x] **Guia de integração com SDKs** (`docs/native.md`): como integrar a Steam (API flat em C), o Epic Online Services (API em C, NAT P2P) e SDKs parecidos por FFI ou por plugin nativo, com os callbacks do SDK chegando na thread do frame. Os SDKs não entram no repositório.
- [~] **Testes em todas as plataformas nativas**: uma biblioteca nativa de teste (`engine/tests/native/NativeTest.c`, com funções, structs, buffers e um callback) compilada para cada plataforma e chamada pelo Lua em 15 testes da engine e no `samples/system/native`, que passa no player desktop, no app macOS, no simulador iOS com framework dinâmico e biblioteca estática, no emulador Android arm64 e, pela bridge, na web com WebGPU e WebGL2. Os testes da engine também passam no CI do Windows e do Linux. Faltam o sample no tvOS, no Mac Catalyst, no Windows, no Linux e em aparelhos reais.
- [x] **Documentação**: `docs/platform_bridge.md`, o guia `docs/native.md` e as páginas da API Lua.

#### Y. Consistência da API Lua

- [x] Todos os nomes que a API Lua recebe ou devolve como string em camelCase, sem aliases: os 54 eventos do `core::LifecycleEvent` (`appBackground`, `sceneEnterTransitionFinished`), os eventos de plataforma (`keyDown`, `touchBegan`, `quitRequested`), as teclas e os controles (`leftShift`, `graveAccent`, `keypad0`, `leftShoulder`, `rightTrigger`), os cursores (`pointingHand`, `resizeAll`), os dispositivos (`keyboardMouse`), os easings (`quadOut`, `elasticInOut`), os modos e políticas (`pingPong`, `pixelPerfect`), os gestos (`doubleTap`, `longPress`), as ações de UI (`uiAccept`), o tipo de asset `tiledWorld` e os códigos de erro da bridge (`noHandler`, `invalidJson`). A engine, os testes, a documentação, os samples, o template, o JavaScript da web, o Java do Android e o código Apple usam os mesmos nomes. Preferências gravadas com os nomes antigos deixam de carregar.
- [x] Nomes Lua iguais aos do C++, na direção que dá o nome mais preciso: tipos `Transform2D`, `Noise2D`, `StaticSpriteBatch` e `HashGrid` (`spatial2d.newHashGrid`), funções como `haylen.elapsed` e `haylen.frameIndex`, `storage.readText` e `storage.writeText`, `input.findTouch`, `localization.findBestMatch`, `native.findSymbol`, `platform.registerHandler`, `assets.unloadGroup`, `window.framebufferSize`, `ui.usingPointer`, `document:replaceChildren`, e do lado C++ `Viewport::getScaling`, `Profiler::beginScope` e `endScope`, `RayBatch::getHit` e os parâmetros `curved` e `closed` do `TweenMotion::path`. Os nomes de tipo Lua são únicos no registro de metatables (`haylen.` e o nome da classe C++, com o contexto na frente quando o nome sozinho é vago, como `Map`, `PhysicsWorld`, `UiDocument` e `NavGrid`).
- [x] Mensagens de erro no padrão do CLAUDE.md nas 13 mensagens do `AppConfig.cpp` e nas mensagens que começavam com minúscula (UI, ImGui, tween, jobs, libffi, drawBatch, captura, clip, metatables protegidas, opções inválidas e tipos sem membro).
- [x] Teclas de dígito com o nome do valor do enum: `Key::Digit0` a `Key::Digit9` em C++ e `'digit0'` a `'digit9'` em Lua (`key:digit1`), com o rótulo de captura mostrando só o número.
- [x] Direção do texto com o nome do valor do enum: `'leftToRight'` e `'rightToLeft'` no Lua, na UI, no rich text (`[p dir=…]`) e nos catálogos de idioma (`@direction`), com uma tabela de nomes só.
- [x] As mensagens com um valor anexado viraram frases completas que citam o valor e dizem o que se espera (como `The texture filter must be "nearest" or "linear", not "x".`), e as mensagens de propriedades da UI começam com maiúscula e dizem o esperado (`The property "style.padding" of a "button" must be a number.`), por `PropertyReader::describeProperty` e `describeKind`.

#### Z. Plugins nativos

Decisões, a partir da documentação oficial dos SDKs (AdMob, UMP, Firebase, StoreKit, Play Billing, Game Center e Play Games), do código do Sokol e do código atual da bridge:

- Um plugin é uma pasta com `plugin.json` (id, nome, versão, descrição, plataformas, dependências de outros plugins, parâmetros por app e o que cada plataforma traz), a API Lua em `source/`, a parte Apple em `apple/` (Swift ou Objective-C, pacotes Swift, xcframeworks, chaves do Info.plist, entitlements e scripts de build), a parte Android em `android/` (módulo de biblioteca Gradle em Kotlin ou Java, com dependências Maven, manifesto e regras do R8), a parte web em `web/` (módulo JavaScript) e, quando preciso, uma biblioteca nativa em C ou C++ em `native/` para os desktops, a mesma da seção `native` do `app.json`.
- Quem escreve um plugin escreve a API Lua e a implementação em Swift ou Objective-C, em Kotlin ou Java e em JavaScript. As chamadas continuam pela bridge assíncrona em JSON, com a resposta na thread do frame.
- O app lista os plugins na seção `plugins` do `app.json`, com a configuração de cada um validada pelos parâmetros do `plugin.json`. Os plugins ficam em `plugins/<id>/` do app, copiados por `make.py plugin add` de uma pasta ou de um repositório git num branch, tag ou commit. O pacote do app leva o Lua dos plugins (`plugins/<id>/source/`), e `require('<id>')` encontra o módulo do plugin.
- Plugins que trazem SDKs de terceiros (anúncios, analytics, crashes, login, compras e serviços de jogos) ficam em repositórios próprios em https://github.com/haylen-org, fora deste repositório, que não usa bibliotecas de terceiros além das dependências da engine. Aqui fica um plugin de demonstração no sample de plugins, só com APIs da plataforma, que testa cada capacidade.
- Apple: quando o app tem plugins, `make.py` gera de novo o `App.xcodeproj` montado com o XcodeGen baixado numa versão fixa e conferido por SHA-256, incluindo um fragmento por plugin com os fontes e os pacotes Swift de cada alvo (o AdMob só existe no iOS, sem tvOS, macOS e Mac Catalyst). O template continua com o projeto gerado ao lado do `project.yml`. O Info.plist e os entitlements continuam escritos pelo `make.py`, que junta as chaves dos plugins.
- Apple: um patch pequeno do Sokol deixa a engine usar uma subclasse do delegate de app e de cena do Sokol, que repassa aos plugins o launch com as opções, as opções de conexão da cena (URLs, atividades e notificação), `openURLContexts`, `continueUserActivity`, o token e as notificações remotas, as sessões de URL em segundo plano e o delegate do `UNUserNotificationCenter`. O swizzling do Firebase fica desligado (`FirebaseAppDelegateProxyEnabled = NO`), porque o repasse é explícito.
- Android: `make.py` inclui o módulo Gradle de cada plugin no projeto montado e aplica os plugins Gradle que eles pedem (Crashlytics e google-services). Cada módulo declara a classe do plugin num `meta-data` do manifesto, e a biblioteca da engine encontra os plugins por um `ContentProvider` que roda antes do `Application.onCreate`, sem exigir uma classe `Application` própria. Uma regra do R8 da biblioteca mantém toda subclasse de `HaylenPlugin`.
- Android: o `HaylenActivity` repassa aos plugins o ciclo da activity, `onNewIntent`, `onActivityResult`, `onRequestPermissionsResult`, `onConfigurationChanged` e `onWindowFocusChanged`, porque o `NativeActivity` não tem a API de Activity Result.
- Views nativas por cima do jogo: no iOS, uma view que deixa os toques passarem, acima da view Metal. No Android, uma janela de painel (`TYPE_APPLICATION_PANEL`, sem foco e do tamanho da view) por view, porque o `NativeActivity` toma a superfície e a fila de input, e as views comuns da activity não desenham nem recebem toques. Na web, uma camada de DOM por cima do canvas. A posição usa âncoras (bordas, cantos e centro) com margens, dentro ou fora da safe area.
- Uma view nativa pode reservar espaço: a engine soma a borda que ela ocupa à safe area, e a UI ancorada na safe area sai de baixo do banner sozinha, com o evento de mudança da safe area.
- UI nativa que cobre o app (anúncios de tela cheia, formulários de consentimento, login e compras) marca o app como coberto: a engine o põe no estado inativo, pausa e silencia até a última cobertura acabar, e o Lua recebe os eventos de estado de sempre.
- Os erros do app (mensagem, arquivo, linha e a pilha do Lua) chegam aos plugins nativos, para o Crashlytics registrar como erros não fatais com os frames do Lua.
- Eventos nativos que chegam antes de o Lua escutar (link profundo na abertura, transações pendentes da loja e a notificação que abriu o app) ficam guardados e chegam ao primeiro ouvinte.
- JSON continua sendo o formato da bridge. Eventos frequentes, como analytics, vão em lote. Dados grandes por frame usam um caminho binário ou C++.

**Formato, configuração e ferramentas**

- [x] `plugin.json` com esquema documentado em `docs/plugins.md` e validado pelo `make.py` (`PluginManifestCheck`), que mostra todos os problemas de uma vez com o caminho e a chave: id em dash-case igual à pasta, versão, plataformas (`ios`, `catalyst`, `tvos`, `macos`, `android`, `web`, `windows`, `linux`), dependências, parâmetros tipados (texto, número, inteiro, booleano, lista, objeto e arquivo) com plataformas, obrigatoriedade e padrão, substituições `${parâmetro}` e as seções `apple`, `android`, `web` e `native`.
- [x] Seção `plugins` do `app.json`: o `make.py` valida para a plataforma do build (plugin desconhecido, parâmetro que falta, tipo errado, arquivo que não existe, plugin requerido ausente e parâmetro que nenhum plugin declara), aplica os padrões e ordena os plugins pelas dependências, e o `AppConfig` da engine (`AppConfig::fromPackage`) aceita a seção como objeto de objetos com ids em dash-case e confere que cada plugin tem `plugins/<id>/plugin.json` no pacote.
- [x] `make.py plugin add <pasta|repositório>` com `--ref` (branch, tag ou commit, pelo fetch de uma ref só, sem copiar o `.git`), `plugin remove <id>`, `plugin list` e `plugin new <pasta>`, com o esqueleto em `templates/plugin/` (API Lua, classe Swift, classe Kotlin, módulo JavaScript, `plugin.json` e README).
- [~] O pacote do app (`make.py package`, a cópia para as plataformas, o índice do Android e o zip da web) leva `plugins/<id>/plugin.json` e `plugins/<id>/source/` de cada plugin listado, e o player de desktop acha o mesmo Lua, com hot reload do Lua dos plugins. Falta o `haylen_add_app` dos apps C++ levar os plugins.
- [x] Arquivos por app, como `GoogleService-Info.plist` e `google-services.json`, como parâmetros do tipo arquivo relativos à pasta do app, copiados como recursos do bundle na Apple e para o projeto Android pela seção `files`.
- [x] `make.py tools` baixa o XcodeGen 2.46.0 conferido por SHA-256 em `.tools/xcodegen/`, também no primeiro uso, e o `App.xcodeproj` do template regenerado com o `plugins.json` vazio sai igual byte a byte.

**Engine (runtime, Lua e C++)**

- [x] `require('<id>')` e `require('<id>.<módulo>')` resolvem os módulos Lua do plugin em `plugins/<id>/source/`, só para plugins listados no `app.json`, e um módulo do app com o mesmo nome de um plugin para o app com os dois arquivos na mensagem.
- [x] `platform.plugins()` devolve os plugins do app com id, versão e se a parte nativa existe nesta plataforma, e `platform.plugin(id)` devolve o handle do plugin (`haylen.AppPlugin`) com `id`, `version`, `config` com os padrões do `plugin.json`, `native`, `call`, `send` e `on` com o prefixo do plugin. `platform.send` manda uma chamada sem resposta, e bibliotecas nativas declaram um plugin por `HaylenNativeApi.registerPlugin`.
- [x] Espaço reservado por views nativas (`platform::NativeViews`, segura entre threads): a engine junta as bordas reservadas à safe area do aparelho ou simulada, publica `windowSafeAreaChanged` e o Lua lê `viewport.reservedInsets()` em unidades de design, com testes da UI ancorada saindo de baixo.
- [x] App coberto por UI nativa: contador seguro entre threads, app inativo, parado e mudo enquanto coberto, qualquer que seja a opção de ciclo de vida, volta ao estado e ao volume de antes sem pulo de relógio, `haylen.appCovered()` no Lua e testes de cobertura aninhada, segundo plano coberto, reinício e threads.
- [x] Erros do app para o nativo: `Engine::reportError` entrega ao host o relatório em JSON (`message`, `file`, `line`, `traceback` e `frames` com `source`, `line`, `function` e `kind`), a web entrega ao `onError` e ao `onAppError` de cada plugin, e o host headless grava para os testes. Falta o Android e a Apple entregarem aos plugins.
- [x] Eventos nativos guardados até o primeiro ouvinte: `Bridge::emit` com retenção (até 32 por nome, o mais antigo sai primeiro), entregues ao primeiro `on` em ordem, pelo `BridgeRelay`, pela `HaylenNativeApi`, pela web (`emit(event, payload, {retain})`), pelo Lua, pelo JNI e pelo `AppleBridge`.
- [x] Handlers nativos com política de thread: `HaylenBridge.Threading.MAIN` (padrão, para UI) ou `BACKGROUND` (um executor compartilhado) no Android, com o parse do JSON na thread do handler, fora da thread do frame.

**Android**

- [x] `dev.haylen.HaylenPlugin` e `HaylenPluginContext` (id, application, activity, configuração com os padrões do `plugin.json`, `register` com e sem política de thread, `registerSuspend` em Kotlin, `emit` e `emitRetained`, overlay, cobertura contada e thread principal).
- [x] `HaylenPluginProvider` (não exportado, antes do `Application.onCreate`) encontra os plugins pelos `meta-data` `dev.haylen.plugin.<id>`, cria cada um uma vez na ordem do `app.json` com as dependências antes, chama `onLoad` e informa os ids à engine. Uma classe que não existe é registrada no log e fica de fora, e a regra do R8 mantém as subclasses de `HaylenPlugin`, conferido num build Release minificado.
- [x] `HaylenActivity` repassa aos plugins o ciclo da activity, `onNewIntent` (com `setIntent`), `onActivityResult` e `onRequestPermissionsResult` (até o primeiro plugin que trata), `onConfigurationChanged`, `onWindowFocusChanged`, `onTrimMemory` e `onAppError` na thread principal, conferido no emulador com um seletor real.
- [x] `HaylenOverlay` e `HaylenPlacement` no Android: uma janela de painel sem foco por view, posicionada por âncora e safe area, com espaço reservado enviado à engine, conferida no emulador em retrato, paisagem, cutout emulado e tela dividida, escondida quando a activity para e sem janelas vazadas quando a activity é recriada. O toque no botão do banner chega ao Lua e os outros toques chegam ao jogo.
- [x] Cobertura do app pelo nativo no Android (`coverApp` e `uncoverApp` contados por plugin, fechados quando a activity é destruída), conferida no emulador com o app inativo e parado.
- [x] `make.py` monta o Android com os plugins pelo `gradle.properties` (`haylen.plugins`, `haylen.gradlePlugins` e `haylen.placeholder.<nome>`): módulos incluídos pelo `settings.gradle.kts`, dependência do app em cada módulo, plugins Gradle no classpath por `buildscript` e aplicados com `apply(plugin = id)`, placeholders do manifesto vindos da configuração e os arquivos por app, conferido com `assembleDebug` de um plugin com meta-data, placeholder, dependência Maven e o google-services.
- [x] Correções da bridge no Android: o JNI anexa cada thread uma vez e solta no destrutor do `pthread_key_create`, e o parse do JSON sai da thread do frame. Custo na thread do frame com 100 chamadas por frame caiu de 10,5 a 11,8 µs para 2,0 a 2,2 µs por chamada, e a ida e volta continua de um frame. Os eventos nativos que chegam sem app rodando esperam no Java e vão para o próximo app.

**Apple**

- [x] `HaylenPlugin.h` no xcframework com o protocolo, `HaylenPluginContext`, `HaylenOverlay`, `HaylenOverlay.Item` e `HaylenPlacement`, com nullability e nomes Swift, e os helpers Swift do template (`context.register` com Codable, `HaylenFailure` e cancelamento, e `context.emit` com Encodable e retenção).
- [x] Descoberta pela chave `HaylenPlugins` do Info.plist dentro do `willFinishLaunching`, com a configuração com os padrões do `plugin.json`, a classe que falta num destino que o plugin não lista pulada em silêncio (o plugin só de iOS no Mac Catalyst e no tvOS) e os outros problemas no log.
- [x] Patch `sokol-apple-delegate.patch` (`sapp_desc.apple.delegate_class` e a configuração de cena com `[self class]`) e `HaylenSceneDelegate`, que repassa aos plugins o launch, as opções de conexão da cena (URLs, atividades e atalhos entregues como no app rodando), `openURLContexts`, `continueUserActivity`, o token e as notificações remotas com os handlers de conclusão chamados uma vez, as sessões de URL em segundo plano e o delegate do `UNUserNotificationCenter`, conferido no simulador com URL a frio e com o app rodando e com `simctl push`.
- [x] `HaylenOverlay` no UIKit e no macOS: view que deixa os toques passarem, acima da view do Sokol, com Auto Layout na safe area ou nas bordas e espaço reservado em pixels do framebuffer, conferida no simulador iOS (retrato, paisagem, segundo plano), no Mac Catalyst (janela redimensionada), no tvOS (o controle continua na UI do jogo) e no macOS.
- [x] Cobertura do app pelo nativo e erros do app para os plugins na Apple (`appDidFail(with:)` na fila principal), conferidos no simulador iOS.
- [x] `make.py` monta a Apple com os plugins: `plugins.json` incluído pelo `project.yml` com fontes, pacotes Swift, frameworks, recursos e scripts por alvo, com filtro que tira o Mac Catalyst dos plugins só de iOS, chaves do Info.plist e a lista `HaylenPlugins` juntadas, entitlements por plataforma no `App.xcconfig` e o projeto gerado de novo pelo XcodeGen quando os plugins pedem, conferido com swift-numerics em todas as plataformas e GoogleMobileAds só no iOS, nos builds do simulador iOS, do Mac Catalyst, do simulador tvOS e do macOS.
- [x] Repasse do delegate do macOS (`HaylenAppDelegate`): launch, `application:openURLs:`, registro de notificações remotas e o centro de notificações, conferido com URL a frio e com o app rodando.

**Web**

- [x] O loader importa o módulo JavaScript de cada plugin antes de o runtime começar, e cada módulo recebe o contexto (`Module.haylen.createPluginContext`) com `register`, `emit` com retenção, a configuração, o overlay, a cobertura e `onAppError`, com chamadas feitas no `load` guardadas até o runtime ficar pronto.
- [x] Camada de overlay de DOM por cima do canvas, que só recebe o ponteiro nos elementos dos plugins, com âncoras, safe area e espaço reservado, conferida no Chrome sem janela: o elemento recebe os próprios cliques e o canvas continua recebendo os outros, e a UI ancorada sai de baixo do espaço reservado.
- [x] `make.py` copia a pasta `web/` de cada plugin para `plugins/<id>/` do site e escreve a lista no `config.json`, e o loader importa os módulos durante o download e chama o `load(context)` de cada um em ordem antes de o app começar, mostrando o erro na página de carregamento quando um plugin falha.

**Desktop e apps C++**

- [x] A parte `native/` do plugin entra como biblioteca nativa do app, compilada e colocada como as da seção `native` do `app.json`, conferido com uma biblioteca CMake no macOS.
- [ ] Apps C++ com plugins no Android e na web pelos templates, e na Apple pelo `run-cpp`, com a mesma montagem.

**Desempenho**

- [ ] Benchmark da bridge no emulador Android, no simulador iOS e na web: ida e volta de uma chamada, chamadas por segundo e custo por frame de N chamadas e de N eventos, com os números na documentação.
- [x] Lote de eventos frequentes do nativo para o Lua e do Lua para o nativo, entregues uma vez por frame. Pronto com a marcação `batched` dos eventos, como o grupo AB descreve.

**Fora deste repositório**

- Os plugins de SDKs de terceiros (Google Sign-In, AdMob com UMP e ATT, Firebase com Analytics, Crashlytics, Messaging e Remote Config, compras com StoreKit 2 e Play Billing, Game Center e Play Games Services) ficam em repositórios próprios em https://github.com/haylen-org, feitos pelo dono depois, fora desta sessão. A pesquisa desses SDKs (versões, chaves, ids de teste, fluxos e requisitos por plataforma) orientou a arquitetura, e o login com Google do Tiny Island continua como está no sample.

**Sample, documentação, testes e regras**

- [x] Sample `samples/system/plugins` com o plugin de demonstração `plugins/native-demo/` (Swift com UIKit e AppKit, Kotlin, JavaScript e C), um teste por capacidade (chamadas na thread principal e em segundo plano, falha tipada, timeout e cancelamento, evento retido na carga, eventos do nativo, configuração com padrão, banner nativo no topo e na base com espaço reservado, toque no banner, tela nativa que cobre o app, seletor de arquivo, URL com o app rodando e a frio, erro do app entregue ao nativo e devolvido ao próximo app, e informações do plugin), validado no simulador iOS, no Mac Catalyst, no simulador tvOS, no app macOS, no emulador Android, na web (WebGPU e WebGL2) e no player de desktop, e no harness sem janela com a parte nativa ausente. O que a plataforma não tem responde `unsupported` (seletor de arquivo no tvOS, banner, cobertura e arquivo no player de desktop).
- [~] Guia `docs/plugins.md` (usar e escrever plugins: formato, Lua, Swift e Objective-C, Kotlin e Java, JavaScript, overlays, espaço reservado, cobertura, ciclo de vida, configuração, arquivos por app e testes), com `docs/platform_bridge.md`, `docs/distribution.md` e `docs/lua-api/platform.md` atualizados. Falta a parte do plugin de demonstração e do sample.
- [x] Testes GoogleTest da parte da engine: resolução do Lua dos plugins e colisão de nomes, configuração com padrões, handles e `send`, espaço reservado com a UI ancorada, app coberto, erros para o host, eventos retidos, `HaylenNativeApi.registerPlugin` e política de thread, com 950 testes passando, também sob ThreadSanitizer.
- [~] Regras dos plugins no CLAUDE.md (pacote, `plugin.json`, configuração, montagem e nomes). Falta completar com as APIs nativas quando o Android e a Apple ficarem prontos.
- [ ] Caminhos da Apple que o simulador não dispara: resultado combinado do fetch em segundo plano (o `simctl` recusa push silencioso), sessões de URL em segundo plano, atalhos, atividades, token do APNs, resposta de notificação tocada e notificações remotas no macOS, conferir num aparelho.

#### AD. Marca nova nas imagens da engine

- [x] Logo da engine (`templates/platform/web/haylen-logo.svg`, splash padrão da Apple, da web e do Android, e ícone das páginas web e do shell dos apps C++) trocada pelo símbolo novo.
- [x] Ícones com o símbolo sobre `#07112f`: ícone do app iOS (1024), ícones do macOS (16 a 1024, na grade de ícones do macOS), ícone em camadas e da loja do tvOS, launcher adaptativo do Android com camada monocromática, e o `logo.png` do app inicial.
- [x] Imagens largas com a logo horizontal de marca branca sobre `#07112f`: top shelf do tvOS e o banner da Android TV.
- [x] Splash do Android (`haylen_splash_logo` e `haylen_splash_icon`) com o símbolo dentro do círculo da máscara, e a documentação das logos e ícones padrão.

#### AE. Frases e expressões reservadas

- [~] Varredura das mensagens: erros e logs da engine (C++, Objective-C, Java, Kotlin, Swift, JavaScript e C), mensagens e saída do `make.py` e das ferramentas, e textos dos samples, sem frase começando em minúscula e com comandos, identificadores, caminhos, chaves e valores entre aspas duplas (inclusive os valores que hoje usam aspas simples), com os testes e os documentos que citam as mensagens atualizados.
- [~] Varredura dos documentos (`docs/`, READMEs dos samples e do plugin de demonstração, `README.md` e `PROJECT.md`): nenhuma frase começa com uma expressão em crase, e toda expressão reservada fica entre crases.
- [~] Varredura dos comentários de código (C++, Objective-C, Java, Kotlin, Swift, JavaScript, Lua, CMake e Python): toda expressão reservada entre crases e nenhuma frase começando em minúscula.
- A primeira passada cobriu a engine portátil, os desktops, a web, as ferramentas, os samples, os templates e 73 documentos, e trocou as aspas simples das mensagens por aspas duplas, com os testes. Faltam o `make.py`, o código do Android e da Apple, o `PROJECT.md` e os documentos que as ondas da Apple e do Android estavam editando.
- [x] O `CLAUDE.md` só cita versões e números de que uma regra depende (o C++20 e os 100% de cobertura), e a regra está nos princípios dele: versões de bibliotecas, ferramentas, SDKs e da engine, e limites, tamanhos, contagens e durações declarados no código ficam fora, com o arquivo que guarda cada valor.

#### AA. Informações do sistema e diálogos nativos

Decisões, a partir da pesquisa das bibliotecas de diálogos, notificações e webview e das APIs de cada plataforma:

- As bibliotecas de diálogos do GitHub não servem para a engine: bloqueiam a thread do frame (`runModal` no macOS, chamadas síncronas no Linux e no Windows) ou abrem processos externos. A engine usa as APIs de cada plataforma, e nada bloqueia o frame: cada diálogo termina de forma assíncrona e entrega o resultado na thread do frame, como a bridge.
- macOS: `NSAlert` e `NSOpenPanel` e `NSSavePanel` como sheets da janela do app. Windows: uma thread de diálogos com COM de thread única, dona da janela do app, com `TaskDialogIndirect` (botões próprios, que pede o manifesto dos Common Controls 6) e `IFileOpenDialog` e `IFileSaveDialog`. Linux: GTK 3 carregado em runtime (`GtkMessageDialog` e `GtkFileChooserNative`, que usa o portal do desktop quando existe), com o loop do GLib avançado a cada frame, e `unsupported` com a explicação quando o GTK 3 não existe. iOS, iPadOS e Mac Catalyst: `UIAlertController` e `UIDocumentPickerViewController`. tvOS: só a caixa de mensagem. Android: `AlertDialog` (até três botões) e o Storage Access Framework. Web: um diálogo de DOM na camada de overlay, `<input type=file>` e o download de um blob.
- Arquivos escolhidos voltam como caminhos que o app lê com o `fs` do Varn em todas as plataformas: o caminho real nos desktops e uma cópia numa pasta temporária do app no mobile e na web. Salvar grava os dados que o app passa no destino que o usuário escolheu, em todas as plataformas.
- As informações do sistema substituem os métodos embutidos da bridge (`device.info`, `system.locale`, `system.openUrl`, `haptics.vibrate`, `engine.info` e `app.version`), que saem da bridge em todas as plataformas, sem aliases. A bridge fica só para métodos dos apps e dos plugins.

**Informações do sistema (`haylen.system`)**

- [x] Parte da engine: `platform::System` (`engine.getSystem()`) e `platform::Dialogs` (`engine.getDialogs()`) com `SystemInfo`, `Theme`, `Battery`, `DialogRequest` e `DialogResult`, os métodos do host (`getSystemInfo`, `getTheme`, `getBattery`, `openUrl`, `vibrate`, `showDialog` com a pasta temporária de cada diálogo e `cancelDialog`), o `SystemState` seguro entre threads para tema e bateria, os eventos `systemThemeChanged` e `batteryChanged`, o nome da GPU pelo `graphics::Device::getAdapterName()` (Metal, D3D11, GL e WebGPU), os módulos `haylen.system` e `haylen.dialogs`, o host sem janela com os diálogos gravados e os valores ajustáveis pelos testes, 16 testes novos, as páginas `docs/lua-api/system.md` e `dialogs.md`, e os métodos embutidos removidos da bridge em todas as plataformas (`DesktopMethods` saiu, e `WindowsMethods` e `LinuxMethods` viraram `WindowsSystem` e `LinuxSystem`). As plataformas têm stubs: os diálogos respondem `unsupported` até cada plataforma ser implementada.
- [x] `system.info()` com os dados fixos, lidos uma vez na abertura: sistema (`macOS`, `Windows`, `Linux`, `iOS`, `iPadOS`, `tvOS`, `Android`, `Web`), versão do sistema, modelo e fabricante do aparelho, tipo do aparelho (`desktop`, `phone`, `tablet`, `tv`, `browser`), nome do processador e núcleos, memória total, nome da GPU que o Sokol criou, idioma, lista de idiomas preferidos e fuso horário. Prontos na web, no Windows e no Linux. Prontos também na Apple (macOS, iOS, iPadOS, Mac Catalyst e tvOS), conferidos nos simuladores e no app macOS. O Android também está pronto, conferido no emulador.
- [x] `system.theme()` com `'light'` ou `'dark'` e o evento `systemThemeChanged` quando o usuário troca, em todas as plataformas (KVO da aparência no macOS, `traitCollection` no iOS, `uiMode` no Android, `WM_SETTINGCHANGE` no Windows, o portal de configurações do desktop pelo GIO no Linux e `matchMedia` na web). Prontos na web, no Windows e no Linux. Prontos também na Apple (macOS, iOS, iPadOS, Mac Catalyst e tvOS), conferidos nos simuladores e no app macOS. O Android também está pronto, conferido no emulador.
- [x] `system.battery()` com o nível de 0 a 1, se está carregando e o estado (`unknown`, `charging`, `discharging`, `full`, `none`), e o evento `batteryChanged`, onde a plataforma informa (IOKit no macOS, `UIDevice` no iOS, `BatteryManager` no Android, `GetSystemPowerStatus` no Windows, `/sys/class/power_supply` no Linux e a Battery Status API na web onde existe). Prontos na web, no Windows e no Linux, onde o Linux lê as baterias a cada 30 segundos numa thread própria. Prontos também na Apple (macOS, iOS, iPadOS, Mac Catalyst e tvOS), conferidos nos simuladores e no app macOS. O Android também está pronto, conferido no emulador.
- [x] `system.openUrl(url)` e `system.vibrate(seconds)` como APIs da engine, no lugar dos métodos embutidos da bridge. Prontos em todas as plataformas, com as permissões conferidas no Android.
- [x] O nome da GPU vem do dispositivo que a engine criou (`graphics::Device`): o `MTLDevice` no Metal, o adaptador DXGI no D3D11, o `GL_RENDERER` no OpenGL e as informações do adaptador no WebGPU. Na web, o WebGL2 lê o `WEBGL_debug_renderer_info` e o WebGPU lê as informações do adaptador.
- [x] Os métodos embutidos da bridge saem do Android, da Apple, da web, do Windows e do Linux, e os samples (plataforma, localização e eventos) e o Tiny Island passam a usar `haylen.system`.

**Diálogos (`haylen.dialogs`)**

- [x] `dialogs.message({title, text, kind, buttons})` com `kind` `'info'`, `'warning'` ou `'error'` e de um a três botões com texto próprio, que devolve o índice do botão escolhido, ou `nil` quando o usuário fecha sem escolher.
- [x] `dialogs.openFiles({title, filters, multiple})` com filtros por nome e extensões, que devolve a lista de arquivos (`name` e `path`) ou `nil` quando o usuário cancela.
- [x] `dialogs.saveFile({title, filters, name, data})`, que grava os dados no destino escolhido e devolve o nome e, onde existe, o caminho, ou `nil` quando o usuário cancela.
- [x] `dialogs.openFolder({title})`, que devolve o caminho da pasta nos desktops, no iOS e no Mac Catalyst, e falha com `unsupported` onde a plataforma não dá um caminho (Android, web e tvOS).
- [x] Cada chamada devolve uma chamada com `:await()`, `cancel()` e timeout, como a bridge, e falha com `unsupported` e uma mensagem clara onde a plataforma não tem o diálogo.
- [x] Implementações: macOS com sheets, Windows com a thread de diálogos, Linux com o GTK 3 carregado em runtime, iOS e Mac Catalyst com `UIAlertController` e `UIDocumentPickerViewController`, tvOS com `UIAlertController`, Android com `AlertDialog` e o Storage Access Framework (cópia dos arquivos para a pasta temporária e gravação pelo `ContentResolver`) e web com o diálogo de DOM, o `<input type=file>` e o download. Prontos: a web (conferida no Chrome sem janela, com WebGPU e WebGL2), o Windows e o Linux, compilados só pelo CI. A Apple também está pronta: sheets no macOS, `UIAlertController` e `UIDocumentPickerViewController` no iOS, no iPadOS e no Mac Catalyst e só a mensagem no tvOS, conferidos nos simuladores, no Mac Catalyst e no app macOS. O Android também está pronto, conferido no emulador. Na web, um seletor sem ativação do usuário falha com o código `failed`, porque o contrato não tem um código próprio para isso.

**Correções de plataforma achadas na revisão**

- [ ] macOS mínimo 14.0: o Sokol chama `-[NSView displayLinkWithTarget:selector:]` sem conferir a versão, uma API do macOS 14, então o app não abre no macOS 13. Subir o mínimo do macOS no `make.py`, no template e no CMake, e registrar na documentação. O Mac Catalyst usa o caminho do UIKit e fica como está.
- [x] Manifesto de aplicativo no Windows (Common Controls 6, que o `TaskDialogIndirect` pede) no player e nos apps do `haylen_add_app`. O arquivo `engine/platform/windows/haylen.manifest` escolhe os Common Controls 6 e o UTF-8 como página de código, e entra no player, nos apps do `haylen_add_app` e no SDK.

**Testes, samples e documentação**

- [x] Testes GoogleTest no host sem janela: pedidos de diálogo gravados e respondidos pelo teste, validação das opções, resultados, cancelamento e timeout, `unsupported`, informações do sistema, tema e bateria com os eventos, e os bindings Lua.
- [x] Sample `samples/system/system-info` (ou um teste no sample de plataforma) com as informações, o tema e a bateria ao vivo, e um sample ou testes de diálogos com mensagem, abrir arquivos, salvar e pasta, validados no macOS, no simulador iOS, no Mac Catalyst, no simulador tvOS, no emulador Android e na web. Windows e Linux compilados e testados no CI. Os testes "System" e "Dialogs" do sample de plataforma foram conferidos no macOS, nos simuladores, no Mac Catalyst, no emulador Android e na web.
- [x] Páginas `docs/lua-api/system.md` e `docs/lua-api/dialogs.md` completas com exemplos, `docs/platform_bridge.md` sem os métodos embutidos e as regras no CLAUDE.md.

**Para os plugins futuros, fora desta etapa**

- Notificações e webview ficam para plugins. Quando o webview for feito, a engine precisa de posição por retângulo em unidades de design nos overlays, de uma camada de overlay no Windows e no Linux, do COM de thread única na thread do frame do Windows (o miniaudio deixa multithread) e de janelas transparentes do Windows que não escondam janelas filhas.

#### AB. Dados dos plugins: binários, streams e permissões

Os plugins de câmera, foto, microfone, áudio, localização e notificações moram fora deste repositório, mas a engine precisa dar a eles o caminho para mandar e receber esses dados de forma assíncrona e eficiente em todas as plataformas. O que já existe cobre parte disso: chamadas assíncronas com resposta na thread do frame, eventos, eventos guardados até o Lua escutar (a notificação que abriu o app e o push que chegou com o app fechado), o repasse do ciclo de vida e dos resultados de permissão e as views nativas por cima do jogo. Faltam os itens abaixo.

- [x] Dados binários na bridge: chamadas, respostas, eventos e eventos retidos levam buffers junto do JSON pela referência `{"$bytes": N}`, sem base64, no C++ (`Bridge::Payload`), no Lua (`platform.bytes`), na `HaylenNativeApi` versão 4, no JavaScript (`Uint8Array` e `ArrayBuffer`), no Java e Kotlin (`byte[]` e `ByteBuffer` direto sem cópia no Java) e no Objective-C e Swift (`NSData`), com o código `invalidBytes` para uma referência a um buffer que não existe, e ida e volta conferida no player de desktop (C), no app macOS e no simulador iOS (Swift), no Chrome com WebGL2 e WebGPU (JS) e no emulador Android (Kotlin).
- [x] Bytes viram recursos da engine sem passar por arquivo: textura de bytes codificados e de pixels RGBA8 crus (`graphics.newTexture(bytes, options)`) e som de bytes codificados ou de amostras float32 e int16 cruas (`audio.newSound(bytes, options)`), de strings binárias do Lua e de spans do C++, com testes.
- [x] Stream de vídeo do nativo para a engine (`platform::VideoStream`, `handle:videoStream(name)` com `texture`, `width`, `height`, `frameCount`, `timestamp` e o evento `frame`): o quadro mais novo, empurrado de qualquer thread, sobe no máximo uma vez por frame, BGRA vira RGBA na thread de quem produz, e o tamanho muda no lugar. Prontos no C++, no Lua, na API C e na web (`context.videoStream(name).push(source)` com `ImageBitmap`, `VideoFrame`, `<video>` ou `<canvas>`). Na Apple, `openVideoStream` e `openAudioStream` do contexto dos plugins (com `CVPixelBuffer` no vídeo). No Android, `HaylenVideoStream` (com `Bitmap` ou `ByteBuffer`) e `HaylenAudioStream`, pelo JNI.
- [x] Stream de áudio do nativo para a engine (`platform::AudioStream`, `handle:audioStream(name)` com `play`, `read`, `sampleRate`, `channels` e `underruns`): buffer circular sem trava com um produtor, tocado como voz do mixer com reamostragem, silêncio e contagem quando falta amostra, e leitura das amostras mais novas, no C++, no Lua, na API C e na web. Na Apple, `openVideoStream` e `openAudioStream` do contexto dos plugins (com `CVPixelBuffer` no vídeo). No Android, `HaylenVideoStream` (com `Bitmap` ou `ByteBuffer`) e `HaylenAudioStream`, pelo JNI. A escuta do barramento master para gravar ficou de fora: o produtor seria a thread de áudio em tempo real, que não pode pegar a trava do histórico do stream, e fazer isso direito pede outro tipo de buffer, um nó de passagem antes da saída e entradas novas na API C.
- [x] Eventos frequentes em lote: quem emite marca o evento como `batched`, e todos os emits desse nome num frame chegam ao Lua como uma lista, em ordem, com buffers e retenção, no C++, no C, no Java, no Objective-C, no Swift, no JavaScript e no Lua, com testes (3000 eventos chegaram em 30 a 38 listas nos aparelhos).
- [~] Permissões em tempo de execução pelos plugins, conferidas de ponta a ponta: Android (`requestPermissions` na activity e o resultado pelo repasse do `onRequestPermissionsResult`), Apple (as APIs de cada framework, com as descrições de uso no `infoPlist` do `plugin.json`), web (`navigator.permissions` e o gesto do usuário) e desktops, documentado no guia de plugins. Na Apple, o plugin de demonstração pede a câmera e as notificações, com a descrição de uso no `infoPlist`, conferido no simulador. No Android, `RequestPermission` do Activity Result API, conferido no emulador. Faltam a web e os desktops.
- [~] Notificações e push nos plugins: o que um plugin de notificações precisa da engine (delegate de notificações na Apple, `onNewIntent` e serviços no Android, service worker na web, eventos guardados com o app fechado), conferido pelo plugin de demonstração com uma notificação local de verdade em cada plataforma que permite, e o toque nela chegando ao Lua, inclusive com o app fechado. Na Apple, uma notificação local do plugin de demonstração, com o toque chegando ao Lua com o app rodando e a frio, conferida no simulador. No Android, a notificação local passa pelo `HaylenLinkActivity` da biblioteca `haylen-links`, conferida no emulador com o app rodando, fechado e com o processo morto. Faltam a web e os desktops.
- [ ] O plugin de demonstração testa cada mecanismo: um resultado binário (uma imagem gerada no nativo e desenhada no Lua), um stream de vídeo gerado no nativo (um padrão animado), um stream de áudio gerado no nativo (um tom), eventos em lote, um pedido de permissão real e uma notificação local tocada, em cada plataforma.
- [ ] Guia `docs/plugins.md` com a parte de dados binários, streams, lotes, permissões e notificações, e as regras no CLAUDE.md.

#### AC. Telas de plugins e o host do Android

Decisões, a partir da pesquisa dos SDKs que abrem telas próprias (RevenueCat, Stripe, BiometricPrompt, Facebook Login, FirebaseUI, Photo Picker, Play Billing, Credential Manager, anúncios de tela cheia, consentimento, In-App Review e Updates) e das APIs de cada plataforma:

- O `HaylenActivity` não pode ser uma `ComponentActivity`, porque o `NativeActivity` estende o `Activity` simples. O paywall do RevenueCat (`PaywallActivityLauncher`), o Stripe PaymentSheet, o BiometricPrompt, o Chrome Auth Tab, o FirebaseUI e o Photo Picker exigem `ComponentActivity` ou `FragmentActivity`, e SDKs que desenham nas views da activity (mensagens in-app, prévia do CameraX e folhas em Compose) não aparecem, porque o `NativeActivity` toma a superfície da janela. O host do Android passa para o GameActivity do AndroidX (`AppCompatActivity`, portanto `FragmentActivity` e `ComponentActivity`), que desenha num `SurfaceView` da hierarquia de views, com um patch próprio do backend Android do Sokol, que não tem suporte ao GameActivity.
- Uma API de telas de plugins em todas as plataformas: o plugin registra uma tela no nativo, o Lua abre com `handle:openScreen(nome, parâmetros, {state = ...})`, a engine cobre o app antes da tela aparecer (e para de desenhar quando a tela é opaca) e descobre quando ela fecha, o resultado resolve a chamada, e quando a chamada não existe mais (processo morto, activity recriada ou redirecionamento na web) o resultado chega como o evento retido `<id>.screenRestored` com o estado que o app salvou.

**Android**

- [x] Spike da migração, num projeto de teste no emulador: o GameActivity 4.4.2 liga com a engine em `c++_static` pela biblioteca estática pronta do AAR (sem depender de `libc++_shared.so`), o Sokol desenha com o patch (606 linhas mudadas, com a thread de render mantida e a entrada vinda da thread principal numa fila), toque com vários dedos, mouse, caneta, teclas e controle chegam, o teclado de software funciona pelo GameTextInput, um botão comum por cima do `SurfaceView` recebe o próprio toque e o jogo recebe o resto, o resultado de outra activity chega com o contexto EGL e os recursos intactos, rotação, Home e volta funcionam, e depois de o processo morrer coberto o estado nativo, o estado do AndroidX e o resultado pendente voltam. O gesto de voltar deslizando falta conferir num aparelho.
- [x] Migração do `HaylenActivity` para o GameActivity: patch do backend Android do Sokol (callbacks do GameActivity, fila de entrada da thread principal para a thread de render), entrada da engine, o host Java (splash, insets, back preditivo, foco de áudio, controles e rede), o overlay com views comuns sobre o `SurfaceView` no lugar das janelas de painel, o teclado de software sem o `EditText` escondido quando o GameTextInput servir, o tema AppCompat, o build e a documentação. Pontos que o spike mostrou: o GameActivity devolve o foco ao `SurfaceView` quando a janela recupera o foco (a subclasse do `SurfaceView` recusa o foco enquanto um campo por cima edita), os eixos do controle só chegam com o foco no `SurfaceView` (o `HaylenActivity` passa os eventos de joystick ao GameActivity primeiro), a soltura de uma tecla cuja pressão um campo consumiu é descartada, o voltar vai todo pelo `OnBackPressedCallback`, a visibilidade do teclado passa pela fila, e os resultados que chegam antes do Lua ficam guardados.
- [x] O ritmo dos frames pelo Choreographer é escolhido em tempo de execução, nos aparelhos com API 29 em diante, e não fica fora do build pelo mínimo 27.
- [x] Coberto por outra activity, o app pode ser congelado pelo Android: uma tela que é activity do próprio app mantém o processo em primeiro plano, e uma tela de outro app (o seletor de documentos) deixa o app congelado depois de pouco mais de um minuto. Os frames, e com eles os timers do Lua, param nos dois casos, como o `docs/lifecycle.md` explica.
- [x] Modo de abertura `singleTop` no lugar do `singleTask`, com uma activity pequena que recebe links e notificações e entrega ao jogo, para o ícone do launcher não destruir as telas de compra, paywall, verificação bancária e login abertas por cima do jogo.
- [x] Telas de plugins no Android sobre o `ActivityResultRegistry` com chaves de texto, sem o repasse manual do `onActivityResult` e sem colisão de request codes, e o resultado guardado quando o processo morre.
- [x] O Android roda os frames enquanto a activity está retomada e tem superfície, mesmo sem o foco da janela, então sob um diálogo, uma mensagem ou a gaveta de notificações os timers, o `cancel()` e os timeouts do Lua agem e o app aparece sob o diálogo quando volta. O app fica `inactive` sem o foco, e um frame só troca os buffers quando desenhou. Conferido no emulador.
- [ ] Telas que são activities (seletores de documentos, pedidos de permissão e telas de plugins em activity própria) pausam o app, e os frames param até elas fecharem. Decidir se o laço de eventos roda com o app pausado e sem superfície, para `cancel()` e timeouts agirem sobre elas.
- [ ] Um stick de controle solto enquanto outra janela tem o foco fica com o último valor, porque o Android só manda os eixos para a janela com foco: zerar os eixos quando o app perde o foco.
- [ ] Uma tela de plugin aberta logo na resposta de um diálogo pode falhar com `notActive`, porque o app só volta a `active` um ou dois frames depois de o foco voltar. Decidir se a abertura espera o app ficar ativo.

**Apple**

- [x] `context.viewController` devolve o view controller mais alto já apresentado, para um plugin apresentar por cima do que já está na tela.
- [x] Telas de plugins com apresentação e fim detectados (conclusão, `UIAdaptivePresentationControllerDelegate` para o gesto de fechar e o fim programático), com o app coberto e sem desenhar enquanto a tela opaca aparece, também para `UIHostingController` de SDKs em SwiftUI.
- [x] Mac Catalyst: plugins abrindo janelas novas sem criar uma segunda janela do Sokol, porque o delegate da engine vale para toda cena nova.

**Web e desktops**

- [x] Web: telas de plugins por popup (`window.open` dentro da ativação do usuário, `popupBlocked` fora dela, resultado por `postMessage` ou pelo `BroadcastChannel` `haylen-screens`) e por redirecionamento (estado salvo no `sessionStorage` e `screenRestored` depois de a página voltar), conferidas no Chrome sem janela, e o `make.py serve` sem o `Cross-Origin-Opener-Policy` por padrão, com `--coop` para páginas que querem isolamento.
- [~] Desktops: a `HaylenNativeApi` expõe a janela do app (`getWindow`) e a cobertura, e a biblioteca do plugin de demonstração abre uma sheet no macOS (conferida no player: resultado, `busy`, sem desenhar por baixo, cancelamento e restauração), uma janela com dono no Windows e uma janela X11 transitória com conexão própria no Linux. Falta compilar e rodar no Windows e no Linux.

**Engine, testes e documentação**

- [x] API de telas de plugins na engine: `handle:openScreen(nome, parâmetros, {state, opaque, timeout})` e `platform.screenShowing()` no Lua, `platform::Screens` (`engine.getScreens()`) no C++, uma tela por vez (`busy`), só com o app ativo (`notActive`), app coberto antes da plataforma abrir a tela e sem desenhar sob uma tela opaca, cancelamento e timeout que mantêm o app coberto até a plataforma confirmar que a tela fechou, e a tela sobrevivendo ao reinício do app com o evento retido `<id>.screenRestored` (`screen`, `state` e `result` ou `error`), com testes e ThreadSanitizer. A API C versão 5 traz o registro de telas, a janela nativa e a cobertura.
- [x] `docs/lifecycle.md` corrigido: as respostas da bridge, os timers e os callbacks de rede esperam enquanto a plataforma não roda frames (Android pausado ou sem foco, iOS e tvOS inativos, aba escondida no navegador).
- [x] O plugin de demonstração abre uma tela nativa de verdade em cada plataforma (uma activity AndroidX com resultado, um view controller e uma tela em SwiftUI, um popup na web e uma janela no desktop) e recebe o resultado, inclusive depois de o processo morrer no Android. Prontos: a Apple (uma tela em UIKit, uma em SwiftUI e uma sheet no macOS), a web e o desktop. No Android, uma activity do AndroidX, com o resultado chegando depois de o processo morrer, conferido no emulador.
- [x] Guia `docs/plugins.md` com as telas de plugins e as regras no CLAUDE.md.

#### AF. Projetos de plataforma do desenvolvedor e requisitos dos plugins

A auditoria do que a engine e o `make.py` impõem hoje mostrou três problemas:

- **O que todo app recebe à força:**
  - cinco frameworks particulares da Apple (UserNotifications, Network, IOKit, UniformTypeIdentifiers e CoreVideo), porque a biblioteca estática não liga nada sozinha;
  - três permissões no AAR (`INTERNET`, `ACCESS_NETWORK_STATE` e `VIBRATE`);
  - o `HaylenPluginProvider` e o `HaylenLinkActivity` exportado sem filtro, que repassa qualquer intent explícito aos plugins;
  - o `enableOnBackInvokedCallback` do app inteiro;
  - o `kotlinx-coroutines`;
  - declarações que são da empresa, como `ITSAppUsesNonExemptEncryption`.
- **O `make.py` apaga o que o desenvolvedor faz:**
  - reescreve o `Info.plist`, o `App.xcconfig`, os entitlements e o splash a cada execução;
  - gera o `App.xcodeproj` de novo;
  - copia arquivos de plugins por cima do projeto;
  - aplica a pasta do app por cima do template novo, então arquivo apagado volta e arquivos de versões diferentes se misturam.
- **Requisito que falta quebra o app:** na Apple é erro de link. No Android, o app provavelmente fecha na abertura sem `ACCESS_NETWORK_STATE`, e fica uma exceção Java pendente na thread do frame sem `VIBRATE`, porque o JNI não confere exceções.

Decisões:

- **O projeto é do desenvolvedor:**
  - `platform/<template>/` do app é o projeto inteiro, compilado no lugar, sem cópia por cima de template. Um app sem essa pasta usa uma cópia do template que o `make.py` guarda em `build/apps/`.
  - O `make.py new` e o `make.py platform add` criam os projetos, e o `make.py platform diff` mostra, sem mudar nada, o que o template atual tem de diferente, para o desenvolvedor adotar o que quiser.
- **Só a pasta gerada é escrita:**
  - O `make.py` escreve só na pasta `haylen/` dentro do projeto, que o `.gitignore` do projeto ignora. Ele nunca edita `project.yml`, `project.pbxproj`, scripts do Gradle, manifestos nem `Info.plist` do desenvolvedor.
  - Os arquivos do desenvolvedor incluem o que é gerado por poucas linhas visíveis, que ele pode tirar:
    - na Apple, `include: [haylen/project.yml]` e `templates: [HaylenIOS]` em cada alvo do `project.yml` (conferido com o XcodeGen fixado: listas do template somam com as do alvo, e o ajuste do alvo vence o do template), e `#include "haylen/Haylen.xcconfig"` no `App.xcconfig`;
    - no Android, o `haylen/haylen.properties` lido pelos scripts do Gradle e as pastas `haylen/assets`, `haylen/res` e `haylen/jniLibs` como fontes extras.
- **Gerar o projeto de novo:**
  - O desenvolvedor edita o `project.yml` e gera de novo com `make.py xcodegen <app>` ou com o XcodeGen fixado.
  - O `make.py run` só gera sozinho quando o `project.yml` inclui a pasta gerada, as entradas mudaram e o `project.pbxproj` não foi editado à mão desde a última geração. Se foi editado, ele para com uma mensagem que explica o que fazer, sem sobrescrever nada.
- **O `Info.plist` e os entitlements são do desenvolvedor:**
  - O `make.py` grava em `haylen/` uma cópia completada: o valor do desenvolvedor vence, chaves dos plugins e do `app.json` só preenchem o que falta, listas ganham os itens que faltam, e dois plugins que discordam param o build com os dois nomes.
  - O desenvolvedor pode apontar o alvo direto para o arquivo dele e deixar de receber o que é gerado.
  - A lista `HaylenPlugins` sai do `Info.plist`: o runtime lê os plugins do pacote e avisa no log quando a classe de um plugin listado não está no app.
- **Requisitos dos plugins em três camadas:**
  - Primeiro, a inclusão aditiva pela pasta gerada: frameworks, pacotes Swift, fontes, recursos e scripts na Apple, e módulos com o manifesto mesclado pelo Gradle no Android.
  - Depois, o `make.py check`, que confere o app compilado e mostra, para cada requisito que falta, quem precisa dele, por quê e o trecho exato com o arquivo onde colocar. Na Apple ele lê o `Info.plist`, os frameworks ligados e os entitlements. No Android, o manifesto final do APK. O `run` roda a mesma conferência como aviso.
  - Por último, a checagem em tempo de execução.
- **Em tempo de execução, loga e ignora:**
  - Cada recurso confere o requisito antes de chamar a API do sistema, o que na Apple evita o encerramento pelo sistema quando falta a descrição de uso.
  - Quando o requisito falta, o recurso registra no log uma vez o que falta e como resolver, e a chamada falha com `unsupported` e `data.missing`. Recursos sem resposta, como vibrar, só não fazem nada.
  - Os plugins têm os mesmos helpers: `HaylenRequirements` na Apple e no Android e `context.require` na web.
- **Núcleo mínimo:**
  - A engine só leva o que todo app precisa para rodar.
  - Na Apple, a lista de frameworks do núcleo sai do `project.yml` do desenvolvedor e vem da engine pela pasta gerada. O UserNotifications deixa de ser ligado: o runtime acha o centro de notificações em tempo de execução, e os métodos de notificação vão para um `HaylenNotificationPlugin.h` que só os plugins de notificação importam.
  - No Android, as três permissões saem do AAR e ficam como padrão visível no manifesto do template, que o desenvolvedor tira quando não quer. Vibrar, o estado da rede e a rede passam a conferir a permissão. O provider dos plugins vai para o artefato `dev.haylen:haylen-plugins`, de que todo módulo de plugin depende. O `HaylenLinkActivity` vai para o `dev.haylen:haylen-links`, só dos plugins de links e notificações. O `kotlinx-coroutines` vai para o `dev.haylen:haylen-coroutines`. O `enableOnBackInvokedCallback` vai para o manifesto do template.
  - Os valores que são escolhas da empresa, como criptografia, controles, barra de status, TV, SDKs e assinatura, ficam só como padrão do template, nunca forçados pelo `make.py`.
- **A engine não se divide em módulos C++ opcionais para o player Lua:**
  - os módulos 2D opcionais são cerca de 9% do código;
  - o player web precisa rodar qualquer app;
  - o Android pronto não tem passo de link por app.

  Opções de CMake por módulo para apps C++ ficam para quando alguém pedir.

Checklist:

- [x] O JNI confere exceções depois de cada chamada ao Java, descreve no logcat, limpa e registra um erro da engine com o nome do método, conferido no emulador com o CheckJNI: a `SecurityException` aparece, é limpa e o app segue.
- [x] Sem `ACCESS_NETWORK_STATE`, o estado da rede não é registrado e não há `networkChanged`. Sem `VIBRATE`, vibrar só registra no log. Sem `INTERNET`, os erros de rede da engine terminam dizendo a permissão que falta e como declarar. Conferido no emulador com as três permissões tiradas, com um log só por falta e sem o app fechar.
- [x] Helpers de requisitos para os plugins:
  - Apple, pronto: `HaylenRequirements` com chave do `Info.plist`, descrição de uso, modo de segundo plano, esquema de URL, classe e entitlement no macOS e no Mac Catalyst, mais o `context.require` em Swift, conferidos no simulador iOS, no Mac Catalyst, no simulador tvOS e no app macOS.
  - Android, pronto: `HaylenRequirements` com permissão declarada e concedida, classe, meta-data, activity, service, provider e esquema, pelo `context.requirements()`, testado pelo teste "Requirements" do sample de plugins no emulador.
  - Web, pronto: `context.require` com contexto seguro, API e política de permissões, conferido no Chrome sem janela com WebGPU e WebGL2.
  - Nas três, `unsupported` com `data.missing` e log uma vez só, documentados no guia de plugins.
- [x] UserNotifications fora do núcleo: o centro de notificações é achado em tempo de execução, sem `class_addProtocol`, os métodos de notificação ficam no `HaylenNotificationPlugin.h`, o framework saiu do template e do `haylen-app.cmake`, e o plugin de demonstração declara os frameworks que usa. O `otool -L` de um app sem plugins não mostra o UserNotifications, e o toque numa notificação chega ao Lua com o app rodando e a frio no simulador.
- [x] O runtime da Apple lê os plugins do pacote pela ordem de carga do `PluginLoadOrder`, sem a chave `HaylenPlugins`, e avisa quando a classe de um plugin listado falta. O `make.py` não repete no `plugins.json` um framework que o alvo do `project.yml` já liga, porque o XcodeGen recusa dependência duplicada.
- [x] AAR mínimo: sem permissões, sem provider, sem activity de links e sem coroutines. Os artefatos `haylen-plugins`, `haylen-links` e `haylen-coroutines` são publicados pelo `make.py engine`, e o template de plugin e o plugin de demonstração dependem deles. O template do Android não repete as versões do AndroidX e declara as permissões de rede e de vibração no próprio manifesto. Conferido com `aapt2`: o AAR não traz permissões nem componentes, e um app sem plugins só exporta o launcher e o `ProfileInstallReceiver` do AndroidX, protegido pela permissão `DUMP`. O build Release com R8 carrega os plugins.
- [ ] Projeto no lugar e pasta `haylen/` gerada:
  - Apple: `haylen/project.yml` com os modelos de alvo, `Haylen.xcconfig`, `Info.plist` e entitlements completados, `Splash.xcassets`, pacote, bibliotecas nativas e plugins, com os produtos do build fora da pasta do projeto.
  - Android: `haylen.properties`, `assets`, `res`, `jniLibs` e plugins.
  - Web: os arquivos do desenvolvedor copiados como estão para a saída, com os gerados ao lado.
  - Os templates são reescritos para esse modelo, com `.gitignore`, e o `App.xcodeproj` do template é gerado de novo.
- [ ] Comandos `make.py prepare`, `xcodegen`, `check`, `platform add` e `platform diff`, com a proteção do `project.pbxproj` editado à mão.
- [~] O `Info.plist` com `UIApplicationSupportsMultipleScenes` verdadeiro no iOS, conferido no iPad e no Mac Catalyst, onde a tela SwiftUI de um plugin abre numa janela própria. No Mac Catalyst essa janela continua visível depois de um cancelamento ou de um timeout da tela, o que falta corrigir.
- [ ] Manifesto de privacidade: auditoria das APIs de motivo obrigatório que a engine e as dependências usam, o arquivo da engine publicado com os artefatos, a seção `apple.privacy` do `plugin.json`, e o `PrivacyInfo.xcprivacy` gerado com a engine, os plugins e o arquivo do desenvolvedor.
- [ ] O código nativo dos samples (plataforma, nativo e o login do Tiny Island) vira plugins locais em `plugins/` de cada sample, e nenhum sample guarda projeto de plataforma, então todos usam a cópia do template.
- [ ] Conferência de ponta a ponta:
  - Com as linhas de inclusão: o simulador iOS, o Mac Catalyst, o simulador tvOS, o macOS, o emulador Android e a web.
  - Sem elas: o `check` mostra os trechos, e o app abre e responde `unsupported` em vez de fechar.
  - Com edições do desenvolvedor: chave própria no `Info.plist`, número de build no `App.xcconfig`, framework e alvo mudados no `project.yml`, `targetSdk`, assinatura e sabor no Gradle, e arquivos apagados que não voltam.
- [ ] Decisões 14.1 (templates, montagem e `make.py new`), `docs/distribution.md`, `docs/plugins.md`, `docs/build.md`, `docs/embedding.md`, `docs/lua-api/system.md` e o CLAUDE.md descrevendo a propriedade dos projetos e os requisitos.
