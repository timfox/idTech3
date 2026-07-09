"""idTech3 Python scripting API (Infernux-style batch + optional Numba JIT)."""

from __future__ import annotations

from typing import Callable, List, Sequence, Union

import _idtech3 as _native

Handle = int
FieldName = str


class Engine:
    """Thin wrapper over the native _idtech3 module."""

    @staticmethod
    def print(msg: str) -> None:
        _native.print(str(msg))

    @staticmethod
    def cvar_get(name: str) -> str:
        return _native.cvar_get(name)

    @staticmethod
    def cvar_set(name: str, value: str) -> None:
        _native.cvar_set(name, str(value))

    @staticmethod
    def exec(cmd: str) -> None:
        _native.exec(cmd)

    @staticmethod
    def milliseconds() -> int:
        return int(_native.milliseconds())

    @staticmethod
    def engine_info() -> str:
        return _native.engine_info()

    @staticmethod
    def db_available() -> bool:
        return bool(_native.db_available())

    @staticmethod
    def db_path() -> str:
        return str(_native.db_path())

    @staticmethod
    def db_exec(sql: str) -> bool:
        return bool(_native.db_exec(str(sql)))

    @staticmethod
    def db_query_one(sql: str):
        return _native.db_query_one(str(sql))

    @staticmethod
    def profile_set(key: str, value: str) -> bool:
        return bool(_native.profile_set(str(key), str(value)))

    @staticmethod
    def profile_get(key: str):
        return _native.profile_get(str(key))

    @staticmethod
    def profile_delete(key: str) -> bool:
        return bool(_native.profile_delete(str(key)))

    @staticmethod
    def on(event: str, callback: Callable[..., None]) -> None:
        if event == "frame":
            _native.on_frame(callback)
        else:
            _native.on_event(event, callback)

    @staticmethod
    def batch_read(handles: Sequence[Handle], field: FieldName) -> List[float]:
        return list(_native.batch_read(list(handles), field))

    @staticmethod
    def batch_write(handles: Sequence[Handle], field: FieldName, values: Sequence[float]) -> None:
        _native.batch_write(list(handles), field, list(values))

    @staticmethod
    def batch_info() -> dict:
        return dict(_native.batch_info())

    @staticmethod
    def spawn_demo_grid(side: int, spacing: float = 2.0) -> int:
        return int(_native.spawn_demo_grid(int(side), float(spacing)))


def demo_handles(count: int) -> List[Handle]:
    return list(range(max(0, int(count))))


__all__ = ["Engine", "Handle", "FieldName", "demo_handles"]
