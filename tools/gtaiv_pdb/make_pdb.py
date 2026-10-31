"""
Writes a PDB holding one public symbol per function of a 32 bit PE image, matched to the image's
CodeView record (GUID + age), so dbghelp based tools (Tracy, debuggers) name the image's functions.

usage: make_pdb.py <image.exe> <functions.json> <out_dir>
functions.json: {"imagebase": int, "functions": [[va, size, name], ...]}
"""
import json
import os
import struct
import sys

BLOCK_SIZE = 4096
IPHR_HASH = 4096
STREAM_INFO, STREAM_TPI, STREAM_DBI, STREAM_IPI, STREAM_NAMES, STREAM_GLOBALS, STREAM_PUBLICS, STREAM_SYMRECS, STREAM_SECHDR = range(1, 10)


def align(n, a=4):
    return (n + a - 1) & ~(a - 1)


def hash_v1(data):
    """the PDB name hash (LLVM hashStringV1 / Microsoft LHashPbCb)"""
    r = 0
    n = len(data)
    whole = n - n % 4
    for i in range(0, whole, 4):
        r ^= struct.unpack_from('<I', data, i)[0]
    p, rem = whole, n % 4
    if rem >= 2:
        r ^= struct.unpack_from('<H', data, p)[0]
        p += 2
        rem -= 2
    if rem == 1:
        r ^= data[p]
    r |= 0x20202020
    r ^= r >> 11
    return (r ^ (r >> 16)) & 0xFFFFFFFF


