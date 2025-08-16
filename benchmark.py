from timeit import timeit
from itertools import repeat

import qoi
import build.qoipy as qoipy

TIMES: int = 10

type Min = float
type Avg = float
type Max = float

def benchmark(stmt: str, count: int, globals: dict[str]) -> tuple[Min, Avg, Max]:
    times: list[float] = []

    for _ in range(count):
        times.append(timeit(stmt, number=1, globals=globals))

    return min(times), sum(times) / count, max(times)


_  = benchmark("list(range(100_000))", TIMES, globals()) # warmup
c_time_min , c_time_avg , c_time_max  = benchmark("qoipy.QOIImage.load('tests/dice.qoi')", TIMES, globals())
py_time_min, py_time_avg, py_time_max = benchmark("qoi.read_qoi_image('tests/dice.qoi')" , TIMES, globals())

output = \
"              min           avg           max\n" + \
"C wrapper:    %f s    %f s    %f s\n" % (c_time_min, c_time_avg, c_time_max) + \
"Pure python:  %f s    %f s    %f s\n" % (py_time_min, py_time_avg, py_time_max)

print(output)

print(f"Speed up form Py to C: best = {py_time_max / c_time_min * 100:.2f}%, avg = {py_time_avg / c_time_avg * 100:.2f}%, worst = {py_time_min / c_time_max * 100:.2f}%,")