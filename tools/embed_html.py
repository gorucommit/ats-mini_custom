#!/usr/bin/env python3
"""
embed_html.py - Convert HTML file to C header for ESP32 firmware

This script reads an HTML file, minifies it, compresses it with gzip,
and outputs a C header file with the compressed data as a byte array.

Usage:
    python embed_html.py <input.html> <output.h>
    
Example:
    python tools/embed_html.py docs/source/control.html ats-mini/control_html.h
    python tools/embed_html.py docs/source/control.html ats-mini/control_html.h --embed-image steampigg.png
"""

import sys
import gzip
import re
import base64
from pathlib import Path


def minify_html(html: str) -> str:
    """Basic HTML minification - remove unnecessary whitespace."""
    # Remove HTML comments (but not conditional comments)
    html = re.sub(r'<!--(?!\[if).*?-->', '', html, flags=re.DOTALL)
    
    # Remove whitespace between tags
    html = re.sub(r'>\s+<', '><', html)
    
    # Collapse multiple spaces to single space
    html = re.sub(r'\s+', ' ', html)
    
    # Remove leading/trailing whitespace
    html = html.strip()
    
    return html


def minify_css(css: str) -> str:
    """Basic CSS minification."""
    # Remove comments
    css = re.sub(r'/\*.*?\*/', '', css, flags=re.DOTALL)
    
    # Remove unnecessary whitespace
    css = re.sub(r'\s+', ' ', css)
    css = re.sub(r'\s*([{};:,>+~])\s*', r'\1', css)
    css = re.sub(r';\s*}', '}', css)
    
    return css.strip()


def minify_js(js: str) -> str:
    """Basic JS minification (conservative - only removes comments and extra whitespace)."""
    # Remove single-line comments (but be careful with URLs)
    lines = []
    for line in js.split('\n'):
        # Find comment start, but not within strings or URLs
        comment_idx = -1
        in_string = None
        i = 0
        while i < len(line):
            c = line[i]
            if in_string:
                if c == in_string and (i == 0 or line[i-1] != '\\'):
                    in_string = None
            elif c in '"\'`':
                in_string = c
            elif c == '/' and i + 1 < len(line) and line[i + 1] == '/' and not in_string:
                comment_idx = i
                break
            i += 1
        
        if comment_idx >= 0:
            line = line[:comment_idx]
        lines.append(line.rstrip())
    
    js = '\n'.join(lines)
    
    # Remove multi-line comments
    js = re.sub(r'/\*.*?\*/', '', js, flags=re.DOTALL)
    
    # Collapse multiple newlines
    js = re.sub(r'\n\s*\n', '\n', js)
    
    # Remove leading whitespace from lines (but preserve some structure)
    js = re.sub(r'\n\s+', '\n', js)
    
    return js.strip()


def process_html(html: str) -> str:
    """Process HTML, minifying embedded CSS and JS."""
    
    # Minify inline CSS
    def replace_style(match):
        content = minify_css(match.group(1))
        return f'<style>{content}</style>'
    
    html = re.sub(r'<style[^>]*>(.*?)</style>', replace_style, html, flags=re.DOTALL | re.IGNORECASE)
    
    # Minify inline JS
    def replace_script(match):
        content = minify_js(match.group(1))
        return f'<script>{content}</script>'
    
    html = re.sub(r'<script[^>]*>(.*?)</script>', replace_script, html, flags=re.DOTALL | re.IGNORECASE)
    
    # Minify HTML structure
    html = minify_html(html)
    
    return html


def to_c_array(data: bytes, var_name: str = 'control_html_gz') -> str:
    """Convert bytes to C array definition."""
    lines = []
    lines.append('// Auto-generated - do not edit manually')
    lines.append(f'// Generated from control.html')
    lines.append('')
    lines.append('#ifndef CONTROL_HTML_H')
    lines.append('#define CONTROL_HTML_H')
    lines.append('')
    lines.append('#include <pgmspace.h>')
    lines.append('')
    lines.append(f'const size_t {var_name}_len = {len(data)};')
    lines.append('')
    lines.append(f'const uint8_t {var_name}[] PROGMEM = {{')
    
    # Format bytes in rows of 16
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_str = ', '.join(f'0x{b:02x}' for b in chunk)
        if i + 16 < len(data):
            hex_str += ','
        lines.append(f'  {hex_str}')
    
    lines.append('};')
    lines.append('')
    lines.append('#endif // CONTROL_HTML_H')
    
    return '\n'.join(lines)


def main():
    args = sys.argv[1:]
    embed_image = None
    if len(args) >= 4 and args[-2] == '--embed-image':
        embed_image = Path(args[-1])
        args = args[:-2]
    if len(args) != 2:
        print(f'Usage: {sys.argv[0]} <input.html> <output.h> [--embed-image <image.png>]')
        sys.exit(1)
    
    input_path = Path(args[0])
    output_path = Path(args[1])
    
    if not input_path.exists():
        print(f'Error: Input file not found: {input_path}')
        sys.exit(1)
    
    # Read HTML
    html = input_path.read_text(encoding='utf-8')
    original_size = len(html)
    
    # Optionally embed image as data URL (resize if too large for firmware ~16KB gzip total)
    if embed_image is not None:
        if not embed_image.exists():
            print(f'Warning: Image not found: {embed_image} (skipping embed)')
        else:
            data = embed_image.read_bytes()
            max_embed_bytes = 3500  # firmware CONTROL_PAGE_GZ_MAX is 16KB; page ~12KB, so image must stay tiny
            if len(data) > max_embed_bytes:
                try:
                    from PIL import Image
                    import io
                    resample = getattr(getattr(Image, 'Resampling', None), 'LANCZOS', None) or getattr(Image, 'LANCZOS', 1)
                    im = Image.open(io.BytesIO(data)).convert('RGBA')
                    im.thumbnail((32, 32), resample)  # small for 16KB gzip limit; CSS scales to 40x40
                    buf = io.BytesIO()
                    im.save(buf, format='PNG', optimize=True)
                    data = buf.getvalue()
                    print(f'Resized image for embed: {embed_image} -> {len(data):,} bytes')
                except Exception as e:
                    print(f'Warning: Image too large ({len(data):,} bytes) and PIL resize failed: {e}')
                    print('Skipping embed. Use a small PNG (< 6KB) or install Pillow for auto-resize.')
                    data = None
            if data:
                b64 = base64.standard_b64encode(data).decode('ascii')
                data_url = f'data:image/png;base64,{b64}'
                filename = embed_image.name
                # Replace any src pointing to this image (e.g. "steampigg.png" or "../../steampigg.png")
                html = re.sub(r'src="[^"]*' + re.escape(filename) + r'"', f'src="{data_url}"', html)
                print(f'Embedded image: {filename} ({len(data):,} bytes)')
    
    # Minify
    html = process_html(html)
    minified_size = len(html)
    
    # Compress
    compressed = gzip.compress(html.encode('utf-8'), compresslevel=9)
    compressed_size = len(compressed)
    
    # Generate C header
    c_code = to_c_array(compressed)
    
    # Write output
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(c_code, encoding='utf-8')
    
    # Print stats
    print(f'Input:      {input_path}')
    print(f'Output:     {output_path}')
    print(f'Original:   {original_size:,} bytes')
    print(f'Minified:   {minified_size:,} bytes ({100*minified_size/original_size:.1f}%)')
    print(f'Compressed: {compressed_size:,} bytes ({100*compressed_size/original_size:.1f}%)')
    print('Done!')


if __name__ == '__main__':
    main()
