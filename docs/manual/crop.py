# Нарезает скриншоты теста для руководства: python3 crop.py <папка со скриншотами> <папка img>
import sys
from PIL import Image

src, out = sys.argv[1].rstrip("/") + "/", sys.argv[2].rstrip("/") + "/"


def save(im, name):
    im.convert("RGB").save(out + name + ".jpg", quality=88)


# Grid 1400×720: поля 14, тулбар 52, отступ 10, сетка 4 × 2 с промежутком 10
W, H, M, BAR, GAP = 1400, 720, 14, 52, 10
rx, ry = M, M + BAR + GAP
rw, rh = W - 2 * M, H - M - ry
cw, ch = (rw - GAP * 3) // 4, (rh - GAP) // 2


def module(im, slot):
    c, r = slot % 4, slot // 4
    x, y = rx + c * (cw + GAP), ry + r * (ch + GAP)
    x1 = rx + rw if c == 3 else x + cw
    y1 = ry + rh if r == 1 else y + ch
    return im.crop((x, y, x1, y1))


grid = Image.open(src + "ui_rubber_grid.png")
more = Image.open(src + "ui_rubber_grid_more.png")
save(grid, "grid")
save(Image.open(src + "ui_rubber_strip.png"), "strip")
save(Image.open(src + "ui_oxygen_grid_more.png"), "grid_oxygen")
save(grid.crop((M, M, W - M, M + BAR)), "toolbar")
# раскладка по умолчанию: Spectrogram, Scope, Oscilloscope, Spectrum, Loudness, Stereo, VU, Tuner
for slot, name in enumerate(["spectrogram", "scope", "oscilloscope", "spectrum", "loudness", "stereo", "vu", "tuner"]):
    save(module(grid, slot), "m_" + name)
# вторая раскладка: слоты 1–4 — Waves, Peak History, Mid / Side, Loudness History
for slot, name in zip([1, 2, 3, 4], ["waves", "peakhistory", "midside", "loudnesshistory"]):
    save(module(more, slot), "m_" + name)
