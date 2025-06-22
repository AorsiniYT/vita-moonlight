# Vita Moonlight Motion

This project is a PlayStation Vita port of Moonlight, with major improvements and updates.

## What's New

- **Automatic pairing fixed:** The pairing process is now more reliable and user-friendly.
- **Absolute Touch (beta):** Experimental support for absolute touch controls.
- **Floating keyboard:** The virtual keyboard now appears elevated, without covering the main screen.
- **Updated libraries:** All dependencies and libraries have been updated for better compatibility and stability.

---

## Credits

- Current improvements and maintenance: **AorsiniYT**
- Based on the work of:
  - [MetalfaceScout/vita-moonlight-motion](https://github.com/MetalfaceScout/vita-moonlight-motion)
  - [xyzz/vita-moonlight](https://github.com/xyzz/vita-moonlight)
  - [moonlight-stream/moonlight-common-c](https://github.com/moonlight-stream/moonlight-common-c)

---

## Build Requirements

- **VitaSDK** installed and configured on your system ([guide here](https://vitasdk.org/)).
- **Submodules updated:**
  ```sh
  git submodule update --init
  ```

---

## Quick Build

1. Install dependencies with [vdpm](https://github.com/vitasdk/vdpm) if you haven't already.
2. Make sure VitaSDK is installed and in your $PATH.
3. Run:
   ```sh
   ./makepsv
   ```
   This will generate a VPK file ready to install on your PS Vita.

---

## Manual Build (optional)

If you prefer to build manually:

```sh
# If you do git pull, make sure to update submodules first
 git submodule update --init
 mkdir build && cd build
 cmake ..
 make
```

---

## Assets

- Icon: [moonlight-stream][moonlight] project logo
- Livearea background: [Moonlight Reflection][reflection] (Public domain)

[moonlight]: https://github.com/moonlight-stream
[reflection]: http://www.publicdomainpictures.net/view-image.php?image=130014&picture=moonlight-reflection

---

## Contributing

1. Fork this repository
2. Write code
3. Send Pull Requests

---

Thanks to all contributors and the Moonlight community!
