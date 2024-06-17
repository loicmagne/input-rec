from pathlib import Path
import argparse
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
            while (
                inputs["time"][i] < t_micro and i < inputs.shape[0] - 1
            ):  # Find the closest timestamp in inputs
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

    print(inputs_aligned)
    print(f"Number of frames (OpenCV): {COUNT_OPENCV}")
    print(f"Number of frames (Manual): {COUNT_MANUAL}")
    print(f"Mean error: {np.mean(ERRORS)}µs")
    print(f"Max error: {np.max(ERRORS)}µs")
    print(f"Min error: {np.min(ERRORS)}µs")


def viz(
    path_video: str,
    path_inputs: str,
):
    """
    Run the video and display the inputs at the bottom of the screen
    Pressing 'q' will exit the video
    Pressing space will pause the video
    Pressing 'j' will go back 1 frame
    Pressing 'k' will go forward 1 frame
    Pressing 'l' will go forward 10 frames
    Pressing 'h' will go back 10 frames
    """

    # Load video
    cap = cv2.VideoCapture(path_video)
    if not cap.isOpened():
        raise ValueError("Unable to open video")

    # Load inputs
    df = pl.read_parquet(path_inputs)

    N = df.shape[0]
    current_frame = 0
    paused = False

    def get_frame(cap, frame_number):
        """Get the current frame of the video capture"""
        cap.set(cv2.CAP_PROP_POS_FRAMES, frame_number)
        ret, frame = cap.read()
        if not ret:
            print("Failed to retrieve frame.")
            return None
        return frame

    while cap.isOpened():
        frame = get_frame(cap, current_frame)
        frame_data = df.row(current_frame, named=True)
        if frame is None:
            break

        frame = cv2.resize(frame, (800, 600))
        new_width = frame.shape[1] + 200
        frame_show = np.zeros((frame.shape[0], new_width, 3), dtype=np.uint8)
        frame_show[:, 200:] = frame

        # Display each key value pair on a new line
        for i, line in enumerate(frame_data.items()):
            cv2.putText(
                frame_show,
                f"{line[0].strip()}: {line[1]}",
                (10, 580 - 20 * i),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.5,
                (0, 255, 0),
                1,
                cv2.LINE_AA,
            )
        cv2.imshow("Video", frame_show)

        # Keyboard controls
        key = cv2.waitKey(25) & 0xFF
        if key == ord("q"):
            break
        elif key == ord(" "):
            paused = not paused
        elif key == ord("j") and current_frame > 0:
            current_frame -= 1
        elif key == ord("k") and current_frame < N - 1:
            current_frame += 1
        elif key == ord("l") and current_frame < N - 10:
            current_frame += 10
        elif key == ord("h") and current_frame > 9:
            current_frame -= 10

        if not paused:
            current_frame += 1

    # Cleanup
    cap.release()
    cv2.destroyAllWindows()


def cli_align(args):
    align(args.video, args.inputs)


def cli_viz(args):
    viz(args.video, args.inputs)


def cli_main():
    parser = argparse.ArgumentParser(description="Input recording tools")

    # Create a subparsers object
    subparsers = parser.add_subparsers(
        dest="command", required=True, help="Pick a command"
    )

    # align command
    align_parser = subparsers.add_parser("align", help="Align inputs to video")
    align_parser.add_argument("video", help="Path to video file")
    align_parser.add_argument("inputs", help="Path to inputs .csv file")
    align_parser.set_defaults(func=cli_align)

    # viz command
    viz_parser = subparsers.add_parser("viz", help="Visualize inputs on video")
    viz_parser.add_argument("video", help="Path to video file")
    viz_parser.add_argument("inputs", help="Path to inputs .parquet file")
    viz_parser.set_defaults(func=cli_viz)

    args = parser.parse_args()
    args.func(args)
