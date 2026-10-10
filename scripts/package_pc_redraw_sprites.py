"""Collect existing Xbox sprites for a PC redraw handoff, without editing assets.

PNG decoding and original PTX payloads are both retained. Localized duplicates
share one editable PNG, with every source mapping recorded in manifest.json.
"""
from __future__ import annotations
import csv
from datetime import datetime
import hashlib
import io
import json
from pathlib import Path
import struct
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/python"))
from PIL import Image, ImageDraw, ImageFont
from analyze_cross_platform import decode_xctd, pak_entries, xbox_dds

REQUIRED = {
    "images/help_buttons.ptx": ("Атлас подсказок", "Заменить Xbox-глифы на мышь/клавиатуру для режима мыши. Сохранить 13 ячеек по 42×42 и их порядок; не переносить Xbox-значения кнопок на новые действия без изменения кода."),
    "images/help_buttons_small.ptx": ("Малый атлас подсказок", "Та же система обозначений в 13 ячейках по 21×21. Сейчас оба атласа скрываются при вводе мышью; новая версия потребует отдельного подключения."),
    "images/help_menu_image_controller.ptx": ("Экран управления", "Заменить рисунок Xbox-контроллера на схему мыши и клавиатуры. Линии-выноски согласовать с позициями подписей. Предусмотреть различия PC/Console мыши, Escape — пауза."),
    "images/help_menu_image_vs_controllers.ptx": ("Справка VS", "Изображение контроллера встроено в иллюстрацию. Предусмотреть мышь для первого игрока и контроллер для второго; не обещать управление второй мышью."),
    "images/plant_side_selected.ptx": ("Выбор стороны растений в VS", "Рука подсолнуха держит Xbox-контроллер. Для игрока с мышью заменить его на мышь/PC-указатель или убрать устройство; сохранить руку, холст и анимационную привязку. Для игрока с геймпадом сохранить исходник."),
    "images/zombie_side_selected.ptx": ("Выбор стороны зомби в VS", "Рука зомби держит Xbox-контроллер. Выполнить пару к растительной стороне; подключать PC-вариант только для игрока с мышью. Второй игрок с контроллером не должен получить ложное обозначение мыши."),
    "images/mail_message_intro.ptx": ("Приветственное письмо", "Внутри изображения текста нарисованы A, левый стик и X. Переписать строку про выбор, листание и удаление писем под действующие кликабельные действия. Сохранить язык и рукописный стиль."),
    "images/survival_button.ptx": ("Survival Endless", "Удалить жёлтый значок Y и дорисовать камень под ним. Сохранить надпись, трещины, траву и звёзды. Сейчас код подавляет весь этот спрайт при вводе мышью; после замены понадобится изменить подключение."),
}
for direction, badge in [("next", "RB"), ("prev", "LB")]:
    for state in ("", "highlight", "disabled"):
        REQUIRED[f"images/store_{direction}button{state}.ptx"] = (
            "Стрелки магазина",
            f"Убрать {badge} с активных состояний, сделать читаемую кликабельную стрелку. Согласовать обычное, наведённое и отключённое состояния; disabled приложен для полного комплекта, без встроенной буквы. Сохранить наклон, размер и прозрачную тень.")

OPTIONAL = {
    "images/gamepad0.ptx": ("Индикатор игрока 1", "Вариант мышь+клавиатура вместо Xbox-контроллера, если такой индикатор будет показан при вводе мышью. Оригинал геймпада остаётся для режима контроллера."),
    "images/gamepad1.ptx": ("Индикатор игрока 2", "Приложен парой к игроку 1 для согласованного оформления. Второй игрок пока использует контроллер: заменять его на мышь без изменения управления не нужно."),
    "reanim/a_button.ptx": ("Отдельная кнопка A", "Альтернативный значок ЛКМ или клика вместо зелёной A. Сейчас спрайт скрыт при вводе мышью; если скрытие достаточно, перерисовка не требуется."),
    "images/help_menu_image_coop_butter.ptx": ("Иллюстрация кооператива", "Сцена с P2 и консольной рамкой. По желанию убрать рамку для примера PC-курсора; сохранить обозначение второго игрока."),
    "images/help_menu_image_coop_sun.ptx": ("Иллюстрация сбора солнца", "Сцена с P2 и консольной рамкой. Если описывается PC-режим, показать клик вместо сбора наведением; второй игрок с контроллером сохраняет свою логику."),
}
for state in ("", " selected", " pressed"):
    OPTIONAL[f"reanim/mainmenu3/unlock{state}.ptx"] = (
        "Кнопка разблокировки Trial",
        "Вариант значка импорта своей Xbox-копии вместо платёжного символа. Это оформление будущего входа в импорт, пока такого действия в главном меню нет. Согласовать три состояния; сохранить анимационную привязку.")

