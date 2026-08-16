#!/usr/bin/env python3
"""
LVGL 9 Font Generator Tool
Generates a 4bpp C font file for LVGL using FORMAT0_FULL continuous character map.

Usage example:
    python tools/gen_font.py --font "C:\\Windows\\Fonts\\segoeui.ttf" --size 240 --output main/ui_font_segoe_240.c --name ui_font_segoe_240 --digits-only
    python tools/gen_font.py --font "C:\\Windows\\Fonts\\segoeui.ttf" --size 32 --output main/ui_font_segoe_32.c --name ui_font_segoe_32
    python tools/gen_font.py --font "C:\\Windows\\Fonts\\segoeui.ttf" --size 24 --output main/ui_font_segoe_24.c --name ui_font_segoe_24
"""

import os
import sys
import argparse
from PIL import Image, ImageFont, ImageDraw

def generate_lvgl_font(font_path, font_size, output_path, font_name, digits_only=False):
    font = ImageFont.truetype(font_path, font_size)

    # Get font metrics
    ascent, descent = font.getmetrics()
    line_height = ascent + descent
    base_line = descent

    range_start = 32  # ASCII Space (' ')
    range_end = 126   # ASCII Tilde ('~')
    range_length = range_end - range_start + 1

    if digits_only:
        valid_chars = set([' ', '-', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'])
    else:
        valid_chars = set(chr(c) for c in range(range_start, range_end + 1))

    glyph_dscs = []
    bitmaps = bytearray()
    bitmap_offset = 0

    # Index 0 is invalid/dummy glyph
    glyph_dscs.append({
        'bitmap_index': 0, 'adv_w': 0, 'box_w': 0, 'box_h': 0, 'ofs_x': 0, 'ofs_y': 0, 'comment': 'dummy'
    })

    for code in range(range_start, range_end + 1):
        c = chr(code)
        if c not in valid_chars:
            glyph_dscs.append({
                'bitmap_index': 0, 'adv_w': 0, 'box_w': 0, 'box_h': 0, 'ofs_x': 0, 'ofs_y': 0, 'comment': f"'{c}' (unused)"
            })
            continue

        if c == ' ':
            adv_w = int(font.getlength(' '))
            glyph_dscs.append({
                'bitmap_index': bitmap_offset,
                'adv_w': adv_w * 16,
                'box_w': 0,
                'box_h': 0,
                'ofs_x': 0,
                'ofs_y': 0,
                'comment': "' '"
            })
            continue

        # Get bounding box
        bbox = font.getbbox(c)  # (left, top, right, bottom)
        left, top, right, bottom = bbox
        w = right - left
        h = bottom - top
        adv_w = int(font.getlength(c))

        if w <= 0 or h <= 0:
            glyph_dscs.append({
                'bitmap_index': bitmap_offset,
                'adv_w': adv_w * 16,
                'box_w': 0,
                'box_h': 0,
                'ofs_x': 0,
                'ofs_y': 0,
                'comment': f"'{c}'"
            })
            continue

        # Render glyph image
        img = Image.new('L', (w, h), 0)
        draw = ImageDraw.Draw(img)
        draw.text((-left, -top), c, fill=255, font=font)

        ofs_x = left
        ofs_y = ascent - bottom  # LVGL offset Y from baseline

        glyph_bitmap = bytearray()
        pixels = list(img.getdata())

        for y in range(h):
            for x in range(0, w, 2):
                p1 = pixels[y * w + x] >> 4
                if x + 1 < w:
                    p2 = pixels[y * w + (x + 1)] >> 4
                else:
                    p2 = 0
                glyph_bitmap.append((p1 << 4) | p2)

        glyph_dscs.append({
            'bitmap_index': bitmap_offset,
            'adv_w': adv_w * 16,
            'box_w': w,
            'box_h': h,
            'ofs_x': ofs_x,
            'ofs_y': ofs_y,
            'comment': f"'{c}'"
        })

        bitmaps.extend(glyph_bitmap)
        bitmap_offset += len(glyph_bitmap)

    # Ensure output parent dir exists
    out_dir = os.path.dirname(output_path)
    if out_dir and not os.path.exists(out_dir):
        os.makedirs(out_dir, exist_ok=True)

    with open(output_path, "w") as f:
        f.write('#include "lvgl.h"\n\n')
        f.write('/*------------------\n * BITMAPS\n *-----------------*/\n')
        f.write('static const uint8_t glyph_bitmap[] = {\n')

        # Format bitmap in rows of 16 bytes
        for i in range(0, len(bitmaps), 16):
            chunk = bitmaps[i:i+16]
            hex_str = ", ".join([f"0x{b:02x}" for b in chunk])
            f.write(f'    {hex_str},\n')

        f.write('};\n\n')

        f.write('/*------------------\n * GLYPH DESCRIPTION\n *-----------------*/\n')
        f.write('static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {\n')

        for g in glyph_dscs:
            f.write(f"    {{.bitmap_index = {g['bitmap_index']}, .adv_w = {g['adv_w']}, .box_w = {g['box_w']}, .box_h = {g['box_h']}, .ofs_x = {g['ofs_x']}, .ofs_y = {g['ofs_y']}}}, /* {g['comment']} */\n")

        f.write('};\n\n')

        f.write('/*------------------\n * UNICODE MAP\n *-----------------*/\n')
        f.write('static const lv_font_fmt_txt_cmap_t cmaps[] = {\n')
        f.write('    {\n')
        f.write(f'        .range_start = {range_start},\n')
        f.write(f'        .range_length = {range_length},\n')
        f.write('        .glyph_id_start = 1,\n')
        f.write('        .unicode_list = NULL,\n')
        f.write('        .glyph_id_ofs_list = NULL,\n')
        f.write('        .list_length = 0,\n')
        f.write('        .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY\n')
        f.write('    }\n')
        f.write('};\n\n')

        f.write('/*------------------\n * FONT DESCRIPTION\n *-----------------*/\n')
        f.write('static const lv_font_fmt_txt_dsc_t font_dsc = {\n')
        f.write('    .glyph_bitmap = glyph_bitmap,\n')
        f.write('    .glyph_dsc = glyph_dsc,\n')
        f.write('    .cmaps = cmaps,\n')
        f.write('    .kern_dsc = NULL,\n')
        f.write('    .kern_scale = 0,\n')
        f.write('    .cmap_num = 1,\n')
        f.write('    .bpp = 4,\n')
        f.write('    .kern_classes = 0,\n')
        f.write('    .bitmap_format = 0,\n')
        f.write('    .stride = 1,\n')
        f.write('};\n\n')

        f.write('/*------------------\n *  PUBLIC FONT\n *-----------------*/\n')
        f.write(f'const lv_font_t {font_name} = {{\n')
        f.write('    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,\n')
        f.write('    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,\n')
        f.write(f'    .line_height = {line_height},\n')
        f.write(f'    .base_line = {base_line},\n')
        f.write('    .subpx = LV_FONT_SUBPX_NONE,\n')
        f.write('    .underline_position = -4,\n')
        f.write('    .underline_thickness = 2,\n')
        f.write('    .dsc = &font_dsc\n')
        f.write('};\n')

    print(f"Successfully generated {output_path} ({font_name})! Total bitmap size: {len(bitmaps)} bytes.")

def main():
    parser = argparse.ArgumentParser(description="LVGL Font Generator")
    parser.add_argument('--font', default=r"C:\Windows\Fonts\segoeui.ttf", help="Path to TTF font file")
    parser.add_argument('--size', type=int, default=120, help="Font size in px")
    parser.add_argument('--output', default=r"main/ui_font_segoe_120.c", help="Output C file path")
    parser.add_argument('--name', default="ui_font_segoe_120", help="Font variable name")
    parser.add_argument('--digits-only', action='store_true', help="Only include digits and basic punctuation")

    args = parser.parse_args()
    generate_lvgl_font(args.font, args.size, args.output, args.name, args.digits_only)

if __name__ == '__main__':
    main()
