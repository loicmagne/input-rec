from pathlib import Path
import tqdm
import cv2
import polars as pl
import numpy as np


def align(
    path_video: str,
    path_inputs: str,
):
    # Load video
    cap = cv2.VideoCapture(path_video)
    if not cap.isOpened():
        raise ValueError("Unable to open video")

    # Load inputs
    inputs = pl.read_csv(path_inputs)
    inputs_aligned = []
    i = 0

    COUNT_OPENCV = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    COUNT_MANUAL = 0
    ERRORS = []

    with tqdm.tqdm(total=int(cap.get(cv2.CAP_PROP_FRAME_COUNT))) as pbar:
        while True:
            ret, frame = cap.read()
            if not ret:
                break

            t_micro = (
                cap.get(cv2.CAP_PROP_POS_MSEC) * 1000
            )  # Frame timestamp in microseconds
            while inputs["time"][i] < t_micro:  # Find the closest timestamp in inputs
                i += 1

            # Pick the closest timestamp between the two neighbors
            prev = inputs["time"][i - 1]
            next = inputs["time"][i]
            row = (
                inputs.row(i - 1, named=True)
                if abs(prev - t_micro) < abs(next - t_micro)
                else inputs.row(i, named=True)
            )
            inputs_aligned.append(row)

            ERRORS.append(abs(row["time"] - t_micro))
            COUNT_MANUAL += 1
            pbar.update(1)
    cap.release()
    inputs_aligned = pl.DataFrame(inputs_aligned)
    inputs_aligned.write_parquet(Path(path_inputs).with_suffix(".parquet"))

    print(f"Number of frames (OpenCV): {COUNT_OPENCV}")
    print(f"Number of frames (Manual): {COUNT_MANUAL}")
    print(f"Mean error: {np.mean(ERRORS)}")
    print(f"Max error: {np.max(ERRORS)}")
    print(f"Min error: {np.min(ERRORS)}")
    print(inputs_aligned)


def main_cli():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("video", help="Path to video file")
    parser.add_argument("inputs", help="Path to inputs .csv file")
    args = parser.parse_args()

    align(args.video, args.inputs)
