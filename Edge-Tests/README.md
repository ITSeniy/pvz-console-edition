# Быстрые тесты сценариев

Каждый батник запускает игру сразу в нужном месте. Перед запуском берётся свежая
копия тестового сохранения `runtime-data/minigames-test/user`, она лежит в
`runtime-data/edge-tests/<имя>/user`. Ваш профиль и другие тесты не меняются.
Редакция — Full (`--pvz_test_unlock=true`).

Переход срабатывает через ~1,5 с после появления главного меню. Дальше
управление полностью ваше, реплея нет. Что проверить, написано в начале каждого
батника (`rem`). Режим мыши PC/Console переключается в Options → PC Settings.

Флаг `--pvz_test_start` принимает ключи через запятую:

| Ключ | Действие |
|---|---|
| `progress=N` | Записать в профиль уровень Adventure (1…50) — для доступа ко всем растениям |
| `mode=N` | Запустить режим N (таблица ниже) |
| `adventure=N` | Уровень Adventure N (1-1 = 1, 5-10 = 50) |
| `credits=0` / `credits=1` | Титры после финала (с песней) / из меню |
| `challenge=N` | Экран мини-игр, страница N (0 Survival, 1 Mini-Games, 2 Bonus Games, 3 Co-op, 4 Puzzle) |
| `sun=N` | Солнце N на каждом новом поле (до 9990) |

`Mode.bat <id>` и `Adventure.bat <уровень>` запускают любой режим или уровень.

## Номера режимов Xbox-версии

Из таблицы ChallengeDefinition в `default.xex` (отличаются от PC-версии).

| id | Режим | id | Режим |
|---|---|---|---|
| 0 | Adventure | 34 | Wall-nut Bowling 2 |
| 2–6 | Survival: Day / Night / Pool / Fog / Roof | 35 | Pogo Party |
| 7–11 | Survival Hard: Day … Roof | 36 | Dr. Zomboss's Revenge |
| 12–16 | Survival Endless: Day / Night / Pool / Fog / Roof | 37 | Art Challenge Wall-nut |
| 17 | ZomBotany (War and Peas) | 38 | Sunny Day |
| 18 | Wall-nut Bowling | 39 | Unsodded |
| 19 | Slot Machine | 40 | Big Time |
| 20 | Heavy Weapon | 41 | Art Challenge Sunflower |
| 21 | Beghouled | 42 | Air Raid |
| 22 | Invisi-ghoul | 43 | Ice Level |
| 23 | Seeing Stars | 44 | Zen Garden |
| 24 | Zombiquarium | 45 | High Gravity |
| 25 | Beghouled Twist | 46 | Grave Danger |
| 26 | Big Trouble Little Zombie | 47 | Can You Dig It? |
| 27 | Portal Combat | 48 | Dark Stormy Night |
| 28 | Column Like You See 'Em | 49 | Bungee Blitz |
| 29 | Bobsled Bonanza | 51 | Tree of Wisdom |
| 30 | Zombies on Speed | 52–69 | Vasebreaker / I, Zombie 1–9 (чётные / нечётные) |
| 31 | Whack a Zombie | 70 / 71 | Vasebreaker Endless / I, Zombie Endless |
| 32 | Last Stand | 79–89 | Co-op (нужны два геймпада) |
| 33 | ZomBotany 2 | 90 | It's Raining Seeds |

Сценарии без батника — в `docs/edge-cases-2026-10-14.md` (Co-op и Versus,
окна и DPI, сохранение посреди уровня, триал).
