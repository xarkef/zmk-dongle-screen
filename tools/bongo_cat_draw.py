from PIL import Image, ImageDraw
W, H, S = 48, 30, 2

def base():
    im = Image.new("1", (W, H), 0); d = ImageDraw.Draw(im)
    d.arc([6, 6, 41, 46], 180, 360, fill=1)          # body
    d.line([(12, 11), (13, 3), (19, 7)], fill=1)     # left ear
    d.line([(35, 11), (34, 3), (28, 7)], fill=1)     # right ear
    d.rectangle([18, 14, 18, 15], fill=1)            # eyes
    d.rectangle([29, 14, 29, 15], fill=1)
    d.point([(22, 18), (23, 19), (24, 19), (25, 18)], fill=1)  # mouth
    d.line([(0, 26), (47, 26)], fill=1)              # desk
    return im, d

def paw_up(d, left):
    box = [8, 14, 15, 20] if left else [32, 14, 39, 20]
    d.ellipse(box, fill=1)
    d.point([(10, 16), (13, 16)] if left else [(34, 16), (37, 16)], fill=0)  # toe beans

def paw_down(d, left):
    box = [8, 22, 17, 28] if left else [30, 22, 39, 28]
    d.ellipse(box, fill=1)
    d.point([(11, 24), (14, 24)] if left else [(33, 24), (36, 24)], fill=0)

frames = {}
for name, l, r in [("idle", "up", "up"), ("left", "down", "up"), ("right", "up", "down")]:
    im, d = base()
    (paw_up if l == "up" else paw_down)(d, True)
    (paw_up if r == "up" else paw_down)(d, False)
    frames[name] = im.resize((W * S, H * S), Image.NEAREST)

sheet = Image.new("1", (W * S * 3 + 20, H * S), 0)
for i, im in enumerate(frames.values()):
    sheet.paste(im, (i * (W * S + 10), 0))
sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST).save("preview.png")
for k, im in frames.items(): im.save(f"{k}.png")
