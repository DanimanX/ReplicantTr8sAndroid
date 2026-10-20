#!/usr/bin/env python3
"""
Replicant TR-8S Android — 1080p promotional video generator.

Uses real emulator screenshots of the Compose UI, a soundtrack rendered by the
native Oboe/C++ drum engine, and a small amount of motion design. Outputs:

    assets/replicant_tr8s_android_promo_1080p.mp4
    assets/replicant_tr8s_android_poster.png
"""

import math
import os
import subprocess
import sys
import wave

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont
from scipy import signal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, "assets")
SHOTS = os.path.join(ROOT, "screenshots")

W, H = 1920, 1080
FPS = 30
DURATION = 48.0
SR = 48000

CYAN = (47, 230, 208)
AMBER = (255, 180, 92)
CORAL = (255, 90, 110)
WHITE = (231, 244, 241)
MUTED = (127, 150, 155)
VIOLET = (167, 139, 250)

F_TITLE = "/usr/share/fonts/TTF/RobotoCondensed-Bold.ttf"
F_BODY = "/usr/share/fonts/TTF/RobotoCondensed-Regular.ttf"
F_MONO = "/usr/share/fonts/TTF/JetBrainsMonoNerdFontMono-Bold.ttf"

_fonts = {}


def font(path, size):
    key = (path, size)
    if key not in _fonts:
        _fonts[key] = ImageFont.truetype(path, size)
    return _fonts[key]


def load_wav(path, n):
    with wave.open(path, "rb") as w:
        ch = w.getnchannels()
        sw = w.getsampwidth()
        raw = np.frombuffer(w.readframes(w.getnframes()), dtype=np.uint8)
    if sw == 2:
        v = raw.view(np.int16).astype(np.float32) / 32768.0
    elif sw == 3:
        b = raw.reshape(-1, 3)
        x = (b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8)
             | (b[:, 2].astype(np.int32) << 16))
        x = np.where(x >= 1 << 23, x - (1 << 24), x).astype(np.float32) / 8388608.0
        v = x
    else:
        v = raw.astype(np.float32) / 127.0 - 1.0
    v = v.reshape(-1, ch)
    if ch == 1:
        v = np.repeat(v, 2, axis=1)
    if len(v) < n:
        v = np.pad(v, ((0, n - len(v)), (0, 0)))
    return v[:n]


def env_ramp(n, points):
    times = np.array([p[0] for p in points], dtype=np.float64)
    gains = np.array([p[1] for p in points], dtype=np.float64)
    t = np.arange(n) / SR
    return np.interp(t, times, gains).astype(np.float32)


def build_audio():
    n = int(DURATION * SR)
    t = np.arange(n) / SR
    rng = np.random.default_rng(808909)

    stem = os.path.join(ASSETS, "replicant_tr8s_android_demo.wav")
    if not os.path.exists(stem):
        raise SystemExit(f"missing {stem}; run ./build-host/render_demo first")
    drums = load_wav(stem, n)

    pad = np.zeros((n, 2), dtype=np.float32)
    for ch, detune in ((0, 0.997), (1, 1.003)):
        p = np.zeros(n, dtype=np.float32)
        for f in (55.0, 82.407, 110.0, 130.813, 164.814):
            p += (2.0 * ((f * detune * t) % 1.0) - 1.0).astype(np.float32) * 0.13
        sos = signal.butter(2, 460.0, "lp", fs=SR, output="sos")
        pad[:, ch] = signal.sosfilt(sos, p).astype(np.float32)

    pad_gain = env_ramp(n, [(0, 0.55), (7, 0.20), (14, 0.16), (34, 0.18),
                            (43, 0.26), (47, 0.30), (48, 0.0)])

    rain = np.zeros((n, 2), dtype=np.float32)
    for ch in range(2):
        noise = rng.standard_normal(n).astype(np.float32)
        sos = signal.butter(2, 2200.0, "hp", fs=SR, output="sos")
        noise = signal.sosfilt(sos, noise).astype(np.float32)
        slow = 0.7 + 0.3 * np.sin(2 * np.pi * t / 19.0 + ch)
        rain[:, ch] = noise * slow * 0.025

    drum_gain = env_ramp(n, [(0, 0.0), (3.0, 0.25), (5.0, 0.95), (41.0, 0.95),
                             (45.0, 0.55), (48.0, 0.0)])

    # A dark filter riser into the final act.
    riser = np.zeros((n, 2), dtype=np.float32)
    mask = (t > 40.0) & (t < 44.0)
    if np.any(mask):
        idx = np.where(mask)[0]
        noise = rng.standard_normal(len(idx)).astype(np.float32)
        sos = signal.butter(2, 1200.0, "hp", fs=SR, output="sos")
        noise = signal.sosfilt(sos, noise).astype(np.float32)
        riser[idx, 0] = noise * np.linspace(0.0, 0.12, len(idx))
        riser[idx, 1] = noise * np.linspace(0.0, 0.12, len(idx))

    mix = (drums * drum_gain[:, None] * 0.98
           + pad * pad_gain[:, None] * 0.62
           + rain * 0.55
           + riser)

    peak = np.abs(mix).max()
    if peak > 0:
        mix *= 0.62 / peak
    fade = int(0.8 * SR)
    mix[:fade] *= np.linspace(0.0, 1.0, fade)[:, None]
    mix[-int(2.4 * SR):] *= np.linspace(1.0, 0.0, int(2.4 * SR))[:, None]

    out = "/tmp/replicant_android_promo_mix.wav"
    pcm = (np.clip(mix, -1.0, 1.0) * 32767.0).astype(np.int16)
    with wave.open(out, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())
    print(f"[audio] wrote {out} ({DURATION:.0f}s, peak {np.abs(mix).max():.2f})")
    return out, mix


