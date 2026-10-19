#!/usr/bin/env python3
"""Generate a deliberately tiny TrueType font that covers printable ASCII.

The design is a hand-drawn 5x7 pixel font on a 5x9 cell (two rows below the
baseline for descenders), set solid on a 6 px monospaced advance.  Most glyphs
are traced from their bitmap into one union outline; diagonal-heavy letters
are instead drawn as mitred 1 px strokes, which is both cleaner and smaller.
With a 144 unit em every coordinate delta fits in the one-byte TrueType
short-vector form, and the font keeps only the tables needed for rendering.

Output: assets/PacmanPixel-ASCII.ttf
"""

from __future__ import annotations

import math
import os
from fontTools.fontBuilder import FontBuilder
from fontTools.misc.timeTools import timestampFromString
from fontTools.pens.ttGlyphPen import TTGlyphPen

# ---------------------------------------------------------------------------
# Design grid
# ---------------------------------------------------------------------------

COLS = 5
ROWS = 9
CELL = 16          # font units per pixel
UPM = ROWS * CELL  # 144
ASCENT = 7 * CELL  # 112: seven rows above the baseline
DESCENT = 2 * CELL  # 32: two rows below the baseline

# .notdef: a hollow box, so missing code points are visible instead of blank.
GLYPHS: dict[str, list[str]] = {
    "\x00": ["#####", "#...#", "#...#", "#...#", "#...#", "#...#", "#####", ".....", "....."],
    " ": ["....."] * 9,

    "!": ["..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#..", ".....", "....."],
    '"': [".#.#.", ".#.#.", ".....", ".....", ".....", ".....", ".....", ".....", "....."],
    "#": [".#.#.", ".#.#.", "#####", ".#.#.", "#####", ".#.#.", ".#.#.", ".....", "....."],
    "$": ["..#..", ".####", "#.#..", ".###.", "..#.#", "####.", "..#..", ".....", "....."],
    "%": ["##..#", "##..#", "...#.", "..#..", ".#...", "#..##", "#..##", ".....", "....."],
    "&": [".##..", "#..#.", "#.#..", ".#...", "#.#.#", "#..#.", ".##.#", ".....", "....."],
    "'": ["..#..", "..#..", ".....", ".....", ".....", ".....", ".....", ".....", "....."],
    "(": ["...#.", "..#..", ".#...", ".#...", ".#...", "..#..", "...#.", ".....", "....."],
    ")": [".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#...", ".....", "....."],
    "*": [".....", "#.#.#", ".###.", "#####", ".###.", "#.#.#", ".....", ".....", "....."],
    "+": [".....", ".....", "..#..", "#####", "..#..", ".....", ".....", ".....", "....."],
    ",": [".....", ".....", ".....", ".....", ".....", "..##.", "..##.", ".##..", "....."],
    "-": [".....", ".....", ".....", ".....", ".###.", ".....", ".....", ".....", "....."],
    ".": [".....", ".....", ".....", ".....", ".....", ".##..", ".##..", ".....", "....."],
    "/": ["....#", "....#", "...#.", "..#..", ".#...", "#....", "#....", ".....", "....."],
    "0": [".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###.", ".....", "....."],
    "1": ["..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###.", ".....", "....."],
    "2": [".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####", ".....", "....."],
    "3": ["####.", "....#", "....#", ".###.", "....#", "....#", "####.", ".....", "....."],
    "4": ["...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#.", ".....", "....."],
    "5": ["#####", "#....", "####.", "....#", "....#", "#...#", ".###.", ".....", "....."],
    "6": ["..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###.", ".....", "....."],
    "7": ["#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#...", ".....", "....."],
    "8": [".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###.", ".....", "....."],
    "9": [".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##..", ".....", "....."],
    ":": [".....", ".....", ".##..", ".##..", ".....", ".##..", ".##..", ".....", "....."],
    ";": [".....", ".....", ".##..", ".##..", ".....", ".##..", ".##..", ".##..", "....."],
    "<": [".....", "...#.", "..#..", ".#...", "#....", ".#...", "..#..", "...#.", "....."],
    "=": [".....", ".....", ".....", "#####", ".....", "#####", ".....", ".....", "....."],
    ">": [".....", ".#...", "..#..", "...#.", "....#", "...#.", "..#..", ".#...", "....."],
    "?": [".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#..", ".....", "....."],
    "@": [".###.", "#...#", "#.###", "#.#.#", "#.###", "#....", ".###.", ".....", "....."],
    "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#", ".....", "....."],
    "B": ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####.", ".....", "....."],
    "C": [".###.", "#...#", "#....", "#....", "#....", "#...#", ".###.", ".....", "....."],
    "D": ["####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####.", ".....", "....."],
    "E": ["#####", "#....", "#....", "####.", "#....", "#....", "#####", ".....", "....."],
    "F": ["#####", "#....", "#....", "####.", "#....", "#....", "#....", ".....", "....."],
    "G": [".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###.", ".....", "....."],
    "H": ["#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#", ".....", "....."],
    "I": [".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###.", ".....", "....."],
    "J": ["..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##..", ".....", "....."],
    "K": ["#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#", ".....", "....."],
    "L": ["#....", "#....", "#....", "#....", "#....", "#....", "#####", ".....", "....."],
    "M": ["#...#", "##.##", "#.#.#", "#...#", "#...#", "#...#", "#...#", ".....", "....."],
    "N": ["#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#", ".....", "....."],
    "O": [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###.", ".....", "....."],
    "P": ["####.", "#...#", "#...#", "####.", "#....", "#....", "#....", ".....", "....."],
    "Q": [".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#", ".....", "....."],
    "R": ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#", ".....", "....."],
    "S": [".###.", "#...#", "#....", ".###.", "....#", "#...#", ".###.", ".....", "....."],
    "T": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#..", ".....", "....."],
    "U": ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###.", ".....", "....."],
    "V": ["#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#..", ".....", "....."],
    "W": ["#...#", "#...#", "#...#", "#...#", "#.#.#", "##.##", "#...#", ".....", "....."],
    "X": ["#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#", ".....", "....."],
    "Y": ["#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#..", ".....", "....."],
    "Z": ["#####", "....#", "...#.", "..#..", ".#...", "#....", "#####", ".....", "....."],
    "[": ["..###", "..#..", "..#..", "..#..", "..#..", "..#..", "..###", ".....", "....."],
    "\\": ["#....", "#....", ".#...", "..#..", "...#.", "....#", "....#", ".....", "....."],
    "]": ["###..", "..#..", "..#..", "..#..", "..#..", "..#..", "###..", ".....", "....."],
    "^": ["..#..", ".#.#.", "#...#", ".....", ".....", ".....", ".....", ".....", "....."],
    "_": [".....", ".....", ".....", ".....", ".....", ".....", ".....", "#####", "....."],
    "`": [".#...", "..#..", ".....", ".....", ".....", ".....", ".....", ".....", "....."],
    "a": [".....", ".....", ".###.", "....#", ".####", "#...#", ".####", ".....", "....."],
    "b": ["#....", "#....", "####.", "#...#", "#...#", "#...#", "####.", ".....", "....."],
    "c": [".....", ".....", ".###.", "#....", "#....", "#....", ".###.", ".....", "....."],
    "d": ["....#", "....#", ".####", "#...#", "#...#", "#...#", ".####", ".....", "....."],
    "e": [".....", ".....", ".###.", "#...#", "#####", "#....", ".###.", ".....", "....."],
    "f": ["..##.", ".#...", "####.", ".#...", ".#...", ".#...", ".#...", ".....", "....."],
    "g": [".....", ".....", ".###.", "#...#", "#...#", ".####", "....#", "#...#", ".###."],
    "h": ["#....", "#....", "####.", "#...#", "#...#", "#...#", "#...#", ".....", "....."],
    "i": ["..#..", ".....", ".##..", "..#..", "..#..", "..#..", ".###.", ".....", "....."],
    "j": ["...#.", ".....", "..##.", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."],
    "k": ["#....", "#....", "#..#.", "#.#..", "##...", "#.#..", "#..#.", ".....", "....."],
    "l": [".##..", "..#..", "..#..", "..#..", "..#..", "..#..", ".###.", ".....", "....."],
    "m": [".....", ".....", "##.#.", "#.#.#", "#.#.#", "#...#", "#...#", ".....", "....."],
    "n": [".....", ".....", "####.", "#...#", "#...#", "#...#", "#...#", ".....", "....."],
    "o": [".....", ".....", ".###.", "#...#", "#...#", "#...#", ".###.", ".....", "....."],
    "p": [".....", ".....", "####.", "#...#", "#...#", "####.", "#....", "#....", "#...."],
    "q": [".....", ".....", ".####", "#...#", "#...#", ".####", "....#", "....#", "....#"],
    "r": [".....", ".....", "#.##.", "##...", "#....", "#....", "#....", ".....", "....."],
    "s": [".....", ".....", ".####", "#....", ".###.", "....#", "####.", ".....", "....."],
    "t": [".#...", ".#...", "####.", ".#...", ".#...", ".#...", "..##.", ".....", "....."],
    "u": [".....", ".....", "#...#", "#...#", "#...#", "#...#", ".####", ".....", "....."],
    "v": [".....", ".....", "#...#", "#...#", "#...#", ".#.#.", "..#..", ".....", "....."],
    "w": [".....", ".....", "#...#", "#...#", "#.#.#", "#.#.#", ".#.#.", ".....", "....."],
    "x": [".....", ".....", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", ".....", "....."],
    "y": [".....", ".....", "#...#", "#...#", "#...#", ".####", "....#", "#...#", ".###."],
    "z": [".....", ".....", "#####", "...#.", "..#..", ".#...", "#####", ".....", "....."],
    "{": ["..###", "..#..", "..#..", ".#...", "..#..", "..#..", "..###", ".....", "....."],
    "|": ["..#..", "..#..", "..#..", "..#..", "..#..", "..#..", "..#..", ".....", "....."],
    "}": ["###..", "..#..", "..#..", "...#.", "..#..", "..#..", "###..", ".....", "....."],
    "~": [".....", ".....", ".##.#", "#..##", ".....", ".....", ".....", ".....", "....."],
}


# A handful of glyphs are cheaper (and cleaner) as real strokes than as a
# staircase of pixels.  Each entry is a list of polylines in design-grid
# units: x in [0, 5], y in [2, 9] with the baseline at y = 2.  A polyline is
# expanded to one closed outline with mitred joins, so a whole W or N costs
# a single contour instead of one contour per stroke.  Stems are 1 px wide,
# matching the pixel-traced glyphs.
#
# Overlapping contours are fine: every contour is clockwise and TrueType
# fills with the non-zero winding rule.


def _polyline_outline(points, width=1.0):
    """Mitered outline of an open polyline, returned as one clockwise contour."""
    half = width / 2
    n = len(points)
    dirs = []
    normals = []
    for i in range(n - 1):
        dx = points[i + 1][0] - points[i][0]
        dy = points[i + 1][1] - points[i][1]
        length = math.hypot(dx, dy)
        dirs.append((dx / length, dy / length))
    for dx, dy in dirs:
        normals.append((-dy, dx))

    def offset(index, side):
        # side=+1 is the left side of the direction of travel.
        x, y = points[index]
        if index == 0:
            nx, ny = normals[0]
            return (x + side * nx * half, y + side * ny * half)
        if index == n - 1:
            nx, ny = normals[-1]
            return (x + side * nx * half, y + side * ny * half)
        nx0, ny0 = normals[index - 1]
        nx1, ny1 = normals[index]
        denom = 1 + nx0 * nx1 + ny0 * ny1
        if denom < 0.2:  # very sharp corner: fall back to a bevel-ish join
            denom = 0.2
        mx = (nx0 + nx1) / denom
        my = (ny0 + ny1) / denom
        return (x + side * mx * half, y + side * my * half)

    left = [offset(i, 1) for i in range(n)]
    right = [offset(i, -1) for i in range(n)]
    outline = left + list(reversed(right))
    area = 0.0
    for i in range(len(outline)):
        x0, y0 = outline[i]
        x1, y1 = outline[(i + 1) % len(outline)]
        area += x0 * y1 - x1 * y0
    if area > 0:  # TrueType outer contours are clockwise (negative area)
        outline.reverse()
    return outline


VECTOR_GLYPHS = {
    "x": [[(0.5, 2.5), (4.5, 7.5)], [(0.5, 7.5), (4.5, 2.5)]],
    "X": [[(0.5, 2.5), (4.5, 8.5)], [(0.5, 8.5), (4.5, 2.5)]],
    "K": [
        [(0.5, 2.5), (0.5, 8.5)],
        [(4.5, 8.5), (0.5, 5.5), (4.5, 2.5)],
    ],
    "k": [
        [(0.5, 2.5), (0.5, 8.5)],
        [(4.5, 7.5), (0.5, 5.0), (4.5, 2.5)],
    ],
    "A": [
        [(0.5, 2.5), (2.5, 8.5), (4.5, 2.5)],
        [(1.5, 5.5), (3.5, 5.5)],
    ],
    "V": [[(0.5, 8.5), (2.5, 2.5), (4.5, 8.5)]],
    "W": [[(0.5, 8.5), (1.5, 2.5), (2.5, 6.5), (3.5, 2.5), (4.5, 8.5)]],
    "M": [[(0.5, 2.5), (0.5, 8.5), (2.5, 5.5), (4.5, 8.5), (4.5, 2.5)]],
    "N": [[(0.5, 2.5), (0.5, 8.5), (4.5, 2.5), (4.5, 8.5)]],
    "Z": [[(0.5, 8.5), (4.5, 8.5), (0.5, 2.5), (4.5, 2.5)]],
    "7": [[(0.5, 8.5), (4.5, 8.5), (1.5, 2.5)]],
    "4": [
        [(3.5, 8.5), (0.5, 4.5), (4.5, 4.5)],
        [(3.5, 8.5), (3.5, 2.5)],
    ],
    "2": [[(0.5, 8.5), (4.5, 8.5), (4.5, 7.0), (0.5, 2.5), (4.5, 2.5)]],
    "*": [
        [(2.5, 3.5), (2.5, 7.5)],
        [(0.8, 4.5), (4.2, 6.5)],
        [(0.8, 6.5), (4.2, 4.5)],
    ],
    "<": [[(4.5, 6.5), (0.5, 4.5), (4.5, 2.5)]],
    ">": [[(0.5, 6.5), (4.5, 4.5), (0.5, 2.5)]],
    "#": [
        [(1.5, 2.5), (1.5, 8.5)],
        [(3.5, 2.5), (3.5, 8.5)],
        [(0.5, 4.5), (4.5, 4.5)],
        [(0.5, 6.5), (4.5, 6.5)],
    ],
}


# ---------------------------------------------------------------------------
# Pixel bitmap -> outline contours
# ---------------------------------------------------------------------------

# A boundary edge is (start, end) in font units, directed so that the filled
# side is on the right.  That makes outer contours clockwise (TrueType's
# convention) and holes automatically run the other way.


def _turn(direction: tuple[int, int], new_direction: tuple[int, int]) -> float:
    """Signed turn angle in (-180, 180]; right turns are negative."""
    cross = direction[0] * new_direction[1] - direction[1] * new_direction[0]
    dot = direction[0] * new_direction[0] + direction[1] * new_direction[1]
    return math.degrees(math.atan2(cross, dot))


def _simplify(points: list[tuple[int, int]]) -> list[tuple[int, int]]:
    """Drop points that lie on the straight line between their neighbours."""
    if len(points) < 3:
        return points
    out: list[tuple[int, int]] = []
    n = len(points)
    for i in range(n):
        a = points[(i - 1) % n]
        b = points[i]
        c = points[(i + 1) % n]
        if (b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0]) != 0:
            out.append(b)
    return out


def trace_bitmap(bitmap: list[str]) -> list[list[tuple[int, int]]]:
    """Return the simplified union outline of a 5x9 pixel bitmap."""
    filled = set()
    for row, line in enumerate(bitmap):
        assert len(line) == COLS, (row, line)
        for col, ch in enumerate(line):
            if ch == "#":
                filled.add((col, row))

    edges = []
    for col, row in filled:
        x0, x1 = col * CELL, (col + 1) * CELL
        # Row 0 is the top of the 9-row em box.
        y0 = (ROWS - row - 1) * CELL
        y1 = (ROWS - row) * CELL
        # Bottom edge (filled above).
        if (col, row + 1) not in filled:
            edges.append(((x0, y0), (x1, y0)))
        # Top edge (filled below).
        if (col, row - 1) not in filled:
            edges.append(((x1, y1), (x0, y1)))
        # Left edge (filled right).
        if (col - 1, row) not in filled:
            edges.append(((x0, y1), (x0, y0)))
        # Right edge (filled left).
        if (col + 1, row) not in filled:
            edges.append(((x1, y0), (x1, y1)))

    outgoing: dict[tuple[int, int], list[int]] = {}
    for index, (start, _end) in enumerate(edges):
        outgoing.setdefault(start, []).append(index)

    used = [False] * len(edges)
    contours: list[list[tuple[int, int]]] = []

    for start_index in range(len(edges)):
        if used[start_index]:
            continue
        loop: list[tuple[int, int]] = []
        index = start_index
        while True:
            used[index] = True
            start, end = edges[index]
            loop.append(start)
            candidates = [i for i in outgoing.get(end, []) if not used[i]]
            if not candidates:
                break
            direction = (end[0] - start[0], end[1] - start[1])
            # "Rightmost" continuation keeps the filled side on the right and
            # splits corner-touching pixels into separate loops.
            index = min(
                candidates,
                key=lambda i: _turn(
                    direction,
                    (
                        edges[i][1][0] - edges[i][0][0],
                        edges[i][1][1] - edges[i][0][1],
                    ),
                ),
            )
        simplified = _simplify(loop)
        if len(simplified) >= 3:
            contours.append(simplified)
    return contours


# More straight-sided letters are cheaper as polylines than as pixel runs.
VECTOR_GLYPHS.update(
    {
        "C": [[(4.5, 8.5), (1.5, 8.5), (0.5, 7.5), (0.5, 3.5), (1.5, 2.5), (4.5, 2.5)]],
        "J": [[(4.5, 8.5), (4.5, 2.5), (0.5, 2.5), (0.5, 4.5)]],
        "U": [[(0.5, 8.5), (0.5, 2.5), (4.5, 2.5), (4.5, 8.5)]],
        "Y": [
            [(0.5, 8.5), (2.5, 5.5), (4.5, 8.5)],
            [(2.5, 5.5), (2.5, 2.5)],
        ],
        "/": [[(0.5, 2.5), (4.5, 8.5)]],
        "\\": [[(0.5, 8.5), (4.5, 2.5)]],
        "^": [[(0.5, 6.5), (2.5, 8.5), (4.5, 6.5)]],
        "(": [[(4.5, 8.5), (2.5, 7.5), (1.5, 5.5), (2.5, 3.5), (4.5, 2.5)]],
        ")": [[(0.5, 8.5), (2.5, 7.5), (3.5, 5.5), (2.5, 3.5), (0.5, 2.5)]],
        "v": [[(0.5, 7.5), (2.5, 2.5), (4.5, 7.5)]],
        "w": [[(0.5, 7.5), (1.5, 2.5), (2.5, 5.5), (3.5, 2.5), (4.5, 7.5)]],
        "y": [
            [(0.5, 7.5), (2.5, 3.0), (4.5, 7.5)],
            [(2.5, 3.0), (0.5, 1.0)],
        ],
        "z": [[(0.8, 7.5), (4.2, 7.5), (0.8, 2.5), (4.2, 2.5)]],
        "$": [
            [(4.5, 8.5), (0.5, 8.5), (0.5, 5.5), (4.5, 5.5), (4.5, 2.5), (0.5, 2.5)],
            [(2.5, 9.0), (2.5, 2.0)],
        ],
    }
)


# ---------------------------------------------------------------------------
# Font assembly
# ---------------------------------------------------------------------------

FAMILY = "Pacman Pixel"
STYLE = "Regular"
MONOSPACE = True  # 6 px advance for every glyph (smaller hmtx)
VERSION = "Version 1.000"
COPYRIGHT = "CC0 1.0 public domain"
FIXED_TIMESTAMP = timestampFromString("Thu Jan 1 00:00:00 2026")


def contours_for(char: str) -> list[list[tuple[int, int]]]:
    if char in VECTOR_GLYPHS:
        return [
            [(round(x * CELL), round(y * CELL)) for x, y in _polyline_outline(line)]
            for line in VECTOR_GLYPHS[char]
        ]
    return trace_bitmap(GLYPHS[char])


def glyph_from_contours(contours: list[list[tuple[int, int]]]):
    pen = TTGlyphPen(None)
    for contour in contours:
        pen.moveTo(contour[0])
        for point in contour[1:]:
            pen.lineTo(point)
        pen.closePath()
    return pen.glyph()


def build(path: str) -> None:
    chars = [chr(code) for code in range(0x20, 0x7F)]
    available = set(GLYPHS) - {"\x00"}
    assert available == set(chars), (
        sorted(set(chars) - available),
        sorted(available - set(chars)),
    )

    glyph_order = [".notdef"] + [f"uni{ord(ch):04X}" for ch in chars]
    glyphs = {}
    metrics = {}
    cmap = {}

    def add_glyph(name: str, contours, advance: int | None = None) -> None:
        if contours:
            x_min = min(x for contour in contours for x, _y in contour)
            x_max = max(x for contour in contours for x, _y in contour)
            if MONOSPACE:
                advance = 6 * CELL
                # Centre the outline in the fixed cell.  The 5 px design grid
                # leaves 1 px of logical side bearing.
                shift = round((advance - (x_max - x_min)) / 2 - x_min)
                contours = [
                    [(x + shift, y) for x, y in contour] for contour in contours
                ]
                x_min += shift
            elif advance is None:
                # One blank pixel between glyphs.
                advance = x_max + CELL
            metrics[name] = (advance, x_min)
        else:
            if advance is None:
                advance = 6 * CELL if MONOSPACE else 3 * CELL
            metrics[name] = (advance, 0)
        glyphs[name] = glyph_from_contours(contours)

    add_glyph(".notdef", contours_for("\x00"), advance=6 * CELL)
    for ch in chars:
        name = f"uni{ord(ch):04X}"
        add_glyph(name, contours_for(ch))
        cmap[ord(ch)] = name

    fb = FontBuilder(UPM, isTTF=True)
    fb.setupGlyphOrder(glyph_order)
    fb.setupCharacterMap(cmap)
    # FontBuilder emits both (3,1) and (0,3); they are identical here.  Keep
    # only the Windows Unicode subtable to save a few dozen bytes.
    fb.font["cmap"].tables = [
        table for table in fb.font["cmap"].tables if (table.platformID, table.platEncID) == (3, 1)
    ]
    fb.setupGlyf(glyphs)
    fb.setupHorizontalMetrics(metrics)
    fb.setupHorizontalHeader(ascent=ASCENT, descent=-DESCENT, lineGap=0)
    fb.setupNameTable(
        {
            "copyright": COPYRIGHT,
            "familyName": FAMILY,
            "styleName": STYLE,
            "fullName": f"{FAMILY} {STYLE}",
            "psName": "PacmanPixel-Regular",
        },
        windows=True,
        mac=False,
    )
    fb.setupOS2(
        sTypoAscender=ASCENT,
        sTypoDescender=-DESCENT,
        sTypoLineGap=0,
        usWinAscent=ASCENT,
        usWinDescent=DESCENT,
        sxHeight=5 * CELL,
        sCapHeight=7 * CELL,
        usDefaultChar=0x20,
        usBreakChar=0x20,
        usFirstCharIndex=0x20,
        usLastCharIndex=0x7E,
        fsType=0,
        ulCodePageRange1=1,
        achVendID="PCPA",
    )
    fb.setupPost(keepGlyphNames=False)
    fb.setupMaxp()
    # Stable bytes: do not rewrite head.created/modified on every run.
    fb.font["head"].created = FIXED_TIMESTAMP
    fb.font["head"].modified = FIXED_TIMESTAMP
    fb.save(path)


if __name__ == "__main__":
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = os.path.join(root, "assets", "PacmanPixel-ASCII.ttf")
    build(out)
    print(f"{out}: {os.path.getsize(out)} bytes")
