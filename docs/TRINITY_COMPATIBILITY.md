# Trinity compatibility and credits

This engine independently integrates compatibility ideas demonstrated by Ernie Miller's GPL-2.0 Trinity ecosystem. Source comments identify each affected implementation site rather than implying that idTech3 authored those ideas.

## Integrated ideas

* **Trinity Engine:** a server advertises `vr_support=1`; clients identify active VR with the value-gated `vr=1` userinfo key; only those clients use 32-bit `usercmd.buttons`. Legacy clients retain the 16-bit wire path.
* **Trinity Engine:** seekable, complete-state `.tvd` recordings and the map-spanning `TVL1` live stream are implemented by the server and client TV subsystems.
* **Trinity game module:** the optional damage-plum presentation setting is exposed as `cg_damagePlums`. Its bind-friendly `tv_forward`, `tv_backward`, `tv_next`, and `tv_prev` playback controls are also supported; `cl_tvSkip` selects the skip interval.
* **Trinity VR:** headset use is treated as a first-class launch profile. Opening an OpenHMD device advertises VR automatically; closing it restores the flatscreen marker.

These compatibility features do not bundle Trinity game data or Quake III assets. Trinity-specific game behavior still requires compatible game modules. The local OpenHMD path supplies orientation and software stereo; it is not a claim of full OpenXR controller, haptics, weapon-wheel, or room-scale parity with Trinity VR.

## Upstream credit

* [Trinity Engine](https://github.com/ernie/trinity-engine) — VR negotiation and TrinityVision design.
* [Trinity](https://github.com/ernie/trinity) — gameplay and presentation ideas.
* [Trinity VR](https://github.com/ernie/trinity-vr) — VR-focused launch and usability ideas.

## TrinityVision playback bindings

```text
bind leftarrow  tv_backward
bind rightarrow tv_forward
bind uparrow    tv_next
bind downarrow  tv_prev
seta cl_tvSkip 10
```

The skip commands apply to seekable `.tvd` files. Live `TVL1` streams support viewpoint switching but intentionally reject backward or forward seeks.