REFERENCE = {}
for name in [
    "shovel", "shovel_hi_res", "shovelbank", "shovelmeter_empty", "shovelmeter_full",
    "board_selector", "board_selector_blue", "board_selector_arrows", "board_selector_arrows_shadow",
    "seed_selector", "seed_selector_blue", "seed_selector_shadow", "bejewel_selector", "bejewel_selector_shadow",
    "seedchooser_button", "seedchooser_button_glow", "seedchooser_button_disabled",
    "store_mainmenubutton", "store_mainmenubuttonhighlight", "store_mainmenubuttondown",
    "help_menu_paper01", "help_menu_paper02", "zenshopbutton", "zenshopbutton_highlight",
]:
    REFERENCE[f"images/{name}.ptx"] = ("Шаблоны и контекст", "Не обязательная перерисовка. Основа для пустой кнопки лопаты, указателя, кнопок Menu/Back/Shop и оформления справки. Консольные рамки и таймер лопаты используются в Console-режиме; не удалять исходники.")
REFERENCE["reanim/co_op_cursor.ptx"] = ("Стрелка курсора", "Стрелка с тенью для сравнения. Новая PC-стрелка может быть отдельным вариантом; это не спрайт растения и не обязательная перерисовка.")

GROUPS = [("01_required", REQUIRED), ("02_optional", OPTIONAL), ("03_reference", REFERENCE)]

def sha(data):
    return hashlib.sha256(data).hexdigest()

