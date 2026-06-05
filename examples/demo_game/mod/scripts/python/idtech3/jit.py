"""
Infernux-style JIT helper (Chen, arXiv:2604.10263 §VI-B).

When Numba is installed, counted ``for i in range(n)`` loops over NumPy
arrays can compile to LLVM with optional ``prange`` parallelism.
"""

from __future__ import annotations

import ast
import functools
import warnings
from typing import Any, Callable, TypeVar

F = TypeVar("F", bound=Callable[..., Any])

_NUMBA_WARNED = False


def _warn_no_numba() -> None:
    global _NUMBA_WARNED
    if not _NUMBA_WARNED:
        _NUMBA_WARNED = True
        warnings.warn(
            "Numba not installed; infernux_jit runs pure Python (see infernux_jit in docs/PYTHON.md)",
            RuntimeWarning,
            stacklevel=3,
        )


class _RangeToPrange(ast.NodeTransformer):
    """Promote ``for i in range(n)`` to ``for i in prange(n)`` when safe."""

    def visit_For(self, node: ast.For) -> ast.AST:
        self.generic_visit(node)
        if (
            isinstance(node.target, ast.Name)
            and isinstance(node.iter, ast.Call)
            and isinstance(node.iter.func, ast.Name)
            and node.iter.func.id == "range"
        ):
            node.iter.func.id = "prange"
        return node


def infernux_jit(*, parallel: bool = True) -> Callable[[F], F]:
    """Decorator mirroring Infernux JIT path with Numba fallback."""

    def decorator(fn: F) -> F:
        try:
            import numba
            from numba import njit, prange

            if parallel:
                src = ast.unparse(fn)
                tree = ast.parse(src)
                tree = _RangeToPrange().visit(tree)
                ast.fix_missing_locations(tree)
                code = compile(tree, fn.__code__.co_filename, "exec")
                ns: dict = {"numba": numba, "prange": prange}
                exec(code, ns)
                compiled = ns[fn.__name__]
                return njit(cache=True, parallel=True)(compiled)  # type: ignore[return-value]

            return njit(cache=True)(fn)  # type: ignore[return-value]
        except Exception:
            _warn_no_numba()

            @functools.wraps(fn)
            def wrapper(*args: Any, **kwargs: Any) -> Any:
                return fn(*args, **kwargs)

            return wrapper  # type: ignore[return-value]

    return decorator


def wave_kernel_y(pos_y: list, t: float, omega: float = 0.15, speed: float = 1.0, amp: float = 2.0) -> None:
    """Paper Eq. (1) slice on flat Y column (demo, pure Python)."""
    import math

    n = len(pos_y)
    for i in range(n):
        xi = float(i % 32)
        zi = float(i // 32)
        pos_y[i] = amp * math.sin(omega * xi + speed * t) + 0.5 * amp * math.sin(omega * zi + 1.3 * speed * t)


__all__ = ["infernux_jit", "wave_kernel_y"]
