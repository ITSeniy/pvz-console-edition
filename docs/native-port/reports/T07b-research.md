# T07b — эталон автоповтора: расследование, патч отложен

## 1. Итог

**Исправление не реализовано.** По Xbox-коду установлен источник опроса:
цикл app pump, который не имеет фиксированного отношения ни к logic update,
ни к present. Поэтому из порога 25 нельзя получить единственную эталонную
задержку в миллисекундах. Оригинальный контейнер запущен в локальной Xenia:
счётчик входных опросов косвенно показывает 172,91/с с `vsync=true` и 320,25/с
с `vsync=false`, при примерно 100 логических шагах/с в обоих случаях.

Это не замер удержания и не измерение на физической Xbox. После предъявления
этих результатов пользователь выбрал: **«Передать результаты бригадиру, патч
пока не вводить»**. Нормализация к произвольным 60/100/200 Гц не внесена;
критерии исправления T07b не объявляются выполненными.

Дата: 17.10.2026. Отдельный worktree:
`C:/Users/ITSeniy/.codex/worktrees/t07b-gamepad-repeat/PVZXboxRecomp`.
HEAD: `eecac622fa8987a25a4b5e8ac464cbdb53846f1d`.

## 2. Изменённые файлы

Единственный результат T07b — этот отчёт. `src/gamepad_repeat.cpp` не создан,
`CMakeLists.txt`, `src/`, реплеи и `scripts/check_*` не менялись.
Статусы README не менялись, коммитов, переключений веток и переноса T05b нет.

В worktree перенесён снимок исходных незакоммиченных результатов прежней волны.
Собственные `.cache/` и `generated/` — обычные копии, не junction на основное
дерево; зависимости и игровые данные подключены для чтения. Выполнен только
`build.bat configure` (успешно); pvz/codegen не собирались. Снимок до исследования:
`runtime-data/t07b/setup.json`; итоговая сверка — `runtime-data/t07b/final-review.json`.
Локальные диагностические скрипты и сырые замеры — в игнорируемом
`runtime-data/t07b/` и не подключены к игре.

## 3. Как воспроизвести статическую часть

Из основного дерева проекта, только чтение:

```powershell
python -I scripts/name_xbox_from_android.py --inspect 82580d88
python -I scripts/name_xbox_from_android.py --inspect 8247cb58
python -I scripts/name_xbox_from_android.py --inspect 824799e0
python -I scripts/name_xbox_from_android.py --inspect 82475c80
python -I scripts/name_xbox_from_android.py --inspect 82475d78
python -I scripts/name_xbox_from_android.py --inspect 822ca100
python -I scripts/name_xbox_from_android.py --inspect 82146a60
```

SHA-256 `.cache/pvz-image.bin`:
`27e8e044a0b7e232ce43e1de2cf06b377040f3d24e67a86047b337bb46d257ea`.
Входной `name-map-golden-2.json` сохранил ранее зафиксированный SHA:
`3d0276f24b6e2e0da2eb097d1fcbce9fffd1a9656182f2c26314400f6777338f`.

## 4. Результаты

### 4.1. Источник шага — проверено на Xbox

| Адрес | Что делает исходный PPC |
|---|---|
| `8247CB58`, сброс `8247CB94..CBA0` | App pump переводит stage `app+550` из 3 в 0. |
| `8247CCD4..CCFC`, вызов `8247CCF4` | При stage 0 обходит четыре Gamepad из `app+34..40`, вызывает slot `+38` vtable `8207DA7C`, то есть `82580D88`; затем stage=1. |
| `8247CCDC..CCE4`, константа `82000A94` | Входной f1=0.0. Gamepad::Update не использует его для продвижения счётчиков повтора. |
| `8247CD88..CDA0` | Вызывает scheduler `824799E0` через app virtual slot +20. |
| `82479A98..82479AFC` | При `app+6B4==0` длительность логического шага = `int(app+51C)/double(app+558)`; конструктор задаёт 10 мс. В другой ветви длительность = `(1000.0/int(app+6B0))/double(app+558)`. |
| `82475C80` | Прибавляет разность guest tick `822CA100` к аккумулятору `app+530`, ограничивает его 200 мс; сохраняет предыдущее время в `app+538`. |
| `82479E70..E80` | В stage 1 вызывает `DoUpdateFrames` (`82478108`) только если в аккумуляторе достаточно времени. |
| `82479ED0..F1C` | В stage 2 возможны дополнительные логические шаги без нового опроса Gamepad. |
| `82479F94`, `82479FB4`, `82479FF8` | Даже без логического шага stage переходит в 3; дальше Draw либо sleep через `822CB310`. Новый pump снова сможет опросить Gamepad. |
| `82475D78` | Внешний цикл вызывает virtual +184 (pump), пока out-параметр не сообщает о состоявшемся update. |

