Put your licensed game data here
================================

**Bare window (skeleton):** rebuild **idtech3_demo.pk3** from this repo; it ships a **native UI stub** in **vm/** so the client can open without **ui.qvm**.

**Play maps / full menus:** add the **full** game .pk3 set from a compatible title (maps, **qagame**, retail UI if you want stock menus).

Copy or symlink your game’s folder contents into this directory.

- If your game uses a folder named **base**, copy everything from that **base** folder *into* this folder (so `.pk3` files sit directly here), OR rename your folder to `base` and use the parent directory as IDTECH3_DEMO_ROOT (see main README).

- If your game uses **baseq3**, either:
  - Rename or symlink it to **base** next to **idtech3_demo**, or
  - Set **DEMO_BASE_DIR=baseq3** in **local.env** (see **demo_skeleton.env.example**).

Do not commit commercial `.pk3` files to git.

Next: build **idtech3_demo.pk3** (see **../README.md**), run **../setup_demo_layout.sh** from the repo, or copy **idtech3_demo.pk3** into **../idtech3_demo/**.
