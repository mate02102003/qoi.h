"""

Python bindings for header-only QOI format implementation

"""


import typing

class pixel:
    r: int
    g: int
    b: int
    a: int

    def __init__(self: typing.Self, r: int = 0, g: int = 0, b: int = 0, a: int = 0) -> None: ...
    def __hash__(self: typing.Self) -> int: ...
    def __repr__(self: typing.Self) -> str: ...

class QOIImage:
    width:      int
    """Width of the image"""
    height:     int
    """Height of the image"""
    chanels:    int
    """3 = RGB, 4 = RGBA"""
    colorspace: int
    """0 = sRGB with linear alpha, 1 = all channels linear"""

    def __init__(
            self: typing.Self, 
            width: int, 
            height: int, 
            colorspace: typing.Literal[0, 1], 
            chanels: typing.Literal[3, 4],
            pixels: typing.Sequence[pixel]
        ) -> None: ...
    
    def __repr__(self: typing.Self) -> str: ...

    @staticmethod
    def load(filepath: str) -> "QOIImage":
        """Load QOI from file!"""

    def write(self: typing.Self, filepath: str) -> "QOIImage":
        """Write QOI to file!"""