# Bonus Games

Отдельное меню с режимами, которые есть в `default.xex`, но не открываются
в обычной игре. Оформление — штатный экран мини-игр.

## Как открыть

В **More Ways to Play → Mini-Games** после всех 20 трофеев мини-игр слева
внизу появляется каменная кнопка **Bonus Games**. Число трофеев игра считает
сама: `LawnApp::GetNumTrophies(1)`, `821AB788`. **Back** в Bonus Games
возвращает в Mini-Games. После победы в бонусной игре экран награды
возвращает в Bonus Games.

Кнопка работает мышью. На геймпаде её пока нет.

## Как устроено

`ChallengeScreen(app, page, user)` (`82166B48`) показывает записи таблицы
`ChallengeDefinition[94]` (`82816830`: режим, кадр иконки, страница, строка,
столбец, имя), у которых совпадает страница:
- 0 — Survival;
- 1 — Mini-Games;
- 3 — Co-op;
- 4 — Puzzle;
- 5 — Versus.

Страница 2 в Xbox-версии нигде не открывается. Это бывшая скрытая страница
«limbo» из PC-версии. Bonus Games — та же страница, открытая через
`LawnApp::ShowChallengeScreen(2)` (`821A32C0`).

Перед первым показом из страницы 2 убираются Zen Garden и Tree of Wisdom
(у них свои экраны), а также служебные Upsell, Intro и Squirrel. Остаются
17 режимов:

| id | Режим | id | Режим |
|---|---|---|---|
| 12 | Survival: Day (Endless) | 41 | Art Challenge Sunflower |
| 13 | Survival: Night (Endless) | 42 | Air Raid |
| 15 | Survival: Fog (Endless) | 43 | Ice Level |
| 16 | Survival: Roof (Endless) | 45 | High Gravity |
| 37 | Art Challenge Wall-nut | 46 | Grave Danger |
| 38 | Sunny Day | 47 | Can You Dig It? |
| 39 | Unsodded | 48 | Dark Stormy Night |
| 40 | Big Time | 49 | Bungee Blitz |
| | | 90 | It's Raining Seeds |

Возврат после победы: выход с экрана награды (`8212C0A8`) выбирает страницу
по фиксированным диапазонам режимов. Endless попал бы в Survival, остальные —
в Mini-Games. Для бонусных режимов страница подменяется на 2.

## Иконки

В атласе `challenge_thumbnails` (22 кадра по 185×139) своих иконок для
бонусных режимов нет. Кадр 13 пустой, кадр 9 — не по теме. Записи
страницы 2 указывают на чужие кадры: например, Art Challenge получил бы
иконку ZomBotany.

- **Survival Endless** — свои карты из `survival_thumbnails`, Hard-кадры
  5/6/8/9. Строка рисуется как на странице Survival.
- **Остальные** — штатная заготовка `IMAGE_CHALLENGE_BLANK` (`82D0A4EC`).
  На время отрисовки строки она подменяет `IMAGE_CHALLENGE_THUMBNAILS`
  (`82D0A388`).

Заголовок: `PICK_CHALLENGE` на странице 2 — «Bonus Games».

## Проверка

`Edge-Tests\35-Bonus-Games.bat` открывает Mini-Games с кнопкой без трофеев
(`--pvz_test_bonus_unlocked=true`). Каждый бонусный режим запускается
отдельным батником (`Edge-Tests\07…34`, `Mode.bat <id>`).
