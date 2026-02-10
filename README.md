# input-rec

## Installation

Check the [release page](https://github.com/loicmagne/input-rec/releases) and download the installer for your OS

## How to use

Add the `Input Recording` source:

![image](https://github.com/user-attachments/assets/4ec81a31-17bc-4859-95c1-c0a4368bd1d1)

That's it! Now every time you record, a `.parquet` file will be created with the same name as your recording, containing the state of your gamepad recorded at 500Hz. This will also work if you disconnect your gamepad midway, connect a new one etc.

## Visualize recorded actions

Install the Python viewer with
```bash
pip install -e python_viewer
```

Then use it with
```bash
irv <path/to/video_file.mp4> # this command will automatically look for the associated .parquet file
irv <path/to/video_file.mp4> --path-parquet <path/to/annotations.parquet> # or explicitely provide the path to the parquet annotations 
```

You should see a [rerun](https://github.com/rerun-io/rerun) window like this one: 
![image](https://github.com/user-attachments/assets/b8a9a280-b1fb-4076-a427-1c8dc0dc0bad)

## TODOs:
- [ ] Support mouse/keyboard recordings
- [ ] Add a GUI to select output format and input device to be recorded
- [ ] Add a visual indicator to verify that recording is working
- [ ] Fix macOS
