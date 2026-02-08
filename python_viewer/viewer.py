import tyro
import rerun as rr
import rerun.blueprint as rrb
import polars as pl
from tyro.conf import Positional
from tqdm import tqdm

from dataclasses import dataclass
from pathlib import Path
import subprocess
import tempfile
import atexit

@dataclass
class ViewerConfig:
    path_video: Positional[Path]
    path_parquet: Path | None = None
    max_duration_seconds: int | None = 600

    def __post_init__(self):
        assert self.path_video.exists(), "Video doesn't exist"
        
        # Convert to MP4 if needed (and optionally trim)
        self.path_mp4, self.temp_file = self._ensure_mp4(self.path_video, self.max_duration_seconds)
        
        # Register cleanup on exit if we created a temp file
        if self.temp_file is not None:
            atexit.register(self._cleanup)
        
        if self.path_parquet is None:
            self.path_parquet = self.path_video.with_suffix(".parquet")
        assert self.path_parquet.exists(), "Parquet annotation file doesn't exist"
    
    def _ensure_mp4(self, video_path: Path, max_duration_seconds: int | None = None) -> tuple[Path, object | None]:
        """
        Ensure we have an MP4 file. If input is already MP4 and no trimming needed, use it directly.
        Otherwise, convert/trim to temporary MP4.
        
        Args:
            video_path: Path to input video
            max_duration_seconds: Optional max duration in seconds (e.g., 600 for 10 min)
        
        Returns:
            tuple: (path_to_mp4, temp_file_object_or_none)
        """
        needs_conversion = video_path.suffix.lower() != '.mp4'
        needs_trimming = max_duration_seconds is not None
        
        # If no conversion or trimming needed, use original
        if not needs_conversion and not needs_trimming:
            print(f"Using MP4 file directly: {video_path.name}")
            return video_path, None
        
        # Need to convert and/or trim to MP4
        action = []
        if needs_conversion:
            action.append("converting")
        if needs_trimming:
            action.append(f"trimming to {max_duration_seconds}s")
        
        print(f"{video_path.name}: {' and '.join(action)}...")
        
        temp_file = tempfile.NamedTemporaryFile(suffix='.mp4', delete=False)
        temp_file.close()  # Close it so ffmpeg can write to it
        
        # Build ffmpeg command
        cmd = ['ffmpeg', '-i', str(video_path)]
        
        # Add duration limit if specified
        if max_duration_seconds is not None:
            cmd.extend(['-t', str(max_duration_seconds)])
        
        # Copy streams without re-encoding
        cmd.extend([
            '-c:v', 'copy',  # Copy video stream
            '-c:a', 'copy',  # Copy audio stream
            '-y',            # Overwrite output file if it exists
            temp_file.name
        ])
        
        subprocess.run(cmd, check=True, capture_output=True)
        
        print(f"Temporary MP4 created at: {temp_file.name}")
        return Path(temp_file.name), temp_file
    
    def _cleanup(self):
        """Clean up temporary MP4 file if one was created"""
        if hasattr(self, 'temp_file') and self.temp_file is not None:
            try:
                Path(self.temp_file.name).unlink(missing_ok=True)
                print(f"Cleaned up temporary file: {self.temp_file.name}")
            except Exception as e:
                print(f"Warning: Could not clean up temporary file: {e}")

MKB_KEYS = ['mouse_left', 'mouse_right', 'mouse_middle', 'cursor_visible', 'cursor_clipped', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'F1', 'F2', 'F3', 'F4', 'F5', 'F6', 'F7', 'F8', 'F9', 'F10', 'F11', 'F12', 'LSHIFT', 'RSHIFT', 'LCTRL', 'RCTRL', 'LALT', 'RALT', 'LEFT', 'UP', 'RIGHT', 'DOWN', 'SPACE', 'ENTER', 'ESCAPE', 'TAB', 'BACKSPACE', 'DELETE', 'INSERT', 'HOME', 'END', 'PAGEUP', 'PAGEDOWN', 'CAPSLOCK', 'NUMLOCK', 'SCROLLLOCK', 'PRINTSCREEN', 'PAUSE', 'MENU']

