"""
Joystick recognition, training, live inference, and held-out evaluation.
"""

from pathlib import Path
import csv
import json
import math

import joblib
import numpy as np

from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import (
    accuracy_score,
    balanced_accuracy_score,
    confusion_matrix,
)
from sklearn.neighbors import KNeighborsClassifier
from sklearn.preprocessing import StandardScaler


DIRECTIONS = (
    "center", "north", "northeast", "east", "southeast",
    "south", "southwest", "west", "northwest",
)

MOTIONS = (
    "still", "horizontal", "vertical", "clockwise", "anticlockwise"
)

POSITION_LABELS = DIRECTIONS
MOTION_LABELS = MOTIONS

POSITION_WINDOW = 5
MOTION_WINDOW = 100
MOTION_TRAIN_STRIDE = 50
MOTION_EVAL_STRIDE = 50
ORDER_POINTS = 32


def normalize(raw_xy, calibration):
    """
    Convert raw joystick coordinates into the calibrated coordinate system.

    +X = east
    +Y = north
    """

    x, y = float(raw_xy[0]), float(raw_xy[1])

    if calibration is None:
        raise ValueError("Calibration is required")

    if "center_x" in calibration:
        cx = calibration["center_x"]
        cy = calibration["center_y"]
        nx = calibration["north_x"]
        ny = calibration["north_y"]
        ex = calibration["east_x"]
        ey = calibration["east_y"]
    else:
        center = calibration["center"]
        north = calibration["north"]
        east = calibration["east"]

        if isinstance(center, dict):
            cx, cy = center["x"], center["y"]
            nx, ny = north["x"], north["y"]
            ex, ey = east["x"], east["y"]
        else:
            cx, cy = center
            nx, ny = north
            ex, ey = east

    east_x = ex - cx
    east_y = ey - cy
    north_x = nx - cx
    north_y = ny - cy

    determinant = east_x * north_y - east_y * north_x

    if abs(determinant) < 1e-9:
        raise ValueError("Calibration axes are degenerate")

    dx = x - cx
    dy = y - cy

    east_component = (
        dx * north_y - dy * north_x
    ) / determinant

    north_component = (
        east_x * dy - east_y * dx
    ) / determinant

    return np.array([
        np.clip(east_component, -1.15, 1.15),
        np.clip(north_component, -1.15, 1.15),
    ], dtype=float)


def _as_xy(window):
    """
    Convert a window into an Nx2 normalized numpy array.
    """

    values = []

    for sample in window:
        if isinstance(sample, dict):
            if "normalized" in sample:
                values.append(sample["normalized"])
            elif "x_norm" in sample and "y_norm" in sample:
                values.append([sample["x_norm"], sample["y_norm"]])
            elif "x" in sample and "y" in sample:
                values.append([sample["x"], sample["y"]])
            elif "x_raw" in sample and "y_raw" in sample:
                values.append([sample["x_raw"], sample["y_raw"]])
            else:
                raise ValueError("Sample has no usable X/Y values")
        else:
            values.append(sample)

    array = np.asarray(values, dtype=float)

    if array.ndim != 2 or array.shape[1] != 2:
        raise ValueError("Expected an Nx2 window")

    return array


def position_features(window):
    """
    Features for the current joystick position.

    The live interface uses the latest five samples. Mean and spread
    make the feature reasonably insensitive to individual ADC readings.
    """

    xy = _as_xy(window)

    if len(xy) < POSITION_WINDOW:
        raise ValueError("Five samples are required")

    xy = xy[-POSITION_WINDOW:]

    mean = np.mean(xy, axis=0)
    std = np.std(xy, axis=0)
    last = xy[-1]
    delta = xy[-1] - xy[0]

    return np.concatenate([
        mean,
        std,
        last,
        delta,
    ]).astype(float)


def baseline_motion_features(window):
    """
    Required eight statistical motion features:

        min X, max X, mean X, std X,
        min Y, max Y, mean Y, std Y

    These describe how much each axis moved but do not retain the order
    in which those values occurred. A clockwise and anticlockwise circle
    can therefore have almost identical statistics.
    """

    xy = _as_xy(window)

    return np.array([
        np.min(xy[:, 0]),
        np.max(xy[:, 0]),
        np.mean(xy[:, 0]),
        np.std(xy[:, 0]),
        np.min(xy[:, 1]),
        np.max(xy[:, 1]),
        np.mean(xy[:, 1]),
        np.std(xy[:, 1]),
    ], dtype=float)


