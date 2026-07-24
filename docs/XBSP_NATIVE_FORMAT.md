# XBSP native format version 1

All integers are little endian. Serialization is field-by-field; compiler
structure padding is never written.

## Header

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 4 | ASCII `XBSP` magic |
| 4 | 4 | version, currently 1 |
| 8 | 4 | header size, minimum 104 |
| 12 | 4 | flags |
| 16 | 8 | directory offset |
| 24 | 4 | directory entry count |
| 28 | 4 | directory entry size, minimum 72 |
| 32 | 8 | complete file size |
| 40 | 8 | native file hash |
| 48 | 16 | map UUID |
| 64 | 8 | compatibility-base hash |
| 72 | 32 | reserved, zero in version 1 |

Version 1 accepts at most 256 entries, satisfying the minimum capacity of 128
while imposing a bounded allocation and validation cost.

## Directory record

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 24 | null-terminated stable name |
| 24 | 2 | schema version |
| 26 | 2 | flags |
| 28 | 4 | compression (`none`, `LZ4`, or `Zstandard`) |
| 32 | 8 | file offset |
| 40 | 8 | stored size |
| 48 | 8 | uncompressed size |
| 56 | 8 | element count |
| 64 | 8 | content hash |

Payloads and the directory may not overlap. Payloads may not overlap each
other. Unknown records can therefore be retained as exact stored byte ranges.
Directory ordering is lexicographic by name in deterministic writers; zero
padding and reserved fields are required.

Compression identifiers are defined, while actual compression codecs and
external-lump manifests remain follow-on implementation work. A compressed
entry is never treated as decoded bytes before its hash, output size, and
configured decompression ceiling have been validated.

