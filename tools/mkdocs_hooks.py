from mkdocs.plugins import event_priority


# Set the destination before other plugins derive page URLs from it.
@event_priority(100)
def on_files(files, **kwargs):
    """Publish the numbered introduction chapter at the site root."""
    introduction = files.get_file_from_path("010-project/010-introduction.md")
    introduction.dest_uri = "index.html"
    return files
