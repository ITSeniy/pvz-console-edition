# Сохранение для проверки мини-игр

В папке `save` три отдельных файла `63E83FFD`, `63E83FFE`, `63E83FFF`
совпадают с содержимым `584109FF.zar` и `pvz 2 1-1 Xenia Canary.zip`.
Архив `63E83FFD.zip` содержит другой вариант прогресса.

Файлы Xenia имеют заголовок 24 байта. RexGlue сохраняет только бинарное
содержимое настройки. Импорт проверяет ID, тип, размер и версию профиля,
снимает заголовки и записывает отдельный профиль:

```powershell
python scripts/import_xenia_profile.py save runtime-data/minigames-test/user
```

Импорт запрещает запись в существующую папку назначения. Исходные файлы
и обычный профиль `runtime-data/user` остаются без изменений.
Отчёт и SHA-256: `runtime-data/minigames-test/import.json`.

Запуск для работы с мини-играми — **Start-Minigames.bat**, либо:

```powershell
.\run.ps1 -Mouse -UserDataRoot runtime-data/minigames-test/user --pvz_test_unlock=true
```

Прогресс и настройки этого запуска записываются в
`runtime-data/minigames-test/user`. Обычный запуск продолжает использовать
`runtime-data/user`. Диагностический реплей для открытия меню:
`tests/replays/imported_minigames.csv`.

`Start-Minigames.bat` включает `pvz_test_unlock=true`: редакция Full нужна,
чтобы Trial не скрывал режимы открытого тестового сохранения. Это отдельный
флаг запуска; трофеи, монеты и прогресс он не изменяет.

Проверка импорта 7 октября 2026: smoke-20261007-112805-552.log,
40 секунд без аварий. Проверка Start-Minigames.bat:
run-20261007-112914.log, 38 секунд без аварий. Список Mini-Games
открывается мышью и показывает 12/20 трофеев; есть закрытые пункты.
Кадр: .cache/imported-minigames-grid.png. SHA-256 исходных трёх файлов
после импорта совпадает; обычный профиль не изменён.

До доработки меню было обнаружено: ЛКМ выделяет строку Whack a Zombie, но
не запускает мини-игру. Корневой экран — vtable 820B899C;
MouseDown/MouseUp доставляются, переход в Board не происходит.
Журнал smoke-20261007-113018-750.log, 65 секунд без аварий;
кадр .cache/imported-whack-2.png. Это воспроизводимый сценарий для
доработки выбора мини-игр мышью. Этот сбой исправлен; результаты и
проверки текущей сборки: `docs/testing-minigames-2026-10-07.md`.

Для доступа к последним пяти мини-играм в тестовом профиле:

```powershell
# Сначала закрыть игру, чтобы она не перезаписала профиль при выходе.
python scripts/unlock_test_minigames.py runtime-data/minigames-test/user
```

Скрипт сохраняет все три исходные настройки в отдельной папке
`runtime-data/minigames-test/save-backups/<дата-время>` и записывает отчёт
`runtime-data/minigames-test/unlock-minigames.json`.

Нативный экран `ChallengeScreen` открывает строку с индексом N при
`N < число трофеев + 3`; последней строке нужен минимум 17 трофеев.
Отдельного флага разблокировки последних пяти мини-игр в сохранении нет.
Поэтому скрипт отмечает пройденными Last Stand, Zombotany 2,
Wall-nut Bowling 2, Pogo Party и Dr. Zomboss's Revenge (ID 30–34).
В текущем тестовом профиле счётчик меняется с 12/20 на 17/20, и весь
список становится доступен. Это подготовка профиля для тестирования,
а не результаты настоящего прохождения. Положительные рекорды,
прогресс Adventure, монеты и остальные настройки сохраняются.