Таким образом, **опрос не является ни «раз в present», ни «раз в logic»**.
В idle-цикле возможен дополнительный опрос без logic, а при catch-up — несколько
logic без опроса. Величина 10 мс относится к логике, а не к периоду Gamepad.
Функция `82479860`, встречающаяся в scheduler, обрабатывает safe-delete list
`app+588`; это не ещё один таймер. `82583148` после update — пустой `blr`,
его также нельзя считать вторым опросом.

Для сверки с generated: pump и accumulator — `pvz_recomp.79.cpp`, scheduler —
`pvz_recomp.112.cpp`, loop wrapper — `pvz_recomp.34.cpp`, Gamepad —
`pvz_recomp.92.cpp`. Доказательство основано на PPC, generated служит навигацией.

### 4.2. Единицы повтора — проверено на Xbox

В `82580DDC..E60` удерживаемая кнопка увеличивает `Gamepad+3C+4*i` на единицу
**за вызов**. Сравнение строгое `counter > threshold`; для направлений
threshold=25. При срабатывании растёт repeat-count `+8C+4*i` (до 6), а counter
устанавливается в новый repeat-count. Listener получает `r6=1`; новые down/up
обрабатываются отдельными ветвями той же процедуры.

После начального counter=0 первый повтор требует 26 продвижений; следующие
интервалы — 25, 24, 23, 22, 21 и затем 20 опросов. Это точные единицы Xbox.
Пересчёт в миллисекунды без известного расписания pump был бы предположением.
Например, 260 мс при 100 Гц или 433,3 мс при 60 Гц — только арифметика для
**новой политики**, не установленный консольный эталон.

### 4.3. Сверка в Xenia — косвенное подтверждение, пределы измерения

Использована уже установленная Xenia Canary `e601b5ab8` от 29.05.2025:
`C:/Users/ITSeniy/Downloads/xenia_canary_windows (1)/xenia_canary.exe`.
SHA-256 EXE: `64766e1d9bdf4525a3d565c8b3968d6515bd48a4e3dea6c866e63498adeb17ba`.
Сторонние бинарники не скачивались и драйверы не устанавливались.

Запущен исходный XBLA-контейнер, не recompiled EXE. Storage/content/cache/log
перенаправлены в `runtime-data/t07b/xenia/`; лог подтверждает **0 title patches**.
Параметры: `headless=true`, `license_mask=-1`, `hid=winkey`, `keyboard_mode=1`,
`time_scalar=1`, далее два отдельных запуска с vsync true/false.
Все запущенные исследованием Xenia-процессы остановлены.

