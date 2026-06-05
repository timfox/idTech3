Put your licensed game data here
================================

**Bare window (skeleton):** this folder includes **z_minimal_bootstrap.pk3** (tiny GPL zip: **default.cfg** + **gameinfo.txt**) so the engine’s filesystem gate passes with no retail files. Rebuild **idtech3_demo.pk3** from this repo; it ships **native UI** in **vm/** plus **fonts**, **HUD gfx**, and **scripts/demo_bootstrap.shader** so the renderer initializes cleanly.

**Play maps / full menus:** add the **full** game .pk3 set from a **compatible title you own** (maps, **qagame**, retail UI if you want stock menus). See **docs/COMPATIBILITY.md** in the engine repo.

Copy or symlink your game’s folder contents into this directory.

- If your install uses a folder named **base**, copy everything from that **base** folder *into* this folder (so `.pk3` files sit directly here), OR rename your folder to `base` and use the parent directory as IDTECH3_DEMO_ROOT (see main README).

- If your install uses a **different base directory name**, either:
  - Rename or symlink it to **base** next to **idtech3_demo**, or
  - Set **DEMO_BASE_DIR** in **local.env** (see **demo_skeleton.env.example**).

Do not commit commercial `.pk3` files to git.

Next: build **idtech3_demo.pk3** (see **../README.md**), run **../setup_demo_layout.sh** from the repo, or copy **idtech3_demo.pk3** into **../idtech3_demo/**.
