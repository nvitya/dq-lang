#!/usr/bin/env python3
import sys
import re

# Settings
MAX_CONST_BLOCK_LENGTH = 32
CONST_NAME_PADDING = 20
STRUCT_FIELD_PADDING = 12
STRUCT_TYPE_PADDING = 8
ARRAY_TYPE_SPACING = 1
ATTR_TYPE_SPACING = 2

def process_file(filepath):
    try:
        with open(filepath, 'r', errors='ignore') as f:
            lines = f.readlines()
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return

    state = 'NORMAL'
    
    # regexes
    re_define = re.compile(r'^\s*#define\s+([A-Za-z0-9_]+)(?:\s+(.*))?')
    re_struct_start = re.compile(r'^\s*typedef\s+struct')
    re_union_start = re.compile(r'^\s*typedef\s+union')
    re_enum_start = re.compile(r'^\s*typedef\s+enum')
    re_end_type = re.compile(r'^\s*}\s*([A-Za-z0-9_]+)\s*;')
    # Match __IO uint32_t ISR; or uint32_t RESERVED[5]; etc
    re_field = re.compile(r'^\s*(__IOM?|__OM?|__IM?|volatile\s+const|volatile)?\s*(uint[0-9]+_t|int[0-9]+_t|[A-Za-z0-9_]+)\s+([A-Za-z0-9_]+)(\[[^\]]+\])?\s*;(.*)')
    re_enum_item = re.compile(r'^\s*([A-Za-z0-9_]+)\s*=\s*([^,/]+)(?:,)?(.*)')
    re_number_suffix = re.compile(r'\b(0x[0-9a-fA-F]+|[0-9]+)[UuLl]+\b')
    re_comment = re.compile(r'/\*!*<*\s*(.*?)\s*\*/')
    
    out_lines = []
    
    struct_name = ""
    struct_fields = []
    
    pending_const_type = None
    pending_consts = []

    def flush_consts():
        nonlocal pending_const_type, pending_consts
        while pending_consts:
            chunk = pending_consts[:MAX_CONST_BLOCK_LENGTH]
            pending_consts = pending_consts[MAX_CONST_BLOCK_LENGTH:]
            out_lines.append(f"const({pending_const_type}):")
            for name, val, comment in chunk:
                line = f"    {name.ljust(CONST_NAME_PADDING)} = {val}"
                if comment:
                    line += f"  // {comment}"
                out_lines.append(line)
            out_lines.append("endconst\n")
        pending_const_type = None
        pending_consts = []

    def add_const(ctype, name, val, comment=""):
        nonlocal pending_const_type
        if pending_const_type != ctype and len(pending_consts) > 0:
            flush_consts()
        pending_const_type = ctype
        pending_consts.append((name, val, comment))
        if len(pending_consts) >= MAX_CONST_BLOCK_LENGTH:
            flush_consts()

    # join lines ending with \
    joined_lines = []
    current_line = ""
    for raw_line in lines:
        line = raw_line.rstrip()
        if line.endswith('\\'):
            current_line += line[:-1]
        else:
            current_line += line
            joined_lines.append(current_line)
            current_line = ""

    for line in joined_lines:
        if state == 'NORMAL':
            m_def = re_define.match(line)
            if m_def:
                name = m_def.group(1)
                val_raw = m_def.group(2)
                
                if val_raw is not None:
                    if re.match(r'^\s*#define\s+' + re.escape(name) + r'\(', line):
                        continue
                        
                    # remove block comments properly
                    val = re.sub(r'/\*.*?\*/', '', val_raw)
                    val = val.split('//')[0].strip()
                    
                    if val:
                        val = re_number_suffix.sub(r'\1', val)
                        # strip C casts like (uint32_t)
                        val = re.sub(r'\(\s*u?int\d+_t\s*\)', '', val)
                        # strip redundant outer parens around single tokens
                        val = re.sub(r'^\s*\(\s*([^() ]+)\s*\)\s*$', r'\1', val)
                        
                        if "Type" not in val and "->" not in val and "volatile" not in val:
                            add_const("uint32", name, val)
                continue
            
            if re_struct_start.match(line):
                flush_consts()
                state = 'STRUCT'
                struct_fields = []
                continue
                
            if re_union_start.match(line):
                state = 'UNION'
                continue
                
            if re_enum_start.match(line):
                flush_consts()
                state = 'ENUM'
                continue
                
        elif state == 'STRUCT':
            m_end = re_end_type.match(line)
            if m_end:
                struct_name = m_end.group(1)
                out_lines.append(f"struct {struct_name}:")
                for f in struct_fields:
                    out_lines.append(f)
                out_lines.append(f"endstruct\n")
                state = 'NORMAL'
                continue
            
            m_field = re_field.match(line)
            if m_field:
                mod = m_field.group(1) or ""
                ctype = m_field.group(2)
                name = m_field.group(3)
                arr = m_field.group(4) or ""
                comment = m_field.group(5) or ""
                
                ctype = ctype.replace('_t', '')
                
                dq_attr = ""
                if mod in ("__IOM", "__IO", "volatile"):
                    dq_attr = "[[regrw]]"
                elif mod in ("__IM", "__I", "volatile const"):
                    dq_attr = "[[regro]]"
                elif mod in ("__OM", "__O"):
                    dq_attr = "[[regwo]]"
                
                dq_attr = dq_attr.ljust(9 + ATTR_TYPE_SPACING)
                
                dq_type = ctype
                if arr:
                    arr_val = arr.strip('[]')
                    arr_val = re_number_suffix.sub(r'\1', arr_val)
                    spacer = " " * ARRAY_TYPE_SPACING
                    dq_type = f"[{arr_val}]{spacer}{dq_type}"
                    
                field_str = f"    {name.ljust(STRUCT_FIELD_PADDING)} : {dq_attr}{dq_type.ljust(STRUCT_TYPE_PADDING)}"
                if comment:
                    cm = re_comment.search(comment)
                    if cm:
                        field_str += f"  // {cm.group(1)}"
                struct_fields.append(field_str)
            else:
                if "{" not in line and "}" not in line and line.strip():
                    struct_fields.append(f"    // unparsed: {line.strip()}")
                    
        elif state == 'UNION':
            m_end = re_end_type.match(line)
            if m_end:
                state = 'NORMAL'
            
        elif state == 'ENUM':
            m_end = re_end_type.match(line)
            if m_end:
                state = 'NORMAL'
                continue
            
            m_item = re_enum_item.match(line)
            if m_item:
                name = m_item.group(1)
                val = m_item.group(2)
                if val:
                    val = val.split('/*')[0].split('//')[0].strip()
                    val = re_number_suffix.sub(r'\1', val)
                else:
                    # In C, enum values can be omitted. We might just default to previous + 1, but for CMSIS they are usually explicit.
                    val = "0" 
                
                comment = m_item.group(3) or ""
                cm_str = ""
                if comment:
                    cm = re_comment.search(comment)
                    if cm:
                        cm_str = cm.group(1)
                
                add_const("int", name, val, cm_str)

    flush_consts()

    for l in out_lines:
        print(l)

if __name__ == '__main__':
    if len(sys.argv) > 1:
        process_file(sys.argv[1])
