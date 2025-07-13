import dataclasses
import enum
import typing

@dataclasses.dataclass(slots=True)
class pixel:
    r: int = 0
    g: int = 0
    b: int = 0
    a: int = 0

    def __copy__(self: typing.Self) -> "pixel":
        return pixel(self.r, self.g, self.b, self.a)

    def __hash__(self: typing.Self) -> int:
        return (self.r * 3 + self.g * 5 + self.b * 7 + self.a * 11) % 64

    def __bytes__(self: typing.Self) -> bytes:
        return self.r.to_bytes() + self.g.to_bytes() + self.b.to_bytes() + self.a.to_bytes()


class QOITags(enum.IntEnum):
    INDEX = 0b00000000
    DIFF  = 0b01000000
    LUMA  = 0b10000000
    RUN   = 0b11000000
    RGB   = 0b11111110
    RGBA  = 0b11111111

class QOIError(Exception): ... 

@dataclasses.dataclass(slots=True)
class QOIImage:
    width     : int
    height    : int
    chanels   : int
    colorspace: int
    pixels    : list[pixel] = dataclasses.field(init=False, default_factory=list[pixel], repr=False)

    @staticmethod
    def magic() -> typing.Literal[b"qoif"]:
        return b"qoif"

def _read_pixels(data: bytes, image: QOIImage) -> None:
    prev_px      = pixel(a = 255)
    lookup_array = [pixel() for _ in range(64)]

    cursor = 0
    while cursor < len(data):
        lookup_array[hash(prev_px)] = prev_px

        if data[cursor] == QOITags.RGBA:
            prev_px.r = data[cursor := cursor + 1]
            prev_px.g = data[cursor := cursor + 1]
            prev_px.b = data[cursor := cursor + 1]
            prev_px.a = data[cursor := cursor + 1]

        elif data[cursor] == QOITags.RGB:
            prev_px.r = data[cursor := cursor + 1]
            prev_px.g = data[cursor := cursor + 1]
            prev_px.b = data[cursor := cursor + 1]

        elif (data[cursor] & 0b11000000) == QOITags.INDEX:
            index   = 0b00111111 & data[cursor]
            prev_px = lookup_array[index]

        elif (data[cursor] & 0b11000000) == QOITags.DIFF:
            prev_px.r += ((data[cursor] >> 4) & 0b00000011) - 2
            prev_px.g += ((data[cursor] >> 2) & 0b00000011) - 2
            prev_px.b += (data[cursor] & 0b00000011)        - 2

        elif (data[cursor] & 0b11000000) == QOITags.LUMA:
            dg    = (data[cursor] & 0b00111111)                    - 32
            dr_dg = (data[cursor := cursor + 1] >> 4 & 0b00001111) - 8
            db_dg = (data[cursor] & 0b00001111)                    - 8

            prev_px.r += dr_dg + dg
            prev_px.g += dg
            prev_px.b += db_dg + dg
        elif (data[cursor] & 0b11000000) == QOITags.RUN:
            times = (data[cursor] & 0b00111111)
            image.pixels += [prev_px.__copy__() for _ in range(times)]

        image.pixels.append(prev_px.__copy__())
        cursor += 1


def read_qoi_image(filepath: str) -> QOIImage:
    with open(filepath, "rb") as input_file:
        if len(input_file.peek(14)) < 14:
            raise QOIError("Incorrect header size!")

        if (magic:=input_file.read(4)) != QOIImage.magic():
            raise QOIError(f"Incorrect magic got: {magic}, expected {QOIImage.magic()}!")
        
        width      = int.from_bytes(input_file.read(4))
        height     = int.from_bytes(input_file.read(4))
        chanels    = int.from_bytes(input_file.read(1))
        colorspace = int.from_bytes(input_file.read(1))

        image = QOIImage(width, height, chanels, colorspace)

        data = input_file.read()

        if data[-8:] != b"\x00\x00\x00\x00\x00\x00\x00\x01":
            raise QOIError("Incorrect end magic!\n")
        
        _read_pixels(data[:-8], image)

        if len(image.pixels) != image.width * image.height:
            raise QOIError(f"Image width ({image.width}) and heigth ({image.height}) doesn't match parsed pixel count ({len(image.pixels)})!")

        return image


def qoi_write_image(filepath: str, width: int, height: int, chanels: int, colorspace: int, pixels: list[pixel]):
    with open(filepath, "wb") as output_file:

        output_file.write(QOIImage.magic())
        output_file.write(width.to_bytes(4))
        output_file.write(height.to_bytes(4))
        output_file.write(chanels.to_bytes(1))
        output_file.write(colorspace.to_bytes(1))

        prev_px      = pixel(a = 255)
        lookup_array = [pixel() for _ in range(64)]
    
        cursor = 0
        tag = None
        while cursor < len(pixels):
            pixel_hash = hash(pixels[cursor])
            
            if pixels[cursor] == prev_px:  # RUN
                tag = QOITags.RUN - 1
                while pixels[cursor] == prev_px: 
                    if tag == QOITags.RGB - 1:
                        output_file.write(tag.to_bytes())
                        tag = QOITags.RUN - 1
                    
                    tag += 1
                    cursor += 1
                
                output_file.write(tag.to_bytes())
                cursor -= 1
            elif lookup_array[pixel_hash] == pixels[cursor]: # INDEX
                output_file.write(pixel_hash.to_bytes())
            elif pixels[cursor].a != prev_px.a: # RGBA
                tag = QOITags.RGBA
                output_file.write(tag.to_bytes())
                output_file.write(bytes(pixels[cursor]))
            else:
                dr = pixels[cursor].r - prev_px.r
                dg = pixels[cursor].g - prev_px.g
                db = pixels[cursor].b - prev_px.b
                dr_dg = dr - dg
                db_dg = db - dg
                
                if -2 <= dr <= 1 and -2 <= dg <= 1 and -2 <= db <= 1: # DIFF
                    tag = QOITags.DIFF | (dr + 2) << 4 | (dg + 2) << 2 | (db + 2)
                    output_file.write(tag.to_bytes())
                elif -32 <= dg <= 31 and -8 <= dr_dg <= 7 and -8 <= db_dg <= 7: # LUMA
                    luma = [QOITags.LUMA | (dg + 32), (dr_dg + 8) << 4 | (db_dg + 8)]
                    output_file.write(luma[0].to_bytes())
                    output_file.write(luma[1].to_bytes())
                elif pixels[cursor].a == prev_px.a: # RGB
                    tag = QOITags.RGB
                    output_file.write(tag.to_bytes())
                    output_file.write(pixels[cursor].r.to_bytes())
                    output_file.write(pixels[cursor].g.to_bytes())
                    output_file.write(pixels[cursor].b.to_bytes())
                    
            
            prev_px = pixels[cursor]
            lookup_array[pixel_hash] = prev_px.__copy__()
            cursor += 1

        output_file.write(b"\x00\x00\x00\x00\x00\x00\x00\x01")
        return True