def background():
    top = np.array([10, 18, 22], dtype=np.float32)
    bottom = np.array([3, 5, 6], dtype=np.float32)
    t = np.linspace(0, 1, H, dtype=np.float32)[:, None, None]
    arr = (top[None, None, :] * (1 - t) + bottom[None, None, :] * t)
    arr = np.repeat(arr, W, axis=1).astype(np.uint8)
    img = Image.fromarray(arr, "RGB").convert("RGBA")

    grid = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(grid)
    for x in range(0, W, 64):
        gd.line((x, 0, x, H), fill=(30, 44, 50, 40), width=1)
    for y in range(0, H, 64):
        gd.line((0, y, W, y), fill=(30, 44, 50, 22), width=1)
    img = Image.alpha_composite(img, grid)

    for color, cx, cy, radius, alpha in (
        (CYAN, 380, 40, 620, 30),
        (CORAL, 1640, 980, 640, 26),
        (VIOLET, 1250, 140, 420, 16),
    ):
        glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        gd = ImageDraw.Draw(glow)
        gd.ellipse((cx - radius, cy - radius, cx + radius, cy + radius),
                   fill=(color[0], color[1], color[2], alpha))
        glow = glow.filter(ImageFilter.GaussianBlur(140))
        img = Image.alpha_composite(img, glow)

    # Vignette
    vig = Image.new("L", (W, H), 0)
    vd = ImageDraw.Draw(vig)
    vd.ellipse((-W * 0.28, -H * 0.35, W * 1.28, H * 1.35), fill=255)
    vig = vig.filter(ImageFilter.GaussianBlur(160))
    dark = Image.new("RGBA", (W, H), (0, 0, 0, 110))
    dark.putalpha(Image.eval(vig, lambda p: 110 - int(p * 110 / 255)))
    img = Image.alpha_composite(img, dark)

    rng = np.random.default_rng(7)
    noise = rng.normal(0, 7, (H, W, 1)).repeat(3, axis=2)
    noise_img = Image.fromarray(np.clip(noise + 128, 0, 255).astype(np.uint8), "RGB")
    noise_img = noise_img.convert("RGBA")
    noise_img.putalpha(10)
    img = Image.alpha_composite(img, noise_img)
    return img


