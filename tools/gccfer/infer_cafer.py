#!/usr/bin/env python3
"""Run CA-FER inference on a video or frame directory."""

from __future__ import annotations

import argparse
from pathlib import Path

import torch

from cafer.config import CULTURES, EXPRESSIONS, NUM_CULTURES
from cafer.dataset import load_frame_directory, load_video_frames, normalize_frames
from cafer.model import CAFER


def parse_culture(name: str) -> int | None:
    n = name.strip().lower()
    if n in ("global", "unknown", "none"):
        return None
    if n in CULTURES:
        return CULTURES.index(n)
    aliases = {
        "cauc": "caucasian",
        "east": "east_asian",
        "south": "south_asian",
        "africa": "african",
    }
    if n in aliases:
        return CULTURES.index(aliases[n])
    raise ValueError(f"Unknown culture: {name}")


def main() -> None:
    parser = argparse.ArgumentParser(description="CA-FER inference")
    parser.add_argument("--input", required=True, help="Video file or directory of frames")
    parser.add_argument("--checkpoint", default="", help="Model checkpoint (.pt)")
    parser.add_argument("--demo-random", action="store_true", help="Allow random weights (no checkpoint)")
    parser.add_argument("--culture", default="global", help="Culture label or global")
    parser.add_argument("--device", default="auto")
    args = parser.parse_args()

    if args.device == "auto":
        device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    else:
        device = torch.device(args.device)

    path = args.input
    if Path(path).is_dir():
        frames = load_frame_directory(path)
    else:
        frames = load_video_frames(path)

    pixel_values = normalize_frames(frames).unsqueeze(0).to(device)
    culture_id = parse_culture(args.culture)
    use_global = culture_id is None

    model = CAFER().to(device)
    ckpt = args.checkpoint
    if not ckpt or not Path(ckpt).is_file():
        if not args.demo_random:
            raise SystemExit("Provide --checkpoint or --demo-random for inference")
        print("Warning: demo-random mode — random weights")
    else:
        state = torch.load(ckpt, map_location=device)
        model.load_state_dict(state, strict=False)
        print(f"Loaded checkpoint {ckpt}")

    model.eval()
    with torch.no_grad():
        if use_global:
            logits, _ = model(pixel_values, use_global=True)
            culture_label = "global"
        else:
            cultures = torch.tensor([culture_id], dtype=torch.long, device=device)
            logits, _ = model(pixel_values, cultures, use_global=False)
            culture_label = CULTURES[culture_id]

    probs = torch.softmax(logits, dim=-1).squeeze(0).cpu().numpy()
    pred = int(probs.argmax())
    print(f"Culture conditioning: {culture_label}")
    print(f"Predicted expression: {EXPRESSIONS[pred]} ({probs[pred]*100:.1f}%)")
    print("All probabilities:")
    for name, p in zip(EXPRESSIONS, probs):
        print(f"  {name:10s} {p*100:5.1f}%")


if __name__ == "__main__":
    main()
