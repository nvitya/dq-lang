def pre_pdf_render(soup, logger):
    """Keep the generated part heading when a section index repeats it."""
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
