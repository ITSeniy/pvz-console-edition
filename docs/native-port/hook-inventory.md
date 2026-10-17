# Опись вмешательств порта

Создано `python -I scripts/inventory_port_hooks.py`. Исходники и поведение игры не меняются.

Имена взяты из исходных комментариев и **не проверены**; T01 ещё не принята.
Каждая строка CSV — место использования, а не уникальная функция или уникальное поле.

Всего хуков: **136**; `REX_HOOK_RAW`: **136**.
Строки по виду: `{"analysis-config": 5, "guest-address": 211, "guest-symbol": 937, "hook": 136, "memory-access": 1713}`.
Original policy: `{"always": 81, "conditional": 45, "never": 10}`.

## Значение original policy

`always` — каждый разобранный путь вызывает оригинал; `conditional` — есть путь без него;
`never` — хук не вызывает рекомпилированный оригинал. Это не означает готовую независимую
нативную подсистему: хук может вызывать другие guest-функции или native API SDK.
`after`/`before`/`around` — расположение вызова относительно остального текста тела;
`helper` — оригинал передан параметром; имя helper и его строка приведены в evidence.
Циклы и switch анализируются консервативно; `flow_overapproximation` перечисляет такие конструкции.

Смещения извлечены из выражений доступа. Пустой offsets означает динамическое/именованное
смещение либо прямой адрес. Полные выражения сохранены; это не восстановленная раскладка классов.
Доступы через Read/Write, REX_LOAD/STORE и byte indexing учитываются синтаксически;
динамические цепочки указателей, алиасы функций/указателей требуют отдельного анализа.

## Подсистемы

| Подсистема | Хуков | Без оригинала |
|---|---:|---:|
| audio | 3 | 1 |
| gameplay | 7 | 0 |
| input | 48 | 0 |
| menus | 31 | 1 |
| platform | 13 | 7 |
| render | 26 | 0 |
| support | 8 | 1 |

## Все хуки

