import typing

class pixel:
    r: int
    g: int
    b: int
    a: int

    def __init__(self: typing.Self, r: int = 0, g: int = 0, b: int = 0, a: int = 0) -> None: ...
    def __hash__(self: typing.Self) -> int: ...
    def __repr__(self: typing.Self) -> str: ...