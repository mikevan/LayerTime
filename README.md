<div align="center" markdown="1">
  <img src=".github/LilyGo_logo.png" alt="LilyGo logo" width="100"/>
</div>

<h1 align = "center">🌟LilyGoLib-PlatformIO🌟</h1>

# `1` Overview

* This repository demonstrates how [LilyGoLib](https://github.com/Xinyuan-LilyGO/LilyGoLib) uses [PlatformIO](https://platformio.org/)

# `2` Platformio IDE Quick Start

1. Install [Visual Studio Code](https://code.visualstudio.com/) and [Python](https://www.python.org/)
2. Search for the `PlatformIO` plugin in the `Visual Studio Code` extension and install it.
3. After the installation is complete, you need to restart `Visual Studio Code`
4. After restarting `Visual Studio Code`, select `File` in the upper left corner of `Visual Studio Code` -> `Open Folder` -> select the `LilyGoLib-PlatformIO` directory
5. Wait for the installation of third-party dependent libraries to complete
6. Click on the `platformio.ini` file, and in the `platformio` column
7. Select the board name you want to use in `default_envs` and uncomment it.
8. The default compiled sketch is [main.cpp](./src/main.cpp) in the src directory. If you need to compile an example in LilyGoLib, uncomment one of the lines src_dir = examples/xxxxx to enable it and make sure only one line is valid.
9. Click the (✔) symbol in the lower left corner to compile
10. Connect the board to the computer USB
11. Click (→) to upload firmware
12. Click (plug symbol) to monitor serial output

> \[!IMPORTANT]
>
> ⚠️ USB ports keep popping in and out?
>
> * T-Watch-S3 see [here](https://github.com/Xinyuan-LilyGO/LilyGoLib/blob/master/docs/lilygo-t-watch-s3.md#t-watch-s3-enter-download-mode)
> * T-Watch-S3-Plus see  [here](https://github.com/Xinyuan-LilyGO/LilyGoLib/blob/master/docs/lilygo-t-watch-s3-plus.md#t-watch-s3-plus-enter-download-mode)
> * T-Watch-Ultra see [here](https://github.com/Xinyuan-LilyGO/LilyGoLib/blob/master/docs/lilygo-t-watch-ultra.md#t-watch-s3-ultra-enter-download-mode)
> * T-LoRa-Pager see [here](https://github.com/Xinyuan-LilyGO/LilyGoLib/blob/master/docs/lilygo-t-lora-pager.md#t-lora-pager-enter-download-mode)
>
> 💠 Quick troubleshooting
> Write the factory [firmware](https://github.com/Xinyuan-LilyGO/LilyGoLib/tree/master/firmware) we provide for hardware diagnosis
