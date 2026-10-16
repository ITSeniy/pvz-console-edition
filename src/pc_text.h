#pragma once
#include "mouse_prompt.h"
#include "pc_locale.h"
#include <array>

namespace pvz {
// Only Escape remains on the keyboard (Start); the mouse covers every other
// action, so controller markup is explained in mouse terms.
struct TextBindings {
  std::u16string start = u"Escape";
};
// `language` is the game's running language; `text` is its LawnStrings entry.
inline std::u16string PcGameText(std::u16string_view key, std::u16string text,
    bool mouse, bool pc, const TextBindings& b = {}, Language language = Language::En) {
  struct Entry { std::u16string_view key; Localized value; };
  // A language without an entry keeps the game's own text (CROW_SELECTION_7
  // already reads "Exit the game." there).
  constexpr std::array platform{
    Entry{u"SELECT_DEVICE", {u"Save folder", u"Speicherordner", u"Carpeta de guardado",
        u"Dossier de sauvegarde", u"Cartella salvataggi", u"セーブフォルダー"}},
    Entry{u"INVALID_LEVEL", {u"This save is from an earlier level. Your profile's current level will be used.",
        u"Dieser Spielstand stammt aus einem früheren Level. Der aktuelle Level deines Profils wird verwendet.",
        u"Esta partida guardada es de un nivel anterior. Se usará el nivel actual de tu perfil.",
        u"Cette sauvegarde provient d'un niveau précédent. Le niveau actuel de votre profil sera utilisé.",
        u"Questo salvataggio risale a un livello precedente. Verrà usato il livello attuale del tuo profilo.",
        u"このセーブデータは前のステージのものです。\n現在のステージを使います。"}},
    Entry{u"LB_VIEW_GAMERCARD", {u"View player profile", u"Spielerprofil anzeigen", u"Ver perfil del jugador",
        u"Afficher le profil du joueur", u"Guarda profilo giocatore", u"プレイヤープロフィールを見る"}},
    Entry{u"VIEW_GAMERCARD", {u"View Player Profile", u"Spielerprofil ansehen", u"Ver perfil del jugador",
        u"Voir le profil du joueur", u"Vedi profilo giocatore", u"プレイヤープロフィールを見る"}},
    Entry{u"GAMERTAG", {u"Player name", u"Spielername", u"Nombre del jugador", u"Nom du joueur",
        u"Nome giocatore", u"プレイヤー名"}},
    Entry{u"NOT_LOGGED_IN", {u"Online Unavailable", u"Online nicht verfügbar", u"En línea no disponible",
        u"En ligne indisponible", u"Online non disponibile", u"オンラインは使えません"}},
    Entry{u"NOT_LOGGED_IN_HOUSES", {
        u"Online features are not available in this PC build yet, so friends' houses cannot be shown.",
        u"Online-Funktionen gibt es in dieser PC-Version noch nicht, daher können die Häuser von Freunden nicht angezeigt werden.",
        u"Las funciones en línea aún no están disponibles en esta versión para PC, así que no se pueden mostrar las casas de tus amigos.",
        u"Les fonctions en ligne ne sont pas encore disponibles dans cette version PC : impossible d'afficher la maison de vos amis.",
        u"Le funzioni online non sono ancora disponibili in questa versione per PC, quindi non è possibile mostrare le case dei tuoi amici.",
        u"このPC版ではまだオンラインが使えないため、\nフレンドのお家は見られません。"}},
    Entry{u"NOT_LOGGED_IN_MESSAGE", {
        u"Online features are not available in this PC build yet, so the online leaderboard cannot be shown.",
        u"Online-Funktionen gibt es in dieser PC-Version noch nicht, daher kann die Online-Bestenliste nicht angezeigt werden.",
        u"Las funciones en línea aún no están disponibles en esta versión para PC, así que no se puede mostrar el marcador en línea.",
        u"Les fonctions en ligne ne sont pas encore disponibles dans cette version PC : impossible d'afficher le classement en ligne.",
        u"Le funzioni online non sono ancora disponibili in questa versione per PC, quindi non è possibile mostrare la classifica online.",
        u"このPC版ではまだオンラインが使えないため、\nオンラインランキングは見られません。"}},
    // Letters were downloaded from Xbox LIVE (mailbox_<lang>.pak) for
    // signed-in profiles only, so offline the mailbox stays empty.
    Entry{u"MAILBOX_EMPTY", {
        u"No letters yet! Zombie mail arrives online,\nand online features are not available in this PC build yet.",
        u"Noch keine Briefe! Zombie-Post kommt online,\nund Online-Funktionen gibt es in dieser PC-Version noch nicht.",
        u"¡Aún no hay cartas! El correo zombi llega en línea,\ny las funciones en línea aún no están disponibles\nen esta versión para PC.",
        u"Pas encore de lettres ! Le courrier zombie arrive en ligne,\net les fonctions en ligne ne sont pas encore disponibles\ndans cette version PC.",
        u"Ancora nessuna lettera! La posta degli zombi arriva online,\ne le funzioni online non sono ancora disponibili\nin questa versione per PC.",
        u"まだメールはありません!ゾンビのメールはオンラインで来ますが、\nこのPC版ではまだオンラインが使えません。"}},
    Entry{u"CROW_SELECTION_7", {u"Exit the game."}},
    Entry{u"CROW_SELECTION_DLC", {u"Spend virtual money on fictional items.",
        u"Gib virtuelles Geld für imaginäre Dinge aus.", u"Gasta dinero virtual en objetos ficticios.",
        u"Dépensez votre argent virtuel pour acheter des objets fictifs.",
        u"Spendi i tuoi soldi virtuali in oggetti fittizi.", u"役に立つアイテム\nにお金を使おう"}},
    Entry{u"NOT_ENOUGH_SPACE", {u"There is not enough free space to save. Free up disk space and try again.",
        u"Zum Speichern ist nicht genug Platz frei. Gib Speicherplatz frei und versuche es erneut.",
        u"No hay suficiente espacio libre para guardar. Libera espacio en el disco e inténtalo de nuevo.",
        u"Espace libre insuffisant pour sauvegarder. Libérez de l'espace disque et réessayez.",
        u"Spazio libero insufficiente per salvare. Libera spazio sul disco e riprova.",
        u"保存するための空きスペースがありません。\nスペースを空けて、もう一度保存してください。"}},
    Entry{u"SAVEGAME_ERROR_DISKFULL", {u"There is not enough free disk space to save.\nFree up disk space and try again.",
        u"Zum Speichern ist nicht genug Platz frei.\nGib Speicherplatz frei und versuche es erneut.",
        u"No hay suficiente espacio libre para guardar.\nLibera espacio en el disco e inténtalo de nuevo.",
        u"Espace disque insuffisant pour sauvegarder.\nLibérez de l'espace disque et réessayez.",
        u"Spazio libero insufficiente per salvare.\nLibera spazio sul disco e riprova.",
        u"保存するための空きスペースがありません。\nスペースを空けて、もう一度保存してください。"}},
    Entry{u"SAVING_LONG", {u"Saving. Please keep the game open.", u"Speichern. Bitte das Spiel nicht schließen.",
        u"Guardando. No cierres el juego.", u"Sauvegarde en cours. Ne fermez pas le jeu.",
        u"Salvataggio in corso. Non chiudere il gioco.", u"保存しています。ゲームを終了しないでください"}},
    Entry{u"SAVING_LIVE_DIALOG", {u"Saving game data", u"Spieldaten werden gespeichert",
        u"Guardando datos del juego", u"Sauvegarde des données", u"Salvataggio dati di gioco", u"ゲームデータを保存中"}},
    Entry{u"GO_TO_MARKETPLACE", {u"Additional Content", u"Zusatzinhalte", u"Contenido adicional",
        u"Contenu supplémentaire", u"Contenuti aggiuntivi", u"追加コンテンツ"}},
    Entry{u"UPSELL_MARKETPLACE", {u"Additional Content", u"Zusatzinhalte", u"Contenido adicional",
        u"Contenu supplémentaire", u"Contenuti aggiuntivi", u"追加コンテンツ"}},
    Entry{u"NOT_SIGNED_IN_MARKETPLACE", {u"Online content purchases are unavailable in this PC build.",
        u"Online-Käufe sind in dieser PC-Version nicht verfügbar.",
        u"Las compras de contenido en línea no están disponibles en esta versión para PC.",
        u"Les achats de contenu en ligne ne sont pas disponibles dans cette version PC.",
        u"Gli acquisti di contenuti online non sono disponibili in questa versione per PC.",
        u"このPC版ではオンラインでコンテンツを購入できません。"}},
    Entry{u"TRIAL_LOCKED_MESSAGE", {
        u"Unlock the full game from the main menu to unearth all that is wonderful about Plants vs. Zombies.",
        u"Schalte das vollständige Spiel im Hauptmenü frei, um alles Wunderbare an Pflanzen gegen Zombies zu Tage zu fördern.",
        u"Desbloquea el juego completo desde el menú principal para obtener todo el maravilloso contenido de Plantas contra Zombis.",
        u"Déverrouillez le jeu complet depuis le menu principal pour découvrir tout ce qu'il y a d'incroyable dans Plantes contre Zombies.",
        u"Sblocca il gioco completo dal menu principale per riesumare tutte le meraviglie di Piante contro zombi.",
        u"プラント vs. ゾンビ の面白さを十分に体験するためにも、メインメニューで完全版を解除してください"}},
    Entry{u"REGISTER", {
        u"Unlock the full game from the main menu to unearth all that is wonderful about Plants vs. Zombies.",
        u"Schalte das vollständige Spiel im Hauptmenü frei, um alles Wunderbare an Pflanzen gegen Zombies zu Tage zu fördern.",
        u"Desbloquea el juego completo desde el menú principal para obtener todo el maravilloso contenido de Plantas contra Zombis.",
        u"Déverrouillez le jeu complet depuis le menu principal pour découvrir tout ce qu'il y a d'incroyable dans Plantes contre Zombies.",
        u"Sblocca il gioco completo dal menu principale per riesumare tutte le meraviglie di Piante contro zombi.",
        u"プラント vs. ゾンビ の面白さを十分に体験するためにも、メインメニューで完全版を解除してください"}},
    Entry{u"ACHIEVEMENT_EARNED_UPSELL_BODY", {u"Achievement earned!", u"Erfolg freigeschaltet!",
        u"¡Logro desbloqueado!", u"Succès débloqué !", u"Obiettivo sbloccato!", u"実績が解除されました!"}},
    // Other languages drop the friends' houses sentence from their own text.
    Entry{u"HELP_TEXT_5_1", {u"Welcome to your house! Crazy Dave will renovate it every so often as you get better at defending your lawn!"}},
    Entry{u"UPSELL_QUIT_GAME", {u"<B> Quit Game", u"<B> Spiel beenden", u"<B> Salir del juego",
        u"<B> Quitter le jeu", u"<B> Esci dal gioco", u"<B>でゲームを終了"}},
    Entry{u"CREDITS_PAUSE_BODY", {
        u"Resume to continue the credits.\n\nYou can also view the credits later\nfrom the options on the main menu.",
        u"Setze fort, um die Mitwirkenden weiter anzuzeigen.\n\nDu kannst dir die Mitwirkenden auch später\nim Hauptmenü unter 'Hilfe und Optionen' ansehen.",
        u"Reanuda para continuar con los créditos.\n\nTambién puedes verlos cuando quieras\ndesde las opciones del menú principal.",
        u"Reprenez pour continuer la liste des crédits.\n\nVous pouvez aussi les voir plus tard\ndepuis le menu des options.",
        u"Riprendi per continuare i riconoscimenti.\n\nSe vuoi, puoi anche vederli più tardi\ntramite le opzioni del menu principale.",
        u"ゲームに戻ると、クレジットを続けます。\n\nメインメニューのオプションからも\nクレジットを見ることができます。"}},
    Entry{u"UPSELL_PAUSE_BODY", {u"Resume to continue.", u"Setze fort, um weiterzuspielen.",
        u"Reanuda para continuar.", u"Reprenez pour continuer.", u"Riprendi per continuare.",
        u"ゲームに戻って、続けよう。"}}};
  for (const auto& e : platform) if (key == e.key) {
    if (const auto value = e.value.In(language); !value.empty()) text = value;
    else if (key == u"HELP_TEXT_5_1") {
      // Cut from the sentence that shows friends' houses with <l>.
      if (const auto glyph = text.find(u"<l>"); glyph != std::u16string::npos) {
        auto end = text.find_last_of(u"!.?。", glyph);
        if (end != std::u16string::npos) text.resize(end + 1);
      }
    }
    break;
  }
  // Select Second Player: player 2 joins with A (or Cross; the glyph
  // alternates while the window is open). Cancelling belongs to player 1:
  // RMB on the mouse, B on a pad. A mouse can also join a pad's game by LMB.
  if (key == u"2P_JOIN_TEXT" || key == u"2P_JOINED") {
    const auto swap = [&](std::u16string_view from, std::u16string_view to) {
      if (const auto at = text.find(from); at != std::u16string::npos) text.replace(at, from.size(), to);
    };
    // Each language's cancel line and join request, as its LawnStrings words them.
    struct Join { std::u16string_view cancel, cancel_mouse, join, join_mouse; };
    constexpr std::array joins{
      Join{u"Press <B> to cancel", u"Right-click to cancel", u"press <A>", u"press <A>\nor click the left mouse button"},
      Join{u"Zum Abbrechen <B> drücken", u"Zum Abbrechen rechtsklicken", u"drück <A>",
          u"drück <A>\noder klick mit der linken Maustaste"},
      Join{u"Pulsa <B> para cancelar", u"Haz clic derecho para cancelar", u"pulsa <A>",
          u"pulsa <A>\no haz clic con el botón izquierdo"},
      Join{u"Appuyez sur <B> pour annuler", u"Faites un clic droit pour annuler", u"appuyez sur <A>",
          u"appuyez sur <A>\nou faites un clic gauche"},
      Join{u"Premi <B> per annullare", u"Fai clic destro per annullare", u"premi <A>",
          u"premi <A>\no fai clic sinistro"},
      Join{u"<B>でキャンセル", u"右クリックでキャンセル", u"<A>を押す", u"<A>を押すか左クリック"}};
    const auto& join = joins[size_t(language)];
    if (mouse) { swap(join.cancel, join.cancel_mouse); return text; }
    swap(join.join, join.join_mouse);
    return text;
  }
  // Player 2 must press the buttons on their own controller to join.
  if (!mouse || key.starts_with(u"2P_") || key == u"P2_JOIN" ||
      key == u"RECONNECT_SECOND_CONTROLLER_FMT") return text;
  if (key == u"CLICK_TO_CONTINUE") return {};
  // How to Play "Controls" shows our mouse picture: its leader lines end at
  // the LSTICK (left), B (right) and BUMPERS (top) labels; the rest is unused.
  if (key == u"HELP_TEXT_2_B" && pc) return {};
  if (key == u"HELP_TEXT_2_BUMPERS" && pc) return {};
  if (key == u"HELP_TEXT_2_A" || key == u"HELP_TEXT_2_X" || key == u"HELP_TEXT_2_START") return {};
  // Mouse hints and the actions the original console text gives to X/Y,
  // bumpers and sticks, worded after the PC version where it has the same
  // action. An empty console text is the same as the PC one.
  struct Action { std::u16string_view key; Localized pc, console; };
  constexpr std::array actions{
    Action{u"ADVICE_CLICK_SEED_PACKET",
        {u"Click a seed packet to select it!", u"Klicke auf ein Samenpäckchen, um es auszuwählen!",
         u"¡Haz clic en un sobre de semillas para seleccionarlo!",
         u"Cliquez sur un paquet de graines pour le sélectionner !",
         u"Fai clic su un sacchetto di semi per selezionarlo!", u"プラントをクリックして選ぼう!"},
        {u"Use the mouse wheel to select a seed packet!", u"Wähle mit dem Mausrad ein Samenpäckchen aus!",
         u"¡Usa la rueda del ratón para seleccionar un sobre de semillas!",
         u"Utilisez la molette de la souris pour sélectionner un paquet de graines !",
         u"Usa la rotellina del mouse per selezionare un sacchetto di semi!", u"マウスホイールでプラントを選ぼう!"}},
    Action{u"ADVICE_CLICK_ON_SUN",
        {u"Click the falling sun to collect it!", u"Klicke auf die herunterfallende Sonne, um sie einzusammeln!",
         u"¡Haz clic en el sol que cae para recogerlo!", u"Cliquez sur le soleil qui tombe pour le ramasser !",
         u"Fai clic sulla sfera solare per raccoglierla!", u"落ちてくる太陽をクリックして集めよう!"},
        {u"Move over to the falling sun to collect it!", u"Fahre über die herunterfallende Sonne, um sie einzusammeln!",
         u"¡Pasa por encima del sol que cae para recogerlo!", u"Passez sur le soleil qui tombe pour le ramasser !",
         u"Passa sulla sfera solare per raccoglierla!", u"落ちてくる太陽にカーソルを合わせて集めよう!"}},
    Action{u"ADVICE_CLICK_SHOVEL",
        {u"Click the shovel, then click the plant you want to dig up!",
         u"Klicke auf die Schaufel und dann auf die Pflanze, die du ausgraben möchtest!",
         u"¡Haz clic en la pala y luego en la planta que quieras quitar!",
         u"Cliquez sur la pelle, puis sur la plante que vous désirez déraciner !",
         u"Fai clic sulla pala, poi sulla pianta che vuoi rimuovere!",
         u"シャベルをクリックしてから、\n取り除きたいプラントをクリックしよう!"},
        {u"Point at a plant, then hold the right mouse button to dig!",
         u"Zeige auf eine Pflanze und halte die rechte Maustaste gedrückt, um zu graben!",
         u"¡Apunta a una planta y mantén pulsado el botón derecho del ratón para excavar!",
         u"Pointez une plante, puis maintenez le bouton droit de la souris pour creuser !",
         u"Punta una pianta, poi tieni premuto il tasto destro del mouse per scavare!",
         u"プラントにカーソルをあわせて、\n右クリックを押し続けて掘り出そう!"}},
    Action{u"ADVICE_CLICK_ON_GRASS",
        {u"Left-click on the grass to plant your seed!", u"Klicke auf das Gras, um das Samenkorn zu pflanzen!",
         u"¡Haz clic en la hierba para plantar una semilla!", u"Cliquez sur votre pelouse pour planter une graine !",
         u"Fai clic sul prato per piantare un seme!", u"芝生をクリックして、プラントを植えよう!"}, {}},
    Action{u"HELP_TEXT_2_LSTICK",
        {u"LMB: Main Action", u"Linke Taste: Hauptaktion", u"Botón izq.: acción principal",
         u"Clic gauche : action principale", u"Tasto sinistro: azione principale", u"左クリック:メイン"}, {}},
    Action{u"HELP_TEXT_2_B",
        {u"RMB: Secondary Action", u"Rechte Taste: Nebenaktion", u"Botón der.: acción secundaria",
         u"Clic droit : action secondaire", u"Tasto destro: azione secondaria", u"右クリック:サブ"}, {}},
    Action{u"HELP_TEXT_2_BUMPERS",
        {u"MMB: Third Action", u"Mittlere Taste: Dritte Aktion", u"Botón central: tercera acción",
         u"Clic molette : troisième action", u"Tasto centrale: terza azione", u"ホイールクリック:サード"}, {}},
    Action{u"ADVICE_HEAVY_WEAPON",
        {u"Move the mouse to drive and aim!\nHold LMB to fire. Click a seed packet to buy an upgrade!",
         u"Bewege die Maus zum Fahren und Zielen!\nHalte die linke Maustaste zum Schießen. Klicke auf ein Samenpäckchen, um ein Upgrade zu kaufen!",
         u"¡Mueve el ratón para conducir y apuntar!\nMantén pulsado el botón izquierdo para disparar. ¡Haz clic en un sobre de semillas para comprar una mejora!",
         u"Déplacez la souris pour conduire et viser !\nMaintenez le clic gauche pour tirer. Cliquez sur un paquet de graines pour acheter une amélioration !",
         u"Muovi il mouse per guidare e mirare!\nTieni premuto il tasto sinistro per sparare. Fai clic su un sacchetto di semi per comprare un potenziamento!",
         u"マウスを動かして芝刈り機を移動!\n左クリックを押し続けて発射!プラントをクリックしてアップグレード!"},
        {u"Move the mouse to drive and aim!\nHold LMB to fire. Wheel: Select upgrade; RMB: Buy.",
         u"Bewege die Maus zum Fahren und Zielen!\nHalte die linke Maustaste zum Schießen. Mausrad: Upgrade wählen; Rechtsklick: Kaufen.",
         u"¡Mueve el ratón para conducir y apuntar!\nMantén pulsado el botón izquierdo para disparar. Rueda: elegir mejora; clic derecho: comprar.",
         u"Déplacez la souris pour conduire et viser !\nMaintenez le clic gauche pour tirer. Molette : choisir une amélioration ; clic droit : acheter.",
         u"Muovi il mouse per guidare e mirare!\nTieni premuto il tasto sinistro per sparare. Rotellina: scegli potenziamento; clic destro: compra.",
         u"マウスを動かして芝刈り機を移動!\n左クリックを押し続けて発射!ホイールでアップグレードを選び、右クリックで購入。"}},
    Action{u"ADVICE_SLOT_MACHINE_SPIN_AGAIN",
        {u"Click the lever to spin again!", u"Klicke auf den Hebel, um noch mal zu drehen!",
         u"¡Haz clic en la palanca para volver a tirar!", u"Cliquez sur le levier pour relancer !",
         u"Fai clic sulla leva per tirare ancora!", u"レバーをクリックしてもう一度回そう!"},
        {u"Scroll the wheel to spin again!", u"Dreh das Mausrad, um noch mal zu drehen!",
         u"¡Gira la rueda del ratón para volver a tirar!", u"Tournez la molette pour relancer !",
         u"Gira la rotellina per tirare ancora!", u"ホイールを回してもう一度回そう!"}},
    Action{u"SLOT_MACHINE_PULL_TOOLTIP",
        {u"Click the lever to spin again!", u"Klicke auf den Hebel, um noch mal zu drehen!",
         u"¡Haz clic en la palanca para volver a tirar!", u"Cliquez sur le levier pour relancer !",
         u"Fai clic sulla leva per tirare ancora!", u"レバーをクリックしてもう一度回そう!"},
        {u"Scroll the wheel to spin again!", u"Dreh das Mausrad, um noch mal zu drehen!",
         u"¡Gira la rueda del ratón para volver a tirar!", u"Tournez la molette pour relancer !",
         u"Gira la rotellina per tirare ancora!", u"ホイールを回してもう一度回そう!"}},
    Action{u"ADVICE_SLOT_MACHINE_PULL",
        {u"Click the lever to spin the slot machine!", u"Klicke auf den Hebel, um den Spielautomaten zu drehen!",
         u"¡Haz clic en la palanca para tirar!", u"Cliquez sur le levier pour lancer la machine à sous !",
         u"Fai clic sulla leva per far girare la slot machine!", u"レバーをクリックして、\nスロットを回そう!"},
        {u"Scroll the wheel to spin the slot machine!", u"Dreh das Mausrad, um den Spielautomaten zu drehen!",
         u"¡Gira la rueda del ratón para tirar!", u"Tournez la molette pour lancer la machine à sous !",
         u"Gira la rotellina per far girare la slot machine!", u"ホイールを回して、\nスロットを回そう!"}},
    Action{u"ADVICE_BEGHOULED_DRAG_TO_MATCH_3",
        {u"Click-and-drag plants to make matches of 3", u"Ziehe Pflanzen mit der Maus und bilde 3er-Gruppen.",
         u"Arrastra las plantas con el ratón para formar tríos.",
         u"Cliquez-glissez les plantes pour faire des séries de 3.",
         u"Trascina le piante con il mouse per abbinarne 3 dello stesso tipo.",
         u"プラントをドラッグして、同じプラントを3つあわせよう"},
        {u"Hold LMB on a plant and move the mouse to make matches of 3",
         u"Halte die linke Maustaste auf einer Pflanze und bewege die Maus, um 3er-Gruppen zu bilden.",
         u"Mantén pulsado el botón izquierdo sobre una planta y mueve el ratón para formar tríos.",
         u"Maintenez le clic gauche sur une plante et déplacez la souris pour faire des séries de 3.",
         u"Tieni premuto il tasto sinistro su una pianta e muovi il mouse per abbinarne 3 dello stesso tipo.",
         u"プラントの上で左クリックを押し続け、\nマウスを動かして同じプラントを3つあわせよう"}},
    Action{u"ADVICE_BEGHOULED_TWIST_TO_MATCH_3",
        {u"Click to twist plants to make matches of 3",
         u"Klicke, um Pflanzen zu vertauschen und 3er-Gruppen zu bilden.",
         u"Haz clic para mover las plantas y formar tríos.",
         u"Cliquez pour échanger les plantes et faire des séries de 3.",
         u"Fai clic per ruotare le piante e abbinarne 3 dello stesso tipo.",
         u"クリックしてプラントをくるっと回して\n同じ種類のものを3つあわせよう"}, {}},
    Action{u"ADVICE_ZOMBIQUARIUM_CLICK_TO_FEED",
        {u"Click on the tank to feed the zombies", u"Klicke auf das Aquarium, um die Zombies zu füttern!",
         u"Haz clic en el tanque para alimentar a los zombis.", u"Cliquez sur le réservoir pour nourrir les zombies.",
         u"Fai clic sull'acquario per nutrire gli zombi.", u"水槽をクリックしてゾンビにエサをあげよう"}, {}},
    Action{u"ADVICE_ZOMBIQUARIUM_BUY_SNORKEL",
        {u"Click the seed packet to buy more snorkel zombies!",
         u"Klicke auf das Samenpäckchen, um weitere Schnorchel-Zombies zu kaufen!",
         u"¡Haz clic en el sobre de semillas para comprar más Zombis buzos!",
         u"Cliquez sur le paquet de graines pour acheter plus de zombies tubas.",
         u"Fai clic sul sacchetto di semi per acquistare altri zombi sommozzatori!",
         u"プラントをクリックして、もっとシュノーケルゾンビを買おう!"},
        {u"Click a seed packet, then right-click to buy more snorkel zombies!",
         u"Klicke auf ein Samenpäckchen und dann mit rechts, um weitere Schnorchel-Zombies zu kaufen!",
         u"¡Haz clic en un sobre de semillas y luego clic derecho para comprar más Zombis buzos!",
         u"Cliquez sur un paquet de graines, puis faites un clic droit pour acheter plus de zombies tubas.",
         u"Fai clic su un sacchetto di semi, poi clic destro per acquistare altri zombi sommozzatori!",
         u"プラントをクリックしてから右クリックで、\nもっとシュノーケルゾンビを買おう!"}},
    Action{u"ADVICE_ZEN_GARDEN_WATER_PLANT",
        {u"Click on a sprout to water it!", u"Klicke auf einen Sprössling, um ihn zu gießen!",
         u"¡Haz clic en una planta que esté brotando para regarla!", u"Cliquez sur une pousse pour l'arroser !",
         u"Fai clic su un germoglio per annaffiarlo!", u"新芽をクリックして、水をあげよう!"}, {}},
    Action{u"ADVICE_USE_SHOVEL_ON_POTS",
        {u"Click on a vase to see what's inside!", u"Klicke auf eine Vase, um deren Inhalt zu sehen!",
         u"¡Haz clic en un jarrón para ver qué hay en su interior!", u"Cliquez sur un vase pour en voir l'intérieur !",
         u"Fai clic su un vaso per vedere cosa c'è dentro!", u"ツボをクリックして、\n何が入っているか中を見よう!"}, {}},
    Action{u"LAST_STAND_BEGIN_ONSLAUGHT",
        {u"Click Start Onslaught to begin the onslaught!", u"Klicke auf 'Angriff starten', um zu beginnen!",
         u"¡Haz clic en Comenzar para iniciar la masacre!", u"Cliquez sur Lancer l'attaque pour commencer le massacre !",
         u"Fai clic su Avvia assalto per iniziare l'assalto!", u"モーレツアタック開始をクリックして、\nモーレツアタックを開始する"}, {}},
    Action{u"ZEN_GARDEN_TUTORIAL_COMPLETE",
        {u"Click Menu, then Exit Game, and choose Adventure to continue!",
         u"Klicke auf Menü, dann auf 'Spiel verlassen', und wähle Abenteuer, um weiterzuspielen!",
         u"¡Haz clic en Menú, luego en Salir del juego, y elige Aventura para continuar!",
         u"Cliquez sur Menu, puis sur Quitter le jeu, et choisissez Aventure pour continuer !",
         u"Fai clic su Menu, poi su Esci dal gioco, e scegli Avventura per continuare!",
         u"メニューをクリックしてゲームを終了し、\nアドベンチャーを選んで続けよう!"}, {}},
    Action{u"CLICK_TO_START",
        {u"Click to Start!", u"Zum Starten klicken!", u"¡Haz clic para comenzar!", u"Cliquez pour commencer !",
         u"Fai clic per iniziare!", u"クリックでゲームをはじめる"}, {}},
    Action{u"CLICK_TO_RESUME",
        {u"Click to resume game", u"Klicken, um weiterzuspielen.", u"Haz clic para reanudar el juego.",
         u"Cliquer pour reprendre le jeu", u"Fai clic per riprendere il gioco.", u"クリックしてゲームに戻る"}, {}},
    Action{u"ROLL_CREDITS",
        {u"Roll Credits!", u"Mitwirkende!", u"¡Esos créditos!", u"Voir les crédits !", u"Riconoscimenti!",
         u"クレジットへ!"}, {}},
    Action{u"COB_CANNON_TOOLTIP",
        {u"Click to launch deadly cobs of corn\n(requires 2 kernel-pults)",
         u"Klicke, um tödliche Maiskolben abzufeuern\n(benötigt 2 Kornapulte).",
         u"Haz clic para lanzar mazorcas letales.\n(requiere 2 Lanzamaíces)",
         u"Cliquez pour lancer des épis de maïs mortels\n(nécessite 2 cata-graines).",
         u"Fai clic per scagliare pannocchie letali\n(richiede 2 chiccopulte).",
         u"クリックで破壊力バツグンの\nトウモロコシを発射します\n(カーネルパルトが2つ必要)"}, {}},
    Action{u"COB_CANNON_DESCRIPTION_HEADER",
        {u"Click on the Cob Cannon to launch deadly cobs of corn.",
         u"Klicke auf die Maiskanone, um tödliche Maiskolben abzufeuern.",
         u"Haz clic sobre el Mazorcañón para lanzar mazorcas letales.",
         u"Cliquez sur le canon à épi pour lancer des épis de maïs mortels.",
         u"Fai clic sulla sparapannocchie per scagliare pannocchie letali.",
         u"コッブキャノンをクリックして、破壊力バツグンのトウモロコシを発射します。"}, {}},
    Action{u"UPSELL_BUY_GAME",
        {u"Unlock Full Game", u"Vollständiges Spiel freischalten", u"Desbloquear juego completo",
         u"Déverrouiller le jeu complet", u"Sblocca il gioco completo", u"完全版を購入"}, {}},
    Action{u"TREE_OF_WISDOM_5",
        {u"Pssst! Turn on 'future' in PC Settings > Cheats to experience zombies... from the FUTURE!",
         u"Pssst! Schalte 'future' unter PC-Einstellungen > Cheats ein und erlebe Zombies ... aus der ZUKUNFT!",
         u"¡Psst! Activa 'future' en Opciones de PC > Trucos y verás zombis... ¡del FUTURO!",
         u"Psst ! Activez 'future' dans Options PC > Codes pour voir des zombies... du FUTUR !",
         u"Psst! Attiva 'future' in Opzioni PC > Trucchi per vedere zombi... dal FUTURO!",
         u"PC設定のチートで\n'future'をオンに\nすると、ゾンビが...\nなんと、未来から\nやって来るぞ!"}, {}},
    Action{u"TREE_OF_WISDOM_48",
        {u"If you turn on 'trickedout' in PC Settings > Cheats, don't be surprised if you see something wacky happen to your lawnmowers.",
         u"Wenn du 'trickedout' unter PC-Einstellungen > Cheats einschaltest, wundere dich nicht, wenn etwas Komisches mit deinen Rasenmähern passiert.",
         u"Si activas 'trickedout' en Opciones de PC > Trucos, luego no te sorprendas de lo que les pueda pasar a tus cortadoras de césped.",
         u"Si vous activez 'trickedout' dans Options PC > Codes, ne soyez pas surpris si vos tondeuses se mettent à agir bizarrement.",
         u"Se attivi 'trickedout' in Opzioni PC > Trucchi, non sorprenderti se poi vedi qualcosa di strano accadere ai tuoi tosaerba.",
         u"PC設定のチートで\n'trickedout'をオン\nにすると、何かトンデモ\nないコトが芝刈り機に\n起こるかもだが、びっく\nりするな。"}, {}},
    Action{u"TREE_OF_WISDOM_33",
        {u"Please do not tap on the glass! Or actually, go ahead; right-click during Zombiquarium to deafen your underwater creatures.",
         u"Nicht an die Scheibe klopfen? Pah! Klicke im Zombiquarium mit rechts, dass es den Bewohnern in den Ohren klingelt.",
         u"¡No golpees el cristal! Bueno, ¿y por qué no? Adelante, haz clic derecho en Zombiquarium para dejar sordos a los zombis.",
         u"On ne cogne pas la vitre ! Ou plutôt si, tiens : faites un clic droit pendant le Zombaquarium pour assourdir vos créatures aquatiques.",
         u"Per favore, non toccare il vetro! Anzi, no, fallo: fai clic destro nel minigioco Acquario zombi per assordare le creature sottomarine.",
         u"ガラスを叩いちゃいかん\nいや、悪くないかも\nしれん。どんどんやれ。\nゾンビ水槽で\n右クリックすると\n水中クリーチャー達の\n耳が聞こえなくなるぞ。"},
        {u"Please do not tap on the glass! Or actually, go ahead; press the middle mouse button during Zombiquarium to deafen your underwater creatures.",
         u"Nicht an die Scheibe klopfen? Pah! Drücke im Zombiquarium die mittlere Maustaste, dass es den Bewohnern in den Ohren klingelt.",
         u"¡No golpees el cristal! Bueno, ¿y por qué no? Adelante, pulsa el botón central del ratón en Zombiquarium para dejar sordos a los zombis.",
         u"On ne cogne pas la vitre ! Ou plutôt si, tiens : appuyez sur le bouton central de la souris pendant le Zombaquarium pour assourdir vos créatures aquatiques.",
         u"Per favore, non toccare il vetro! Anzi, no, fallo: premi il tasto centrale del mouse nel minigioco Acquario zombi per assordare le creature sottomarine.",
         u"ガラスを叩いちゃいかん\nいや、悪くないかも\nしれん。どんどんやれ。\nゾンビ水槽で\nホイールクリックすると\n水中クリーチャー達の\n耳が聞こえなくなるぞ。"}}};
  for (const auto& action : actions)
    if (key == action.key) return std::u16string((pc || action.console.en.empty() ? action.pc : action.console)(language));
  const auto replace = [&](std::u16string_view from, std::u16string_view to) {
    size_t at = 0;
    while ((at = text.find(from, at)) != std::u16string::npos) {
      text.replace(at, from.size(), to); at += to.size();
    }
  };
  // Seed selection and stick markup; nothing in the game's text uses these
  // outside the entries above, so they only keep stray markup readable.
  constexpr Localized seed{u"LMB: ", u"Linksklick: ", u"Clic izq.: ", u"Clic gauche : ", u"Clic sinistro: ", u"左クリック:"};
  constexpr Localized wheel{u"Wheel: ", u"Mausrad: ", u"Rueda: ", u"Molette : ", u"Rotellina: ", u"ホイール:"};
  constexpr Localized stick{u"the mouse", u"der Maus", u"el ratón", u"la souris", u"il mouse", u"マウス"};
  replace(u"<L><R>", (pc ? seed : wheel)(language));
  replace(u"<l>", stick(language)); replace(u"<r>", stick(language));
  // <X>/<Y> remain controller glyphs: those actions have no mouse button.
  text = MousePrompt(std::move(text), pc, language);
  replace(u"Escape", b.start);
  return text;
}
}
