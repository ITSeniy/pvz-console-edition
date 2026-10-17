# Граница рендера Xbox: T08

Исследовано 17.10.2026 на Xbox-образе SHA-256
`27e8e044a0b7e232ce43e1de2cf06b377040f3d24e67a86047b337bb46d257ea`.
Вывод: **нативный бэкенд возможен с условиями**. Одних `Graphics::DrawImage`
недостаточно: нужны примитивы Image/DDImage, матрицы, треугольники, bitmap-font,
render targets и обработка непосредственных изменений состояния D3D. Полного
проверенного ABI всех операций пока нет; это проектная граница, не готовый backend.

«Проверено» ниже означает Xbox-инструкции, RTTI или измеренный запуск. Имена
Android/PC сами по себе доказательством Xbox не считаются. Псевдосигнатуры
показывают смысл аргументов, а не готовые декларации для `REX_IMPORT`.

## Проверенная часть API

Xbox имеет RTTI/vtable Graphics `82063ABC`, Image `82064A5C`, MemoryImage
`82068FCC`, DDImage `82068DE4`, CanvasImage `82069524`, ImageFont `8207A134`,
D3DInterface `82063EC4`, NativeDisplay `8207F2A4`. У Graphics/D3DInterface/
NativeDisplay в найденных таблицах только один виртуальный destructor: искать
весь draw API через выравнивание vtable здесь невозможно.

| Xbox | Операция / псевдосигнатура | Доказательство Xbox |
|---|---|---|
| `8249B1A0` | `Graphics::DrawString(string*, x, y)` | Font из Graphics+40; вызов Font vtable+28, передача Graphics, цвета и клипа |
| `8249B1D8` | `Graphics::DrawImageMatrix(image*, matrix*, srcRect*)` | Translation +8/+C, clip +20, tint +30, draw mode +44 → Image vtable+3C |
| `8249B408` | `Graphics::DrawTrianglesTex(image*, vertices*, triangleCount)` | Translation, tint/clip → Image vtable+48. Pool передаёт 150 треугольников по `821DA7AC..858` |
| `8249B4A0` / `8249B4E8` | `Translate(int,int)` / `TranslateF(float,float)` | Прибавление к float-полям +8/+C; у первой integer→float |
| `8249B508` | `SetScale(float sx,sy,ox,oy)` | Запись +10/+14 и origin с учётом translation в +18/+1C |
| `8249B588` | `ClearRect(x,y,w,h)` | Пересечение с clip, Image vtable+14 |
| `8249B650` | `FillRect(x,y,w,h)` | Alpha-gate по цвету +3C, пересечение с clip, Image vtable+C с цветом/draw mode |
| `8249BC50` | `DrawImage(image*,x,y,srcRect*)` | Проверка границ source; translation/scale/clip; Image Blt либо StretchBlt |
| `8249C150` | `DrawImage(image*,destRect*,srcRect*)` | Конвертация destRect в float с translation, Image vtable+34 |
| `8249C230` | `DrawImageF(image*,float x,y,srcRect*)` | Image vtable+2C, масштабируемая ветвь +34 |
| `8249C378` | `DrawImageRotatedF(image*,x,y,rotation,srcRect?)` | Translation, опциональный полный source rect, Image vtable+30; точный тип rotation ABI ещё не закреплён |
| `8249C430` | `DrawImageMatrix(image*,matrix*)` | Строит полный source rect по Image+24/+28; Image vtable+3C |
| `8249C4E0` | `ClearClipRect()` | Восстановление сохранённого clip либо `(0,0,dest.width,dest.height)` |
| `8249C590` | Прямое присваивание clip `(x,y,w,h)` | Четыре записи в +20..2C; точное исходное C++-имя не установлено |
| `8249C5D0` / `8249C690` | `SetClipRect(x,y,w,h)` / Rect-wrapper | Сброс clip и пересечение; wrapper раскрывает Rect |
| `8249C6A8` / `8249C760` | `ClipRect(x,y,w,h)` / Rect-wrapper | Translation и пересечение с текущим clip; wrapper раскрывает Rect |
| `8249D0E8` | `DrawImage(image*,int x,y)` | Width/height изображения, scale==1 fast path и вызов BC50 |
| `8249D250` | `DrawImageCel(image*,int x,y,cel)` | Передаёт индекс ячейки в BEF0; тот строит source rect и вызывает BC50 |
| `8249D388` | DrawImageCel с float-координатами | Подготовка source rect и вызов C230; не объявлять матричным API по комментарию порта |
| `824CB1C0` | `DDImage::BltMatrix(...)` | DDImage slot15; `824CB264` → D3D `824A3FB8`, либо software fallback |
| `824CB300` | `DDImage::BltTrianglesTex(...)` | DDImage slot18; `824CB3A0` → D3D `824A45C0` |
| `824CC9D0` / `824CCBD8` | `DDImage::Blt(...)` / `BltF(...)` | Slots10/11; соответственно D3D `824A4E48` / `824A49D8` |
| `824CCD90` | `DDImage::BltRotated(...)` | Slot12, D3D `824A4CF0` и большой software fallback |
| `824D0DD0` / `824CFDB0` | DDImage stretch/mirror paths | Slots13/14; D3D `824A4BC0` / `824A4A78`; точный порядок всех аргументов не восстановлен |
| `824D5ED0` | MemoryImage fill path | Slot3, запись CPU-пикселей; нельзя заменить только GPU-ветвь |
| `8255BD80` | `ImageFont::DrawString(font*,graphics*,x,y,string*,color*,clip*)` | ImageFont slot10 соответствует вызову B1A0; далее виртуальная передача в slot14 `8255D8F0` |
| `8255D8F0` | Расширенная glyph/layer отрисовка ImageFont | `8255E4B4` → Graphics BC50 для glyph-rect. Полная логика layout ещё не восстановлена |
| `824A3FB8` | D3D матричный blit | Вызывается DDImage::BltMatrix и напрямую TodBltMatrix; последовательность XDK draw/setup calls |
| `824A45C0` | D3D textured triangle batch | DDImage triangles и прямой TodTriangleGroup flush; XYZ/UV/color данные и количество |
| `822B2138` | D3D swap/present | Единственный `VdSwap` caller; существующий хук startup_video используется как граница интервала |