def main():
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    name = "pvz-console-edition-pc-redraw-sprites-" + stamp
    archive = ROOT / "dist" / (name + ".zip")
    folder = ROOT / "runtime-data/pc-redraw-sprites" / name
    folder.mkdir(parents=True)
    sources, records, unique = [], [], {}
    seen_common = set()
    for locale in ("main", "en", "de", "es", "fr", "it", "jp"):
        original = ROOT / "assets" / (locale + ".pak")
        if original.read_bytes()[:4] == bytes.fromhex("0ff512ed"):
            path, provenance = decode_xctd(original, ROOT / "research/cross-platform/xbox-decoded", None)
        else:
            path = original
        sources.append({"package": original.name, "sha256": sha(original.read_bytes()),
                        "decoded_sha256": sha(path.read_bytes())})
        for asset, raw, metadata in pak_entries(path):
            selection = next(((group, wanted[asset]) for group, wanted in GROUPS if asset in wanted), None)
            if not selection:
                continue
            if locale == "main": seen_common.add(asset)
            group, (category, task) = selection
            image = Image.open(io.BytesIO(xbox_dds(raw))).convert("RGBA")
            pixel_sha = sha(struct.pack("<II", *image.size) + image.tobytes())
            key = (asset, pixel_sha)
            png = f"{group}/png/{locale}/{asset[:-4]}.png"
            ptx = f"{group}/original_ptx/{locale}/{asset}"
            duplicate = key in unique
            if duplicate:
                png, ptx = unique[key]["png"], unique[key]["original_ptx"]
                # Equal decoded pixels may still have different native PTX padding.
                if sha((folder / ptx).read_bytes()) != sha(raw):
                    ptx = f"{group}/original_ptx/{locale}/{asset}"
                    dest = folder / ptx
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    dest.write_bytes(raw)
            else:
                dest = folder / png
                dest.parent.mkdir(parents=True, exist_ok=True)
                image.save(dest)
                dest = folder / ptx
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(raw)
                unique[key] = {"png": png, "original_ptx": ptx, "group": group,
                               "asset": asset, "locale": locale, "size": image.size}
            width, height, stride, fmt = struct.unpack(">4I", raw[-16:])
            records.append({"group": group, "category": category, "source_package": original.name,
                            "source_asset": asset, "source_ptx_sha256": sha(raw),
                            "source_payload_offset_in_decoded_pak": metadata["offset"],
                            "width": width, "height": height, "png": png, "original_ptx": ptx,
                            "pixel_sha256": pixel_sha, "shares_editable_png": duplicate,
                            "ptx_format": f"0x{fmt:08x}", "ptx_stride": stride, "task_ru": task})
    wanted = set(REQUIRED) | set(OPTIONAL) | set(REFERENCE)
    if wanted - seen_common: raise ValueError(f"Missing original sprites: {wanted - seen_common}")

    font = ImageFont.truetype("C:/Windows/Fonts/consola.ttf", 15)
    heading = ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 24)
    for group, _ in GROUPS:
        items = [item for item in unique.values() if item["group"] == group]
        for page in range((len(items) + 19) // 20):
            sheet = Image.new("RGB", (1440, 1130), "#edf0f3")
            draw = ImageDraw.Draw(sheet)
            draw.text((16, 10), f"{group} — {page+1}", fill="#202b38", font=heading)
            for index, item in enumerate(items[page*20:(page+1)*20]):
                x, y = (index % 4) * 360, 52 + (index // 4) * 215
                for cy in range(y+4, y+167, 12):
                    for cx in range(x+6, x+354, 12):
                        draw.rectangle((cx,cy,min(cx+11,x+353),min(cy+11,y+166)),
                                       fill="#b7bcc2" if ((cx-x-6)//12 + (cy-y-4)//12) % 2 else "#d7dce1")
                image = Image.open(folder / item["png"])
                image.thumbnail((330, 153))
                sheet.paste(image, (x+180-image.width//2, y+85-image.height//2), image)
                label = item["locale"] + " / " + item["asset"].removesuffix(".ptx")
                draw.text((x+7,y+171), label[:38], fill="#202b38", font=font)
                draw.text((x+7,y+189), label[38:76] or str(item["size"]), fill="#202b38", font=font)
            sheet.save(folder / f"preview-{group}-{page+1}.png")

    counts = {group: sum(u["group"] == group for u in unique.values()) for group, _ in GROUPS}
    manifest = {"product": "pvz-console-edition", "purpose": "pc-sprite-redraw-handoff",
                "unique_sprites": len(unique), "source_variants": len(records),
                "unique_by_group": counts, "source_packages": sources, "sprites": records}
    (folder / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    with (folder / "SPRITES.csv").open("w", encoding="utf-8-sig", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=["group","source_package","source_asset","width","height","png","original_ptx","shares_editable_png","task_ru"])
        writer.writeheader()
        writer.writerows({key: r[key] for key in writer.fieldnames} for r in records)

    tasks = ["# Задания на перерисовку\n"]
    for group, wanted in GROUPS:
        tasks.append(f"\n## {group}\n")
        for asset, (category, task) in wanted.items():
            variants = [r for r in records if r["source_asset"] == asset]
            paths = list(dict.fromkeys(r["png"] for r in variants))
            tasks.append(f"\n### {asset}\n\n{category}. {task}\n\n")
            tasks.append("\n".join(f"- [{p}]({p})" for p in paths) + "\n")
    (folder / "TASKS.md").write_text("".join(tasks), encoding="utf-8")

    readme = f"""# Спрайты для адаптации PvZ Console Edition под ПК

Собрано {len(unique)} различных спрайтов, соответствующих {len(records)} исходным
записям main/en/de/es/fr/it/jp PAK. Идентичные по пикселям языковые варианты
одного ресурса используют один PNG: точные соответствия в SPRITES.csv и manifest.json.

- **01_required** — {counts['01_required']} спрайтов: встроенные кнопки/рисунки Xbox,
  письмо, Survival Endless, выбор стороны VS и комплект стрелок магазина.
- **02_optional** — {counts['02_optional']} спрайтов: индикаторы игроков,
  кооперативные иллюстрации, отдельная A и оформление будущего импорта Full.
- **03_reference** — {counts['03_reference']} спрайтов: шаблоны кнопок,
  лопата, стрелка, рамки и фон справки. Не обязательный список перерисовки.

Начните с **preview-01_required-*.png** и **TASKS.md**. PNG в `png/` готовы
для рисования: исходный размер и прозрачность сохранены, подложка обзора
в сами спрайты не добавлена. В `original_ptx/` лежат исходные PTX для сравнения
и последующего подключения. Не нужно рисовать в PTX.

## Ограничения для художника

1. Сохранять размер холста, прозрачность, положение элемента и границы тени.
   Не обрезать пустые поля: от них зависят привязки и хитбоксы.
2. Все состояния кнопки выполнять согласованно. В магазине это normal,
   highlight и disabled; отдельного pressed PTX у этих стрелок нет.
3. `help_buttons` — 13 колонок по 42×42; малый атлас — по 21×21.
   Размеры и порядок ячеек подтверждены properties/resources.xml.
   Статическая перерисовка не решает смену пользовательских назначений клавиш:
   для произвольных клавиш лучше использовать отдельные иконки мыши и текст кода.
4. Не стирать геймпадные исходники: при активном Xbox/DualShock они нужны.
   PC-варианты должны подключаться отдельно по владельцу ввода.
5. Console-режим мыши сохраняет колесо выбора и выкапывание удержанием ПКМ;
   PC-режим использует выбор карточки/лопаты на ЛКМ и отмену на ПКМ.
   Схему управления не подписывать так, будто у обоих режимов одинаковые действия.
6. Часть подсказок сейчас скрывается кодом. Для появления новых рисунков
   нужно будет подключить PC-ресурсы: этот архив содержит исходники, не готовый мод.
7. Не редактировать локальные игровые PAK. Возвращать PNG с теми же именами
   и каталогами; после утверждения будет отдельная конвертация и интеграция.

## Что здесь не требует рисования

Обычный текст подсказок из LawnStrings, названия Menu/Back/Let's Rock и
позиционирование/нажатия обрабатываются кодом. Пакеты растений, солнца,
растения, зомби и фоны уровней не имеют подтверждённой нужды в PC-перерисовке
и не включены. Ранее замеченный артефакт горохострела не воспроизводится.
Схемы DualShock уже существуют отдельно в проекте; они не являются PC-графикой.

Языки: main — общие ресурсы/основной английский набор; en — английские
региональные переопределения, de/es/fr/it/jp — соответствующие локализации.
Их письма и надгробия приложены, чтобы не оставить встроенные Xbox-кнопки
при смене языка. Буква на английской монетке Full в en — £, в main — $;
эти монеты относятся к optional, не к обязательному устранению Xbox-глифов.

Хеши исходных PAK, исходных PTX и декодированных пикселей — manifest.json.
Все файлы архива проверены после записи; сохранения, EXE и целые PAK не включены.
"""
    (folder / "README.md").write_text(readme, encoding="utf-8")
    inventory = [{"path": f.relative_to(folder).as_posix(), "bytes": f.stat().st_size,
                  "sha256": sha(f.read_bytes())} for f in sorted(folder.rglob("*")) if f.is_file()]
    (folder / "files.json").write_text(json.dumps(inventory, indent=2), encoding="utf-8")
    archive.parent.mkdir(exist_ok=True)
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED) as output:
        for file in sorted(folder.rglob("*")):
            if file.is_file(): output.write(file, name + "/" + file.relative_to(folder).as_posix())
    with zipfile.ZipFile(archive) as output:
        prefix = name + "/"
        assert set(output.namelist()) == {prefix + f["path"] for f in inventory} | {prefix + "files.json"}
        for file in inventory:
            data = output.read(prefix + file["path"])
            assert len(data) == file["bytes"] and sha(data) == file["sha256"]
        for item in unique.values():
            im = Image.open(io.BytesIO(output.read(prefix + item["png"])))
            assert im.size == item["size"] and im.mode == "RGBA"
        for record in records:
            assert sha(output.read(prefix + record["original_ptx"])) == record["source_ptx_sha256"]
    digest = sha(archive.read_bytes())
    archive.with_suffix(".zip.sha256").write_text(f"{digest}  {archive.name}\n", encoding="ascii")
    report = {"archive": str(archive), "bytes": archive.stat().st_size, "sha256": digest,
              "sprites": len(unique), "source_variants": len(records), "groups": counts,
              "files_verified": len(inventory) + 1, "all_png_dimensions_alpha_verified": True}
    (ROOT / "runtime-data/pc-redraw-sprites-latest.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))

if __name__ == "__main__":
    main()
