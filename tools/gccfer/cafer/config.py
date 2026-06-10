"""GCC-FER / CA-FER configuration constants (paper §II–IV)."""

NUM_EXPRESSIONS = 7
NUM_CULTURES = 4
NUM_FRAMES = 16
INPUT_SIZE = 224
NUM_AUS = 20
AU_STATS = 4
EMBED_DIM = 128
LATENT_DIM = 768
DROPOUT = 0.3
FOCAL_GAMMA = 2.0
FOCAL_ALPHA = 1.0
LABEL_SMOOTHING = 0.1
LEARNING_RATE = 1e-5
BATCH_SIZE = 8
EARLY_STOP_PATIENCE = 7

EXPRESSIONS = [
    "angry",
    "disgust",
    "fear",
    "happy",
    "neutral",
    "sad",
    "surprise",
]

CULTURES = [
    "caucasian",
    "east_asian",
    "south_asian",
    "african",
]

IMAGENET_MEAN = (0.485, 0.456, 0.406)
IMAGENET_STD = (0.229, 0.224, 0.225)

VIVIT_MODEL = "google/vivit-b-16x2"  # factorised encoder variant
