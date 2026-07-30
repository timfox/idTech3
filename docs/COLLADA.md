# Native Collada

The Vulkan renderer registers `.dae` files directly through `RE_RegisterModel`.
Static mesh geometry is loaded in-process by `renderers/common/tr_model_mesh_import.c`;
it does not require an offline conversion to PMD/PSA.

Supported native render path:

- `<triangles>` and triangle-only `<polylist>`
- `VERTEX` → `POSITION` source resolution
- Optional `TEXCOORD` input
- Optional primitive `material` symbol as the surface shader name
- In-memory MD3-style renderer surface finalization, the same mesh-import path used by STL and other lightweight interchange formats

The legacy `modules/collada` asset module still exists for tools and PMD/PSA-style asset workflows. That module is separate from renderer-native `.dae` model registration.
