Bitmap Font Storage Format
1. Purpose

The font system is intended primarily for:

Normal text rendering.
Selected Nerd Font symbols/icons.
Multiple font sizes.
Multiple visual styles such as regular, bold, and italic.
Direct use with the e-ink framebuffer.
Storage on external flash using LittleFS.

The format is intentionally designed so that only the required glyphs need to be included. A complete Nerd Font does not need to be stored.

2. Design Goals

The format should:

Minimize external flash usage.
Avoid storing redundant metadata.
Allow sparse Unicode glyph sets.
Provide deterministic glyph lookup.
Avoid binary searches.
Avoid per-glyph offsets.
Avoid per-glyph metadata.
Store bitmaps in the same representation used by the e-ink framebuffer.
Support multiple font sizes and visual styles.
Be simple to read from LittleFS.
Be extensible through a format version.
3. Directory Structure

Fonts are organized first by visual style and nominal size.

fonts/
├── regular16/
│   ├── info
│   ├── undefined.bmp
│   ├── F200/
│   │   ├── index
│   │   ├── F200.bmp
│   │   ├── F220.bmp
│   │   └── F240.bmp
│   └── ...
│
├── bold16/
│   ├── info
│   ├── undefined.bmp
│   └── ...
│
├── italic16/
│   ├── info
│   ├── undefined.bmp
│   └── ...
│
└── bolditalic16/
    ├── info
    ├── undefined.bmp
    └── ...

Type/size directory

The directory name describes the intended font style and nominal size.

Examples:

regular16
regular24
bold16
bold24
italic16
bolditalic16


The actual raster dimensions and baseline are obtained from info; the nominal size in the directory name is primarily an identifier.

4. Font Metadata

Each TypeSize directory contains one info file.

Example:

regular16/info


Metadata is stored once per font variant/size. It is not repeated in Unicode range directories or bitmap files.

4.1 info Format

The serialized format is exactly 12 bytes:

Offset	Size	Field
0	4	Magic
4	1	Version
5	1	Bits per pixel
6	2	Width
8	2	Height
10	2	Baseline

All multi-byte integer fields use little-endian encoding.

Fields
Magic

Four bytes:

EFT\0


This identifies the file as an E-Ink Font Format file.

Version

Current version:

1


Allows the format to evolve later.

Bits per pixel

Current value:

1


The format is currently designed around 1-bit-per-pixel glyphs.

The field is retained so that future versions can potentially support other pixel depths.

Width

Glyph cell width in pixels.

Height

Glyph cell height in pixels.

Baseline

Baseline position for the font in the fixed glyph cell.

This is stored because different font sizes and visual styles may have different rasterization/baseline requirements.

5. Glyph Cells

The font uses fixed-size glyph cells.

For a font with:

width  = 16
height = 16
bpp    = 1


every glyph occupies a 16×16 pixel cell.

No per-glyph width, height, or advance-width metadata is stored.

The number of bytes occupied by a glyph is derived from the metadata:

row_bytes = (width + 7) / 8;
glyph_bytes = row_bytes * height;


For example:

16 × 16 @ 1bpp
= 32 bytes/glyph

24 × 24 @ 1bpp
= 72 bytes/glyph

32 × 32 @ 1bpp
= 128 bytes/glyph

6. Bitmap Representation

Glyph bitmaps use the same pixel/bit representation as the e-ink framebuffer.

The bitmap is stored:

Left to right.
Then top to bottom.
Row-major.
1bpp.
With the same bit ordering used by the e-ink framebuffer.

There is no image-file header.

The .bmp extension is only a naming convention; these are raw bitmap data files, not standard BMP image files.

For a 16×16 glyph:

row 0 → bytes 0–1
row 1 → bytes 2–3
row 2 → bytes 4–5
...
row 15 → bytes 30–31


The exact bit ordering within each byte is identical to the e-ink framebuffer's bit ordering.

This allows the glyph data to be used without converting between incompatible pixel formats.

7. Unicode Organization

Unicode data is divided into two levels.

7.1 256-Codepoint Range

Each range directory represents 256 consecutive Unicode codepoints.

For example:

F200/


represents:

U+F200 – U+F2FF


The range is determined from the codepoint by:

range = codepoint & 0xFF00;


The directory name uses uppercase hexadecimal.

Examples:

F200/
E000/
2000/

8. Range Index

Each 256-codepoint range contains an index file.

Example:

F200/index


The index contains exactly:

32 bytes


with no header or additional metadata.

Since:

256 codepoints / 8 = 32 bytes


the index contains one presence bit per possible codepoint.

8.1 Bit Mapping

For F200/index:

bit 0    → U+F200
bit 1    → U+F201
bit 2    → U+F202
...
bit 255  → U+F2FF


The index answers only:

Does this glyph exist in the font package?

It does not contain bitmap offsets or glyph locations.

Example lookup:

slot = codepoint & 0xFF;

exists =
    index[slot >> 3] &
    (1 << (slot & 7));


If the bit is not set, the glyph is considered unavailable.

9. 32-Codepoint Bitmap Chunks

Each 256-codepoint range is subdivided into eight 32-codepoint bitmap chunks.

For example:

