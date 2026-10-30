SQLTools - Histórico de alterações
=================================


3.5.1
-----
Data: 2026-10-30

  Melhorias
    * Tunnel SSH: o botão "Test" avisa quando o servidor só aceita algoritmos de
      criptografia mais antigos e menos seguros.

  Correções de bugs
    * Tunnel SSH: servidores com SSH mais antigo não falham mais com "Unable to
      exchange encryption keys". O SQLTools tenta primeiro os algoritmos modernos
      e, só se o servidor não aceitar nenhum, tenta os antigos automaticamente. A
      chave do servidor continua sendo verificada.
    * Tunnel SSH: corrigida uma corrupção de memória ao entrar com chave privada
      RSA (um bug da biblioteca de SSH, agora corrigido).


3.5.0
-----
Data: 2026-10-29

  Novas funcionalidades
    * Tunnel SSH: conecte em bancos que só são acessíveis por um servidor SSH, sem
      precisar do PuTTY. Na janela Connect, clique em "SSH Tunnel..." e informe o
      host, a porta e o usuário SSH; depois escolha senha ou chave privada
      (formato OpenSSH, com passphrase opcional). O botão "Test" confere os dados
      do SSH antes de conectar.
    * A chave do servidor SSH é verificada: o fingerprint de um servidor
      desconhecido é exibido para confirmação, e uma chave alterada aborta a
      conexão. Só algoritmos modernos são aceitos, a porta local fica aberta
      apenas para o SQLTools e a senha SSH é salva criptografada, como a senha do
      banco.
    * Se a conexão SSH cair, o Reconnect recria o tunnel automaticamente.

  Melhorias
    * A barra de conexão mostra o host real do banco seguido de (SSH) quando há
      tunnel.
    * Quando o nome da conexão não cabe na barra, um tooltip com o texto completo
      aparece depois que o mouse fica parado sobre ela.


3.4.0
-----
Data: 2026-10-28
  * View > Dark Theme agora troca o tema na hora, sem reiniciar o SQLTools. As
    janelas abertas, barras de ferramentas, ícones, editor, grades e painéis
    acompanham a troca.
  * Object Viewer: visual moderno. Ícones novos, fonte e cores do tema, seleção
    em linha inteira e combo no tema escuro. O comportamento não muda.
  * What's New: a barra de rolagem segue o tema escuro (também em outros campos
    de texto multilinha).
  * Copiar como > SQL Inserts agora gera INSERT INTO owner.tabela quando o
    resultado vem de uma única tabela (joins, subselects e consultas WITH
    continuam usando &table).
  * Arrastar um objeto da Lista de objetos para o editor não repete mais o owner
    que já está digitado: "owner." recebe só o nome, "owner" recebe ".nome" e,
    se o owner for diferente, o que foi escrito é mantido e entra owner.nome.
  * A caixa "Error happened while running the script" agora segue o tema.
  * Sem mais flashes brancos ao abrir o SQLTools (fundo da janela principal,
    animação dos diálogos e a Lista de objetos flutuante).
  * What's New: histórico em inglês e em português (Brasil), escolhido pelas
    bandeiras no topo da janela.


3.3.2
-----
Data: 2026-10-27
  * Corrigidos defeitos visuais (áreas pretas e setas sobrando) ao arrastar o
    divisor do painel Valor.
  * O Oracle Instant Client embutido agora leva sua licença (ORACLE_LICENSE.txt)
    junto das DLLs extraídas, como exigem os termos Oracle Free Use.
  * Adicionados LICENSE (GPLv3) e THIRD-PARTY-NOTICES.md ao repositório, e o
    contribuidor da modernização aos créditos.
  * Painel Valor: o zoom do texto (Ctrl + roda) é mantido ao mudar de célula,
    até o SQLTools ser fechado.
  * Painel Valor: nova opção "Format CDATA". JSON ou XML dentro de uma seção
    CDATA é formatado e mostrado com as suas próprias cores.
  * Painel Valor: Shift + roda e os gestos horizontais do trackpad rolam o texto
    para os lados.


3.3.1
-----
Data: 2026-10-27
  * Os owners favoritos agora funcionam em dois níveis: os favoritos globais
    (Settings > Favorite Owners) aparecem em todas as conexões, e a estrela do
    painel de owners salva os favoritos por conexão (owner-favorites.ini na
    pasta de configuração). Nenhum owner é favorito por padrão.
  * As configurações abertas pela Lista de objetos agora têm o visual moderno e
    o tema escuro.
  * Lista de objetos: sem flash de cor ao trocar de owner, as caixas de filtro
    seguem o tema e Ctrl+Backspace funciona nas caixas de filtro.


3.3.0
-----
Data: 2026-10-27
  * Novo visual moderno em todo o aplicativo: barras de ferramentas, abas, barra
    de status, painéis e grade planos, com um novo conjunto de ícones vetoriais.
  * Nova Lista de objetos (Alt+3): barra de tipos, owners favoritos, filtros,
    indicadores de status e botão de configuração.
  * Redesenhados a tela Connect, as Settings, as preferências de DDL e todos os
    demais diálogos e caixas de mensagem, com caixas de seleção, botões, combos e
    botões de rotação modernos.
  * Tema escuro: View > Dark Theme (vale a partir da próxima vez que o SQLTools
    for iniciado). Cobre o editor, as grades, painéis, diálogos, menus e ícones.
  * Cursor peek: visual moderno, tema escuro, cores de sintaxe durante a digitação
    e mais palavras-chave e funções SQL.
  * Editor: barra contínua de alterações na margem, seta de execução espaçada,
    régua desligada por padrão. Ícones do painel Valor e do Output modernizados.
  * A coluna de número de linha da grade ajusta a largura conforme o número de
    linhas.


3.2.0
-----
Data: 2026-10-24
  * Versão usada para validar o fluxo de atualização (sem mudanças funcionais).


3.1.0
-----
Data: 2026-10-24
  * O Oracle Instant Client 19c agora vem embutido no SQLTools.exe: não é
    necessário instalar o Oracle Client (um OCI.DLL já disponível na máquina
    continua tendo preferência).
  * Novos Help > Check for Updates e Help > What's New. O SQLTools avisa quando
    há uma nova versão minor. Ele nunca se atualiza sozinho: você decide.
  * Opções de DDL: "Prefix with Schema Name" vem marcada por padrão.


3.0.2
-----
Data: 2026-09-10
  * Cursor peek: redesenhado com ícones vetoriais, dimensionamento mais
    inteligente, navegação de volta e uma correção de sintaxe; copie a seleção ou
    todo o conteúdo com um botão, atalhos e o menu de contexto.
  * Suporte a gestos de trackpad no editor e barra de ferramentas do painel Valor
    redesenhada.
  * O título da janela agora mostra a versão do SQLTools.


3.0.1
-----
Data: 2026-09-09
  * Novo painel lateral recolhível Valor, com decodificação automática de
    BLOB/hex e realce de sintaxe para XML, JSON, texto e hex dump.
  * Ctrl+A e Ctrl+C isolados no painel e cópia seletiva de texto.
  * Janela flutuante de peek de cursor PL/SQL, com edição ao vivo da declaração
    original.
  * Linhas-guia de escopo para IF/THEN/END IF e laços FOR.
  * Ctrl + roda do mouse altera o zoom da fonte do editor.
  * Botões de troca rápida de owner (DBAMV e MVINTEGRA) na lista de objetos.
  * Rolagem horizontal na grade com Shift + roda e trackpad.
  * Suporte a caracteres UTF-8.
  * A posição de rolagem é mantida ao ordenar colunas no DB Browser.