Дополнительные discovery-пробы (без присвоения достоверного имени) включены в
машинные данные. Не следует автоматически экспортировать их как high/medium.
Например, `8255D4D8`/`8255CE78` — font metrics/layer preparation, а совпадение
слота/формы ещё не проверяет всё исходное имя. NativeDisplay destructor
`82588508` установлен RTTI; полный самостоятельный API этого класса не восстановлен.

## Состояние Graphics

**Проверено:** `82499FA8` копирует GraphicsState, `8249DB80` строит Graphics
(vtable `82063ABC`, state с +4). Существующий Graphics нельзя заменять структурой
Android/PC: смещения и дополнительные члены различаются.

| От Graphics | Тип/роль | Подтверждение |
|---:|---|---|
| +4 | Image* destination | Все primitive dispatch paths |
| +8/+C | float translation X/Y | B4A0/B4E8 |
| +10/+14 | float scale X/Y | B508 и BC50/C230 |
| +18/+1C | float scale origin | B508 и scale-ветви |
| +20..2C | clip Rect, четыре BE s32 | C590/C6A8 |
| +30..3C | Color, четыре BE слова | SetColor `8249A0C0`, FillRect проверяет alpha +3C |
| +40 | Font* | Get/SetFont `8249A0B0/B8`, B1A0 |
| +44 | draw mode | Get/SetDrawMode `8249A0F8/F0`; additivity передаётся Image/D3D |
| +48 | colorize images, byte | `8249A100/108`, выбор tint/white в blit |
| +49 | fast stretch, byte | `8249A110/118`, stretch path |
| +4B | linear blend/filter flag, byte | `8249A120/128`, matrix/triangle paths |
| +4D, +50..5C, +60..6C | Дополнительное состояние сохранённого clip | C4E0/B478; полный контракт stack/restore ещё не разобран |

Прямые записи этих полей бывают inline. Перехват только setters не даёт
полной картины: бэкенд должен читать актуальный state на draw либо полностью
замещать согласованную совокупность операций. Числовую семантику всех режимов
смешивания и linear/fast флагов ещё нужно сравнить с Xenos.

## Измерения на запуске

**Проверено:** EXE `554ca10e65ef3cd35cc2aaed7551fa1bfb3bd1b25b2ae1c4198fabf1184cc921`,
68 entry-проб, по **180 полных present-интервалов**, четыре независимых процесса.
Источник — `runtime-data/render-boundary/20261017-102807-826/runs.json`,
агрегат `research/cross-platform/render-boundary/measured.json`.
Итого 720 интервалов и 884715 входов в измеряемые функции; счётчики вложенные,
это **не** количество GPU draw calls. Игра работала с текущими хуками порта.