Разметка подтверждена `PlayerInfo::SyncDetails` (`821D9030`): версия 19,
100 записей big-endian int32 начинаются с байта `0x10` настройки
`63E83FFF`. Меняются только пять нулевых записей по смещениям
`0x88`, `0x8C`, `0x90`, `0x94`, `0x98`. При повторном запуске скрипт
не меняет уже положительные значения. Обычный профиль `runtime-data/user`
и исходные файлы Xenia в `save` остаются без изменений.

Проверка разблокировки 7 октября 2026: пять отдельных копий профиля,
по 38 секунд на Last Stand, ZomBotany 2, Wall-nut Bowling 2, Pogo Party
и Dr. Zomboss's Revenge. Во всех случаях нажатие мышью дало нативную
активацию `locked false` и отрисовку соответствующего Board либо выбора
семян, без аварий. Полное прохождение в эту проверку не входит.
Отчёт: `runtime-data/minigames-unlock-validation.json`, проверка журнала:
`scripts/check_minigames_unlock.py`. Журналы: smoke-20261007-125224-500,
125314-617, 125352-975, 125431-233 и 125509-522.
Рабочий тестовый профиль изменён после успешной проверки копий;
резервная копия — `runtime-data/minigames-test/save-backups/20261007-125600-003260`.

9 октября 2026 тестовый профиль с 17/20 трофеев также импортирован в
Xenia Canary из `F:\Cracks\xenia_canary_windows`. Установленная версия
`canary_experimental@0b041cd2a` хранит настройки в `584109FF.gpd` активного
профиля `E03000001173D07C`, а не в отдельных файлах старого формата.
Скрипт `scripts/export_test_profile_to_xenia.py` добавляет бинарные настройки
в секцию 3 XDBF с заголовками 24 байта и обновляет таблицу свободного места.
Разметка сверена с исходниками установленной версии:
[GpdInfo](https://github.com/xenia-canary/xenia-canary/blob/0b041cd2a/src/xenia/kernel/xam/xdbf/gpd_info.cc),
[XdbfHeader и XdbfEntry](https://github.com/xenia-canary/xenia-canary/blob/0b041cd2a/src/xenia/kernel/xam/xdbf/xdbf_io.h).

```powershell
python scripts/export_test_profile_to_xenia.py `
  runtime-data/minigames-test/user/584109FF/profile/User `
  'F:\Cracks\xenia_canary_windows\content\E03000001173D07C\FFFE07D1\00010000\E03000001173D07C\584109FF.gpd' `
  --backup-root 'F:\Cracks\xenia_canary_windows\save-backups' `
  --report runtime-data/xenia-test-save-import.json
```

Перед записью скрипт требует закрыть Xenia, проверяет формат и версию
профиля, сохраняет исходный GPD и выполняет замену через временный файл.
Все три перенесённые настройки совпадают с исходными побайтно; 14 остальных
записей GPD, включая достижения и изображение игры, сохранены вместе с их
исходными смещениями. Проверены повторный импорт и отказ на семи вариантах
повреждённого GPD: всего 9/9 проверок. Исходный тестовый профиль не изменён.
Отчёт: `runtime-data/xenia-test-save-import.json`.

Резервная копия предыдущего профиля Xenia:
`F:\Cracks\xenia_canary_windows\save-backups\pvz-test-20261009-103306-885648\584109FF.gpd`.
Для восстановления закрыть Xenia и скопировать этот файл на путь назначения
из `RESTORE.txt` в той же папке.

Для проверки добавлен `F:\Cracks\xenia_canary_windows\Start-PvZ-Test.bat`:
запускает существующий XBLA-контейнер PvZ с `--license_mask=1` и явным
`--storage_root`, чтобы использовать импортированный профиль. Общий
`xenia-canary.config.toml` не редактировался; параметры командной строки
хранятся отдельно от сохраняемых значений конфигурации в
[ConfigVar](https://github.com/xenia-canary/xenia-canary/blob/0b041cd2a/src/xenia/base/cvar.h).
Эта проверка подтверждает структуру и содержимое сохранения; запуск и
ручная проверка мини-игр в Xenia пока не выполнялись.
