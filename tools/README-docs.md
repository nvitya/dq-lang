# Building the Documentation

Run these commands from the repository root.

## Setup on Ubuntu or Debian

Install the native libraries and fonts used by the PDF renderer:

```bash
sudo apt-get install python3-venv libpango-1.0-0 libpangoft2-1.0-0 libharfbuzz-subset0 fonts-dejavu-core
python3 -m venv build/docs-venv
source build/docs-venv/bin/activate
python -m pip install -r requirements-docs.txt
```

For other operating systems, follow the
[WeasyPrint installation instructions](https://doc.courtbouillon.org/weasyprint/stable/first_steps.html).

## Preview and Build

Preview the website without rebuilding the PDF after every edit:

```bash
mkdocs serve
```

Build and validate the website together with the PDF book:

```bash
ENABLE_PDF_EXPORT=1 mkdocs build --strict
```

The book is written to `site/pdf/dq-lang-book.pdf`, with a cover and a table of
contents. Chapters follow the `nav` order in `mkdocs.yml`. The MkDocs hook numbers
parts, chapters, and subsections from that hierarchy: `1. DQ Project`,
`1.1. Introduction`, `1.1.1` for its first subsection, and `2. DQ Programming
Language` for the next part. Navigation, headings, and contents use the same
numbers; additional PDF heading numbering is disabled.

A section's `index.md` is its overview page and shares the section number; it
does not advance the chapter counter. The PDF hook removes its repeated heading
and contents entry while retaining the overview text and links.

The Standard Packages chapters include the package-local `stdpkg/*/docs/index.md`
manuals through `pymdownx.snippets`. Edit those manuals for API changes; the
short files in `docs/040-packages/` select the source for each chapter. Missing
includes fail the build, and `mkdocs serve` watches the package documentation.

The [coverage inventory](../docs/090-appendices/050-documentation-coverage.md)
maps the previous site and archived design notes to the current chapters.

The PDF plugin adds a download link to the generated website when PDF export is
enabled. GitHub Actions enables PDF export for validation and deployment, so the
published book is available at `pdf/dq-lang-book.pdf` relative to the site root.
PDF export uses installed fonts without fetching Google Fonts; the website
retains its theme fonts.
