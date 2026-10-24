from bs4 import BeautifulSoup
from mkdocs.plugins import event_priority


# Set the destination before other plugins derive page URLs from it.
@event_priority(100)
def on_files(files, **kwargs):
    """Publish the numbered introduction chapter at the site root."""
    introduction = files.get_file_from_path("010-project/010-introduction.md")
    introduction.dest_uri = "index.html"
    return files


@event_priority(50)
def on_nav(nav, **kwargs):
    """Use the book hierarchy for part and chapter numbers in every output."""
    def number(items, prefix=()):
        for index, item in enumerate(items, 1):
            numbers = (*prefix, index)
            item.title = f'{".".join(map(str, numbers))}. {item.title}'
            if item.is_section:
                number(item.children, numbers)
            elif item.is_page:
                item.book_number = numbers

    number(nav.items)
    return nav


def on_page_content(html, page, **kwargs):
    """Extend each chapter number with its subsection numbers, keeping anchors."""
    if not hasattr(page, "book_number"):
        return html

    soup = BeautifulSoup(html, "html.parser")
    counts = [0] * 5
    labels = {}
    for heading in soup.find_all(["h1", "h2", "h3", "h4", "h5", "h6"]):
        depth = int(heading.name[1]) - 1
        if depth:
            counts[depth - 1] += 1
            counts[depth:] = [0] * (5 - depth)
        else:
            counts = [0] * 5
        number = ".".join(map(str, (*page.book_number, *counts[:depth])))
        label = f"{number}." if depth == 0 else number
        heading.insert(0, f"{label} ")
        if heading.get("id"):
            labels[heading["id"]] = label

    # The sidebar TOC is built before this event; number it with the same labels.
    items = list(page.toc.items)
    while items:
        item = items.pop()
        if item.id in labels:
            item.title = f"{labels[item.id]} {item.title}"
        items.extend(item.children)
    return str(soup)