def mkb_viewer(cfg):
    rr.init("input-rec-mkb-viewer", spawn=True)

    # Log video as static data (logged once)
    video_asset = rr.AssetVideo(path=cfg.path_mp4)
    rr.log("video", video_asset, static=True)
    
    # Get video frame timestamps
    frame_timestamps_ns = video_asset.read_frame_timestamps_nanos()
    
    # Log VideoFrameReferences to sync video with timeline
    rr.send_columns(
        "video",
        indexes=[rr.TimeColumn("time", duration=1e-9 * frame_timestamps_ns)],
        columns=rr.VideoFrameReference.columns_nanos(frame_timestamps_ns),
    )

    # log actions 
    df = pl.read_parquet(cfg.path_parquet)

    if cfg.max_duration_seconds is not None:
        max_time_us = cfg.max_duration_seconds * 1_000_000
        max_index = df["time"].search_sorted(max_time_us)
        df = df[:max_index+1]
        print(f"Filtered annotations to {cfg.max_duration_seconds}s: {len(df)} rows")
    

    prev_row = None
    for row in tqdm(df.rows(named=True)):
        time = row["time"] / 1_000_000 # microseconds to seconds
        rr.set_time("time", duration=time)

        # log absolute mouse on top of video
        x, y = row["mouse_x"], row["mouse_y"]
        rr.log("video", rr.Points2D(positions=[[x, y]], radii=[6], colors=[[0, 255, 0]]))

        # log relative mouse events
        x_rel, y_rel = row["mouse_delta_x"], row["mouse_delta_y"]

        rr.log( # x, y axes
            "mouse_rel",
            rr.Arrows2D(
                origins=[[0, 50], [-100, 0], [0, 0]],
                vectors=[[0, -100], [200, 0], [x_rel, y_rel]],
                colors=[[127, 127, 127], [127, 127, 127], [251, 44, 54]],
                radii=[0.5,0.5,1],
            ),
        )

        if prev_row is not None:
            for key in MKB_KEYS:
                if key in {'mouse_left', 'mouse_right', 'mouse_middle'}:
                    color = (127, 34, 254)
                else:
                    color = (0, 166, 244)

                if prev_row[key] != row[key]:
                    if row[key]:
                        rr.log("event_log/key", rr.TextLog(f"{key} pressed", level=rr.TextLogLevel.INFO, color=color))
                    else:
                        rr.log("event_log/key", rr.TextLog(f"{key} released", level=rr.TextLogLevel.INFO, color=color))      

            for key in ['active_executable', 'active_window']:
                if prev_row[key] != row[key]:
                    rr.log("event_log/key", rr.TextLog(f"{key} changed: {row[key]}", level=rr.TextLogLevel.INFO, color=(251, 44, 54)))


        prev_row = row

    blueprint = rrb.Blueprint(
        
        rrb.Grid(
            rrb.Horizontal(
                contents=[
                    rrb.Spatial2DView(
                        origin="/video",
                        name="Video Player with mouse",
                        background=(0,0,0),
                        visual_bounds=rrb.VisualBounds2D(x_range=[0, 1920], y_range=[0, 1080]),
                    ),

                    rrb.Spatial2DView(
                        origin="/mouse_rel",
                        name="Relative mouse events",
                        background=(0,0,0),
                        visual_bounds=rrb.VisualBounds2D(x_range=[-100, 100], y_range=[-50, 50]),
                    ),
                ]
            ),
            rrb.TextLogView(
                origin="/event_log",
                name="Event Logs",
                columns=rrb.TextLogColumns(
                    timeline_columns=["time"],
                    text_log_columns=["body"],
                ),
                rows=rrb.TextLogRows(
                    filter_by_log_level=["INFO", "WARN", "ERROR"],
                ),
                format_options=rrb.TextLogFormat(
                    monospace_body=False,
                ),
            ),
            grid_columns=1
        ),
        collapse_panels=True,
    )

    rr.send_blueprint(blueprint)

def viewer(cfg):
    # Check if the parquet file is a gamepad of keyboard file
    df = pl.read_parquet(cfg.path_parquet)

    if "mouse_x" in df.columns:
        mkb_viewer(cfg)
    elif "DPAD_UP" in df.columns:
        raise NotImplementedError("Gamepad viewer not implemented")
    else:
        raise RuntimeError(f"Unknown parquet file format: {df.columns}")

def viewer_cli():
    cfg = tyro.cli(ViewerConfig)
    viewer(cfg) 

if __name__ == "__main__":
    viewer_cli()