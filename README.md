# input-rec

## Installation

Check the [release page](https://github.com/loicmagne/input-rec/releases) and download the installer for your OS

## How to use

Add the `Input Recording` source:

![image](https://github.com/user-attachments/assets/4ec81a31-17bc-4859-95c1-c0a4368bd1d1)

That's it! Now every time you record, a `.parquet` file will be created with the same name as your recording, containing the state of your gamepad recorded at 500Hz. This will also work if you disconnect your gamepad midway, connect a new one etc.

## TODOs:
- [ ] Support keyboard recordings
- [ ] Add a GUI to select output format and input device to be recorded
- [ ] Add a visual indicator to verify that recording is working 
