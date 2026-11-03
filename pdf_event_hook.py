def pre_pdf_render(soup, logger):
    """Prepare the book for printing and remove repeated part headings."""
    # Use installed fallback fonts so exporting the book works offline.
    for stylesheet in soup.select('link[rel="stylesheet"]'):
        if stylesheet.get("href", "").startswith("https://fonts.googleapis.com/"):
            stylesheet.decompose()

    # Long appendix indexes should continue across pages below their headings.
    style = soup.new_tag("style")
    style.string = """
        [data-url^="/090-appendices/"] table { break-inside: auto; }
        [data-url^="/090-appendices/"] tr { break-inside: avoid; }
    """
    soup.head.append(style)

    for article in soup.select("article[data-url]"):
        heading = article.find("h1", recursive=False)
        section = article.find("section", recursive=False)
        if heading is None or section is None:
            continue
        index_heading = section.find("h2", recursive=False)
        if index_heading is None or heading.get_text() != index_heading.get_text():
            continue

        # The PDF plugin has already built its contents from both headings.
        for link in soup.select("#doc-toc a"):
            if link.get("href") == f"#{index_heading.get('id')}":
                link.parent.decompose()
        index_heading.decompose()
    return soup