def device_frame(path, height=None, width=None, radius_ratio=0.045, camera=True):
    src = Image.open(path).convert("RGB")
    if width is not None:
        w = width
        h = int(src.height * w / src.width)
    else:
        h = height
        w = int(src.width * h / src.height)
    scale = h / src.height
    bezel = max(8, int(min(w, h) * 0.018))
    fw, fh = w + bezel * 2, h + bezel * 2
    outer_r = int(min(fw, fh) * radius_ratio)

    screen = src.resize((w, h), Image.LANCZOS)
    frame = Image.new("RGBA", (fw, fh), (0, 0, 0, 0))
    d = ImageDraw.Draw(frame)
    d.rounded_rectangle((0, 0, fw - 1, fh - 1), radius=outer_r,
                        fill=(9, 14, 17, 255), outline=(52, 68, 74, 255), width=2)

    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (0, 0, w - 1, h - 1), radius=max(4, outer_r - bezel), fill=255
    )
    frame.paste(screen, (bezel, bezel), mask)

    # Inner glass edge and a restrained reflection.
    d.rounded_rectangle((bezel - 1, bezel - 1, bezel + w, bezel + h),
                        radius=max(4, outer_r - bezel), outline=(0, 0, 0, 150), width=1)
    refl = Image.new("RGBA", (fw, fh), (0, 0, 0, 0))
    rd = ImageDraw.Draw(refl)
    rd.polygon([(bezel, bezel), (bezel + int(w * 0.62), bezel),
                (bezel + int(w * 0.18), bezel + h), (bezel, bezel + h)],
               fill=(255, 255, 255, 9))
    refl.putalpha(Image.composite(refl.getchannel("A"), Image.new("L", (fw, fh), 0), mask.resize((fw, fh), Image.LANCZOS)) if False else refl.getchannel("A"))
    frame = Image.alpha_composite(frame, refl)

    if camera and h > w:
        cx, cy = fw // 2, bezel // 2 + 1
        d.ellipse((cx - 3, cy - 3, cx + 3, cy + 3), fill=(20, 28, 32, 255))
        d.ellipse((cx - 2, cy - 2, cx + 2, cy + 2), fill=(8, 12, 14, 255))
    return frame


def shadow_for(frame):
    sw, sh = frame.size
    shadow = Image.new("RGBA", (sw + 160, sh + 160), (0, 0, 0, 0))
    mask = frame.getchannel("A")
    black = Image.new("RGBA", frame.size, (0, 0, 0, 210))
    shadow.paste(black, (80, 90), mask)
    return shadow.filter(ImageFilter.GaussianBlur(34))


def glow_for(w, h, color, alpha=90):
    g = Image.new("RGBA", (w + 300, h + 300), (0, 0, 0, 0))
    d = ImageDraw.Draw(g)
    d.rounded_rectangle((150, 150, 150 + w, 150 + h), radius=60,
                        fill=(color[0], color[1], color[2], alpha))
    return g.filter(ImageFilter.GaussianBlur(80))


def paste_device(layer, frame, shadow, glow, x, y):
    if glow is not None:
        layer.alpha_composite(glow, (x - 150, y - 150))
    layer.alpha_composite(shadow, (x - 80, y - 80))
    layer.alpha_composite(frame, (x, y))


def spaced(draw, xy, text, fnt, fill, spacing=3, anchor="la"):
    widths = [draw.textlength(c, font=fnt) for c in text]
    total = sum(widths) + spacing * max(0, len(text) - 1)
    x, y = xy
    if anchor[0] == "m":
        x -= total / 2
    elif anchor[0] == "r":
        x -= total
    va = anchor[1] if len(anchor) > 1 else "a"
    for c, w in zip(text, widths):
        draw.text((x, y), c, font=fnt, fill=fill, anchor="l" + va)
        x += w + spacing
    return total


