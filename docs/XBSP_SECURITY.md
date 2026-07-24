# XBSP and BSPX security

Map files are untrusted input.

The shared parser uses subtraction-based range checks and overflow-checked
count/stride multiplication. It bounds legacy headers, extension directory
counts, native directory counts, names, directory record sizes, compression
identifiers, and all payload ranges. Duplicate names and overlapping payloads
are rejected.

External virtual paths must be relative, contain no empty, `.` or `..`
segments, contain no control characters, and use neither an absolute prefix
nor a drive-letter prefix. Archive/search-root resolution must repeat the
containment check after canonicalization.

Before codec integration, the loader must expose bounded settings for total
file size, external bytes, decompressed bytes per lump, total lump count,
entity count, and string-table size. A codec must allocate only after checking
the declared uncompressed size against those ceilings, then verify the exact
decoded length and content hash.

Reference validation is a second stage after byte-range validation. Node
children, leaf-surface ranges, model spans, visibility streams, patch
dimensions, string offsets, typed entities, and streaming dependencies must
all be checked before publishing a normalized world.

