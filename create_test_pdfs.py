import os

def create_simple_pdf(filename, title, num_pages, keyword="Antigravity"):
    # Generate a syntactically valid PDF with text and page numbers
    objects = []
    
    def add_object(content):
        objects.append(content)
        return len(objects)

    # 1: Catalog
    # 2: Pages root
    # For each page:
    #   Page object
    #   Content stream object
    # Font object

    page_obj_ids = []
    
    # We will build objects list
    # Obj 1: Catalog (placeholder)
    # Obj 2: Pages (placeholder)
    # Obj 3: Font
    font_content = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>"
    
    # We will assemble all objects first
    raw_objects = {}
    next_id = 4
    
    content_ids = []
    for p in range(1, num_pages + 1):
        # Content stream
        text_lines = [
            f"BT",
            f"/F1 22 Tf",
            f"50 720 Td",
            f"({title} - Page {p}) Tj",
            f"/F1 12 Tf",
            f"0 -40 Td",
            f"(This is page {p} of {num_pages} in document '{title}'.) Tj",
            f"0 -25 Td",
            f"(Designed for testing lightweight offline PDF viewing performance.) Tj",
            f"0 -25 Td",
            f"(Special search test keyword: {keyword} at section {p}.4) Tj",
            f"0 -30 Td",
            f"(Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor) Tj",
            f"0 -20 Td",
            f"(incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud) Tj",
            f"0 -20 Td",
            f"(exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.) Tj",
            f"ET"
        ]
        stream_data = "\n".join(text_lines).encode("latin1")
        stream_obj = f"<< /Length {len(stream_data)} >>\nstream\n".encode("latin1") + stream_data + b"\nendstream"
        
        cid = next_id
        next_id += 1
        raw_objects[cid] = stream_obj
        
        pid = next_id
        next_id += 1
        page_obj_ids.append(pid)
        page_content = f"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents {cid} 0 R /Resources << /Font << /F1 3 0 R >> >> >>".encode("latin1")
        raw_objects[pid] = page_content

    # Catalog & Pages
    catalog = b"<< /Type /Catalog /Pages 2 0 R >>"
    raw_objects[1] = catalog
    
    kids_str = " ".join(f"{pid} 0 R" for pid in page_obj_ids)
    pages = f"<< /Type /Pages /Kids [{kids_str}] /Count {num_pages} >>".encode("latin1")
    raw_objects[2] = pages
    raw_objects[3] = font_content.encode("latin1")

    # Output PDF
    with open(filename, "wb") as f:
        f.write(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
        offsets = {}
        for obj_id in sorted(raw_objects.keys()):
            offsets[obj_id] = f.tell()
            f.write(f"{obj_id} 0 obj\n".encode("latin1"))
            f.write(raw_objects[obj_id])
            f.write(b"\nendobj\n")
            
        xref_offset = f.tell()
        f.write(f"xref\n0 {len(raw_objects) + 1}\n0000000000 65535 f \n".encode("latin1"))
        for obj_id in sorted(raw_objects.keys()):
            f.write(f"{offsets[obj_id]:010d} 00000 n \n".encode("latin1"))
            
        f.write(b"trailer\n")
        f.write(f"<< /Size {len(raw_objects) + 1} /Root 1 0 R >>\n".encode("latin1"))
        f.write(b"startxref\n")
        f.write(f"{xref_offset}\n".encode("latin1"))
        f.write(b"%%EOF\n")

os.makedirs("sample_docs", exist_ok=True)
create_simple_pdf("sample_docs/1_Quick_Start_Guide.pdf", "Quick Start Guide", 3, "OfflineFast")
create_simple_pdf("sample_docs/2_Windows_Architecture.pdf", "Windows Architecture Overview", 10, "Win32Native")
create_simple_pdf("sample_docs/3_PdfViewer_Manual.pdf", "PdfViewer User Manual", 25, "ReadingProgress")
create_simple_pdf("sample_docs/4_Research_Paper.pdf", "Deep Learning in Documents", 50, "PDFiumEngine")
create_simple_pdf("sample_docs/5_Large_Encyclopedia.pdf", "Comprehensive Encyclopedia", 200, "PersistenceSQLite")

print("Created 5 sample PDF documents successfully!")