Menu: scene1 без Board; Adventure: mode0/scene3; Zombiquarium: mode24/scene3;
Versus: mode76/scene3. Board-сцены не paused. Взято первое полное окно после
достижения gate, поэтому Adventure показывает раннюю стадию, не плотную волну.
CSV, копии профилей, логи и кадры перечислены в runs.json. Replay не обязан
закончиться внутри измеряемого окна. Повторность/статистическая стабильность
частот на других кадрах не утверждается.

| Entry / роль | Menu | Adventure | Zombiquarium | Versus |
|---|---:|---:|---:|---:|
| B1A0 DrawString | 19,000 | 0 | 0 | 2,000 |
| B408 triangles | 0 | 4,000 | 0 | 8,000 |
| B4A0 Translate | 45,000 | 8,644 | 9,000 | 7,661 |
| B508 SetScale | 19,000 | 1,000 | 1,000 | 4,000 |
| B650 FillRect | 0 | 1,000 | 1,000 | 0 |
| BC50 source-rect blit | 0 | 69,000 | 90,000 | 32,000 |
| C150 rect→rect | 0 | 0 | 2,000 | 0 |
| C230 float blit | 0 | 0 | 0 | 0 |
| C378 rotated | 0 | 0 | 0 | 0 |
| C430 matrix | 0 | 0 | 0 | 6,000 |
| D0E8 full-image blit | 1,000 | 6,000 | 6,000 | 7,000 |
| D250 cel | 1,000 | 0 | 0 | 0 |
| D388 float cel | 19,000 | 0 | 0 | 4,000 |
| 824CB1C0 DDImage matrix | 153,822 | 126,089 | 36,000 | 425,144 |
| 824CB300 DDImage triangles | 0 | 0 | 0 | 8,000 |
| 824A3FB8 D3D matrix | 161,822 | 126,089 | 39,000 | 462,144 |
| 824A45C0 D3D triangles | 0 | 1,072 | 0 | 8,433 |
| 8255BD80 ImageFont draw | 0 | 4,000 | 6,000 | 13,000 |
| 821E0828 Reanimation track | 277,000 | 269,200 | 62,000 | 624,172 |
| 821E1560 Reanimation group | 6,000 | 16,222 | 2,000 | 30,644 |
| 8227C240 Tod matrix | 153,822 | 126,089 | 36,000 | 419,144 |
| 82186540 triangle-group flush | 6,000 | 18,944 | 2,000 | 31,111 |

Все числа — среднее entry-вызовов за present-интервал, округление до 0,001;
целые суммы/min/max всех 68 адресов сохранены в JSON. Ноль не доказывает,
что операция вообще не используется игрой. Lifetime-данные отдельно включают
загрузку: LoadPTXImage был вызван 5311/5391/5342/5414 раз соответственно,
но во всех четырёх измеряемых окнах — ноль.

Ограничения счётчиков: вход через `sub_X` не замечает прямой вызов `__imp__sub_X`
из самого порта; например, native draw-preview может делегировать original прямо.
Поэтому таблица считает symbol entry, не все вызовы оригинального тела.
`outside_graphics` означает отсутствие **измеренного** Graphics entry в thread-local
стеке, а не автоматически доказанный обход всего API. Callers ограничены 128
различными LR на адрес; overflow во всех четырёх итоговых запусках **0**.
Счётчики атомарные, интервал задаётся present-потоком, фоновые вызовы могут
попасть на соседнюю границу. Накладные расходы отдельно не профилировались.

## Обходы Graphics и D3D

**Проверено кодом и измерением:**

1. Reanimation group `821E1560` → track `821E0828` → atlas helper
   `821E0388` → TodBltMatrix `8227C240`. В зависимости от destination тот
   идёт через Image slot15 `824CB1C0` либо напрямую D3D `824A3FB8`.
   DrawImage hooks не покрывают этот путь. Для Tod matrix все вызовы окна
   прошли вне измеренных Graphics entry: 27688/22696/6480/75446.
2. `82186540` (роль TodTriangleGroup flush установлена по накопленным vertices,
   texture +0, count +6004, mode +6008) напрямую вызывает `824A45C0`
   по `821865F4`. Его вызывает в том числе Reanimation::DrawTrack. Software
   fallback использует Image vtable+48. В Versus 78 из 1518 backend triangle
   entries пришли вне измеренного Graphics стека; это наблюдение, не доля всех draw.
