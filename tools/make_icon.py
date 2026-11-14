"""
make_icon.py - Generates or converts an image into a multi-resolution Windows .ico file.

Usage:
  python tools/make_icon.py                      # Generates a default modern PDF icon
  python tools/make_icon.py path/to/image.png    # Converts your image (PNG/JPG) to resources/app.ico
"""

import sys
import os
from PIL import Image, ImageDraw, ImageFont

def generate_default_icon(output_path="resources/app.ico"):
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    
    # We create a high-res 256x256 image first
    size = 256
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Document dimensions with padding for shadow
    pad = 20
    doc_left = pad + 15
    doc_top = pad
    doc_right = size - pad - 15
    doc_bottom = size - pad
    corner_fold = 60
    
    # Draw soft drop shadow behind the page
    for offset in range(12, 0, -2):
        alpha = int(25 * (1.0 - offset / 14.0))
        shadow_rect = [
            doc_left + offset // 2,
            doc_top + offset // 2 + 6,
            doc_right + offset // 2 + 2,
            doc_bottom + offset // 2 + 4
        ]
        draw.rounded_rectangle(shadow_rect, radius=12, fill=(0, 0, 0, alpha))
        
    # Main document polygon (with top-right folded corner)
    doc_points = [
        (doc_left, doc_top + 10),
        (doc_left + 10, doc_top),
        (doc_right - corner_fold, doc_top),
        (doc_right, doc_top + corner_fold),
        (doc_right, doc_bottom - 10),
        (doc_right - 10, doc_bottom),
        (doc_left + 10, doc_bottom),
        (doc_left, doc_bottom - 10),
    ]
    
    # Fill document background (clean crisp bright white/off-white)
    draw.polygon(doc_points, fill=(245, 247, 250, 255))
    
    # Outline the document
    draw.line(doc_points + [doc_points[0]], fill=(200, 205, 215, 255), width=3)
    
    # Draw the folded corner flap
    fold_points = [
        (doc_right - corner_fold, doc_top),
        (doc_right - corner_fold, doc_top + corner_fold),
        (doc_right, doc_top + corner_fold),
    ]
    draw.polygon(fold_points, fill=(215, 220, 230, 255))
    draw.line(fold_points, fill=(180, 185, 195, 255), width=2)
    
    # Subtle page lines in the background
    line_x1 = doc_left + 22
    line_x2 = doc_right - 22
    for ly in [80, 102, 124, 146]:
        if ly < doc_top + corner_fold:
            draw.line([(line_x1, ly), (doc_right - corner_fold - 15, ly)], fill=(215, 222, 232, 255), width=4)
        else:
            draw.line([(line_x1, ly), (line_x2, ly)], fill=(215, 222, 232, 255), width=4)

    # Red/Crimson Modern PDF Badge in lower half
    badge_left = doc_left + 16
    badge_right = doc_right - 16
    badge_top = 160
    badge_bottom = 215
    badge_radius = 8
    
    # Gradient/Vibrant PDF Red: #E5252A / #D32F2F
    badge_rect = [badge_left, badge_top, badge_right, badge_bottom]
    draw.rounded_rectangle(badge_rect, radius=badge_radius, fill=(229, 37, 42, 255))
    
    # Draw white "PDF" text on the badge
    # Try loading Arial or bold default font, fallback to default
    try:
        font = ImageFont.truetype("arialbd.ttf", 36)
    except Exception:
        try:
            font = ImageFont.truetype("arial.ttf", 36)
        except Exception:
            font = ImageFont.load_default()
            
    text = "PDF"
    # Measure text
    bbox = draw.textbbox((0, 0), text, font=font)
    text_w = bbox[2] - bbox[0]
    text_h = bbox[3] - bbox[1]
    
    tx = badge_left + (badge_right - badge_left - text_w) // 2
    ty = badge_top + (badge_bottom - badge_top - text_h) // 2 - 2
    draw.text((tx, ty), text, fill=(255, 255, 255, 255), font=font)
    
    # Save as multi-resolution ICO
    sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    img.save(output_path, format="ICO", sizes=sizes)
    print(f"[OK] Generated default icon at: {output_path}")

def convert_image_to_icon(input_image_path, output_path="resources/app.ico"):
    if not os.path.exists(input_image_path):
        print(f"[ERROR] Input image not found: {input_image_path}")
        return False
        
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    img = Image.open(input_image_path)
    if img.mode != "RGBA":
        img = img.convert("RGBA")
        
    sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    img.save(output_path, format="ICO", sizes=sizes)
    print(f"[OK] Successfully converted '{input_image_path}' to '{output_path}' with {len(sizes)} resolutions!")
    return True

if __name__ == "__main__":
    out = "resources/app.ico"
    if len(sys.argv) > 1:
        convert_image_to_icon(sys.argv[1], out)
    else:
        generate_default_icon(out)