F200 – F21F
F220 – F23F
F240 – F25F
F260 – F27F
F280 – F29F
F2A0 – F2BF
F2C0 – F2DF
F2E0 – F2FF


Corresponding files are named after the first codepoint in the chunk:

F200.bmp
F220.bmp
F240.bmp
F260.bmp
F280.bmp
F2A0.bmp
F2C0.bmp
F2E0.bmp

9.1 Fixed 32-Glyph Layout

A bitmap chunk always has 32 fixed glyph slots.

For:

F240.bmp


the slots are:

slot 0  → U+F240
slot 1  → U+F241
slot 2  → U+F242
...
slot 31 → U+F25F


The bitmap offset is therefore deterministic:

slot = codepoint & 0x1F;

offset = slot * glyph_bytes;


No offset table is required.

9.2 Missing Glyph Slots

A bitmap chunk is created only when at least one of its 32 glyphs exists.

However, once the chunk exists, it always contains all 32 fixed slots.

Slots corresponding to unavailable glyphs are unused/zeroed.

This deliberately trades some space inside a partially populated 32-glyph chunk for:

constant-time lookup;
no per-glyph offsets;
no glyph index;
very simple file layout.

Completely empty 32-codepoint chunks are not stored.

10. Glyph Lookup

Given a Unicode codepoint:

U+F247


the lookup proceeds as follows.

Step 1 — Determine the 256-codepoint range
U+F247 → F200


Look for:

F200/


If the range does not exist, the glyph is unavailable.

Step 2 — Check the presence index

Open:

F200/index


Calculate:

slot = codepoint & 0xFF;


Check the corresponding bit.

If the bit is clear:

glyph unavailable

Step 3 — Determine the 32-codepoint chunk
chunk = codepoint & 0xE0;


For U+F247:

F247 & FFE0 = F240


Therefore:

F200/F240.bmp

Step 4 — Determine the glyph slot
slot = codepoint & 0x1F;


For U+F247:

F247 & 0x1F = 0x07


Therefore the glyph is slot 7.

Step 5 — Determine bitmap offset
offset = slot * glyph_bytes;


For a 16×16 1bpp font:

7 × 32 = 224


The glyph begins at byte 224 in F240.bmp.

11. Missing Glyph Handling

A glyph that is not present in the compiled font does not need a bitmap entry.

The renderer uses a dedicated fallback bitmap:

undefined.bmp


This file exists once per font style/size:

regular16/undefined.bmp
bold16/undefined.bmp
italic16/undefined.bmp


It contains exactly one glyph-sized bitmap using the same bitmap format as normal glyphs.

The font lookup layer can report that a glyph is unavailable, and the renderer can use the undefined glyph.

The storage layer should use a pointer/result indicating absence rather than using the null character ('\0') as a missing-glyph marker. U+0000 is itself a valid Unicode codepoint.

12. Storage Efficiency

The format deliberately avoids redundant information.

A 256-codepoint range requires only:

32-byte presence index


Bitmap storage is allocated only for 32-codepoint chunks containing at least one glyph.

Within an existing chunk, glyph positions are fixed and therefore require no offset table.

For a 16×16 1bpp font:

1 glyph      = 32 bytes
32 glyphs    = 1024 bytes


For a 24×24 1bpp font:

1 glyph      = 72 bytes
32 glyphs    = 2304 bytes


For a 32×32 1bpp font:

1 glyph      = 128 bytes
32 glyphs    = 4096 bytes

13. Example

Suppose the font contains only:

U+F240
U+F241
U+F245


Then:

regular16/
└── F200/
    ├── index
    └── F240.bmp


index marks:

F240 = present
F241 = present
F245 = present


All other codepoints in F200–F2FF are absent.

F240.bmp still contains 32 slots:

slot 0  → F240
slot 1  → F241
slot 2  → F242
slot 3  → F243
slot 4  → F244
slot 5  → F245
...
slot 31 → F25F


The unused slots contain zero/unused bitmap data.

No F200.bmp, F220.bmp, F260.bmp, etc. are created unless at least one glyph in those ranges exists.

14. Deliberate Non-Goals

The following are not part of the current format:

TTF/OTF files on the ESP32.
Runtime font rasterization.
Per-glyph variable dimensions.
Per-glyph advance widths.
Per-glyph offsets.
Binary-search-based glyph lookup.
A global Unicode glyph table.
Storing the entire Nerd Font.
Runtime glyph caching.
PC-side font generation implementation details.

The PC generator is a build-time concern and can be implemented after the storage format is finalized.

15. Current Format Summary
fonts/
└── TypeSize/
    ├── info                  12 bytes
    ├── undefined.bmp         1 glyph
    │
    └── CodepointRange/
        ├── index             32 bytes
        ├── XXXX.bmp          32 glyph slots
        ├── XXXX.bmp
        └── ...


Where:

TypeSize
    ↓
font style + nominal size

info
    ↓
how to interpret every glyph bitmap

CodepointRange
    ↓
256 Unicode codepoints

index
    ↓
256-bit glyph existence map

32-codepoint bitmap
    ↓
fixed slots for 32 consecutive codepoints

glyph slot
    ↓
direct byte offset

bitmap
    ↓
native e-ink framebuffer representation


The resulting lookup is deterministic and requires no search or per-glyph location metadata.
