# Third-party licensing boundary

The engine and first-party modules in this repository are distributed under
the GNU General Public License, version 2, as described in [`LICENSE.md`](../LICENSE.md).

The repository also contains independent third-party components. Their source
and license terms remain separate and must not be relabeled as GPL-2.0:

| Component | Location | License evidence |
| --- | --- | --- |
| SDL | `third_party/src/sdl/`, FetchContent at build time | Upstream license in the component; prefer the pinned FetchContent source over committing a cache |
| Duktape | `third_party/duktape/` | `third_party/duktape/LICENSE.txt` |
| Dear ImGui / cimgui | `third_party/src/cimgui/` | `third_party/src/cimgui/LICENSE`, `third_party/src/cimgui/imgui/LICENSE.txt` |
| ENet | `third_party/src/enet/` | `third_party/src/enet/LICENSE` |
| GLM | `third_party/src/glm/` | `third_party/src/glm/copying.txt` |
| libpng, zlib, Ogg, Opus, Opusfile, Vorbis, FLAC, zstd | `third_party/src/` | The corresponding upstream `LICENSE`/`COPYING` files |
| FreeUSD | `third_party/FreeUSD/` | `third_party/FreeUSD/LICENSE` |
| EnTT | `third_party/include/entt/` | `third_party/include/entt/LICENSE` |
| cflux2 and related assets | `third_party/src/cflux2/` | `third_party/src/cflux2/LICENSE` |

Some components contain multiple licenses for different files or optional
subcomponents. Distribution must preserve their notices and comply with the
license that applies to each copied or linked component. The top-level GPL
notice does not relicense third-party code.

Do not add proprietary game data, SDK credentials, deployment credentials,
generated build trees, benchmark logs, crash dumps, or local machine paths to
this repository. Keep non-redistributable assets outside the repository or in
a separately access-controlled distribution.

This file is an engineering boundary document, not a legal opinion. Before a
release, verify the complete dependency inventory and corresponding notices.
