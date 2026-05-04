# Real-time rendering reading list (engine upgrade path)

Curated references for **image-quality upgrades** in an id Tech 3–style fork: practical papers with implementation detail, aligned with a suggested **integration order** (temporal stability and materials first; ocean last).

**JCGT links** below were verified against the journal’s `papers.json` routing (volume / issue / article index).

---

## Suggested integration order (whole-frame impact first)

1. **Temporal AA / supersampling** — stabilize shading and motion (foundation for SSR, fog history, etc.). See Advances **SIGGRAPH 2014** course: *High-Quality Temporal Supersampling* (UE4 TAA), [Advances 2014](https://advances.realtimerendering.com/s2014/).
2. **Specular AA** — reduce sparkle on glossy normal-mapped surfaces (pairs well with TAA). JCGT paper below.
3. **SSR** — screen-space reflections without hardware RT. JCGT paper below.
4. **Volumetric fog** — froxel-style participating media aligned with depth. SIGGRAPH **Advances 2014** talk + this repo’s Vulkan path (`r_volumetricFog`, `vk_volumetric_*`).
5. **Shadow filtering** — moment-based soft filtering. JCGT paper below.
6. **Weighted blended OIT** — particles, glass, smoke without exact sorting. JCGT paper below; see also `r_oit` / `SIGGRAPH_FEATURES_ROADMAP.md`.
7. **Stochastic light culling** — complements tiled/clustered lists when pushing many lights. JCGT paper below.
8. **Water / ocean** — only after the stack above; map-dependent.

---

## Core papers (with canonical links)

### 1. Efficient GPU Screen-Space Ray Tracing

**Why:** Production-oriented SSR / SSRT foundation with GLSL-oriented detail.

- **JCGT:** [Efficient GPU Screen-Space Ray Tracing](https://jcgt.org/published/0003/04/04/) (McGuire, Mara, Assarsson)

### 2. Volumetric fog (unified compute, atmospheric scattering)

**Why:** Reference for **game-style** volumetric fog: froxel volume, lights, integration with forward/deferred.

- **SIGGRAPH Advances 2014** (course talk, materials on course page):  
  Bartlomiej Wronski — *Volumetric Fog: Unified Compute Shader-Based Solution to Atmospheric Scattering*  
  [Advances in Real-Time Rendering — SIGGRAPH 2014](https://advances.realtimerendering.com/s2014/)

**Related (Frostbite volumetric lighting framework):**  
Sébastien Hillaire — *Towards Unified and Physically-Based Volumetric Lighting in Frostbite*  
[Advances in Real-Time Rendering — SIGGRAPH 2015](https://advances.realtimerendering.com/s2015/)

### 3. Stable geometric specular antialiasing (projected-space filtering)

**Why:** High-value **PBR stability** under motion (reduces shimmer on glossy normals).

- **JCGT:** [Stable Geometric Specular Antialiasing with Projected-Space NDF Filtering](https://jcgt.org/published/0010/02/02/) (Tokuyoshi, Kaplanyan)

### 4. Improved moment shadow maps

**Why:** Filterable shadows with good quality/cost tradeoffs vs. huge PCF kernels.

- **JCGT:** [Improved Moment Shadow Maps for Translucent Occluders, Soft Shadows and Single Scattering](https://jcgt.org/published/0006/01/03/) (Peters et al.)

### 5. Weighted blended order-independent transparency

**Why:** Practical OIT for layered transparency without exact depth sorting.

- **JCGT:** [Weighted Blended Order-Independent Transparency](https://jcgt.org/published/0002/02/09/) (McGuire, Bavoil)

### 6. Stochastic light culling

**Why:** Mitigates bias/artifacts in aggressive light culling when many dynamic lights matter.

- **JCGT:** [Stochastic Light Culling](https://jcgt.org/published/0005/01/02/) (Tokuyoshi, Harada)

---

## Water and ocean (secondary; map-specific)

### 7. An efficient method for real-time ocean simulation

Large-scale ocean surface / LOD-oriented approach (often used as a **game-ready** baseline vs. heavy CFD).

- **Springer LNCS chapter** (book: *Technologies for E-Learning and Digital Entertainment*):  
  Haogang Chen, Qicheng Li, Guoping Wang, Feng Zhou, Xiaohui Tang — *An Efficient Method for Real-Time Ocean Simulation*  
  DOI: [10.1007/978-3-540-73011-8_3](https://doi.org/10.1007/978-3-540-73011-8_3)

### 8. Realistic water volumes in real-time

**Why:** Volume between surfaces (absorption, refraction context), not only a flat ocean plane.

- **Typical citation:** Lionel Baboud, Xavier Décoret — presented at **Eurographics Workshop on Natural Phenomena** (often listed as 2006).  
  HAL mirror (PDF): [inria-00510227](https://inria.hal.science/inria-00510227/)  
  *Confirm the exact venue/year in your ACM DL or Eurographics record if you need a formal bibliography.*

### 9. Wave particles

**Why:** Secondary waves, interactions, augmenting a base heightfield.

- **ACM SIGGRAPH 2007:** Cem Yuksel, Donald H. House, John Keyser — *Wave Particles*  
  DOI: [10.1145/1275808.1276501](https://doi.org/10.1145/1275808.1276501)

### 10. Water wave packets

**Why:** Physically richer wave structure than simple summed Gerstner stacks; more R&D cost.

- **ACM Transactions on Graphics (SIGGRAPH 2017):** Stefan Jeschke, Chris Wojtan — *Water Wave Packets*  
  DOI: [10.1145/3072959.3073678](https://doi.org/10.1145/3072959.3073678)  
  Project page: [ISTA Visual Computing](https://visualcomputing.ist.ac.at/publications/2017/WWP/)

---

## Cross-links in this repository

| Topic | Where it lives |
|--------|----------------|
| OIT / WBOIT status | `docs/SIGGRAPH_FEATURES_ROADMAP.md` |
| Volumetric fog implementation notes | `docs/VOLUMETRIC_FOG_ENHANCEMENTS.md`, `src/renderers/vulkan/vk_volumetric_*.c` |
| Ocean citation hygiene (venues/titles) | `docs/OCEAN_RENDERING_REFERENCES.md` |

---

## Maintenance

When adding new references, prefer **exact titles + venue + year + DOI or JCGT path**; avoid paraphrasing course section names as paper titles.