3. PoolEffect initialize `821D9CE8` подтверждён строкой `820C564C` и созданием
   128×64 caustic MemoryImage. Update pixels — `821D9EA0`; draw — `821DA128`
   (150 triangles × три слоя; сравнение с PC здесь только дополнительный справочник).
   `821DA7C8/7E0/7F8/858` вызывают Graphics triangles. Однако **между ними**,
   `821DA814..890`, код напрямую проходит `Graphics.dest+70 → +30 → +60` и
   меняет слово устройства `+480` и dirty-биты в `+18`. Точный смысл этих
   sampler/addressing битов пока гипотеза; их наличие и значения записей проверены.
   Следовательно, даже полный Graphics API нуждается в адаптации этого state path.
   В измеренном Zombiquarium B408 не вызван: приписывать его артефакт именно
   PoolEffect без дальнейшей проверки нельзя. Pool entries наблюдались в lifetime
   Adventure (LR `821DA7E4/7FC/85C`), а не обязательно в конечном трёхсекундном окне.
4. PTX loader напрямую создаёт XDK texture header и привязывает packed physical
   data; это ещё один обязательный шов при удалении Xenos.

Все найденные прямые `b/bl` в исследуемые D3D draw-операции (по всему списку
генерированных функций; косвенные вызовы этим поиском не покрыты):

| D3D target | Вызывающие функции Xbox |
|---|---|
| `824A3FB8` | `8227C240`, `824A48F8`, `824A49D8`, `824A4A78`, `824A4BC0`, `824A4CF0`, `824CB1C0` |
| `824A4370` | `824CAF18`, `824CB088` |
| `824A45C0` | `82186540`, `824A46E0`, `824A4DA8`, `824CB300` |
| `824A0FF8` / `824A1228` | `824CB440` / `824CB548` |
| `824A2A80` | `824C8B58`, `824C8BB8` |
| `824A2C58` / `824A2EA8` | `824C7B70` / `824C7AF0` |
| `824A49D8` | `824A4E48`, `824CCBD8` |
| `824A4A78` / `824A4CF0` | `824CFDB0` / `824CCD90` |
| `824A4BC0` | `824D0DD0`, `824D1DE8` |
| `824A4E48` | `824CC9D0` |

Не каждому дополнительному helper приписано исходное имя. Полнота относительно
всех XDK-функций/косвенных dispatch и всех сцен не доказана. Таблица перечисляет
именно найденные швы, а не обещает отсутствие прочих обходов.

## PTX, Image и пиксели до GPU

**Проверено по `82480430`:** loader строит `.ptx`-путь и через PakInterface
`824B2D20` получает `(data,size)`. В `82480620..30` указатель trailer равен
`data+size-16`; BE width/height записываются в Image+24/+28. Создаётся DDImage
через `824CB5C8`, TextureData связывается с Image+44; XDK header/data связываются
в `82617C48` и `82617CB8` (`824807BC/7CC`). Сначала доступен CPU compressed
payload, потом GPU-ссылка; отдельного RGBA буфера у каждого PTX ожидать нельзя.

T09 уже проверила декодирование базового уровня всех 3464 PTX: linear DXT5,
stride/padding и перестановка байтов 16-битных слов. Это источник пикселей для
собственного texture upload. Также нужны mutable MemoryImage (caustics, caches),
render-to-image, синхронизация CPU/GPU и сохранение lifetime/atlas cell metadata.
`controller_assets.cpp:LoadImage` особо показывает, что нынешние packed textures
живут в physical mapping; переносить эти Xenos addresses в host backend нельзя.

## Что уже перехвачено портом

Текущий T03 — исторический снимок до новых probes; здесь исходники перечитаны.
`mouse_bridge.cpp` перехватывает D0E8, BC50, C058, C430, B408, D388, CFB8,
DA90, D250 и Tod C190/C240; `challenge_mouse.cpp` — ClipRect C6A8.
Это корректировки input/cursor/glyph/atlas, не готовая абстракция renderer.
`seed_image_fixes.cpp` — Reanimation DrawRenderGroup и seed cache; `controller_assets.cpp`
— packed read и transform hooks; `startup_video.cpp` — swap/pacing.
Гостевой GPU и Xenos backend продолжают выполнять настоящую отрисовку.

## Сопоставление с PvZ-Portable