| Подсистема | Символ | Исходник | Original | Позиция | Имя из комментария |
|---|---|---|---|---|---|
| audio | `__imp__XAudioSubmitRenderDriverFrame` | `src/audio_diagnostics.cpp:11` | never | none | __imp__XAudioSubmitRenderDriverFrame |
| audio | `sub_82199968` | `src/audio_fixes.cpp:17` | always | around |  |
| menus | `sub_821A32C0` | `src/bonus_games.cpp:124` | always | after | LawnApp::ShowChallengeScreen(page) |
| menus | `sub_82169968` | `src/bonus_games.cpp:136` | conditional | after | ChallengeScreen::GamepadButtonDown |
| platform | `__imp__XUsbcamCreate` | `src/camera_compat.cpp:5` | never | none | __imp__XUsbcamCreate |
| platform | `__imp__XUsbcamDestroy` | `src/camera_compat.cpp:6` | never | none | __imp__XUsbcamDestroy |
| platform | `__imp__XUsbcamGetState` | `src/camera_compat.cpp:7` | never | none | __imp__XUsbcamGetState |
| platform | `__imp__XUsbcamSetConfig` | `src/camera_compat.cpp:8` | never | none | __imp__XUsbcamSetConfig |
| platform | `__imp__XUsbcamSetView` | `src/camera_compat.cpp:9` | never | none | __imp__XUsbcamSetView |
| platform | `__imp__XUsbcamSetCaptureMode` | `src/camera_compat.cpp:10` | never | none | __imp__XUsbcamSetCaptureMode |
| platform | `__imp__XUsbcamReadFrame` | `src/camera_compat.cpp:11` | never | none | __imp__XUsbcamReadFrame |
| render | `sub_8249C6A8` | `src/challenge_mouse.cpp:157` | always | after | Graphics::ClipRect |
| render | `sub_821684F0` | `src/challenge_mouse.cpp:177` | always | around | ChallengeScreen::Draw |
| render | `sub_82167958` | `src/challenge_mouse.cpp:187` | always | around | ChallengeScreen::DrawChallenge |
| input | `sub_82169058` | `src/challenge_mouse.cpp:203` | always | around | ChallengeScreen::Update |
| input | `sub_82166FB0` | `src/challenge_mouse.cpp:231` | always | after | ChallengeScreen deleting destructor |
| render | `sub_824B2D20` | `src/controller_assets.cpp:68` | conditional | around | PakInterface::ReadFile(path, data**, size*) |
| render | `sub_821E0068` | `src/controller_assets.cpp:284` | always | around |  |
| support | `sub_82616558` | `src/deterministic_replay.cpp:59` | conditional | around | Xbox time32 825F9600 -> 82616558 -> KeQuerySystemTime (8275A65C). |
| support | `sub_82478108` | `src/deterministic_replay.cpp:72` | conditional | after |  |
| support | `sub_824752F0` | `src/deterministic_replay.cpp:87` | conditional | after |  |
| support | `sub_824AC4C0` | `src/deterministic_replay.cpp:94` | always | around |  |
| support | `sub_824943A0` | `src/deterministic_replay.cpp:120` | always | after |  |
| support | `sub_8247FFC0` | `src/deterministic_replay.cpp:139` | always | around |  |
| menus | `sub_82473000` | `src/game_settings.cpp:84` | conditional | after | Widget::GamepadButtonDown |
| menus | `sub_824AE488` | `src/game_settings.cpp:590` | conditional | around |  |
| menus | `sub_8219B580` | `src/game_settings.cpp:692` | always | after | HouseChooserScreen deleting destructor |
| menus | `sub_821BE0B0` | `src/game_settings.cpp:693` | always | after |  |
| menus | `sub_82172788` | `src/game_settings.cpp:694` | always | after |  |
| menus | `sub_8228A4B0` | `src/game_settings.cpp:695` | always | after |  |
| menus | `sub_821B7F80` | `src/game_settings.cpp:696` | always | after | LeaderboardMenu deleting destructor |
| menus | `sub_8212D448` | `src/game_settings.cpp:697` | always | after |  |
| menus | `sub_82213DF8` | `src/game_settings.cpp:699` | always | after |  |
| menus | `sub_82120C30` | `src/game_settings.cpp:701` | always | after |  |
| menus | `sub_8219A2F0` | `src/game_settings.cpp:702` | always | after |  |
| menus | `sub_8219A070` | `src/game_settings.cpp:713` | always | after |  |
| menus | `sub_82187F70` | `src/game_settings.cpp:714` | always | around | GameButton destructor |
| menus | `sub_821AFCA0` | `src/game_settings.cpp:851` | always | before |  |
| menus | `sub_821993F8` | `src/game_settings.cpp:863` | always | around |  |
| menus | `sub_8219B8A0` | `src/game_settings.cpp:877` | always | around |  |
| menus | `sub_8220A0F8` | `src/game_settings.cpp:903` | always | around |  |
| menus | `sub_82208950` | `src/game_settings.cpp:959` | always | after |  |
| menus | `sub_82198850` | `src/game_settings.cpp:971` | always | around |  |
| menus | `sub_821988D0` | `src/game_settings.cpp:1013` | always | around |  |
| menus | `sub_82198938` | `src/game_settings.cpp:1025` | always | around |  |
| menus | `sub_82198720` | `src/game_settings.cpp:1039` | always | after |  |
| menus | `sub_82198B08` | `src/game_settings.cpp:1051` | always | after |  |
| menus | `sub_82198B30` | `src/game_settings.cpp:1056` | conditional | after |  |
| input | `sub_824AE518` | `src/mouse_bridge.cpp:2319` | conditional | after | ButtonWidget::MouseEnter |
| input | `sub_824A5F48` | `src/mouse_bridge.cpp:2326` | conditional | after | WidgetManager::SetGamepadFocus |
| input | `sub_8216EF50` | `src/mouse_bridge.cpp:2357` | conditional | around | Coin::Collect (player) |
| audio | `sub_821ABDE8` | `src/mouse_bridge.cpp:2388` | conditional | after | LawnApp::PlaySample |
| input | `sub_824A85F0` | `src/mouse_bridge.cpp:2402` | conditional | around | Dialog::MouseDrag |
| render | `sub_82146A60` | `src/mouse_bridge.cpp:2422` | always | after |  |
| render | `sub_82188310` | `src/mouse_bridge.cpp:2436` | conditional | after |  |
| input | `sub_824A6140` | `src/mouse_bridge.cpp:2484` | always | around |  |
| input | `sub_824A6198` | `src/mouse_bridge.cpp:2494` | always | around |  |
| input | `sub_82580C58` | `src/mouse_bridge.cpp:2507` | conditional | after | Input: right stick X |
| input | `sub_8216DC40` | `src/mouse_bridge.cpp:2512` | conditional | around |  |
| input | `sub_8216ED60` | `src/mouse_bridge.cpp:2539` | conditional | after |  |
| input | `sub_8216F3D0` | `src/mouse_bridge.cpp:2551` | conditional | around |  |
| input | `sub_821516D8` | `src/mouse_bridge.cpp:2589` | conditional | around | Challenge::BeghouledDragUpdate(x,y) |
| input | `sub_82154930` | `src/mouse_bridge.cpp:2603` | always | around |  |
| input | `sub_8227C190` | `src/mouse_bridge.cpp:2626` | always | after |  |
| input | `sub_8227C240` | `src/mouse_bridge.cpp:2644` | conditional | after |  |
| input | `sub_82580C80` | `src/mouse_bridge.cpp:2655` | conditional | after | Input: right stick Y |
| input | `sub_821667C0` | `src/mouse_bridge.cpp:2659` | always | after | Challenge::FireHeavyWeapon |
| input | `sub_82165B58` | `src/mouse_bridge.cpp:2664` | always | around | Challenge::UpdateHeavyWeapon |
| input | `sub_8218B8A0` | `src/mouse_bridge.cpp:2726` | always | around |  |
| input | `sub_821D22C0` | `src/mouse_bridge.cpp:2864` | always | around |  |
| input | `sub_8218D0F8` | `src/mouse_bridge.cpp:2880` | conditional | around |  |
| input | `sub_8212A658` | `src/mouse_bridge.cpp:3037` | always | after |  |
| input | `sub_8212BFE0` | `src/mouse_bridge.cpp:3063` | always | around |  |
| input | `sub_8218A140` | `src/mouse_bridge.cpp:3075` | always | after |  |
| render | `sub_82196600` | `src/mouse_bridge.cpp:3081` | conditional | after |  |
| input | `sub_82210308` | `src/mouse_bridge.cpp:3096` | always | around |  |
| input | `sub_8220AFA8` | `src/mouse_bridge.cpp:3112` | always | around | SeedChooser::SeedHitTest |
| input | `sub_82206A18` | `src/mouse_bridge.cpp:3159` | always | around |  |
| input | `sub_8220B7B0` | `src/mouse_bridge.cpp:3188` | conditional | after |  |
| input | `sub_82208B18` | `src/mouse_bridge.cpp:3218` | always | around |  |
| input | `sub_82143880` | `src/mouse_bridge.cpp:3278` | always | around |  |
| render | `sub_8249D0E8` | `src/mouse_bridge.cpp:3287` | conditional | after | Graphics::DrawImage(image, x, y) |
| render | `sub_8249C430` | `src/mouse_bridge.cpp:3316` | conditional | around |  |
| input | `sub_82580A78` | `src/mouse_bridge.cpp:3335` | conditional | after |  |
| render | `sub_8249D388` | `src/mouse_bridge.cpp:3353` | conditional | after |  |
| render | `sub_8249B408` | `src/mouse_bridge.cpp:3378` | conditional | after |  |
| render | `sub_8249BC50` | `src/mouse_bridge.cpp:3384` | conditional | after |  |
| render | `sub_8249C058` | `src/mouse_bridge.cpp:3390` | conditional | after |  |
| render | `sub_8249CFB8` | `src/mouse_bridge.cpp:3430` | conditional | after |  |
| render | `sub_8249DA90` | `src/mouse_bridge.cpp:3436` | conditional | after |  |
| input | `sub_8220E5D0` | `src/mouse_bridge.cpp:3453` | always | around |  |
| render | `sub_82284D98` | `src/mouse_bridge.cpp:3483` | always | around |  |
| input | `sub_82210768` | `src/mouse_bridge.cpp:3497` | always | after |  |
| render | `sub_82492A48` | `src/mouse_bridge.cpp:3534` | always | around |  |
| input | `sub_82146678` | `src/mouse_bridge.cpp:3561` | always | after |  |
| input | `sub_821C6468` | `src/mouse_bridge.cpp:3588` | always | after |  |
| input | `sub_824A6890` | `src/mouse_bridge.cpp:3642` | conditional | after | WidgetManager::GamepadButtonDown |
| input | `sub_824A68C0` | `src/mouse_bridge.cpp:3737` | conditional | after | WidgetManager::GamepadButtonUp |
| render | `sub_8249D250` | `src/mouse_bridge.cpp:3745` | conditional | after | Graphics::DrawImageCel |
| gameplay | `sub_82284C60` | `src/pc_game_fixes.cpp:96` | always | around |  |
| gameplay | `sub_82285B70` | `src/pc_game_fixes.cpp:129` | always | around |  |
| gameplay | `sub_82284848` | `src/pc_game_fixes.cpp:140` | always | after |  |
| gameplay | `sub_82139DE8` | `src/pc_game_fixes.cpp:159` | always | around | Board::Pause(bool) |
| gameplay | `sub_821A5DB8` | `src/pc_game_fixes.cpp:172` | always | around | LawnApp::Update |
| gameplay | `sub_8227FCA0` | `src/pc_game_fixes.cpp:226` | always | around | TodFoley::GamePause(bool) |
| gameplay | `sub_8227FC10` | `src/pc_game_fixes.cpp:244` | always | after | TodFoley::StopFoley(type) |
| menus | `sub_82289228` | `src/pc_menus.cpp:186` | conditional | after | TypingCheck::Check |
| menus | `sub_822CA128` | `src/pc_menus.cpp:190` | never | none |  |
| menus | `sub_822CA130` | `src/pc_menus.cpp:194` | conditional | around | XamShowMarketplaceUI wrapper |
| menus | `sub_821C0630` | `src/pc_menus.cpp:246` | conditional | around |  |
| menus | `sub_821A25C8` | `src/pc_menus.cpp:321` | always | after |  |
| support | `sub_##address` | `src/render_probe.cpp:212` | never | none | sub_##address |
| render | `sub_8220D990` | `src/seed_image_fixes.cpp:18` | always | around | DrawSeedPacket. |
| render | `sub_821E1560` | `src/seed_image_fixes.cpp:52` | always | around | Reanimation::DrawRenderGroup |
| render | `sub_821DEFC0` | `src/seed_image_fixes.cpp:61` | conditional | around |  |
| render | `sub_82123220` | `src/service_mouse.cpp:585` | always | helper | Almanac::DrawPlants |
| render | `sub_82123CD8` | `src/service_mouse.cpp:586` | always | helper | Almanac::DrawZombies |
| input | `sub_821A8CE8` | `src/service_mouse.cpp:587` | always | after | LawnApp::SeedTypeAvailable |
| input | `sub_82124D08` | `src/service_mouse.cpp:591` | always | after | Almanac::ZombieIsShown |
| input | `sub_82192080` | `src/service_mouse.cpp:596` | always | around | ZenGardenControls::Update |
| render | `sub_821249A8` | `src/service_mouse.cpp:613` | always | around | Almanac::Draw |
| input | `sub_82122DD0` | `src/service_mouse.cpp:620` | always | around | Almanac::Update |
| render | `sub_821923C0` | `src/service_mouse.cpp:637` | conditional | around | ZenGardenControls::Draw |
| input | `sub_8228CE18` | `src/service_mouse.cpp:671` | always | around | ZenGarden::InitLevel (also used by garden switches) |
| input | `sub_82189B40` | `src/service_mouse.cpp:686` | conditional | after | Controls::UpdateInput |
| input | `sub_82191DA8` | `src/service_mouse.cpp:695` | always | around | TreeOfWisdomControls::Update |
| input | `sub_82139808` | `src/service_mouse.cpp:716` | conditional | after | Board::PickUpTool(hit,player) |
| input | `sub_82217888` | `src/service_mouse.cpp:726` | always | around | Store::GamepadButtonDown (prices and confirmation) |
| input | `sub_8219ACD0` | `src/service_mouse.cpp:737` | always | around | HouseChooserDialog::GamepadButtonDown |
| platform | `sub_822DB228` | `src/startup_video.cpp:73` | always | after | WMV player creation |
| platform | `sub_822DAC58` | `src/startup_video.cpp:80` | conditional | around | WMV render/status |
| platform | `sub_822B2138` | `src/startup_video.cpp:89` | always | around | D3DDevice swap, the sole VdSwap caller |
| support | `sub_82492618` | `src/state_dump.cpp:200` | always | around |  |
| platform | `sub_82616510` | `src/windows_identity.cpp:38` | always | around | Native HRESULT wrapper for XamUserGetSigninInfo. |
| platform | `sub_822C9E48` | `src/windows_identity.cpp:56` | conditional | after |  |
| platform | `sub_822CA0B0` | `src/windows_identity.cpp:61` | always | around |  |

