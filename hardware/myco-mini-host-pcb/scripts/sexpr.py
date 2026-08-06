"""Minimal S-expression parser/serializer for KiCad files."""

def tokenize(text):
    tokens = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c in ' \t\r\n':
            i += 1
            continue
        if c == '(':
            tokens.append('(')
            i += 1
            continue
        if c == ')':
            tokens.append(')')
            i += 1
            continue
        if c == '"':
            j = i + 1
            buf = []
            while j < n and text[j] != '"':
                if text[j] == '\\' and j + 1 < n:
                    buf.append(text[j + 1])
                    j += 2
                    continue
                buf.append(text[j])
                j += 1
            tokens.append(('str', ''.join(buf)))
            i = j + 1
            continue
        j = i
        while j < n and text[j] not in ' \t\r\n()':
            j += 1
        tokens.append(('atom', text[i:j]))
        i = j
    return tokens

def parse(text):
    tokens = tokenize(text)
    pos = [0]
    def read():
        t = tokens[pos[0]]
        pos[0] += 1
        if t == '(':
            lst = []
            while tokens[pos[0]] != ')':
                lst.append(read())
            pos[0] += 1
            return lst
        elif isinstance(t, tuple):
            return t
        else:
            raise ValueError(f"unexpected token {t}")
    exprs = []
    while pos[0] < len(tokens):
        exprs.append(read())
    return exprs

def A(s):
    """construct a bare atom token"""
    return ('atom', str(s))

def S(s):
    """construct a quoted string token"""
    return ('str', str(s))

def atom_val(t):
    if isinstance(t, tuple):
        return t[1]
    return t

def serialize(node, indent=0):
    pad = '  ' * indent
    if isinstance(node, list):
        if not node:
            return pad + '()'
        head = node[0]
        parts = [serialize_inline(head)]
        simple = True
        for child in node[1:]:
            if isinstance(child, list):
                simple = False
        if simple:
            for child in node[1:]:
                parts.append(serialize_inline(child))
            return pad + '(' + ' '.join(parts) + ')'
        else:
            out = pad + '(' + serialize_inline(head)
            for child in node[1:]:
                if isinstance(child, list):
                    out += '\n' + serialize(child, indent + 1)
                else:
                    out += ' ' + serialize_inline(child)
            out += ')'
            return out
    else:
        return pad + serialize_inline(node)

def serialize_inline(node):
    if isinstance(node, tuple):
        tag, val = node
        if tag == 'str':
            return '"' + val.replace('\\', '\\\\').replace('"', '\\"') + '"'
        return val
    if isinstance(node, list):
        return '(' + ' '.join(serialize_inline(c) for c in node) + ')'
    return str(node)

def find_all(node, tag):
    """recursively find all list nodes whose head atom == tag"""
    results = []
    if isinstance(node, list):
        if node and not isinstance(node[0], list):
            h = atom_val(node[0])
            if h == tag:
                results.append(node)
        for child in node:
            results.extend(find_all(child, tag))
    return results

def find_first(node, tag):
    r = find_all(node, tag)
    return r[0] if r else None

def get_symbol_block(root_exprs, symbol_name):
    """root_exprs = list of top-level exprs (kicad_symbol_lib ...). Return the
    (symbol "name" ...) node whose exact name matches."""
    lib = root_exprs[0]
    for child in lib:
        if isinstance(child, list) and child and atom_val(child[0]) == 'symbol':
            nm = atom_val(child[1])
            if nm == symbol_name:
                return child
    return None