def _resample_path(xy, count=ORDER_POINTS):
    """
    Resample a trajectory to a fixed number of points by time/index.

    This retains movement order while allowing trials to have slightly
    different effective speeds.
    """

    if len(xy) < 2:
        raise ValueError("At least two samples are required")

    old_t = np.linspace(0.0, 1.0, len(xy))
    new_t = np.linspace(0.0, 1.0, count)

    x = np.interp(new_t, old_t, xy[:, 0])
    y = np.interp(new_t, old_t, xy[:, 1])

    return np.column_stack([x, y])


def ordered_motion_features(window):
    """
    Order-aware motion representation.

    This representation is intentionally insensitive to where a circular
    movement starts. It uses:
      - X/Y velocity sequence for horizontal vs. vertical sweeps
      - speed and radius over time
      - signed angular change (dtheta) to distinguish clockwise from
        anticlockwise
      - summary statistics for scale and stillness

    The absolute starting angle is not used as a feature, so a circle can
    begin at a different joystick position during evaluation.
    """

    xy = _as_xy(window)
    path = _resample_path(xy)

    velocity = np.diff(path, axis=0)
    speed = np.hypot(velocity[:, 0], velocity[:, 1])
    radius = np.hypot(path[:, 0], path[:, 1])

    angles = np.unwrap(
        np.arctan2(path[:, 1], path[:, 0])
    )
    dtheta = np.diff(angles)

    # Signed area-like term between adjacent samples. For a circle around
    # the origin, its sign follows the turning direction and is rotation
    # invariant.
    signed_turn = (
        path[:-1, 0] * path[1:, 1]
        - path[:-1, 1] * path[1:, 0]
    )

    summary = np.array([
        np.mean(path[:, 0]),
        np.mean(path[:, 1]),
        np.std(path[:, 0]),
        np.std(path[:, 1]),
        np.ptp(path[:, 0]),
        np.ptp(path[:, 1]),
        np.mean(speed),
        np.std(speed),
        np.mean(radius),
        np.std(radius),
        np.sum(dtheta),
        np.mean(np.abs(dtheta)),
    ], dtype=float)

    features = np.concatenate([
        velocity.ravel(),
        speed,
        radius,
        dtheta,
        signed_turn,
        summary,
    ])

    return features.astype(float)


def _load_csv(path, calibration):
    rows = []

    with Path(path).open(
        "r",
        newline="",
        encoding="utf-8",
    ) as file:
        reader = csv.DictReader(file)

        for row in reader:
            raw = [
                float(row["x_raw"]),
                float(row["y_raw"]),
            ]

            xy = normalize(raw, calibration)

            rows.append(xy)

    return np.asarray(rows, dtype=float)


def _trial_path(trial):
    path = trial.get("_csv_path")

    if path is None:
        raise ValueError(
            f"Trial {trial.get('trial_id')} has no _csv_path"
        )

    return Path(path)


def _motion_windows(xy, stride=MOTION_TRAIN_STRIDE):
    """
    Create 100-sample (~2 second) movement windows.

    Training uses a 50-sample stride to get more examples from each required
    six-second recording. Evaluation also uses the required 50-sample stride.
    All windows remain inside their original recording.
    """

    if len(xy) < MOTION_WINDOW:
        return []

    stride = int(stride)
    if stride <= 0:
        raise ValueError("Motion-window stride must be positive")

    return [
        xy[start:start + MOTION_WINDOW]
        for start in range(
            0,
            len(xy) - MOTION_WINDOW + 1,
            stride,
        )
    ]


def _position_windows(xy):
    """
    Use five-sample windows from a held-position recording.
    """

    windows = []

    step = POSITION_WINDOW

    for start in range(
        0,
        len(xy) - POSITION_WINDOW + 1,
        step,
    ):
        windows.append(
            xy[start:start + POSITION_WINDOW]
        )

    return windows


def _model_score(model, features):
    """
    Return a simple [0,1] confidence-like score.
    """

    if hasattr(model, "predict_proba"):
        probabilities = model.predict_proba(
            np.asarray(features).reshape(1, -1)
        )[0]

        return float(np.max(probabilities))

    return None