## Полностью обходящие оригинал

`__imp__XAudioSubmitRenderDriverFrame`, `__imp__XUsbcamCreate`, `__imp__XUsbcamDestroy`, `__imp__XUsbcamGetState`, `__imp__XUsbcamSetConfig`, `__imp__XUsbcamSetView`, `__imp__XUsbcamSetCaptureMode`, `__imp__XUsbcamReadFrame`, `sub_822CA128`, `sub_##address`

## Макросы SDK и связь с документацией

Ищутся все определения hook/stub/export и legacy aliases из локального hook.h: 
`REXCRT_EXPORT`, `REXCRT_EXPORT_STUB`, `REX_EXPORT`, `REX_EXPORT_STUB`, `REX_EXPORT_STUB_RETURN`, `REX_HOOK`, `REX_HOOK_RAW`, `REX_STUB`, `REX_STUB_LOG`, `REX_STUB_RETURN`, `XAM_EXPORT`, `XAM_EXPORT_STUB`, `XBOXKRNL_EXPORT`, `XBOXKRNL_EXPORT_STUB`.

Без точного упоминания символа/адреса или имени файла в существующих docs/*.md: **42** хуков.
Для них документированная точка входа — эта опись. Ссылка на документ по имени исходника
не доказывает, что документ описывает каждую ветвь конкретного хука.

## SHA-256 входов

Манифест: `2bba0d29866eb62e1f77e07589eb19e6c3ccbb6e9aefd43c8809ca521c1f2df6`.

| Вход | SHA-256 |
|---|---|
| `config/analysis.toml` | `cb43775859b5684c65196b8465599bb26256be0586775a2c623954fa06dc7d51` |
| `docs/bonus-games.md` | `309df459d5c5c4f1a045bc8fed93442068d48af3970597c9d37a71a04e6ff174` |
| `docs/cross-platform-research.md` | `cebebdd41097cd3a611f0b739d98dc09e1c2b81adb60a9ee1647e5e4d4f162b2` |
| `docs/distribution.md` | `a056d1927f2a1d671949daf98c67b1647c35a3341384f98ef9a5e8e931cf973c` |
| `docs/edge-cases-2026-10-14.md` | `b185060d9edc91b2e3251600ad8874a4596b9975bca14a05fb32b4bf3ed5a2ff` |
| `docs/feedback-2026-10-14.md` | `71b1118b59d5e169f6c2d8456cbe8ca6b8b55a5ee7b878b97f129766b5ec938b` |
| `docs/frame-pacing-2026-10-14.md` | `f3015c2b8c9394dadbd4516c56068dfa6cdfc2585def374322e2139463bfa917` |
| `docs/heavy-weapon-original.md` | `221043cd948d4bfd7e6aa09f9f8f5aadf4990741b4b385db6bcce9b4e6284725` |
| `docs/heavy-weapon.md` | `053398733539652c5a879bca5bb78fd4993ef6588666fc86489d394eac9ca56e` |
| `docs/input-coverage.md` | `51404acbd7bb315d28735b3b6f3d16878a7877e3154c44f788b27751d77342b0` |
| `docs/input-feedback-2026-10-09.md` | `1a15a030bc81d1575b25698192629712c83ff15d45483da4adad09bba329e7c3` |
| `docs/item-drag.md` | `531779da0677091117b5f5f03e0bfe2024de757db1ad86fa645fc065f4674e2c` |
| `docs/minigames-mouse.md` | `5d45d9bea95dc199802a17b0c8b899c09a570b09d034be96ad6f6cfb1e001d44` |
| `docs/minigames-save.md` | `4e35f4e4bbdf618275ea03fc1fe87234cf04c88b0605245700acc94960f5d8bf` |
| `docs/mouse-hook-audit-2026-10-09.md` | `fbb8d355fd544e0f62d3a8159d7c3d51ad4b530de5c59c1bf33accf04923ee03` |
| `docs/mouse-hook-implementation-2026-10-09.md` | `17641da2c075fb8e2d0504fed353baae92819b7e2134e86b1e4421e97c4909b2` |
| `docs/native-port-feasibility.md` | `e0e399fa912924fec551d64c1af9945c48562324574c93326080caf939ccd11b` |
| `docs/research.md` | `449d3f174631b8082154d08ecde900e23eab59330c550eba446ac8777f22d213` |
| `docs/services-mouse.md` | `3c5e64e4ce352cf30d0767ce92dea04a4c86b4fb545b414bf45818edc2ce1d57` |
| `docs/startup-performance-2026-10-11.md` | `7deb9ca08d4a253ab6180ad405c276a84c1dc1fa9f66c8e8319352f99d7c8b6f` |
| `docs/sun-pickup-fix.md` | `bb3f09734a6e8383c0beee680dfcd45b52f1e1d30330285ab8b1d49760af043d` |
| `docs/technical-notes.md` | `0097002870bdf776e814f39a8ce2ef1d0f9a11fb11de6892865fbe5833c5c48e` |
| `docs/testing-2026-10-06.md` | `4ec5fc65d76c9fb179decff5d9c7665d91fa44162278b0968e2030db8c579955` |
| `docs/testing-2026-10-07.md` | `c6ef8f56ac9cbb43142f491c8a992790e8e6dd7edb291843cec56782994d5990` |
| `docs/testing-item-drag-2026-10-08.md` | `f1370e9ac90d378eeb735d36913fae9d5e0fe86e0a5e7863bfa92870817f8caf` |
| `docs/testing-minigames-2026-10-07.md` | `25b1dde03695bc6a6cbb340dcdbd16cf118d28ce7e228fd8c941fbdb44e0e92d` |
| `docs/vita-touch-research.md` | `6ae632ea2d84630bf5d872fcb9f61d04c302fc9a16928d5f39567b73ea63e867` |
| `research/rexglue-source/include/rex/hook.h` | `c59cfc587abd493df0e5c9d841c98ca4be54616d78fb51e6da6c5a8d2c93cd89` |
| `scripts/inventory_port_hooks.py` | `14a28a3ffd01f2dad58b5991839037cb15c85dca1f34b24c4aa16210adf93342` |
| `src/achievement_toast.h` | `c6392e29dcc63f4afb48bca0409b49860af9aa1a1c1898fd510db514b56454d0` |
| `src/audio_diagnostics.cpp` | `b3ba3ed5b5769d8b26dcf9b42d0af5a60e878a0d3cf45f63bd411cb6d536a3f6` |
| `src/audio_fixes.cpp` | `96dd7397c00028dfd628f2c7c62361177aa3fe29d565ba885a0912591ced8d4b` |
| `src/board_mapping.h` | `38b07364fc0f3a9c64b3a676ec1480e913ff33973c96936dd9e16394f9905f85` |
| `src/bonus_games.cpp` | `a981d042cadffdf0e4e034225e7d3f4a2aaccee6dee2d91a36dab5241c16cbb6` |
| `src/bonus_games.h` | `3964a24249f93357ef66a1a958a9d46d32749a30445eab674f6102d369949a98` |
| `src/bridge_mnk_driver.h` | `02d0d87ebbbda33b7a8ea8b3c4c32f9e78da973b72b623a2e52bb989353708f7` |
| `src/camera_compat.cpp` | `b4ec35bb50155c47d3337490257e296096ff28c979dabbf324ba2ee2bbdba9ea` |
| `src/challenge_mouse.cpp` | `50082f56c76fbd34324fb62260e2c324261dccbf77dd03fa8a699ac7ccde574a` |
| `src/challenge_mouse.h` | `e44193f3c7cccf2e86ca9dee4dd57589c43373b7a8fe0f65e317138210bf96d4` |
| `src/controller_assets.cpp` | `3a92af52606ab1752dfda95c7d3f34dafc05d906e9a5f3e638d979f7d7d16c69` |
| `src/controller_assets.h` | `3abc91dfabee8b10d6660c8cc3ae6d3304141795ddb8917b4ed6e40d8a54e0c4` |
| `src/crt_compat.cpp` | `310263162deb2ce1f89780a8fda852801be0bb6a816510dcc1ceb1fd2227cc58` |
| `src/deterministic_replay.cpp` | `a7a13c7c00ac3b068deaa39546d4b2e841591cf3cef4e2b97895cf8a145990c7` |
| `src/display_sync.cpp` | `3943f36ca6df63e258b92124b1c28f0fcaf3d4a652d6f9fc8eead7333a0d0196` |
| `src/display_sync.h` | `0b2345622851bb099282c4565f10c64040b2042b8bb5648955ed8f115f400d70` |
| `src/frame_capture.cpp` | `099fdb58c9be5eec66c865bbcdbcccadd6beb506a689ad10094e30754deb741b` |
| `src/game_edition.cpp` | `78a9fe721192e3dec5dcdcb9131bb7dc39a88131a3b0c06ce656de59f7c315b3` |
| `src/game_edition.h` | `7350949199f2e194d7ca6fda40ccaf7570d42225963a15c4634cf0422c2afe5d` |
| `src/game_settings.cpp` | `2c60f312110a00ff0822105ac69b0b27dbda90a042464c40c300c11a51ca2e33` |
| `src/game_settings.h` | `82b838801fe8879f2b7b4ef5ffe0b84bb1b9af1949de6fb1ca244bf23cf128d3` |
| `src/guest_call.h` | `6e588e285b4e5f6acb7cf1771a08d2cbe186511c310f79649923ab02b1fbc5ed` |
| `src/guest_layout.h` | `767abc57b5b00a98fcb188b0f6c1fc2082385c0aa28e84d84938594f213ce686` |
| `src/guest_memory.h` | `2e77f051e41e2b1cd233eaae0cf7e5ad4be4a1c4fb777ef72a506c40ba771564` |
| `src/heavy_weapon_pointer.h` | `518651e05802268b1bdf54d67f332a6f79025d4a0cdac5c5998d945cd9cac3ec` |
| `src/hitch_profiler.cpp` | `595e3ffbfb5914362edbac4c260de98571f8dfae8223555fc2bb919f3118306f` |
| `src/hitch_profiler.h` | `4d08cc328a5022a91ab0bd814cd43177cf3f610febcfa46dd87dfa7a28d494b7` |
| `src/import_dialog.h` | `c689840a7484a00a3ffe22b6b7b820372911be47d7f90310428afe2c7835e899` |
| `src/input_activity.h` | `89eaec3f50bcbebdfdcaf373da04bb63d50d0d503e4fde1dd0de6bb4d13be37b` |
| `src/input_backend.cpp` | `5f51491bfe19bf24cc3703b1ce9fdd29117ec9d48270f9578405acf3a29855f1` |
| `src/input_backend.h` | `a2bb4e28943cb148bb218b91b21a07bdd41e97be008fae94d4010008bebd4878` |
| `src/input_replay.cpp` | `c689172051e6d1a7e5c89638a7cfe16aacd077929d7f5782732e3d66b20559f3` |
| `src/input_trace.h` | `c0611709d818f6eee6e560f01952bf3d3e96c24ceb82f0f8d5b346ddb43c045b` |
| `src/input_widget_inventory.h` | `c51c194e98bd89ca45f0c3aa67a4a31b3878332ea81f3b342eba79b6d38282b9` |
| `src/key_bindings.cpp` | `36e588fb91421a31c5dcc5012465eddfd28412bcce5f8b0c48c7c0b75f9dcdbb` |
| `src/key_bindings.h` | `82514078558aad1edf29ded2f56c11f638e776ffc5bf0761c7d175688a3f1395` |
| `src/local_players.h` | `bf886d70eb0a27f2fd176afaaf2cb108a2853685e512698049050d6cca7215dd` |
| `src/main.cpp` | `9bd6990b1c6492fdb43f54aa8dca8f40ac7a917d4af5aa4ba5e4def20a9e262c` |
| `src/menu_mapping.h` | `60cc11f69b4c0743b98ec446e4af1f0a2332c3729505bfa798040f2c848c91ff` |
| `src/mouse_bridge.cpp` | `74c4b4ee4b4031fd5551dd7bd7f4728e18e2d29f0029fa4bd732f1d87e8d5b25` |
| `src/mouse_bridge.h` | `f69c2bf3afd1ac1b989acf06b6b401b14e2ca372ecdc92a41f48a68aac4c66ef` |
| `src/mouse_input.h` | `ea6b8cb80fb699b8f77044e623174b5eba50ca03395b635a52b8bd344ad2fa38` |
| `src/mouse_prompt.h` | `b13f97bd78f25d7a0b3e9b61cdd3e36537999558e72bd2ea63d5c4d26effabc1` |
| `src/pc_game_fixes.cpp` | `a6c070c74381bab7dadd1d8eb1c88a7390002a67daa04b05fa6a62fbbfa36658` |
| `src/pc_locale.h` | `e82b58d95332b953207c38fb497c41ae0d4161b95b419df46ad9bd5a065af505` |
| `src/pc_menu_dialog.h` | `6523359c363e128249d6ab469041a03e157c8fe6c804c995b927a2116f8a21f8` |
| `src/pc_menus.cpp` | `39c192e1f22aeb58a57ba1fec09df679afc4944e7b1fcb4166eee1919050393f` |
| `src/pc_menus.h` | `8d4d0343a69cb63f949a8f223dae0edb5af41c40d3d2206f28c889edae40c7b0` |
| `src/pc_text.h` | `09e9fb6d021fe54f9c57e8af45880170370c2a2615eda348edee6f76876bd36a` |
| `src/pc_text_runtime.h` | `bacf74cb7a96aecdeb8c78fff479f76e49d9c962387e658be95ec985fe085cb4` |
| `src/pointer_mapping.h` | `3c8127e1dadc4d90f7ae60f72dd2b2bdb620e74830f9bdebeda9bff74f8d456b` |
| `src/pvz_app.h` | `7f16d05094f82f3b9d4a7a04df4b284f55dd722ce442b469fedd6778e5ec3b63` |
| `src/render_probe.cpp` | `c4c783a8a4d2d1b2abcd0fa03b8bd60fa2c184b4312a146ccb9e6193f37f35c3` |
| `src/render_scale.cpp` | `55685587ed4bb6984a385892c4c986631c778ba1eb7a65aafd641ce9df9bd511` |
| `src/render_scale.h` | `57d6bb5cfe9b3962023f8c9f0ff66aeb9af92a735c2164ede348f580282fabd0` |
| `src/replay_event.h` | `ee130e6233fa526be1770bfa8d7ad3a080b3a9cbf5a20d71424220e97d7cf7ff` |
| `src/replay_fixtures.cpp` | `5557214d83511ee184d53260808420292bcd0f037bf4c48c583042d8246bad2c` |
| `src/replay_fixtures.h` | `859a9d94c3f30474761c62f96b92506217a48e24bc125b139805b996f60e8d40` |
| `src/seed_image_fixes.cpp` | `b66bb12f465034a9ed128643dda66334d88ed6c06cf2cdc2337eb689e37b17ac` |
| `src/service_mouse.cpp` | `f5e2a154b98c59a7bbf90346303c3efb6a44fc08971c66678d9be5597f17b9f4` |
| `src/service_mouse.h` | `b1ed28ce6cc86b5aa571863adda02ecef0768ed77cce4fc98f3a87433347918d` |
| `src/startup_video.cpp` | `fb1d0e6ac327d2d6751f99b73ffd0e923581daa74ad130687e185e515d6c79e1` |
| `src/state_dump.cpp` | `6de81a0ef58aed4e8cde78b89cf69f594ac56a3a02066247a710c519703bc967` |
| `src/widget_tree.h` | `9362aa192da34c21e32c130296beb60d62133253890c9713b67a55de008da9cd` |
| `src/windows_identity.cpp` | `f5cc1e0b850da0e83a8d2a20cf8eebbbd26e32adc34a4996de4752628df78a96` |
| `src/xbox_names.h` | `f23b80b605b6e1937d6a633b1458213b73aa322a05a6cd7e82de06336498e724` |
