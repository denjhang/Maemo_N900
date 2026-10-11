# -*- coding: utf-8 -*-
# SkyRaid - landscape shoot-em-up for Nokia N900 (Maemo 5) and PC
# Usage: python skyraid.py [--frames DIR]
#   --frames DIR : save a screenshot PNG every 60 frames (debug only: costs fps)
# Performance notes: sprites pre-rendered once, text cached, no per-frame
# scaling when the display is already 800x480 (always true on N900).
import sys, os, random, math
import pygame

W, H = 800, 480
FPS = 30
KEY_COLOR = (255, 0, 255)  # magic transparent color for sprites

def load_args():
    frames_dir = None
    a = sys.argv[1:]
    i = 0
    while i < len(a):
        if a[i] == "--frames" and i + 1 < len(a):
            frames_dir = a[i + 1]; i += 2
        else:
            i += 1
    return frames_dir

# ---- pre-rendered sprites (built once after pygame.init) ----
def build_sprites():
    sp = {}
    ship = pygame.Surface((34, 46))
    ship.set_colorkey(KEY_COLOR)
    ship.fill(KEY_COLOR)
    c = (60, 200, 255)
    ship.fill(c, (15, 2, 6, 36))            # fuselage
    ship.fill(c, (0, 24, 34, 8))            # wings
    ship.fill((255, 255, 255), (16, 0, 4, 6))  # nose
    ship.fill((255, 150, 40), (16, 40, 4, 5))  # exhaust
    sp["ship"] = ship
    ship_icon = pygame.Surface((10, 14))
    ship_icon.set_colorkey(KEY_COLOR)
    ship_icon.fill(KEY_COLOR)
    ship_icon.fill(c, (2, 0, 6, 14))
    sp["ship_icon"] = ship_icon
    for name, size, color in (("e1", 26, (255, 90, 90)),
                              ("e2", 32, (255, 170, 60)),
                              ("e3", 40, (190, 120, 255))):
        s = pygame.Surface((size, size))
        s.set_colorkey(KEY_COLOR)
        s.fill(KEY_COLOR)
        s.fill(color, (0, size // 2 - 5, size, 10))
        s.fill(color, (size // 2 - 5, 0, 10, size))
        s.fill((30, 30, 30), (size // 2 - 3, size // 2 - 2, 6, 6))
        sp[name] = s
    return sp

SPRITES = None

class Star:
    __slots__ = ("x", "y", "speed", "bright")
    def __init__(self):
        self.reset(first=True)
    def reset(self, first=False):
        self.x = random.randint(0, W - 1)
        self.y = random.randint(0, H - 1) if first else -1
        self.speed = random.choice((1, 1, 2, 3))
        self.bright = random.choice((90, 120, 160, 220))
    def update(self):
        self.y += self.speed
        if self.y > H: self.reset()
    def draw(self, surf):
        surf.fill((self.bright, self.bright, self.bright),
                  (self.x, self.y, 1 if self.speed < 3 else 2, self.speed + 1))

class Bullet:
    __slots__ = ("x", "y", "vy", "w", "h", "color", "dead")
    def __init__(self, x, y, vy=-(H // 20), w=3, h=9, color=(255, 240, 120)):
        self.x, self.y, self.vy = x, y, vy
        self.w, self.h, self.color = w, h, color
        self.dead = False
    def update(self):
        self.y += self.vy
        if self.y < -12 or self.y > H + 12: self.dead = True
    def draw(self, surf):
        surf.fill(self.color, (self.x - self.w // 2, self.y - self.h // 2, self.w, self.h))

class Particle:
    __slots__ = ("x", "y", "vx", "vy", "life", "color")
    def __init__(self, x, y, color):
        ang = random.random() * 6.283
        spd = random.uniform(1.0, 4.5)
        self.x, self.y = float(x), float(y)
        self.vx, self.vy = spd * math.cos(ang), spd * math.sin(ang)
        self.life = random.randint(10, 22)
        self.color = color
    def update(self):
        self.x += self.vx; self.y += self.vy; self.life -= 1
    def draw(self, surf):
        if self.life > 0:
            surf.fill(self.color, (int(self.x), int(self.y), 3, 3))

class Player:
    def __init__(self):
        self.w, self.h = 34, 40
        self.x, self.y = W // 2, H - 70
        self.cool = 0
        self.lives = 3
        self.invul = 0
    def rect(self):
        return pygame.Rect(self.x - self.w // 2, self.y - self.h // 2, self.w, self.h)
    def update(self, keys, mouse_pos, mouse_down):
        vx = vy = 0
        if keys[pygame.K_LEFT] or keys[pygame.K_a]: vx -= 6
        if keys[pygame.K_RIGHT] or keys[pygame.K_d]: vx += 6
        if keys[pygame.K_UP] or keys[pygame.K_w]: vy -= 6
        if keys[pygame.K_DOWN] or keys[pygame.K_s]: vy += 6
        self.x += vx; self.y += vy
        if mouse_down and mouse_pos is not None:
            tx, ty = mouse_pos[0], mouse_pos[1] - 40
            self.x += (tx - self.x) // 3
            self.y += (ty - self.y) // 3
        self.x = max(self.w // 2, min(W - self.w // 2, self.x))
        self.y = max(60, min(H - self.h // 2, self.y))
        if self.cool > 0: self.cool -= 1
        if self.invul > 0: self.invul -= 1
    def firing(self, keys, mouse_down):
        return self.cool <= 0 and (keys[pygame.K_SPACE] or mouse_down)
    def draw(self, surf):
        if self.invul > 0 and (self.invul // 3) % 2 == 0:
            return  # blink while invulnerable
        surf.blit(SPRITES["ship"], (int(self.x) - 17, int(self.y) - 23))

class Enemy:
    TYPES = [
        dict(hp=1, pts=100, sprite="e1", shoot=0),
        dict(hp=2, pts=250, sprite="e2", shoot=90),
        dict(hp=4, pts=500, sprite="e3", shoot=55),
    ]
    def __init__(self, wave):
        t = random.choice(self.TYPES[: min(3, 1 + wave // 3)])
        self.hp, self.pts = t["hp"], t["pts"]
        self.sprite = t["sprite"]
        s = SPRITES[self.sprite].get_width()
        self.shoot_gap = t["shoot"]
        self.w = self.h = s
        self.x = random.randint(30, W - 30)
        self.y = -s
        self.vy = random.choice((2, 3)) + wave // 4
        self.vx = random.choice((-2, -1, 0, 1, 2))
        self.cool = random.randint(40, 120) if self.shoot_gap else 0
        self.dead = False
    def rect(self):
        return pygame.Rect(self.x - self.w // 2, self.y - self.h // 2, self.w, self.h)
    def update(self):
        self.y += self.vy; self.x += self.vx
        if self.x < 20 or self.x > W - 20: self.vx = -self.vx
        if self.y > H + 30: self.dead = True
        if self.cool > 0: self.cool -= 1
    def wants_fire(self):
        if self.shoot_gap and self.cool <= 0 and 0 < self.y < H // 2:
            self.cool = self.shoot_gap
            return True
        return False
    def draw(self, surf):
        surf.blit(SPRITES[self.sprite], (int(self.x) - self.w // 2, int(self.y) - self.h // 2))

def make_font(size):
    try:
        return pygame.font.SysFont(None, size)
    except Exception:
        return pygame.font.Font(None, size)

def render_text(font, text, color):
    try:
        return font.render(text, 1, color)
    except Exception:
        return font.render(text, 0, color)

def main():
    global SPRITES
    frames_dir = load_args()
    if frames_dir and not os.path.isdir(frames_dir):
        os.makedirs(frames_dir)
    pygame.init()
    info = pygame.display.Info()
    if info.current_w == W and info.current_h == H:
        screen = pygame.display.set_mode((W, H), pygame.FULLSCREEN)
    elif info.current_w <= W:
        screen = pygame.display.set_mode((W, H), pygame.FULLSCREEN)
    else:
        screen = pygame.display.set_mode((W, H))
    pygame.display.set_caption("SkyRaid")
    canvas = screen  # same size -> draw straight onto the display surface
    scaled = None    # only PC windows larger than 800x480 would need this; we open at 1:1
    clock = pygame.time.Clock()
    SPRITES = build_sprites()
    font_big, font_small = make_font(48), make_font(22)
    white, yellow = (255, 255, 255), (255, 255, 0)

    stars = [Star() for _ in range(50)]
    player = Player()
    bullets, enemy_bullets, enemies, particles = [], [], [], []
    score, wave, frame = 0, 1, 0
    spawn_timer = 30
    state = "play"  # play | gameover
    score_img = render_text(font_small, "SCORE 000000", white)
    wave_img = render_text(font_small, "WAVE 1", yellow)
    lives_dirty = True

    running = True
    while running:
        mouse_pos, mouse_down = None, False
        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                running = False
            elif ev.type == pygame.KEYDOWN:
                if ev.key == pygame.K_ESCAPE or ev.key == pygame.K_F4:
                    running = False
                elif ev.key == pygame.K_RETURN and state == "gameover":
                    main(); return
            elif ev.type == pygame.MOUSEBUTTONDOWN:
                mouse_down = True
                mouse_pos = ev.pos
        mouse_down = mouse_down or pygame.mouse.get_pressed()[0]
        if pygame.mouse.get_pressed()[0]:
            mouse_pos = pygame.mouse.get_pos()

        if state == "play":
            keys = pygame.key.get_pressed()
            player.update(keys, mouse_pos, mouse_down)
            if player.firing(keys, mouse_down):
                bullets.append(Bullet(player.x - 8, player.y - 24))
                bullets.append(Bullet(player.x + 8, player.y - 24))
                player.cool = 6
            spawn_timer -= 1
            if spawn_timer <= 0:
                enemies.append(Enemy(wave))
                spawn_timer = max(18, 55 - wave * 2)
            for b in bullets: b.update()
            for b in enemy_bullets: b.update()
            for e in enemies:
                e.update()
                if e.wants_fire():
                    dx, dy = player.x - e.x, player.y - e.y
                    dist = max(1, (dx * dx + dy * dy) ** 0.5)
                    enemy_bullets.append(Bullet(e.x, e.y + 10,
                        vy=dy * 5.0 / dist, w=5, h=5, color=(255, 80, 220)))
            for e in enemies:
                for b in bullets:
                    if not b.dead and e.rect().collidepoint((b.x, b.y)):
                        b.dead = True; e.hp -= 1
                if e.hp <= 0 and not e.dead:
                    e.dead = True; score += e.pts
                    for _ in range(14):
                        particles.append(Particle(e.x, e.y,
                            SPRITES[e.sprite].get_at((e.w // 2, e.h // 2))))
                    new_wave = score // 2000 + 1
                    if new_wave > wave:
                        wave = new_wave
                        wave_img = render_text(font_small, "WAVE %d" % wave, yellow)
            pr = player.rect()
            for e in enemies:
                if pr.colliderect(e.rect()) and player.invul <= 0:
                    e.dead = True
                    player.lives -= 1; player.invul = 90
                    lives_dirty = True
                    for _ in range(20):
                        particles.append(Particle(player.x, player.y, (255, 255, 255)))
            for b in enemy_bullets:
                if not b.dead and pr.collidepoint((b.x, b.y)) and player.invul <= 0:
                    b.dead = True
                    player.lives -= 1; player.invul = 90
                    lives_dirty = True
                    for _ in range(20):
                        particles.append(Particle(player.x, player.y, (255, 255, 255)))
            bullets = [b for b in bullets if not b.dead]
            enemy_bullets = [b for b in enemy_bullets if not b.dead]
            enemies = [e for e in enemies if not e.dead]
            for p in particles: p.update()
            particles = [p for p in particles if p.life > 0]
            if player.lives <= 0:
                state = "gameover"
        for s in stars: s.update()

        # ---- render ----
        canvas.fill((8, 10, 26))
        for s in stars: s.draw(canvas)
        for e in enemies: e.draw(canvas)
        for b in enemy_bullets: b.draw(canvas)
        for b in bullets: b.draw(canvas)
        for p in particles: p.draw(canvas)
        if state == "play": player.draw(canvas)
        if score != getattr(main, "_sc", -1):
            main._sc = score
            score_img = render_text(font_small, "SCORE %06d" % score, white)
        canvas.blit(score_img, (10, 6))
        canvas.blit(wave_img, (W - wave_img.get_width() - 10, 6))
        if lives_dirty:
            lives_dirty = False
            canvas.fill((8, 10, 26), (W - 80, H - 26, 80, 20))
            for i in range(player.lives):
                canvas.blit(SPRITES["ship_icon"], (W - 20 - i * 16, H - 22))
        if state == "gameover":
            t1 = render_text(font_big, "GAME OVER", (255, 60, 60))
            t2 = render_text(font_small, "SCORE %06d" % score, white)
            canvas.blit(t1, (W // 2 - t1.get_width() // 2, H // 2 - 40))
            canvas.blit(t2, (W // 2 - t2.get_width() // 2, H // 2 + 20))
        pygame.display.flip()

        frame += 1
        if frames_dir and frame % 60 == 0:
            pygame.image.save(canvas, os.path.join(frames_dir, "frame_%05d.png" % frame))
        clock.tick(FPS)
    pygame.quit()

if __name__ == "__main__":
    main()