def train_models(training_trials, calibration):
    """
    Fit all three required models using training recordings only.

    Returns a serializable bundle:
      - position classifier
      - scaled 3-NN motion baseline
      - order-aware Random Forest motion classifier
      - metadata
    """

    position_x = []
    position_y = []

    baseline_x = []
    baseline_y = []

    ordered_x = []
    ordered_y = []

    for trial in training_trials:
        if not trial.get("accepted", False):
            continue

        path = _trial_path(trial)
        xy = _load_csv(path, calibration)

        position_label = trial["position"]
        motion_label = trial["motion"]

        # Position model: ONLY the nine held-position recordings.
        # The four movement recordings are intentionally excluded because
        # their backend position label is "center"; including them would
        # overweight the center class and violate the assignment.
        if motion_label == "still":
            for window in _position_windows(xy):
                position_x.append(
                    position_features(window)
                )
                position_y.append(position_label)

        # Motion models: use all 13 behaviors.
        for window in _motion_windows(xy, MOTION_TRAIN_STRIDE):
            baseline_x.append(
                baseline_motion_features(window)
            )
            baseline_y.append(motion_label)

            ordered_x.append(
                ordered_motion_features(window)
            )
            ordered_y.append(motion_label)

    if not position_x:
        raise ValueError("No position training samples found")

    if not baseline_x:
        raise ValueError("No motion training samples found")

    # Position classifier.
    position_scaler = StandardScaler()
    position_features_scaled = position_scaler.fit_transform(
        np.asarray(position_x)
    )

    position_model = KNeighborsClassifier(
        n_neighbors=3,
        weights="distance",
    )

    position_model.fit(
        position_features_scaled,
        position_y,
    )

    # Required scaled 3-NN motion baseline.
    motion_scaler = StandardScaler()
    baseline_scaled = motion_scaler.fit_transform(
        np.asarray(baseline_x)
    )

    motion_baseline = KNeighborsClassifier(
        n_neighbors=3,
        weights="distance",
    )

    motion_baseline.fit(
        baseline_scaled,
        baseline_y,
    )

    # Order-aware classifier.
    ordered_scaler = StandardScaler()
    ordered_scaled = ordered_scaler.fit_transform(
        np.asarray(ordered_x)
    )

    motion_ordered = RandomForestClassifier(
        n_estimators=200,
        random_state=42,
        class_weight="balanced",
        n_jobs=-1,
    )

    motion_ordered.fit(
        ordered_scaled,
        ordered_y,
    )

    # Training can use all CPU cores, but live prediction processes one
    # sample at a time. Switching to one worker after fitting avoids the
    # joblib/scikit-learn parallelism overhead and related warnings during
    # continuous live inference. This does not retrain or change the
    # learned forest.
    motion_ordered.n_jobs = 1

    bundle = {
        "version": 1,
        "position": {
            "scaler": position_scaler,
            "model": position_model,
        },
        "motion_baseline": {
            "scaler": motion_scaler,
            "model": motion_baseline,
        },
        "motion_ordered": {
            "scaler": ordered_scaler,
            "model": motion_ordered,
        },
        "metadata": {
            "position_labels": list(
                position_model.classes_
            ),
            "motion_labels": list(
                motion_baseline.classes_
            ),
            "position_window": POSITION_WINDOW,
            "motion_window": MOTION_WINDOW,
            "order_points": ORDER_POINTS,
            "motion_train_stride": MOTION_TRAIN_STRIDE,
            "motion_eval_stride": MOTION_EVAL_STRIDE,
            "training_trials": sum(
                1 for t in training_trials
                if t.get("accepted", False)
            ),
            "position_training_trials": sum(
                1 for t in training_trials
                if t.get("accepted", False) and t.get("motion") == "still"
            ),
            "motion_training_windows": len(baseline_x),
        },
    }

    return bundle