def read_image(path):
    """section headers (raw and parsed), RSDS guid / age / pdb name and the file header timestamp"""
    d = open(path, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    nsec, timestamp = struct.unpack_from('<HI', d, pe + 6)
    opt_size = struct.unpack_from('<H', d, pe + 20)[0]
    opt = pe + 24
    if struct.unpack_from('<H', d, opt)[0] != 0x10B:
        sys.exit('only PE32 images are supported')

    sec_off = opt + opt_size
    raw_headers = d[sec_off:sec_off + nsec * 40]
    sections = [struct.unpack_from('<8sIIIIIIHHI', raw_headers, i * 40) for i in range(nsec)]

    def file_offset(rva):
        for s in sections:
            va, raw_size, raw_ptr = s[2], s[3], s[4]
            if va <= rva < va + max(s[1], raw_size):
                return rva - va + raw_ptr
        return None

    dbg_rva, dbg_size = struct.unpack_from('<II', d, opt + 96 + 6 * 8)
    base = file_offset(dbg_rva)
    for i in range(dbg_size // 28):
        typ, size, rva, ptr = struct.unpack_from('<IIII', d, base + i * 28 + 12)
        if typ == 2 and d[ptr:ptr + 4] == b'RSDS':
            guid = d[ptr + 4:ptr + 20]
            age = struct.unpack_from('<I', d, ptr + 20)[0]
            pdb_name = d[ptr + 24:ptr + size].split(b'\0')[0].decode('ascii', 'replace')
            return raw_headers, sections, guid, age, pdb_name, timestamp
    sys.exit('image has no RSDS CodeView record')


def section_of(rva, sections):
    """1 based section index and offset of an rva"""
    for i, s in enumerate(sections):
        if s[2] <= rva < s[2] + max(s[1], s[3]):
            return i + 1, rva - s[2]
    return 0, 0


def build_symbol_records(functions, sections, imagebase):
    """S_PUB32 records: the record stream and (stream offset, name, segment, segment offset) per record"""
    stream = bytearray()
    entries = []
    for va, _size, name in functions:
        seg, seg_off = section_of(va - imagebase, sections)
        if not seg:
            continue
        name_bytes = name.encode('utf-8')[:4000]
        body = struct.pack('<HIIH', 0x110E, 2, seg_off, seg) + name_bytes + b'\0'  # S_PUB32, cvpsfFunction
        total = align(2 + len(body))
        entries.append((len(stream), name_bytes, seg, seg_off))
        stream += struct.pack('<H', total - 2) + body + b'\0' * (total - 2 - len(body))
    return bytes(stream), entries


def build_gsi_hash(entries):
    """GSI hash table (shared by globals and publics): records bucketed by name hash"""
    buckets = [[] for _ in range(IPHR_HASH)]
    for offset, name, _seg, _off in entries:
        buckets[hash_v1(name) % IPHR_HASH].append((offset, name))

    records = []
    bitmap = [0] * ((IPHR_HASH + 32) // 32)
    bucket_starts = []
    for i, bucket in enumerate(buckets):
        if not bucket:
            continue
        bucket.sort(key=lambda e: (len(e[1]), e[1].lower(), e[0]))
        bitmap[i // 32] |= 1 << (i % 32)
        bucket_starts.append(len(records) * 12)
        records.extend(offset for offset, _name in bucket)

    header = struct.pack('<IIII', 0xFFFFFFFF, 0xEFFE0000 + 19990810, len(records) * 8, (len(bitmap) + len(bucket_starts)) * 4)
    data = b''.join(struct.pack('<II', offset + 1, 1) for offset in records)
    data += b''.join(struct.pack('<I', w) for w in bitmap)
    data += b''.join(struct.pack('<I', o) for o in bucket_starts)
    return header + data


def build_publics(entries):
    gsi = build_gsi_hash(entries)
    addr_map = sorted(entries, key=lambda e: (e[2], e[3], e[1]))
    header = struct.pack('<IIIIHHII', len(gsi), len(addr_map) * 4, 0, 0, 0, 0, 0, 0)
    return header + gsi + b''.join(struct.pack('<I', e[0]) for e in addr_map)


def build_type_stream():
    """empty TPI / IPI stream"""
    return struct.pack('<IIIIIHHIIiIiIiI', 20040203, 56, 0x1000, 0x1000, 0, 0xFFFF, 0xFFFF, 4, 0x3FFFF, 0, 0, 0, 0, 0, 0)


def build_info(guid, age, timestamp):
    """PDB info stream, its named stream map holds /names"""
    names = b'/names\0'
    capacity = 8
    bucket = (hash_v1(b'/names') & 0xFFFF) % capacity
    data = struct.pack('<III', 20000404, timestamp, age) + guid
    data += struct.pack('<I', len(names)) + names
    data += struct.pack('<II', 1, capacity)          # entries, capacity
    data += struct.pack('<II', 1, 1 << bucket)       # present bit vector
    data += struct.pack('<I', 0)                     # deleted bit vector
    data += struct.pack('<II', 0, STREAM_NAMES)      # "/names" -> stream
    data += struct.pack('<I', 20140508)              # feature: VC140
    return data


def build_names():
    """empty /names string table"""
    return struct.pack('<III', 0xEFFEEFFE, 1, 1) + b'\0' + struct.pack('<III', 1, 0, 0)


def build_dbi(sections, age):
    section_contribs = struct.pack('<I', 0xEFFE0000 + 19970605)

    section_map = struct.pack('<HH', len(sections) + 1, len(sections) + 1)
    for i, s in enumerate(sections):
        ch = s[9]
        flags = 0x8 | 0x100  # AddressIs32Bit, IsSelector
        flags |= 0x1 if ch & 0x40000000 else 0
        flags |= 0x2 if ch & 0x80000000 else 0
        flags |= 0x4 if ch & 0x20000000 else 0
        section_map += struct.pack('<HHHHHHII', flags, 0, 0, i + 1, 0xFFFF, 0xFFFF, 0, s[1])
    section_map += struct.pack('<HHHHHHII', 0x208, 0, 0, len(sections) + 1, 0xFFFF, 0xFFFF, 0, 0xFFFFFFFF)

    file_info = struct.pack('<HH', 0, 0)
    debug_header = [0xFFFF] * 11
    debug_header[5] = STREAM_SECHDR
    debug_header = struct.pack('<11H', *debug_header)

    header = struct.pack('<iIIHHHHHHiiiiiIiiHHI',
                         -1, 19990903, age,
                         STREAM_GLOBALS, 0x8E00, STREAM_PUBLICS, 0, STREAM_SYMRECS, 0,
                         0, len(section_contribs), len(section_map), len(file_info), 0, 0,
                         len(debug_header), 0, 0, 0x14C, 0)
    return header + section_contribs + section_map + file_info + debug_header


def write_msf(path, streams):
    """MSF 7.0 container: block 0 super block, blocks 1 and 2 free page maps"""
    next_block = [3]

    def alloc(count):
        first = next_block[0]
        next_block[0] += count
        return list(range(first, first + count))

    stream_blocks = [alloc((len(s) + BLOCK_SIZE - 1) // BLOCK_SIZE) for s in streams]
    directory = struct.pack('<I', len(streams))
    directory += b''.join(struct.pack('<I', len(s)) for s in streams)
    directory += b''.join(struct.pack('<I', b) for blocks in stream_blocks for b in blocks)
    dir_blocks = alloc((len(directory) + BLOCK_SIZE - 1) // BLOCK_SIZE)
    block_map = alloc(1)[0]
    num_blocks = next_block[0]
    if num_blocks >= BLOCK_SIZE or len(dir_blocks) * 4 > BLOCK_SIZE:
        sys.exit('symbol data too large for a single free page map interval')

    out = bytearray(num_blocks * BLOCK_SIZE)

    def put(block, data):
        out[block * BLOCK_SIZE:block * BLOCK_SIZE + len(data)] = data

    put(0, struct.pack('<32sIIIIII', b'Microsoft C/C++ MSF 7.00\r\n\x1aDS\0\0\0', BLOCK_SIZE, 1, num_blocks, len(directory), 0, block_map))

    fpm = bytearray(b'\xff' * BLOCK_SIZE)
    for b in range(num_blocks):
        fpm[b // 8] &= ~(1 << (b % 8)) & 0xFF
    put(1, fpm)
    put(2, fpm)

    for data, blocks in zip(streams, stream_blocks):
        for i, b in enumerate(blocks):
            put(b, data[i * BLOCK_SIZE:(i + 1) * BLOCK_SIZE])
    for i, b in enumerate(dir_blocks):
        put(b, directory[i * BLOCK_SIZE:(i + 1) * BLOCK_SIZE])
    put(block_map, b''.join(struct.pack('<I', b) for b in dir_blocks))

    with open(path, 'wb') as f:
        f.write(out)


def main():
    image_path, functions_path, out_dir = sys.argv[1:4]
    raw_headers, sections, guid, age, pdb_name, timestamp = read_image(image_path)
    export = json.load(open(functions_path, encoding='utf-8'))

    symrecs, entries = build_symbol_records(export['functions'], sections, export['imagebase'])
    streams = [b''] * 10
    streams[STREAM_INFO] = build_info(guid, age, timestamp)
    streams[STREAM_TPI] = build_type_stream()
    streams[STREAM_DBI] = build_dbi(sections, age)
    streams[STREAM_IPI] = build_type_stream()
    streams[STREAM_NAMES] = build_names()
    streams[STREAM_GLOBALS] = build_gsi_hash([])
    streams[STREAM_PUBLICS] = build_publics(entries)
    streams[STREAM_SYMRECS] = symrecs
    streams[STREAM_SECHDR] = raw_headers

    out_path = os.path.join(out_dir, os.path.basename(pdb_name.replace(chr(92), '/')))
    write_msf(out_path, streams)
    print(f'{out_path}: {len(entries)} public symbols, age {age}, guid {guid.hex()}')


if __name__ == '__main__':
    main()
