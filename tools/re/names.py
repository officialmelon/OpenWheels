"""Helpers for splitting demangled C++ names into (class, method)."""

THUNK_PREFIXES = (
    "non-virtual thunk to ",
    "virtual thunk to ",
    "covariant return thunk to ",
)
DATA_PREFIXES = (
    "vtable for ",
    "typeinfo for ",
    "typeinfo name for ",
    "construction vtable for ",
    "VTT for ",
    "guard variable for ",
)


def _split_depth0(s, sep):
    """Split s on sep occurring at template/paren depth 0."""
    out, depth, cur, i = [], 0, [], 0
    while i < len(s):
        c = s[i]
        if c in "<(":
            depth += 1
        elif c in ">)":
            depth -= 1
        if depth == 0 and s.startswith(sep, i):
            out.append("".join(cur))
            cur = []
            i += len(sep)
            continue
        cur.append(c)
        i += 1
    out.append("".join(cur))
    return out


def qualified_name(dem):
    """Return (prefix, qualified function/data name without params, params)."""
    prefix = ""
    for p in THUNK_PREFIXES + DATA_PREFIXES:
        if dem.startswith(p):
            prefix = p.strip()
            dem = dem[len(p):]
            break
    dem = dem.replace("(anonymous namespace)", "anon_ns")
    # find first '(' at depth 0 that starts the parameter list
    depth = 0
    name_end = len(dem)
    i = 0
    while i < len(dem):
        c = dem[i]
        if c == "<":
            # 'operator<' / 'operator<<' are not template brackets
            if dem.endswith("operator", 0, i) or dem.endswith("operator<", 0, i):
                i += 1
                continue
            depth += 1
        elif c == ">":
            if dem.endswith("operator", 0, i) or dem.endswith("operator-", 0, i) or dem.endswith("operator>", 0, i):
                i += 1
                continue
            depth -= 1
        elif c == "(":
            if depth == 0:
                if dem.endswith("operator", 0, i) and dem[i:i + 2] == "()":
                    i += 2
                    continue
                name_end = i
                break
            depth += 1
        elif c == ")":
            depth -= 1
        i += 1
    name = dem[:name_end]
    params = dem[name_end:]
    # strip a leading return type (template functions): last depth-0 space
    parts = _split_depth0(name, " ")
    if len(parts) > 1:
        # keep 'operator new' style names intact
        joined = []
        for p in parts:
            if joined and joined[-1].endswith("operator"):
                joined[-1] += " " + p
            else:
                joined.append(p)
        name = joined[-1]
    return prefix, name, params


def class_and_method(dem):
    prefix, name, params = qualified_name(dem)
    comps = _split_depth0(name, "::")
    if len(comps) == 1:
        return prefix, "", comps[0], params
    return prefix, "::".join(comps[:-1]), comps[-1], params


def top_component(dem):
    prefix, name, params = qualified_name(dem)
    return _split_depth0(name, "::")[0]
