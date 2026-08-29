def parse(text):
    i, n = 0, len(text)
    def rd():
        nonlocal i
        while i < n and text[i] in " \t\r\n": i += 1
        if text[i] == "(":
            i += 1; out = []
            while True:
                while i < n and text[i] in " \t\r\n": i += 1
                if text[i] == ")": i += 1; return out
                out.append(rd())
        if text[i] == '"':
            i += 1; buf = []
            while text[i] != '"':
                if text[i] == "\\": i += 1
                buf.append(text[i]); i += 1
            i += 1; return "".join(buf)
        j = i
        while i < n and text[i] not in ' \t\r\n()': i += 1
        return text[j:i]
    return rd()

def kids(node, tag):
    return [c for c in node if isinstance(c, list) and c and c[0] == tag]

def one(node, tag, default=None):
    k = kids(node, tag)
    return k[0][1] if k and len(k[0]) > 1 else default
