# GDB pretty-printers for DQ runtime types.
#
# Loaded from .vscode/launch.json. The first useful type is DQ `str`,
# represented in debug info as ODynStrMgr *.

import gdb


_PRINTER_NAME = "dq-runtime-printers"
_MAX_PREVIEW_CHARS = 512


def _inferior():
    inferior = gdb.selected_inferior()
    if inferior is None:
        raise gdb.GdbError("no selected inferior")
    return inferior


def _ptr_size():
    return gdb.lookup_type("void").pointer().sizeof


def _read_uint(addr, size):
    data = _inferior().read_memory(addr, size).tobytes()
    return int.from_bytes(data, byteorder="little", signed=False)


def _read_int(addr, size):
    data = _inferior().read_memory(addr, size).tobytes()
    return int.from_bytes(data, byteorder="little", signed=True)


def _escape_text(text):
    out = []
    for ch in text:
        code = ord(ch)
        if ch == "\\":
            out.append("\\\\")
        elif ch == "\"":
            out.append("\\\"")
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\r":
            out.append("\\r")
        elif ch == "\t":
            out.append("\\t")
        elif 32 <= code < 127:
            out.append(ch)
        elif code <= 0xFF:
            out.append("\\x%02x" % code)
        elif code <= 0xFFFF:
            out.append("\\u%04x" % code)
        else:
            out.append("\\U%08x" % code)
    return "".join(out)


class DqDynStrPrinter:
    """Pretty-printer for DQ str / ODynStrMgr *."""

    def __init__(self, val):
        self.val = val
        self.ptr_size = _ptr_size()
        self.addr = int(val)
        self.refcount = 0
        self.dataptr = 0
        self.length = 0
        self.capacity = 0
        self.chwidth = 1
        self._loaded = False
        self._error = None

    def _load_header(self):
        if self._loaded or self.addr == 0:
            return
        try:
            base = self.addr
            ps = self.ptr_size

            # ODynStrMgr layout:
            #   refcount : int      (DQ int is pointer-sized)
            #   dataptr  : pointer
            #   length   : uint32
            #   capacity : uint32
            #   chwidth  : uint8
            self.refcount = _read_int(base, ps)
            self.dataptr = _read_uint(base + ps, ps)
            self.length = _read_uint(base + 2 * ps, 4)
            self.capacity = _read_uint(base + 2 * ps + 4, 4)
            self.chwidth = _read_uint(base + 2 * ps + 8, 1)
            if self.chwidth not in (1, 2, 4):
                self.chwidth = 1
            self._loaded = True
        except Exception as exc:
            self._error = str(exc)

    def _read_text(self):
        self._load_header()
        if self._error:
            return "<error: %s>" % self._error
        if self.addr == 0 or self.length == 0 or self.dataptr == 0:
            return ""

        shown_chars = min(self.length, _MAX_PREVIEW_CHARS)
        byte_count = shown_chars * self.chwidth

        try:
            data = _inferior().read_memory(self.dataptr, byte_count).tobytes()
            if self.chwidth == 1:
                text = data.decode("utf-8", errors="replace")
            elif self.chwidth == 2:
                text = data.decode("utf-16-le", errors="replace")
            else:
                text = data.decode("utf-32-le", errors="replace")
        except Exception as exc:
            return "<error: %s>" % exc

        if self.length > shown_chars:
            text += "...<truncated>"
        return text

    def to_string(self):
        self._load_header()
        text = self._read_text()
        return '"%s" len=%d cap=%d refs=%d chwidth=%d' % (
            _escape_text(text),
            self.length,
            self.capacity,
            self.refcount,
            self.chwidth,
        )

    def children(self):
        self._load_header()
        yield "length", self.length
        yield "capacity", self.capacity
        yield "refcount", self.refcount
        yield "chwidth", self.chwidth
        yield "dataptr", self.dataptr

    def display_hint(self):
        return None


class DqCStringPrinter:
    """Pretty-printer for fixed DQ cstring(N), emitted as cchar[N + 1]."""

    def __init__(self, val):
        self.val = val
        self.byte_count = val.type.sizeof
        self.maxlen = max(0, self.byte_count - 1)
        self.length = 0
        self._text = ""
        self._error = None
        self._loaded = False

    def _read_bytes(self):
        if self.val.address is not None:
            addr = int(self.val.address)
            return _inferior().read_memory(addr, self.byte_count).tobytes()

        # Fallback for non-addressable array values.
        data = []
        low, high = self.val.type.range()
        for index in range(low, high + 1):
            data.append(int(self.val[index]) & 0xFF)
        return bytes(data)

    def _load(self):
        if self._loaded:
            return
        self._loaded = True
        try:
            data = self._read_bytes()
            nul = data.find(b"\x00")
            if nul < 0:
                raw = data
            else:
                raw = data[:nul]
            self.length = len(raw)
            self._text = raw.decode("utf-8", errors="replace")
        except Exception as exc:
            self._error = str(exc)

    def to_string(self):
        self._load()
        if self._error:
            return '<error: %s> len=0 max=%d' % (self._error, self.maxlen)
        return '"%s" len=%d max=%d' % (
            _escape_text(self._text),
            self.length,
            self.maxlen,
        )

    def children(self):
        self._load()
        yield "length", self.length
        yield "maxlen", self.maxlen

    def display_hint(self):
        return None


def _type_name(gdb_type):
    stripped = gdb_type.strip_typedefs()
    return stripped.tag or stripped.name or str(stripped)


def dq_lookup_pretty_printer(val):
    try:
        gdb_type = val.type.strip_typedefs()
        if gdb_type.code == gdb.TYPE_CODE_PTR:
            if _type_name(gdb_type.target()) == "ODynStrMgr":
                return DqDynStrPrinter(val)
        elif gdb_type.code == gdb.TYPE_CODE_ARRAY:
            if _type_name(gdb_type.target()) == "cchar":
                return DqCStringPrinter(val)
    except Exception:
        return None
    return None


dq_lookup_pretty_printer.name = _PRINTER_NAME


def register_dq_printers(objfile=None):
    if objfile is None:
        printers = gdb.pretty_printers
    else:
        printers = objfile.pretty_printers

    printers[:] = [
        printer
        for printer in printers
        if getattr(printer, "name", None) != _PRINTER_NAME
    ]
    printers.append(dq_lookup_pretty_printer)


register_dq_printers()
