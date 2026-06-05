# Infernux-style wave demo (Chen, arXiv:2604.10263 Experiment 1)
# Load: py_reload scripts/python/demo_wave.py

from idtech3.engine import Engine, demo_handles
from idtech3.jit import wave_kernel_y

_SIDE = 32
_COUNT = Engine.spawn_demo_grid(_SIDE, spacing=2.0)
_HANDLES = demo_handles(_COUNT)
_TIME = 0.0


def _on_frame(msec: int, real_msec: int) -> None:
    global _TIME
    _TIME += msec * 0.001
    flat = Engine.batch_read(_HANDLES, "position")
    ys = [flat[i + 1] for i in range(0, len(flat), 3)]
    wave_kernel_y(ys, _TIME)
    for i, y in enumerate(ys):
        flat[i * 3 + 1] = y
    Engine.batch_write(_HANDLES, "position", flat)


Engine.on("frame", _on_frame)
Engine.print(f"demo_wave: {_COUNT} entities, batch bridge (Infernux §VI-A)")