WinKey увеличивает packet_number при каждом GetState даже при отпущенных кнопках:
[исходник именно e601b5ab8, строки 141–147 и 242](https://github.com/xenia-canary/xenia-canary/blob/e601b5ab8/src/xenia/hid/winkey/winkey_input_driver.cc#L141-L147).
В guest он виден в `Gamepad+17C`. ReadProcessMemory читает его и `app+54C`
приблизительно раз в 1–2 мс, не записывая guest-память.

| Валидный замер | Окно, с | Samples | Изменение packet | Изменение logic | Packet proxy, Гц | Logic, Гц |
|---|---:|---:|---:|---:|---:|---:|
| `checked-vsync-true.csv` | 9,999695 | 6486 | 1729 | 999 | 172,9053 | 99,9031 |
| `checked-vsync-false-active.csv` | 9,998487 | 6536 | 3202 | 1000 | 320,2485 | 100,0151 |

В каждом валидном sample подтверждены: Gamepad vtable `8207DA7C`, user=0,
Board=null, `app+AB4=FFFFFFFF`; ошибок ReadProcessMemory=0. Остальные прямые
callsite `822CB588` находятся по `821B0190/1B0`, `821CA010`, `822099BC`;
их ветви требуют Board или присвоенного `app+AB4`. Проверки этих двух полей
уменьшают риск учёта постороннего GetState, но packet остаётся **прокси**, а не
трассировкой входов в `82580D88`. Значение `app+6B0=60` — гостевое поле,
не измеренная частота физического монитора. `app+6B4=0` в обоих окнах.

Сырые CSV, JSON с границами/аргументами и probe — `runtime-data/t07b/xenia/`.
Самая ранняя проба `packet-poll.csv` дала 173,02 Гц, но не имела всех проверок
валидности и содержала один переход на 0; она не используется как основной замер.
`checked-vsync-false.csv` попала в загрузку до pump (оба счётчика=0) и также
**исключена**, а не выдана за частоту без VSync.

Удержания 50/150/300/1000 мс не вводились. Попытка использовать имеющуюся .NET
ViGEm-библиотеку закончилась `VigemBusNotFoundException` до создания контроллера;
отсутствующий драйвер не устанавливался. Поэтому фактическая задержка первого
повтора не измерена. Headless Xenia не доказывает частоту на физической Xbox,
но наблюдаемое расписание согласуется с отсутствием фиксированного poll/logic.

### 4.4. Связь с частотой рисования — частичная проверка

Установлен другой счётчик, растущий от рисования: `Board+560C`.
`Board::Draw 82146A60` инициализирует его по `82146B74`, затем увеличивает по
`82146B80..94`. `+5824/+5820/+5828` в соседней ветви участвуют в оценке частоты
рисования по guest tick, а не в счётчике логики. CutScene читает draw-count
в ветви `8217C7D8..E0` (внутри `8217C5C8`); зависимость первого update от первого
Draw уже исследована T05b. Это не доказательство общего ускорения игрового мира
при 100 FPS. Аудит остальных update/draw-потребителей и тест 60/100 Гц не завершены.

## 5. Критерии приёмки

| Критерий | Статус |
|---|---|
| Эталон по Xbox-коду, адреса и источник шага | **Частично:** цикл и единицы установлены; фиксированной частоты и единственного интервала в мс код не задаёт. Физическая Xbox не измерена. |
| Совпадение задержки/интервала с эталоном при 60/100 Гц | **Не выполнен:** патч и матрица отложены по решению пользователя. |
| 50 мс даёт один шаг, нет потерянных нажатий | **Не проверен для исправления:** исправления нет. T07 содержит исходные наблюдения порта. |
| Default прежний, build.bat test и регрессии | **Изменений поведения нет:** src/CMake/реплеи неизменны. В T07b игровые регрессии не запускались; общий CTest основного дерева после T04 — 13/13, это не тест T07b-патча. |
| pc_settings_back пять раз с флагом | **Не выполнен:** флаг не добавлен. Проверки и ожидаемые исходы не менялись. |

## 6. Не проверено и риски

- Нет инструментального измерения физической Xbox и прямой трассы Gamepad entry
  либо listener-repeat в Xenia. Частоты из таблицы нельзя объявлять консольным нормативом.
- Нет матрицы D-pad/stick, pause/menu, обоих Co-op-пользователей, cursor/card cycle,
  переходов mouse/pad, VSync 60/100 и no-VSync для предлагаемого патча.
- Внешний семплер и эмулятор могут менять расписание; partial polling не заменяет
  тест доставленных событий. Один усреднённый показатель не фиксирует jitter.
- Порог 25 сам по себе не доказывает, что желательны 250/260 либо 417/433 мс.
  Нужен утверждённый UX-норматив или измеренный консольный baseline.

## 7. Передача бригадиру

T02, T04 и T05c сдаются своими отчётами. По T07b установлен важный предел:
сам оригинал привязывает повтор к переменному app pump. Требование «вернуть
эталонную частоту» пока не определяет единственный таймер. **Патч не вводить** —
актуальное решение пользователя. Для возобновления требуется новое решение
бригадира: конкретная целевая задержка/ускорение в мс либо дополнительный
измеренный baseline на консоли. Старый декремент/whitelist из T07 не применялся.
T06 и перенос T05b/T05c в основное дерево не начинались.

## Приложение. Воспроизведение Xenia-пробы

Из T07b worktree; использовать новые имена файлов, чтобы сохранить старые замеры.
Сначала сохранить Python из `runtime-data/t07b/xenia/packet_checked.py`, полный
текст которого приведён ниже. Запускать только один игровой процесс за раз.

```powershell
$t07Xenia = 'C:/Users/ITSeniy/Downloads/xenia_canary_windows (1)/xenia_canary.exe'
$t07Run = "$PWD/runtime-data/t07b/xenia-review"
New-Item -ItemType Directory -Force $t07Run | Out-Null
$t07Game = 'F:/src/PVZXboxRecomp/Plants vs. Zombies (World) (XBLA)/584109FF/000D0000/281104A67F7C961E2736A5F6F524D43FC81EB79158'
$t07Args = @(('"'+$t07Game+'"'), ('--storage_root="'+$t07Run+'"'),
  ('--content_root="'+$t07Run+'/content"'), ('--cache_root="'+$t07Run+'/cache"'),
  ('--log_file="'+$t07Run+'/game.log"'), '--headless=true', '--license_mask=-1',
  '--hid=winkey', '--keyboard_mode=1', '--vsync=true', '--time_scalar=1')
$t07Process = Start-Process $t07Xenia -ArgumentList $t07Args -WorkingDirectory $t07Run -WindowStyle Hidden -PassThru
@{pid=$t07Process.Id;arguments=$t07Args} | ConvertTo-Json | Set-Content runtime-data/t07b/xenia/process.json
try {
  python -I runtime-data/t07b/xenia/packet_checked.py "$t07Run/checked.csv" 10
} finally {
  if (!$t07Process.HasExited) {$t07Process.Kill(); $t07Process.WaitForExit()}
}
# Для второго отдельного процесса: новое t07Run, --vsync=false.
```


<!-- T07B_PROBE_SOURCE -->
```python
"""Read-only original-Xenia probe; run from the T07b worktree.

Args: output CSV, seconds (default 10). Process receipt contains the PID.
Measures the WinKey packet proxy, not a physical controller hold.
"""
import ctypes as c
import csv
import json
import sys
import time
from pathlib import Path

k = c.WinDLL('kernel32', use_last_error=True)
k.OpenProcess.argtypes = [c.c_uint32, c.c_bool, c.c_uint32]
k.OpenProcess.restype = c.c_void_p
k.ReadProcessMemory.argtypes = [c.c_void_p, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
k.CloseHandle.argtypes = [c.c_void_p]
receipt = json.loads(Path('runtime-data/t07b/xenia/process.json').read_text())
h = k.OpenProcess(0x10, False, receipt['pid'])
if not h:
    raise OSError(c.get_last_error(), 'OpenProcess')

def read(address, size=4):
    data = c.create_string_buffer(size)
    n = c.c_size_t()
    if not k.ReadProcessMemory(h, 0x100000000 + address, data, size, c.byref(n)) or n.value != size:
        raise OSError(c.get_last_error(), 'ReadProcessMemory')
    return data.raw

def word(address):
    return int.from_bytes(read(address), 'big')

target = Path(sys.argv[1])
duration = float(sys.argv[2]) if len(sys.argv) > 2 else 10
rows = []
invalid = 0
try:
    deadline = time.perf_counter() + 30
    while True:
        try:
            app = word(0x82D0BC88)
            if (app and word(app) == 0x820C013C and word(app + 0x54C) > 0
                    and word(word(app + 0x34) + 0x17C) > 0):
                break
        except OSError:
            pass
        if time.perf_counter() >= deadline:
            raise RuntimeError('Original LawnApp was not ready')
        time.sleep(.1)
    time.sleep(2)
    start = time.perf_counter()
    while time.perf_counter() - start < duration:
        try:
            pad = word(app + 0x34)
            if word(pad) != 0x8207DA7C or word(pad + 4) != 0:
                raise RuntimeError('Unexpected Gamepad ABI')
            board, ab4 = word(app + 0x900), word(app + 0xAB4)
            if board != 0 or ab4 != 0xFFFFFFFF:
                raise RuntimeError('Other GetState callers may be active')
            rows.append([time.perf_counter() - start, word(pad + 0x17C),
                         word(app + 0x54C), word(app + 0x550), word(app + 0x6B0),
                         read(app + 0x6B4, 1)[0], board, ab4])
        except OSError:
            invalid += 1
        time.sleep(.001)
finally:
    k.CloseHandle(h)

with target.open('x', newline='') as out:
    writer = csv.writer(out)
    writer.writerow(['seconds', 'packet', 'logic', 'stage', 'guest_display_hz',
                     'guest_vsync_mode', 'board', 'user_ab4'])
    writer.writerows(rows)
elapsed = rows[-1][0] - rows[0][0]
result = {'pid': receipt['pid'], 'arguments': receipt['arguments'],
          'samples': len(rows), 'invalid_reads': invalid, 'elapsed': elapsed,
          'packet_delta': rows[-1][1] - rows[0][1],
          'logic_delta': rows[-1][2] - rows[0][2],
          'packet_proxy_hz': (rows[-1][1] - rows[0][1]) / elapsed,
          'logic_hz': (rows[-1][2] - rows[0][2]) / elapsed,
          'first': rows[0], 'last': rows[-1],
          'note': 'No held input; packet proxy only; not physical Xbox frequency.'}
target.with_suffix('.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
```

## Приёмка и решение бригадира (17.10.2026)

**Итог: исследование принято; решение о целевой частоте принято бригадиром, см. карточку T07b.**

- Важный вывод подтверждён по коду: оригинал сам опрашивает Gamepad раз за проход app pump.
  Частота pump не привязана ни к логике, ни к present (Xenia: около 173 Гц с VSync, около
  320 Гц без VSync). Следовательно, «консольного эталона» в миллисекундах нет: на консоли
  задержка повтора тоже зависела от загрузки цикла. Сам порог в опросах (26, затем 25…20)
  установлен точно.
- Решение: продвигать счётчик повтора **один раз за логический шаг (100 Гц)**. Причины:
  поведение не зависит от FPS и VSync (связано с жалобой «ускорение при 100 FPS»); нажатие
  до 260 мс даёт ровно один шаг, а обычное нажатие длится 80–200 мс; это точно та же
  механика оригинала (порог и ускорение), только на стабильном такте.
- Реализация — под флагом. Включение по умолчанию — после ручной проверки владельцем на
  физическом геймпаде.
