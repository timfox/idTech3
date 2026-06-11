#!/usr/bin/env bash
exec python3 "$(dirname "$0")/../../scripts/genetic_gan_decode.py" "$@"
