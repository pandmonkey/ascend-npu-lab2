#!/usr/bin/env python3
"""Convert report.md to report.pdf using a simple text-to-PS-to-PDF pipeline."""
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MD = ROOT / "report" / "report.md"
PS = ROOT / "report" / "report.ps"
PDF = ROOT / "report" / "report.pdf"

def md_to_ps(md_text):
    """Convert markdown to simple PostScript."""
    lines = md_text.split('\n')
    ps_lines = []
    ps_lines.append("%!PS-Adobe-3.0")
    ps_lines.append("%%Orientation: Portrait")
    ps_lines.append("%%Pages: 1")
    ps_lines.append("%%Page: 1 1")
    ps_lines.append("/Courier findfont 9 scalefont setfont")
    ps_lines.append("72 750 moveto")
    
    y = 750
    line_height = 11
    left_margin = 50
    page_width = 550
    char_width = 5.4  # approx for Courier 9pt
    
    for line in lines:
        if y < 50:
            ps_lines.append("showpage")
            ps_lines.append("%%Page: 1 1")
            ps_lines.append("/Courier findfont 9 scalefont setfont")
            ps_lines.append(f"72 750 moveto")
            y = 750
        
        # Handle long lines - wrap
        max_chars = int(page_width / char_width)
        if len(line) > max_chars:
            while line:
                chunk = line[:max_chars]
                line = line[max_chars:]
                # Escape special PS chars
                chunk = chunk.replace('\\', '\\\\').replace('(', '\\(').replace(')', '\\)')
                ps_lines.append(f"({chunk}) show")
                y -= line_height
                if y < 50:
                    ps_lines.append("showpage")
                    ps_lines.append(f"72 750 moveto")
                    y = 750
                ps_lines.append(f"72 {y} moveto")
        else:
            line_escaped = line.replace('\\', '\\\\').replace('(', '\\(').replace(')', '\\)')
            ps_lines.append(f"({line_escaped}) show")
            y -= line_height
            ps_lines.append(f"72 {y} moveto")
    
    ps_lines.append("showpage")
    ps_lines.append("%%EOF")
    return '\n'.join(ps_lines)

def main():
    md_text = MD.read_text(encoding='utf-8')
    ps_text = md_to_ps(md_text)
    PS.write_text(ps_text, encoding='utf-8')
    print(f"Wrote {PS}")
    
    # Convert PS to PDF
    result = subprocess.run(
        ['ps2pdf', '-sPAPERSIZE=a4', str(PS), str(PDF)],
        capture_output=True, text=True
    )
    if result.returncode == 0:
        print(f"Wrote {PDF}")
    else:
        print(f"ps2pdf failed: {result.stderr}")
        # Fallback: just copy as .txt
        print("Falling back to report.md as text file")

if __name__ == '__main__':
    main()