Прочитан перечень операций upstream на commit
[`04e3fdc935b2a250891d083ffe4ac82dd774e058`](https://github.com/wszqkzqk/PvZ-Portable/tree/04e3fdc935b2a250891d083ffe4ac82dd774e058),
без переноса реализации. [GLInterface.h](https://github.com/wszqkzqk/PvZ-Portable/blob/04e3fdc935b2a250891d083ffe4ac82dd774e058/src/SexyAppFramework/graphics/GLInterface.h)
и [GLImage.h](https://github.com/wszqkzqk/PvZ-Portable/blob/04e3fdc935b2a250891d083ffe4ac82dd774e058/src/SexyAppFramework/graphics/GLImage.h)
имеют blit/stretch/mirror/rotate/matrix, textured triangles/strips, color/blend,
clip, transform stack, texture creation/cache и flush. Эти семейства подходят
для большей части найденной Xbox-границы. Проект использует SDL и OpenGL ES 2.0;
его PC-бэкенд не доказывает совместимость с консольными атласами, pool-state или ABI.
Хеши двух прочитанных заголовков и перечень операций — `portable-operations.json`.
Код библиотек/портов в `src/` не копировался.

## Объём и следующий проверяемый шаг

Нужны шесть групп работ: (1) единый state/clip/transform и примитивы;
(2) textures/BC3/atlases/lifetime; (3) fonts и CPU/offscreen caches;
(4) Tod/Reanim matrix и triangle batching; (5) explicit pool/XDK state adapters;
(6) presentation и кадры для сравнения. Это перечень компонентов, не оценка дней.
Сначала стоит записывать host-команды параллельно Xbox и сверять изображения,
покрывая каждый найденный шов. До этого отказ от Xenos объявлять готовым нельзя.

## Воспроизведение диагностики

`src/render_probe.cpp` инертен без `PVZ_RENDER_PROBE_OVERLAY`. Обычная сборка
не включает этот файл. Для исследования используется только CMake cache injection:
пять **производных** TU включают исходники порта, переименовав лишь определения
15 выбранных hooks; probe передаёт вызов в прежний hook, затем тот решает, вызывать
ли оригинал. Таким образом сохраняются имеющиеся исправления. CMakeLists и
существующие src-файлы не редактируются. Исследовательский EXE находится отдельно.

```powershell
python -I research/cross-platform/render-boundary/build_overlay.py
& 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe' -S . -B build '-DCMAKE_PROJECT_pvz-console-edition_INCLUDE=F:/src/PVZXboxRecomp/research/cross-platform/render-boundary/overlay.cmake' '-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=F:/src/PVZXboxRecomp/research/cross-platform/render-boundary/bin'
./build.bat --target pvz
./research/cross-platform/render-boundary/run_probe.ps1
# Подставить новый runs.json, напечатанный runner:
python -I research/cross-platform/render-boundary/callgraph.py runtime-data/render-boundary/20261017-102807-826/runs.json
python -I research/cross-platform/render-boundary/inspect.py
python -I research/cross-platform/render-boundary/inspect.py 821da128 82480430

# Вернуть обычный build cache и EXE:
& 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe' -S . -B build -U CMAKE_PROJECT_pvz-console-edition_INCLUDE -U CMAKE_RUNTIME_OUTPUT_DIRECTORY
./build.bat --target pvz
./build.bat test
```

Большие исследовательские результаты и помощники находятся в игнорируемом
`research/cross-platform/render-boundary/`. Чтобы воспроизведение не зависело
от их сохранности, ниже приведены точные тексты помощников; сохранить каждый
в указанный путь и запускать с `-I`. Исходные game/SDK inputs только читаются.

### research/cross-platform/render-boundary/build_overlay.py

```python
"""Generate T08 derived wrapper TUs and a CMake end-of-configure injection.

No original source, game assets, generated source or CMakeLists is modified.
Run before configuring the existing build with the printed extra CMake argument.
Remove that CMake cache argument and rebuild to restore the normal executable.
"""
from pathlib import Path
import re,json,hashlib
ROOT=Path(__file__).resolve().parents[3]
OUT=Path(__file__).resolve().parent
probe=ROOT/'src/render_probe.cpp'
text=probe.read_text(encoding='utf-8')
points=re.findall(r'X\(([0-9A-F]{8}), (true|false), ([A-Za-z_0-9]+)\)',text)
assert points and len({p[0] for p in points})==len(points)
header=(ROOT/'generated/default/pvz_funcs.h').read_text()
for address,_,_ in points:
    assert 'sub_'+address in header,address
renamed={p[2].removeprefix('t08_body_') for p in points if p[2].startswith('t08_body_')}
inputs={probe.relative_to(ROOT).as_posix():hashlib.sha256(probe.read_bytes()).hexdigest()}
replacements=[]
consumed=set()
for source in sorted((ROOT/'src').glob('*.cpp')):
    raw=source.read_bytes(); content=raw.decode('utf-8-sig')
    hooks=re.findall(r'REX_HOOK_RAW\((\w+)\)',content)
    if not renamed.intersection(hooks): continue
    consumed.update(renamed.intersection(hooks))
    inputs[source.relative_to(ROOT).as_posix()]=hashlib.sha256(raw).hexdigest()
    wrapper=OUT/(source.stem+'_overlay.cpp')
    pre=['// Derived T08 TU: only selected hook DEFINITIONS are renamed.',
         '#include <rex/hook.h>','#undef REX_HOOK_RAW',
         '#define T08_RENAME_I(name) T08_RENAME_##name',
         '#define T08_RENAME(name) T08_RENAME_I(name)',
         '#define REX_HOOK_RAW(name) extern "C" REX_FUNC(T08_RENAME(name))']
    pre += ['#define T08_RENAME_'+name+' '+('t08_body_'+name if name in renamed else name) for name in hooks]
    pre += ['#include "'+source.as_posix()+'"']
    wrapper.write_text('\n'.join(pre)+'\n',encoding='utf-8')
    replacements.append((source.relative_to(ROOT).as_posix(),wrapper.as_posix()))
assert consumed==renamed,(renamed-consumed)
cmake=['function(t08_overlay)','  get_target_property(t08_sources pvz SOURCES)']
for original,derived in replacements:
    cmake+=['  list(REMOVE_ITEM t08_sources "'+original+'" "'+(ROOT/original).as_posix()+'")',
            '  list(APPEND t08_sources "'+derived+'")']
cmake+=['  set_property(TARGET pvz PROPERTY SOURCES "${t08_sources}")',
        '  target_sources(pvz PRIVATE "'+probe.as_posix()+'")',
        '  set_source_files_properties("'+probe.as_posix()+'" PROPERTIES COMPILE_DEFINITIONS PVZ_RENDER_PROBE_OVERLAY=1)',
        'endfunction()','cmake_language(DEFER CALL t08_overlay)']
overlay=OUT/'overlay.cmake';overlay.write_text('\n'.join(cmake)+'\n',encoding='utf-8')
(OUT/'build-inputs.json').write_text(json.dumps(dict(inputs=inputs,points=len(points),renamed=sorted(renamed)),indent=2)+'\n',encoding='utf-8')
print('-DCMAKE_PROJECT_pvz-console-edition_INCLUDE='+overlay.as_posix())
print('points',len(points),'renamed',len(renamed),'derived TUs',len(replacements))
```


### research/cross-platform/render-boundary/run_probe.ps1

```powershell
[CmdletBinding()]
param([string[]]$Scenes=@('menu','adventure','aquarium','versus'))
$ErrorActionPreference='Stop'
$repoRoot='F:/src/PVZXboxRecomp'
Set-Location $repoRoot
$outputRoot=Join-Path $repoRoot ('runtime-data/render-boundary/'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $outputRoot | Out-Null
$executable=Join-Path $repoRoot 'research/cross-platform/render-boundary/bin/pvz-console-edition.exe'
function Hash([string]$path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
$exeHash=Hash $executable
$cases=@{
 menu=@{replay='services_menus_regression.csv';minimum=20;maximum=45;fresh=$false}
 adventure=@{replay='adventure_mouse.csv';minimum=38;maximum=75;fresh=$true}
 aquarium=@{replay='feedback_quarium_pc.csv';minimum=25;maximum=60;fresh=$false}
 versus=@{replay='feedback_vs_two_pads.csv';minimum=56;maximum=90;fresh=$false}
}
$result=[ordered]@{executable=$executable;executable_sha256=$exeHash;runs=[ordered]@{}}
foreach ($sceneName in $Scenes) {
 $case=$cases[$sceneName]
 $directory=Join-Path $outputRoot $sceneName
 New-Item -ItemType Directory -Path $directory | Out-Null
 $profile=Join-Path $directory 'user'
 if ($case.fresh) {
  New-Item -ItemType Directory -Path $profile | Out-Null
  Copy-Item -LiteralPath build/pc_defaults.toml -Destination (Join-Path $profile 'pvz.toml')
 } else { Copy-Item -LiteralPath runtime-data/zen-shop-probe-user -Destination $profile -Recurse }
 $replay=[IO.Path]::GetFullPath('tests/replays/'+$case.replay)
 $trace=Join-Path $directory 'render.jsonl'
 $log=Join-Path $directory 'game.log'
 $arguments=@('--game_data_root',"$repoRoot/assets",'--user_data_root',$profile,
 '--cache_root',"$directory/cache",'--log_file',$log,'--log_flush_interval','1',
 '--fullscreen=false','--pvz_skip_intro=true','--pvz_render_scale=1','--pvz_vsync=false',
 '--pvz_pause_on_focus_loss=false','--pvz_mouse_cursor=true','--pvz_input_trace=true',
 '--pvz_test_unlock=true','--pvz_replay_controller=true','--pvz_cursor_mode=pc',
 "--pvz_input_replay=$replay","--pvz_render_probe=$trace","--pvz_render_probe_scene=$sceneName",
 '--pvz_render_probe_frames=180',"--pvz_capture_frame=$directory/frame.ppm",
 '--pvz_capture_delay=12','--pvz_capture_count=16','--pvz_capture_interval=4')
 $quotedArguments=$arguments | ForEach-Object { '"'+$_.Replace('"','\"')+'"' }
 Write-Output "T08 starting $sceneName ($directory)"
 $process=Start-Process -FilePath $executable -ArgumentList $quotedArguments -WorkingDirectory $repoRoot -WindowStyle Hidden -PassThru
 $timer=[Diagnostics.Stopwatch]::StartNew()
 $complete=$false
 while (-not $process.HasExited -and $timer.Elapsed.TotalSeconds -lt $case.maximum) {
  Start-Sleep -Milliseconds 500
  if (Test-Path -LiteralPath $trace) {
   $last=Get-Content -LiteralPath $trace -Tail 1 -ErrorAction SilentlyContinue
   $complete=$last -match '"record":"totals"'
  }
  if ($complete -and $timer.Elapsed.TotalSeconds -ge $case.minimum) { break }
 }
 $earlyExit=$process.HasExited
 if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
 $timer.Stop()
 $text=Get-Content -LiteralPath $log -Raw
 $entry=[ordered]@{trace=$trace;log=$log;seconds=$timer.Elapsed.TotalSeconds;complete=$complete;
  early_exit=$earlyExit;replay=$replay;replay_sha256=(Hash $replay);log_sha256=(Hash $log);
  fatal=[bool]($text -match '\[FATAL\]|\[critical\]|Unhandled guest|access violation')}
 if (Test-Path -LiteralPath $trace) { $entry['trace_sha256']=Hash $trace }
 $result.runs[$sceneName]=$entry
 $result | ConvertTo-Json -Depth 7 | Set-Content -Encoding utf8 (Join-Path $outputRoot 'runs.json')
 if ($earlyExit -or $entry.fatal -or -not $complete) { throw "T08 capture incomplete: $sceneName" }
 if ((Hash $executable) -ne $exeHash) { throw 'T08 executable changed' }
 Write-Output "T08 complete $sceneName"
}
Write-Output "T08 result $outputRoot/runs.json"
```


### research/cross-platform/render-boundary/inspect.py

```python
"""T08 reproducible, read-only Xbox/Android rendering boundary evidence.

Derived research helper, no changes to game, inputs or name-map predictions.
Run python -I research/cross-platform/render-boundary/inspect.py [hex-address ...].
"""
from pathlib import Path
import json,sys,hashlib
ROOT=Path(__file__).resolve().parents[3]
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'scripts'))
from name_xbox_from_android import Xbox,Android,BASE
from capstone import Cs,CS_ARCH_PPC,CS_MODE_32,CS_MODE_BIG_ENDIAN
x=Xbox(); a=Android(json.loads((ROOT/'research/cross-platform/android-symbols.json').read_text()))
cs=Cs(CS_ARCH_PPC,CS_MODE_32|CS_MODE_BIG_ENDIAN)
def code(addr):
    return [f'{i.address:08x}: {i.mnemonic:10} {i.op_str}' for i in cs.disasm(x.data[addr-BASE:addr-BASE+x.size(addr)],addr)]
targets=['Sexy::Graphics','Sexy::Image','Sexy::MemoryImage','Sexy::DDImage','Sexy::CanvasImage','Sexy::ImageFont','Sexy::D3DInterface','Sexy::NativeDisplay']
if len(sys.argv)>1:
    for arg in sys.argv[1:]:
        address=int(arg,16)
        print('\n'+arg+' size='+str(x.size(address))+' strings='+repr(x.strings(address)))
        print('\n'.join(code(address)))
    sys.exit()
result={'inputs':{},'vtables':{},'android_functions':[],'xbox_functions':[]}
for path in [ROOT/'.cache/pvz-image.bin',ROOT/'generated/default/pvz_funcs.h',ROOT/'Chinese Android TV Image/lib/armeabi/libGameMain.so',ROOT/'research/cross-platform/android-symbols.json',ROOT/'scripts/name_xbox_from_android.py',Path(__file__)]:
    result['inputs'][path.relative_to(ROOT).as_posix()]=hashlib.sha256(path.read_bytes()).hexdigest()
for (cls,off),(vt,slots) in x.vtables.items():
    if cls in targets or any(t in cls for t in ['Display','D3D','ImageFont']):
        result['vtables'][cls+':'+str(off)]={'address':hex(vt),'slots':[hex(v) for v in slots]}
for address,names in a.by_addr.items():
    if any(name.startswith(tuple(t+'::' for t in targets)) or name.startswith(('PoolEffect::','Reanimation::','TodBlt')) for name in names):
        result['android_functions'].append({'address':hex(address),'names':names,'size':a.size[address],'calls':[(hex(c),a.name(c)) for c in a.calls(address)]})
for address in x.funcs:
    if 0x8249a000<=address<0x824b3f00 or 0x8227c000<=address<0x8227d000:
        result['xbox_functions'].append({'address':hex(address),'size':x.size(address),'strings':x.strings(address),'calls':[hex(c) for c in x.calls(address)],'code':code(address)})
(OUT/'static.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'vtables':{k:dict(address=v['address'],slots=len(v['slots'])) for k,v in result['vtables'].items()},'android_functions':len(result['android_functions']),'xbox_functions':len(result['xbox_functions'])},indent=2))
```


### research/cross-platform/render-boundary/callgraph.py

```python
"""T08 exact direct PPC branch references, input hashes and trace aggregation."""
from pathlib import Path
import re,struct,json,hashlib,bisect,sys,statistics
ROOT=Path(__file__).resolve().parents[3]
OUT=Path(__file__).resolve().parent
image=ROOT/'.cache/pvz-image.bin'; raw=image.read_bytes(); base=0x82000000
starts=sorted({int(s,16) for s in re.findall(r'sub_([0-9A-Fa-f]{8})',(ROOT/'generated/default/pvz_funcs.h').read_text())})
def owner(address):
    index=bisect.bisect_right(starts,address)-1
    return starts[index] if index>=0 else 0
targets={0x824A3FB8,0x824A4370,0x824A45C0,0x824A2A80,0x824A2C58,0x824A2EA8,0x824A0FF8,0x824A1228,
         0x824A4DA8,0x824A46E0,0x824A49D8,0x824A4CF0,0x824A4E48,0x824A4A78,0x824A4BC0,
         0x82480430,0x82617C48,0x82617CB8,0x8255BD80,0x821E0828}
calls={hex(t):[] for t in sorted(targets)}
for address,end in zip(starts,starts[1:]):
    for pc in range(address,end,4):
        off=pc-base
        if off<0 or off+4>len(raw): continue
        word=struct.unpack_from('>I',raw,off)[0]
        if word>>26!=18 or word&2: continue
        displacement=word&0x3fffffc
        if displacement&0x2000000: displacement-=0x4000000
        dest=pc+displacement
        if dest in targets:
            calls[hex(dest)].append(dict(caller=hex(address),pc=hex(pc),tail=not bool(word&1)))
result=dict(inputs={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in [image,ROOT/'generated/default/pvz_funcs.h',Path(__file__)]},calls=calls)
(OUT/'callgraph.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
for dest,entries in calls.items():
    print(dest,':',', '.join(sorted({e['caller'] for e in entries})))
if len(sys.argv)>1:
    runs_path=Path(sys.argv[1]); runs=json.loads(runs_path.read_text(encoding='utf-8-sig'))
    combined={'inputs':{str(runs_path):hashlib.sha256(runs_path.read_bytes()).hexdigest()},'scenes':{}}
    for name,entry in runs['runs'].items():
        trace=Path(entry['trace']); records=[json.loads(l) for l in trace.read_text().splitlines()]
        header=records[0]; frames=[r for r in records if r['record']=='frame']; total=records[-1]
        assert len(frames)==180 and total['record']=='totals'
        combined['inputs'][str(trace)]=hashlib.sha256(trace.read_bytes()).hexdigest()
        rows=[]
        for index,address in enumerate(header['addresses']):
            values=[f['calls'][index] for f in frames]
            rows.append(dict(address=hex(address),total=sum(values),min=min(values),max=max(values),mean=statistics.mean(values),
                 outside_graphics=sum(f['outside_graphics'][index] for f in frames),lifetime=total['lifetime_calls'][index],
                 callers=[dict(lr=hex(lr),function=hex(owner(lr-4))) for lr in total['callers'][index]],
                 caller_overflow=total['caller_overflow'][index]))
        combined['scenes'][name]=dict(frames=len(frames),modes=sorted({f['mode'] for f in frames}),scenes=sorted({f['scene'] for f in frames}),rows=rows)
    (OUT/'measured.json').write_text(json.dumps(combined,indent=2)+'\n',encoding='utf-8')
    print('Aggregated',list(combined['scenes']))
```

