"""System-wide GDB startup hook for the DQ pretty-printers."""

from pathlib import Path
import runpy


_SOURCE_PRINTERS = Path(__file__).resolve().with_name("dq_printers.py")
_INSTALLED_PRINTERS = Path("@DQ_GDB_PRINTERS_FILE@")
_PRINTERS = (
    _SOURCE_PRINTERS if _SOURCE_PRINTERS.is_file() else _INSTALLED_PRINTERS
)

if not _PRINTERS.is_file():
    raise RuntimeError("DQ GDB pretty-printers not found at %s" % _PRINTERS)

runpy.run_path(str(_PRINTERS))
