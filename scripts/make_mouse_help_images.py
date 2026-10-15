"""Draw the How to Play "Controls" mouse illustrations (our own art).

They replace help_menu_image_controller (384x293) while the mouse owns
input. Leader lines end where the game prints its labels:
  left  (15, 93)   HELP_TEXT_2_LSTICK  -> LMB
  right (352, 97)  HELP_TEXT_2_B       -> RMB (Console)
  top   (100, 6)   HELP_TEXT_2_BUMPERS -> MMB (Console)
Output: resources/mouse/help_menu_image_mouse_{pc,console}.ptx and .png previews.

VS Pick Sides shows a controller icon per player (gamepad0 yellow for player 1,
gamepad1 cyan for player 2, 170x122). The mouse player gets a mouse in the
same glow: resources/mouse/vs_mouse_player{1,2}.ptx.
Needs Pillow (tools/python is searched).

    python scripts/make_mouse_help_images.py
"""
import io
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools' / 'python'))
sys.path.insert(0, str(ROOT / 'scripts'))
from PIL import Image, ImageChops, ImageDraw  # noqa: E402
from extract_ps3_controller_assets import xbox_ptx  # noqa: E402

W, H, K = 384, 293, 4          # image size and supersampling
OUTLINE = (40, 40, 44, 255)
BODY = (236, 236, 232, 255)
SHADE = (205, 205, 200, 255)
LINE = (255, 255, 255, 255)
GREEN, RED, BLUE = (70, 170, 60, 255), (205, 60, 45, 255), (50, 95, 205, 255)


def s(*values):
    return [v * K for v in values]


def line(draw, a, b):
    draw.line(s(*a, *b), fill=OUTLINE, width=7 * K)
    draw.line(s(*a, *b), fill=LINE, width=3 * K)


def make(console: bool) -> Image.Image:
    image = Image.new('RGBA', (W * K, H * K), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    # Leader lines first; the mouse body covers their inner ends.
    line(draw, (150, 93), (15, 93))
    if console:
        line(draw, (236, 97), (352, 97))
        line(draw, (192, 60), (100, 6))
    left, top, right, bottom = 117, 30, 267, 282
    draw.rounded_rectangle(s(left, top, right, bottom), radius=72 * K, fill=OUTLINE)
    draw.rounded_rectangle(s(left + 4, top + 4, right - 4, bottom - 4), radius=68 * K, fill=BODY)
    # Lower shading, like the controller art.
    draw.rounded_rectangle(s(left + 14, 190, right - 14, bottom - 12), radius=50 * K, fill=SHADE)
    draw.rounded_rectangle(s(left + 4, 150, right - 4, 200), radius=10 * K, fill=BODY)
    # Buttons: split at the centre and above the palm.
    split = 128
    buttons = [(GREEN, (left + 4, top + 4, 192, split))]
    if console:
        buttons.append((RED, (192, top + 4, right - 4, split)))
    for color, box in buttons:
        mask = Image.new('L', image.size, 0)
        ImageDraw.Draw(mask).rounded_rectangle(s(left + 4, top + 4, right - 4, bottom - 4), radius=68 * K, fill=255)
        tint = Image.new('L', image.size, 0)
        ImageDraw.Draw(tint).rectangle(s(*box), fill=255)
        # The button colour stays inside the body outline.
        image.paste(Image.new('RGBA', image.size, color), (0, 0), ImageChops.multiply(mask, tint))
    draw = ImageDraw.Draw(image)
    draw.line(s(192, top + 2, 192, split), fill=OUTLINE, width=4 * K)
    draw.line(s(left + 4, split, right - 4, split), fill=OUTLINE, width=4 * K)
    # Wheel.
    draw.rounded_rectangle(s(180, 52, 204, 104), radius=12 * K, fill=OUTLINE)
    draw.rounded_rectangle(s(184, 56, 200, 100), radius=8 * K, fill=BLUE if console else (120, 120, 125, 255))
    for y in (66, 78, 90):
        draw.line(s(186, y, 198, y), fill=OUTLINE, width=2 * K)
    return image.resize((W, H), Image.LANCZOS)


PLAYER_GLOW = ((255, 226, 20), (40, 215, 235))  # gamepad0 / gamepad1 outlines


def make_player_icon(glow) -> Image.Image:
    w, h = 170, 122
    body = Image.new('RGBA', (w * K, h * K), (0, 0, 0, 0))
    draw = ImageDraw.Draw(body)
    # A mouse lying slightly tilted, sized like the controller in the icon.
    left, top, right, bottom = 52, 10, 118, 112
    draw.rounded_rectangle(s(left, top, right, bottom), radius=32 * K, fill=OUTLINE)
    draw.rounded_rectangle(s(left + 3, top + 3, right - 3, bottom - 3), radius=29 * K, fill=BODY)
    draw.rounded_rectangle(s(left + 10, 72, right - 10, bottom - 9), radius=22 * K, fill=SHADE)
    draw.rectangle(s(left + 3, 62, right - 3, 78), fill=BODY)
    split = 52
    draw.line(s(85, top + 2, 85, split), fill=OUTLINE, width=3 * K)
    draw.line(s(left + 3, split, right - 3, split), fill=OUTLINE, width=3 * K)
    draw.rounded_rectangle(s(80, 22, 90, 44), radius=5 * K, fill=OUTLINE)
    draw.rounded_rectangle(s(82, 24, 88, 42), radius=3 * K, fill=(120, 120, 125, 255))
    body = body.rotate(-12, resample=Image.BICUBIC, center=(85 * K, 61 * K))
    # Soft coloured glow around the silhouette, as on the controller icons.
    alpha = body.getchannel('A')
    from PIL import ImageFilter
    halo = alpha.filter(ImageFilter.MaxFilter(13 * K // 2 * 2 + 1)).filter(ImageFilter.GaussianBlur(2 * K))
    icon = Image.new('RGBA', body.size, glow + (0,))
    icon.putalpha(halo)
    icon.alpha_composite(body)
    return icon.resize((w, h), Image.LANCZOS)


def save_ptx(image, path):
    dds = io.BytesIO()
    image.save(dds, format='DDS', pixel_format='DXT5')
    path.write_bytes(xbox_ptx(dds.getvalue()))
    print(path)


def main():
    out = ROOT / 'resources' / 'mouse'
    out.mkdir(parents=True, exist_ok=True)
    for name, console in (('pc', False), ('console', True)):
        image = make(console)
        image.save(out / f'help_menu_image_mouse_{name}.png')
        save_ptx(image, out / f'help_menu_image_mouse_{name}.ptx')
    for player, glow in enumerate(PLAYER_GLOW, 1):
        icon = make_player_icon(glow)
        icon.save(out / f'vs_mouse_player{player}.png')
        save_ptx(icon, out / f'vs_mouse_player{player}.ptx')


if __name__ == '__main__':
    main()
