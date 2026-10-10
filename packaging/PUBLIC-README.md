# pvz-console-edition

Windows x64, версия 0.1.0. Управление мышью; Escape — пауза, ПКМ — отмена.
Запуск: `Start.bat`. Режим мыши меняется в Options → PC Settings.

Архив содержит порт и необходимые библиотеки. Игровые файлы нужно
импортировать из своей Xbox-копии; в архив они не включены.
Для разового импорта нужен Python 3.10+. Самой игре Python не требуется.

Из PowerShell в папке распакованного архива:

```powershell
.\Import-Game.ps1 -Package 'C:\Games\PvZ\584109FF\000D0000\container' -Edition Trial
```

Для собственной купленной полной копии:

```powershell
.\Import-Game.ps1 -Package 'C:\Games\PvZ\584109FF\000D0000\container' -Edition Full
```

Ответьте `YES` на подтверждение владения. Импорт проверяет файлы, а не
покупку в Xbox Live. Без подтверждённого импорта игра работает в Trial.
Поддерживается Xbox TitleID 584109FF, версия 0.0.1.4;
точные SHA256 указаны в `config/game.lock.json`.

Подробнее: `docs/distribution.md`. Сохранения: `runtime-data/user`.
После изменения редакции перезапустите игру.