def predict_live(history, bundle):
    """
    Predict position from the latest five samples and motion from the
    latest two-second / 100-sample window.
    """

    if bundle is None:
        return {
            "position": "not trained",
            "motion": "not trained",
        }

    if len(history) < POSITION_WINDOW:
        return {
            "position": "warming up",
            "motion": "warming up",
        }

    xy = _as_xy(history)

    # Position.
    position_window = xy[-POSITION_WINDOW:]

    pf = position_features(position_window)

    position_scaled = bundle["position"]["scaler"].transform(
        pf.reshape(1, -1)
    )

    position_model = bundle["position"]["model"]

    position = position_model.predict(
        position_scaled
    )[0]

    position_score = _model_score(
        position_model,
        position_scaled[0],
    )

    result = {
        "position": str(position),
        "position_score": position_score,
    }

    # Motion requires two seconds.
    if len(xy) < MOTION_WINDOW:
        result["motion"] = "warming up"
        return result

    motion_window = xy[-MOTION_WINDOW:]

    baseline_features = baseline_motion_features(
        motion_window
    )

    baseline_scaled = bundle[
        "motion_baseline"
    ]["scaler"].transform(
        baseline_features.reshape(1, -1)
    )

    baseline_model = bundle[
        "motion_baseline"
    ]["model"]

    ordered_features = ordered_motion_features(
        motion_window
    )

    ordered_scaled = bundle[
        "motion_ordered"
    ]["scaler"].transform(
        ordered_features.reshape(1, -1)
    )

    ordered_model = bundle[
        "motion_ordered"
    ]["model"]

    # The order-aware model is the primary live motion prediction.
    motion = ordered_model.predict(
        ordered_scaled
    )[0]

    motion_score = _model_score(
        ordered_model,
        ordered_scaled[0],
    )

    baseline_motion = baseline_model.predict(
        baseline_scaled
    )[0]

    baseline_score = _model_score(
        baseline_model,
        baseline_scaled[0],
    )

    result.update({
        "motion": str(motion),
        "motion_score": motion_score,
        "motion_baseline": str(baseline_motion),
        "motion_baseline_score": baseline_score,
    })

    return result


def save_evaluation_plots(results, output_dir):
    """
    Save the required movement-model comparison plot plus both confusion
    matrices. Each plot is generated from the frozen evaluation results.
    """
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt

    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    labels = results["motion_baseline"]["labels"]
    baseline_cm = np.asarray(
        results["motion_baseline"]["confusion_matrix"],
        dtype=float,
    )
    ordered_cm = np.asarray(
        results["motion_ordered"]["confusion_matrix"],
        dtype=float,
    )

    # One comparison plot: per-class recall (row-wise diagonal / row sum)
    # for the two movement models.
    baseline_recall = np.divide(
        np.diag(baseline_cm),
        baseline_cm.sum(axis=1),
        out=np.zeros(len(labels), dtype=float),
        where=baseline_cm.sum(axis=1) > 0,
    )
    ordered_recall = np.divide(
        np.diag(ordered_cm),
        ordered_cm.sum(axis=1),
        out=np.zeros(len(labels), dtype=float),
        where=ordered_cm.sum(axis=1) > 0,
    )

    x = np.arange(len(labels))
    width = 0.36

    fig, ax = plt.subplots(figsize=(10, 5))
    b1 = ax.bar(x - width / 2, baseline_recall, width, label="Statistical 3-NN")
    b2 = ax.bar(x + width / 2, ordered_recall, width, label="Order-aware RF")

    ax.set_ylabel("Per-class accuracy")
    ax.set_xlabel("Movement class")
    ax.set_title("Movement Model Comparison on Held-out Evaluation Data")
    ax.set_xticks(x)
    ax.set_xticklabels(labels, rotation=20)
    ax.set_ylim(0, 1.05)
    ax.legend()

    for bars in (b1, b2):
        for bar in bars:
            height = bar.get_height()
            ax.annotate(
                f"{height:.2f}",
                xy=(bar.get_x() + bar.get_width() / 2, height),
                xytext=(0, 3),
                textcoords="offset points",
                ha="center",
                va="bottom",
                fontsize=8,
            )

    fig.tight_layout()
    comparison_path = output_dir / "movement_model_comparison.png"
    fig.savefig(comparison_path, dpi=160)
    plt.close(fig)

    def save_cm(cm, title, filename):
        fig, ax = plt.subplots(figsize=(7, 6))
        image = ax.imshow(cm)
        ax.set_title(title)
        ax.set_xlabel("Predicted")
        ax.set_ylabel("True")
        ax.set_xticks(np.arange(len(labels)))
        ax.set_yticks(np.arange(len(labels)))
        ax.set_xticklabels(labels, rotation=25, ha="right")
        ax.set_yticklabels(labels)

        for i in range(len(labels)):
            for j in range(len(labels)):
                ax.text(j, i, str(int(cm[i, j])), ha="center", va="center")

        fig.colorbar(image, ax=ax)
        fig.tight_layout()
        path = output_dir / filename
        fig.savefig(path, dpi=160)
        plt.close(fig)
        return path

    baseline_path = save_cm(
        baseline_cm,
        "Statistical Baseline Confusion Matrix",
        "motion_baseline_confusion.png",
    )
    ordered_path = save_cm(
        ordered_cm,
        "Order-aware Model Confusion Matrix",
        "motion_ordered_confusion.png",
    )

    return {
        "comparison_plot": str(comparison_path),
        "motion_baseline_confusion_plot": str(baseline_path),
        "motion_ordered_confusion_plot": str(ordered_path),
    }