def text_glow(layer, xy, text, fnt, fill, anchor="la", blur=14, alpha=90):
    g = Image.new("RGBA", layer.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(g)
    d.text(xy, text, font=fnt, fill=(fill[0], fill[1], fill[2], alpha), anchor=anchor)
    g = g.filter(ImageFilter.GaussianBlur(blur))
    layer.alpha_composite(g)
    ImageDraw.Draw(layer).text(xy, text, font=fnt, fill=fill, anchor=anchor)


def text_block(layer, x, y, eyebrow, title, bullets, accent=CYAN, title_size=86, max_width=780):
    d = ImageDraw.Draw(layer)
    f_eye = font(F_MONO, 20)
    f_title = font(F_TITLE, title_size)
    f_bullet = font(F_MONO, 19)
    f_body = font(F_BODY, 24)

    d.ellipse((x, y + 6, x + 10, y + 16), fill=accent)
    spaced(d, (x + 22, y), eyebrow, f_eye, accent, spacing=2)

    yy = y + 42
    for line in title.split("\n"):
        text_glow(layer, (x, yy), line, f_title, WHITE, blur=18, alpha=50)
        yy += int(title_size * 0.92)

    yy += 18
    for b in bullets:
        d.rectangle((x, yy + 9, x + 18, yy + 11), fill=accent)
        d.text((x + 30, yy), b, font=f_bullet, fill=MUTED, anchor="la")
        yy += 36
    return yy


def build_scenes():
    bg = background()
    scenes = []

    phone_perform = device_frame(os.path.join(SHOTS, "phone_perform.png"), height=800)
    phone_steps = device_frame(os.path.join(SHOTS, "phone_steps.png"), height=760)
    phone_mixer = device_frame(os.path.join(SHOTS, "phone_mixer.png"), height=760)
    phone_kits = device_frame(os.path.join(SHOTS, "phone_kits.png"), height=760)
    phone_midi = device_frame(os.path.join(SHOTS, "phone_midi.png"), height=830)
    tablet_land = device_frame(os.path.join(SHOTS, "tablet_landscape.png"), width=1080, radius_ratio=0.035, camera=False)
    tablet_port = device_frame(os.path.join(SHOTS, "tablet_portrait.png"), height=720, radius_ratio=0.035, camera=False)

    frames = {
        "phone_perform": phone_perform, "phone_steps": phone_steps,
        "phone_mixer": phone_mixer, "phone_kits": phone_kits,
        "phone_midi": phone_midi, "tablet_land": tablet_land,
        "tablet_port": tablet_port,
    }
    shadows = {k: shadow_for(v) for k, v in frames.items()}
    glows = {
        "cyan": glow_for(700, 700, CYAN, 70),
        "coral": glow_for(700, 700, CORAL, 70),
        "violet": glow_for(700, 700, VIOLET, 70),
    }

    # 0 — Intro
    s = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(s)
    d.rounded_rectangle((W // 2 - 56, 250, W // 2 + 56, 362), radius=20,
                        fill=(47, 230, 208, 22), outline=(47, 230, 208, 120), width=2)
    spaced(d, (W // 2, 286), "R8", font(F_TITLE, 58), CYAN, spacing=2, anchor="ma")
    text_glow(s, (W // 2, 420), "REPLICANT", font(F_TITLE, 156), WHITE, anchor="ma", blur=30, alpha=70)
    spaced(d, (W // 2, 590), "TR-8S // ANDROID", font(F_MONO, 32), CYAN, spacing=4, anchor="ma")
    d.line((W // 2 - 300, 660, W // 2 + 300, 660), fill=(47, 230, 208, 90), width=1)
    spaced(d, (W // 2, 700), "A RHYTHM PERFORMER REBUILT FOR TOUCH", font(F_MONO, 21), MUTED, spacing=3, anchor="ma")
    spaced(d, (W // 2, 770), "NATIVE C++ ENGINE  ·  KOTLIN + COMPOSE  ·  ADAPTIVE UI", font(F_MONO, 15), (90, 110, 116), spacing=2, anchor="ma")
    scenes.append(s)

    # 1 — Phone perform
    s = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    text_block(s, 110, 190, "01 // PERFORMANCE", "HARDWARE FEEL.\nTOUCH SPEED.",
               ["OBOE / AAUDIO LOW-LATENCY PATH",
                "11 ACB + FM + PCM VOICES",
                "LIVE SCOPE + PER-VOICE KNOBS",
                "48 kHz · FLOAT32 · 1-BURST BUFFER"], accent=CORAL, title_size=96)
    paste_device(s, phone_perform, shadows["phone_perform"], glows["coral"], 1340, 105)
    scenes.append(s)

    # 2 — Tablet + phone steps
    s = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(s)
    spaced(d, (110, 92), "02 // ADAPTIVE LAYOUT", font(F_MONO, 20), CYAN, spacing=3)
    text_glow(s, (110, 132), "PHONE. TABLET. STAGE.", font(F_TITLE, 78), WHITE, blur=18, alpha=50)
    spaced(d, (110, 230), "ONE COMPOSE LAYOUT FROM 6\" TO 13\"", font(F_MONO, 19), MUTED, spacing=2)
    paste_device(s, tablet_land, shadows["tablet_land"], glows["cyan"], 60, 330)
    paste_device(s, phone_steps, shadows["phone_steps"], glows["violet"], 1370, 255)
    scenes.append(s)

    # 3 — MIDI
    s = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    text_block(s, 100, 180, "03 // USB MIDI", "IN. OUT. IN SYNC.",
               ["CLASS-COMPLIANT USB MIDI HOSTS",
                "MIDI 2.0 UMP TRANSLATION",
                "24 PPQN CLOCK + TRANSPORT SYNC",
                "LIVE MONITOR + CHANNEL FILTER"], accent=VIOLET, title_size=94)
    paste_device(s, phone_midi, shadows["phone_midi"], glows["violet"], 1320, 115)
    scenes.append(s)

    # 4 — Library
    s = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(s)
    spaced(d, (W // 2, 78), "04 // THE FACTORY LIBRARY", font(F_MONO, 20), AMBER, spacing=3, anchor="ma")
    text_glow(s, (W // 2, 122), "128 KITS. 128 PATTERNS. 522 TONES.", font(F_TITLE, 76), WHITE, anchor="ma", blur=18, alpha=50)
    spaced(d, (W // 2, 220), "BRING YOUR OWN TR-8S v3.00 FIRMWARE  ·  NO PROPRIETARY DATA SHIPPED", font(F_MONO, 18), MUTED, spacing=1, anchor="ma")
    paste_device(s, phone_kits, shadows["phone_kits"], glows["cyan"], 330, 280)
    paste_device(s, phone_mixer, shadows["phone_mixer"], glows["coral"], 1030, 280)
    scenes.append(s)

    # 5 — Outro collage
    s = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(s)
    paste_device(s, tablet_port, shadows["tablet_port"], glows["cyan"], 120, 290)
    paste_device(s, phone_perform, shadows["phone_perform"], glows["coral"], 1300, 260)
    text_glow(s, (W // 2 + 70, 70), "BUILD THE RHYTHM. ANYWHERE.", font(F_TITLE, 72), WHITE, anchor="ma", blur=20, alpha=50)
    scenes.append(s)

    # 6 — End card
    s = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(s)
    d.rounded_rectangle((W // 2 - 48, 250, W // 2 + 48, 346), radius=16,
                        fill=(47, 230, 208, 18), outline=(47, 230, 208, 110), width=2)
    spaced(d, (W // 2, 279), "R8", font(F_TITLE, 48), CYAN, spacing=2, anchor="ma")
    text_glow(s, (W // 2, 390), "REPLICANT TR-8S", font(F_TITLE, 110), WHITE, anchor="ma", blur=26, alpha=60)
    spaced(d, (W // 2, 520), "ANDROID // LOW-LATENCY RHYTHM PERFORMER", font(F_MONO, 24), CYAN, spacing=3, anchor="ma")
    spaced(d, (W // 2, 600), "KOTLIN · COMPOSE · OBOE · C++", font(F_MONO, 18), MUTED, spacing=2, anchor="ma")
    spaced(d, (W // 2, 680), "github.com/DanimanX/ReplicantTr8sAndroid", font(F_MONO, 22), (180, 210, 208), spacing=1, anchor="ma")
    d.line((W // 2 - 360, 750, W // 2 + 360, 750), fill=(47, 230, 208, 80), width=1)
    spaced(d, (W // 2, 785), "MIT LICENSED  ·  BUILT BY DANIMANX", font(F_MONO, 14), (80, 98, 104), spacing=2, anchor="ma")
    scenes.append(s)
    return scenes, bg


SCENE_STARTS = [0.0, 5.0, 13.0, 21.0, 29.0, 37.0, 44.0]
SCENE_ENDS = [5.0, 13.0, 21.0, 29.0, 37.0, 44.0, 48.0]


def scene_alpha(t, idx):
    start, end = SCENE_STARTS[idx], SCENE_ENDS[idx]
    if t < start or t > end:
        return 0.0
    local = t - start
    total = end - start
    fade_in = 0.7 if idx > 0 else 1.2
    fade_out = 0.55 if idx < len(SCENE_STARTS) - 1 else 1.4
    return min(1.0, local / fade_in, (total - local) / fade_out)


def waveform_strip(draw, envelope, t, accent):
    if not envelope:
        return
    n = len(envelope)
    idx = int((t / DURATION) * (n - 1))
    span = 96
    vals = []
    for i in range(span):
        j = idx - span // 2 + i
        if 0 <= j < n:
            vals.append(envelope[j])
        else:
            vals.append(0.0)
    base = H - 54
    bw = W / span
    for i, v in enumerate(vals):
        hgt = int(3 + v * 24)
        color = (accent[0], accent[1], accent[2], 42)
        draw.rectangle((i * bw + 1, base - hgt, i * bw + bw - 2, base + hgt * 0.35),
                       fill=color)


def envelope_from_audio(mix):
    mono = np.abs(mix).mean(axis=1)
    win = int(SR * 0.02)
    usable = len(mono) // win
    env = mono[:usable * win].reshape(usable, win).max(axis=1)
    env = env / (env.max() + 1e-9)
    return env.tolist()


def render(video_path, poster_path, audio_path, mix):
    scenes, bg = build_scenes()
    env = envelope_from_audio(mix)
    cmd = [
        "ffmpeg", "-y",
        "-f", "rawvideo", "-vcodec", "rawvideo", "-pix_fmt", "rgb24",
        "-s", f"{W}x{H}", "-r", str(FPS), "-i", "-",
        "-i", audio_path,
        "-c:v", "libx264", "-preset", "medium", "-crf", "18",
        "-pix_fmt", "yuv420p", "-profile:v", "high", "-level", "4.2",
        "-af", "alimiter=limit=0.95", "-c:a", "aac", "-b:a", "192k",
        "-shortest", "-movflags", "+faststart", video_path,
    ]
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
    total = int(DURATION * FPS)
    accents = [CYAN, CORAL, CYAN, VIOLET, AMBER, CYAN, CYAN]
    for frame_idx in range(total):
        t = frame_idx / FPS
        img = bg.copy()

        # Find the active scene and crossfade a short overlap.
        idx = max(i for i, st in enumerate(SCENE_STARTS) if t >= st)
        alpha = scene_alpha(t, idx)
        if idx < len(scenes) - 1:
            nxt = idx + 1
            if t >= SCENE_STARTS[nxt] - 0.25:
                a2 = min(1.0, (t - (SCENE_STARTS[nxt] - 0.25)) / 0.5)
                a1 = 1.0 - a2
                layer = scenes[idx].copy()
                layer.putalpha(layer.getchannel("A").point(lambda p, a=a1: int(p * a)))
                img.alpha_composite(layer)
                layer2 = scenes[nxt].copy()
                layer2.putalpha(layer2.getchannel("A").point(lambda p, a=a2: int(p * a)))
                img.alpha_composite(layer2)
            else:
                layer = scenes[idx].copy()
                layer.putalpha(layer.getchannel("A").point(lambda p, a=alpha: int(p * a)))
                dx = int(3 * math.sin(t * 0.55))
                dy = int(2 * math.cos(t * 0.43))
                img.alpha_composite(layer, (dx, dy))
        else:
            layer = scenes[idx].copy()
            layer.putalpha(layer.getchannel("A").point(lambda p, a=alpha: int(p * a)))
            img.alpha_composite(layer)

        d = ImageDraw.Draw(img, "RGBA")
        waveform_strip(d, env, t, accents[idx])
        # Thin scanline accent.
        d.line((0, 0, W, 0), fill=(47, 230, 208, 26), width=2)

        proc.stdin.write(img.convert("RGB").tobytes())

        if abs(t - 9.6) < 1.0 / FPS:
            img.convert("RGB").save(poster_path)

    proc.stdin.close()
    proc.wait()
    if proc.returncode != 0:
        raise SystemExit("ffmpeg failed")


def main():
    audio_path, mix = build_audio()
    os.makedirs(ASSETS, exist_ok=True)
    video = os.path.join(ASSETS, "replicant_tr8s_android_promo_1080p.mp4")
    poster = os.path.join(ASSETS, "replicant_tr8s_android_poster.png")
    render(video, poster, audio_path, mix)
    print(f"[video] {video}")
    print(f"[poster] {poster}")


if __name__ == "__main__":
    main()
