import os, sys, pygame
os.environ["SDL_VIDEODRIVER"]="dummy"; os.environ["SDL_AUDIODRIVER"]="dummy"
sys.argv=["skyraid.py","--frames","af_frames"]
import skyraid
class FakeKeys:
    def __getitem__(self, k): return 1 if k == pygame.K_SPACE else 0
pygame.key.get_pressed = lambda: FakeKeys()
skyraid.main()