def evaluate_models(evaluation_trials, bundle):
    """
    Evaluate frozen models on the held-out evaluation trials.

    No fitting occurs here.
    """

    position_true = []
    position_pred = []

    baseline_true = []
    baseline_pred = []

    ordered_true = []
    ordered_pred = []

    for trial in evaluation_trials:
        if not trial.get("accepted", False):
            continue

        xy = _load_csv(
            _trial_path(trial),
            bundle["calibration"],
        )

        position_label = trial["position"]
        motion_label = trial["motion"]

        # Position evaluation: ONLY the nine held-position evaluation
        # recordings. The four movement recordings are not position tests;
        # their backend position label is "center" only for bookkeeping.
        if motion_label == "still":
            position_predictions = []

            for window in _position_windows(xy):
                features = position_features(window)

                scaled = bundle["position"][
                    "scaler"
                ].transform(
                    features.reshape(1, -1)
                )

                prediction = bundle["position"][
                    "model"
                ].predict(scaled)[0]

                position_predictions.append(
                    prediction
                )

            if position_predictions:
                counts = {}

                for prediction in position_predictions:
                    counts[prediction] = (
                        counts.get(prediction, 0) + 1
                    )

                final_position = max(
                    counts,
                    key=counts.get,
                )

                position_true.append(position_label)
                position_pred.append(final_position)

        # Motion: evaluate each two-second window.
        for window in _motion_windows(xy, MOTION_EVAL_STRIDE):
            baseline_features = (
                baseline_motion_features(window)
            )

            baseline_scaled = bundle[
                "motion_baseline"
            ]["scaler"].transform(
                baseline_features.reshape(1, -1)
            )

            baseline_prediction = bundle[
                "motion_baseline"
            ]["model"].predict(
                baseline_scaled
            )[0]

            ordered_features = (
                ordered_motion_features(window)
            )

            ordered_scaled = bundle[
                "motion_ordered"
            ]["scaler"].transform(
                ordered_features.reshape(1, -1)
            )

            ordered_prediction = bundle[
                "motion_ordered"
            ]["model"].predict(
                ordered_scaled
            )[0]

            baseline_true.append(motion_label)
            baseline_pred.append(
                baseline_prediction
            )

            ordered_true.append(motion_label)
            ordered_pred.append(
                ordered_prediction
            )

    position_labels = list(
        bundle["position"]["model"].classes_
    )

    motion_labels = list(
        bundle["motion_baseline"]["model"].classes_
    )

    results = {
        "evaluated_observations": {
            "position": len(position_true),
            "motion": len(baseline_true),
        },
        "position": {
            "accuracy": float(
                accuracy_score(
                    position_true,
                    position_pred,
                )
            ),
            "balanced_accuracy": float(
                balanced_accuracy_score(
                    position_true,
                    position_pred,
                )
            ),
            "labels": position_labels,
            "confusion_matrix": confusion_matrix(
                position_true,
                position_pred,
                labels=position_labels,
            ).tolist(),
        },
        "motion_baseline": {
            "accuracy": float(
                accuracy_score(
                    baseline_true,
                    baseline_pred,
                )
            ),
            "balanced_accuracy": float(
                balanced_accuracy_score(
                    baseline_true,
                    baseline_pred,
                )
            ),
            "labels": motion_labels,
            "confusion_matrix": confusion_matrix(
                baseline_true,
                baseline_pred,
                labels=motion_labels,
            ).tolist(),
        },
        "motion_ordered": {
            "accuracy": float(
                accuracy_score(
                    ordered_true,
                    ordered_pred,
                )
            ),
            "balanced_accuracy": float(
                balanced_accuracy_score(
                    ordered_true,
                    ordered_pred,
                )
            ),
            "labels": motion_labels,
            "confusion_matrix": confusion_matrix(
                ordered_true,
                ordered_pred,
                labels=motion_labels,
            ).tolist(),
        },
    }

    return results
