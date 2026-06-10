#!/usr/bin/env python3
"""Train CA-FER on GCC-FER manifest (Algorithm 1 Phase 2)."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
import torch
from torch.utils.data import DataLoader, Subset
from sklearn.model_selection import StratifiedKFold
from tqdm import tqdm

from cafer.config import (
    BATCH_SIZE,
    EARLY_STOP_PATIENCE,
    FOCAL_ALPHA,
    FOCAL_GAMMA,
    LABEL_SMOOTHING,
    LEARNING_RATE,
    NUM_CULTURES,
)
from cafer.dataset import GccferManifestDataset, collate_batch
from cafer.focal_loss import focal_loss
from cafer.model import CAFER, CultureAgnosticFER
from metrics import unweighted_average_recall, weighted_average_recall


def stratified_labels(ds: GccferManifestDataset):
    return [expr for _, _, expr in ds.rows]


def evaluate(model, loader, device, use_global=False):
    model.eval()
    y_true = []
    y_pred = []
    with torch.no_grad():
        for pixels, cultures, labels in loader:
            pixels = pixels.to(device)
            cultures = cultures.to(device)
            labels = labels.to(device)
            if isinstance(model, CAFER):
                logits, _ = model(pixels, cultures, use_global=use_global)
            else:
                logits = model(pixels)
            pred = logits.argmax(dim=-1)
            y_true.extend(labels.cpu().tolist())
            y_pred.extend(pred.cpu().tolist())
    from cafer.config import NUM_EXPRESSIONS

    uar = unweighted_average_recall(y_true, y_pred, NUM_EXPRESSIONS)
    war = weighted_average_recall(y_true, y_pred, NUM_EXPRESSIONS)
    return uar, war


def train_fold(model, train_loader, val_loader, device, epochs: int, use_global: bool):
    opt = torch.optim.AdamW(model.parameters(), lr=LEARNING_RATE)
    best_uar = 0.0
    val_uar = 0.0
    val_war = 0.0
    patience = 0
    best_state = None

    for epoch in range(epochs):
        model.train()
        for pixels, cultures, labels in tqdm(train_loader, desc=f"epoch {epoch+1}", leave=False):
            pixels = pixels.to(device)
            cultures = cultures.to(device)
            labels = labels.to(device)
            opt.zero_grad(set_to_none=True)
            if isinstance(model, CAFER):
                logits, _ = model(pixels, cultures, use_global=use_global)
            else:
                logits = model(pixels)
            loss = focal_loss(logits, labels, FOCAL_GAMMA, FOCAL_ALPHA, LABEL_SMOOTHING)
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            opt.step()

        val_uar, val_war = evaluate(model, val_loader, device, use_global=use_global)
        if val_uar > best_uar:
            best_uar = val_uar
            best_state = {k: v.cpu().clone() for k, v in model.state_dict().items()}
            patience = 0
        else:
            patience += 1
            if patience >= EARLY_STOP_PATIENCE:
                break

    if best_state:
        model.load_state_dict(best_state)
    return best_uar, val_war


def main() -> None:
    parser = argparse.ArgumentParser(description="Train CA-FER")
    parser.add_argument("--manifest", required=True, help="GCC-FER CSV manifest")
    parser.add_argument("--priors", default="", help="cultural_priors.json from extract_au_priors.py")
    parser.add_argument("--output", default="checkpoints", help="Checkpoint directory")
    parser.add_argument("--epochs", type=int, default=20)
    parser.add_argument("--folds", type=int, default=5)
    parser.add_argument("--baseline", action="store_true", help="Train culture-agnostic ViViT only")
    parser.add_argument("--global-prior", action="store_true", help="Use global AU prior at train time")
    parser.add_argument("--device", default="auto")
    args = parser.parse_args()

    if args.device == "auto":
        device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    else:
        device = torch.device(args.device)

    ds = GccferManifestDataset(args.manifest)
    labels = stratified_labels(ds)
    skf = StratifiedKFold(n_splits=args.folds, shuffle=True, random_state=42)
    out_dir = Path(args.output)
    out_dir.mkdir(parents=True, exist_ok=True)

    fold_scores_uar = []
    fold_scores_war = []
    for fold, (train_idx, val_idx) in enumerate(skf.split(np.zeros(len(ds)), labels)):
        train_ds = Subset(ds, train_idx.tolist())
        val_ds = Subset(ds, val_idx.tolist())
        train_loader = DataLoader(train_ds, batch_size=BATCH_SIZE, shuffle=True, collate_fn=collate_batch)
        val_loader = DataLoader(val_ds, batch_size=BATCH_SIZE, shuffle=False, collate_fn=collate_batch)

        if args.baseline:
            model = CultureAgnosticFER().to(device)
        else:
            init_emb = None
            if args.priors and Path(args.priors).is_file():
                data = json.loads(Path(args.priors).read_text(encoding="utf-8"))
                init_emb = np.array(data["initial_embeddings"], dtype=np.float32)
            model = CAFER(culture_embeddings=torch.from_numpy(init_emb) if init_emb is not None else None)
            model = model.to(device)

        uar, war = train_fold(model, train_loader, val_loader, device, args.epochs, args.global_prior)
        fold_scores_uar.append(uar)
        fold_scores_war.append(war)
        ckpt = out_dir / f"{'baseline' if args.baseline else 'cafer'}_fold{fold}.pt"
        torch.save(model.state_dict(), ckpt)
        print(f"Fold {fold}: UAR={uar:.2f}% WAR={war:.2f}% -> {ckpt}")

    print(f"Mean UAR: {np.mean(fold_scores_uar):.2f}% | Mean WAR: {np.mean(fold_scores_war):.2f}%")


if __name__ == "__main__":
    main()
