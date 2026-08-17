#!/usr/bin/env python3
import sys
import re

MAX_CONST_BLOCK_LENGTH = 32
CONST_NAME_PADDING = 20
STRUCT_FIELD_PADDING = 12
STRUCT_TYPE_PADDING = 8
ARRAY_TYPE_SPACING = 1
ATTR_TYPE_SPACING = 2

def process_file(filepath, no_comments=False):
    try:
        with open(filepath, 'r', errors='ignore') as f:
            lines = f.readlines()
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
        return

    out_lines = []
    
    pending_const_type = None
    pending_consts = []
    known_consts = set()

    def flush_consts():
        nonlocal pending_const_type, pending_consts
        while pending_consts:
            chunk = pending_consts[:MAX_CONST_BLOCK_LENGTH]
            pending_consts = pending_consts[MAX_CONST_BLOCK_LENGTH:]
            out_lines.append(f"const({pending_const_type}):")
            for name, val, comment in chunk:
                line = f"    {name.ljust(CONST_NAME_PADDING)} = {val}"
                if comment and not no_comments:
                    line += f"  // {comment}"
                out_lines.append(line)
            out_lines.append("endconst\n")
        pending_const_type = None
        pending_consts = []

    def add_const(ctype, name, val, comment=""):
        nonlocal pending_const_type
        if name in known_consts:
            return
        if pending_const_type != ctype and len(pending_consts) > 0:
            flush_consts()
        pending_const_type = ctype
        pending_consts.append((name, val, comment))
        known_consts.add(name)
        if len(pending_consts) >= MAX_CONST_BLOCK_LENGTH:
            flush_consts()

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

    re_define = re.compile(r'^\s*#define\s+([A-Za-z0-9_]+)(?:\s+(.*))?')
    re_number_suffix = re.compile(r'\b(0x[0-9a-fA-F]+|[0-9]+)[UuLl]+\b')
    re_ptr_cast = re.compile(r'^\s*\(?\s*\(\s*([A-Za-z0-9_]+)\s*\*\s*\)\s*(.+?)\)?\s*$')
    re_comment = re.compile(r'/\*!*<*\s*(.*?)\s*\*/')

    # C parser state
    state = 'NORMAL'
    context_stack = []
    # context is dict: {'type': 'struct'|'union', 'fields': [], 'name': ''}
    
    top_structs = [] # list of (name, lines)
    
    anon_counter = 1

    def format_field(mod, ctype, name, arr, comment):
        ctype = ctype.replace('_t', '')
        if "RSSLIB_" in ctype:
            ctype = "uint32"
        # Map ATSAM register types
        c_stripped = ctype.strip()
        if c_stripped.endswith('Reg8'): ctype = 'uint8'
        elif c_stripped.endswith('Reg16'): ctype = 'uint16'
        elif c_stripped.endswith('Reg32'): ctype = 'uint32'
        elif c_stripped.endswith('Reg64'): ctype = 'uint64'
        elif c_stripped in ('RoReg', 'RwReg', 'WoReg', 'Reg'): ctype = 'uint32'
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
            arr_val = arr_val.replace('0X', '0x')
            arr_val = arr_val.replace('~', ' NOT ').replace('&', ' AND ').replace('|', ' OR ').replace('^', ' XOR ').replace('/', ' IDIV ').replace('%', ' IMOD ')
            spacer = " " * ARRAY_TYPE_SPACING
            dq_type = f"[{arr_val}]{spacer}{dq_type}"
            
        field_str = f"{name.ljust(STRUCT_FIELD_PADDING)} : {dq_attr}{dq_type.ljust(STRUCT_TYPE_PADDING)}"
        if comment and not no_comments:
            cm = re_comment.search(comment)
            if cm:
                field_str += f"  // {cm.group(1)}"
        return field_str

    pending_struct_end = False

    for line in joined_lines:
        if state == 'NORMAL':
            m_def = re_define.match(line)
            if m_def:
                name = m_def.group(1)
                val_raw = m_def.group(2)
                
                if val_raw is not None:
                    if re.match(r'^\s*#define\s+' + re.escape(name) + r'\(', line):
                        continue
                        
                    val = val_raw.split('/*')[0].split('//')[0].strip()
                    
                    if val:
                        val = re_number_suffix.sub(r'\1', val)
                        val = val.replace('0X', '0x')

                        m_ptr = re_ptr_cast.match(val)
                        if m_ptr:
                            ptr_type = m_ptr.group(1).replace('_t', '')
                            ptr_val = m_ptr.group(2)
                            if name in known_consts:
                                continue
                            flush_consts()
                            out_lines.append(f"const {name.ljust(12)} :? = ^{ptr_type}({ptr_val})")
                            known_consts.add(name)
                            continue
                            
                        val = re.sub(r'\(\s*u?int\d+_t\s*\)', '', val)
                        val = re.sub(r'^\s*\(\s*([^() ]+)\s*\)\s*$', r'\1', val)
                        val = val.replace('~', ' NOT ').replace('&', ' AND ').replace('|', ' OR ').replace('^', ' XOR ').replace('/', ' IDIV ').replace('%', ' IMOD ')
                        
                        val = val.replace('0x0x', '0x')
                        if "*)" in val.replace(" ", "") or "!RESET" in val or "!" in val:
                            continue
                            
                        if '{' in val or '}' in val or ',' in val:
                            continue
                            
                        temp_val = re.sub(r'0x[0-9a-fA-F]+', '', val)
                        temp_val = re.sub(r'\b[0-9]+\b', '', temp_val)
                        words = re.findall(r'[A-Za-z_][A-Za-z0-9_]*', temp_val)
                        valid = True
                        for w in words:
                            if w not in known_consts and w not in ('NOT', 'AND', 'OR', 'XOR', 'IDIV', 'IMOD', 'SHL', 'SHR'):
                                valid = False
                                break
                        
                        if valid:
                            add_const("uint32", name, val)
                continue
            
            if re.match(r'^\s*typedef\s+struct\s*\{?', line):
                flush_consts()
                state = 'IN_STRUCT'
                context_stack = [{'type': 'struct', 'fields': [], 'name': ''}]
                continue
                
            m_cstruct = re.match(r'^\s*struct\s+([A-Za-z0-9_]+)\s*\{?', line)
            if m_cstruct:
                flush_consts()
                state = 'IN_STRUCT'
                context_stack = [{'type': 'struct', 'fields': [], 'name': m_cstruct.group(1)}]
                continue

            if re.match(r'^\s*typedef\s+union\s*\{?', line):
                flush_consts()
                state = 'IN_STRUCT'
                context_stack = [{'type': 'union', 'fields': [], 'name': ''}]
                continue
                
            m_cunion = re.match(r'^\s*union\s+([A-Za-z0-9_]+)\s*\{?', line)
            if m_cunion:
                flush_consts()
                state = 'IN_STRUCT'
                context_stack = [{'type': 'union', 'fields': [], 'name': m_cunion.group(1)}]
                continue
            if re.match(r'^\s*typedef\s+enum\s*\{?', line):
                flush_consts()
                state = 'ENUM'
                continue
                
        elif state == 'IN_STRUCT':
            m_nested_struct = re.match(r'^\s*struct\s*\{', line)
            if m_nested_struct:
                context_stack.append({'type': 'struct', 'fields': [], 'name': ''})
                continue
                
            m_nested_union = re.match(r'^\s*union\s*\{', line)
            if m_nested_union:
                context_stack.append({'type': 'union', 'fields': [], 'name': ''})
                continue
                
            m_end = re.match(r'^\s*\}\s*([A-Za-z0-9_]*)(?:(\[[^\]]+\]))?\s*;', line)
            if m_end:
                name = m_end.group(1).replace('_t', '')
                arr = m_end.group(2) or ""
                
                ctx = context_stack.pop()
                if not context_stack:
                    # Top level struct ended
                    struct_name = name.replace('_t', '') if name else ctx['name'].replace('_t', '')
                    lines_to_add = []
                    if ctx['type'] == 'union':
                        lines_to_add.append(f"union {struct_name}:")
                        for f in ctx['fields']:
                            lines_to_add.append(f)
                        lines_to_add.append(f"endunion\n")
                    else:
                        lines_to_add.append(f"struct {struct_name}:")
                        for f in ctx['fields']:
                            lines_to_add.append(f)
                        lines_to_add.append(f"endstruct\n")
                    flush_consts()
                    for l in lines_to_add:
                        out_lines.append(l)
                    state = 'NORMAL'
                else:
                    # Nested ended
                    parent_ctx = context_stack[-1]
                    if ctx['type'] == 'struct':
                        # Hoist it to top level
                        top_struct_name = f"{context_stack[0].get('final_name', 'AnonStruct')}_{name}"
                        # Wait, we don't know the final name yet! 
                        # We can just generate a unique name
                        top_struct_name = f"NestedStruct_{anon_counter}"
                        anon_counter += 1
                        
                        lines_to_add = []
                        lines_to_add.append(f"struct {top_struct_name}:")
                        for f in ctx['fields']:
                            lines_to_add.append("    " + f)
                        lines_to_add.append(f"endstruct\n")
                        flush_consts()
                        for l in lines_to_add:
                            out_lines.append(l)
                        
                        # Add field to parent
                        if not name:
                            name = f"s_anon_{anon_counter}"
                            anon_counter += 1
                        field_str = format_field("", top_struct_name, name, arr, "")
                        parent_ctx['fields'].append("    " + field_str)
                    elif ctx['type'] == 'union':
                        # Inline union
                        if not name:
                            # Anonymous union
                            first_field = ""
                            if ctx['fields']:
                                m = re.search(r'^\s*([A-Za-z0-9_]+)\s*:', ctx['fields'][0])
                                if m:
                                    first_field = m.group(1)
                            name = f"u_{first_field}" if first_field else f"u_anon_{anon_counter}"
                            anon_counter += 1
                            
                        parent_ctx['fields'].append(f"    union {name}:")
                        for f in ctx['fields']:
                            parent_ctx['fields'].append("    " + f)
                        parent_ctx['fields'].append(f"    endunion")
                continue

            # Check for regular field
            re_field = re.compile(r'^\s*(__IOM?|__OM?|__IM?|volatile\s+const|volatile)?\s*(uint[0-9]+_t|int[0-9]+_t|[A-Za-z0-9_]+)\s+([A-Za-z0-9_]+)(\[[^\]]+\])?\s*;(.*)')
            m_field = re_field.match(line)
            if m_field:
                mod = m_field.group(1) or ""
                ctype = m_field.group(2)
                fname = m_field.group(3)
                arr = m_field.group(4) or ""
                comment = m_field.group(5) or ""
                
                f_str = format_field(mod, ctype, fname, arr, comment)
                context_stack[-1]['fields'].append("    " + f_str)
            else:
                if "{" not in line and "}" not in line and line.strip():
                    if not no_comments:
                        context_stack[-1]['fields'].append(f"    // unparsed: {line.strip()}")
            
        elif state == 'ENUM':
            m_end = re.match(r'^\s*\}\s*([A-Za-z0-9_]*)\s*;', line)
            if m_end:
                state = 'NORMAL'
                continue
            
            re_enum_item = re.compile(r'^\s*([A-Za-z0-9_]+)\s*=\s*([^,/]+)(?:,)?(.*)')
            m_item = re_enum_item.match(line)
            if m_item:
                name = m_item.group(1)
                val = m_item.group(2)
                if val:
                    val = val.split('/*')[0].split('//')[0].strip()
                    val = re_number_suffix.sub(r'\1', val)
                    val = val.replace('0X', '0x')
                    val = val.replace('~', ' NOT ').replace('&', ' AND ').replace('|', ' OR ').replace('^', ' XOR ').replace('/', ' IDIV ').replace('%', ' IMOD ')
                    
                    val = val.replace('!RESET', '1').replace('!DISABLE', '1').replace('!SUCCESS', '1')
                    val = val.replace('0x0x', '0x')
                    temp_val = re.sub(r'0x[0-9a-fA-F]+', '', val)
                    temp_val = re.sub(r'\b[0-9]+\b', '', temp_val)
                    words = re.findall(r'[A-Za-z_][A-Za-z0-9_]*', temp_val)
                    valid = True
                    for w in words:
                        if w not in known_consts and w not in ('NOT', 'AND', 'OR', 'XOR', 'IDIV', 'IMOD', 'SHL', 'SHR'):
                            valid = False
                            break
                    if not valid or "!" in val:
                        continue
                else:
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
        
    for name, lines in top_structs:
        for l in lines:
            print(l)

if __name__ == '__main__':
    no_comments = '--no-comments' in sys.argv
    args = [a for a in sys.argv[1:] if a != '--no-comments']
    if len(args) > 0:
        process_file(args[0], no_comments)
