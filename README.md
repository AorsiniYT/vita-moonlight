![Moonlight Vita Logo](resources/img/demo_icon.jpg)

# Moonlight Vita

Moonlight client for PlayStation Vita that allows you to stream games from your PC to your handheld console.

## 🚀 Features

- User interface optimized for touchscreen and PS Vita controls
- Low latency for a smooth gaming experience
- Integration with Moonlight Game Streaming
- UI based on Borealis, a modern user interface framework

## 📦 Requirements

- PlayStation Vita with firmware 3.60 or higher
- Custom Firmware installed (H-encore, Henkaku, etc.)
- PC with NVIDIA GameStream compatible GPU
- Moonlight application installed on the host PC

## 🛠️ Installation

1. Make sure you have Vitashell installed on your PS Vita
2. Copy the `moonlight_vita.vpk` file to your PS Vita
3. Install it using Vitashell
4. Launch the application from LiveArea

## 🔧 Building

### Prerequisites

#### To build for PS Vita:
- **Git**
  - Latest stable version recommended
  - [Download Git](https://git-scm.com/downloads)

#### To build for Windows (on Ubuntu):
```bash
# Install cross-compilation tools for Windows
sudo apt update
sudo apt install -y g++-mingw-w64-x86-64 gcc-mingw-w64-x86-64 mingw-w64-tools

# During installation, select "posix" when prompted
# (Select option 1: x86_64-w64-mingw32)

# Install additional required tools
sudo apt install -y cmake make pkg-config
```

### Building for PS Vita

1. Clone the repository:
   ```bash
   git clone https://github.com/AorsiniYT/Moonlight-Vita.git -b vita
   cd Moonlight-Vita
   ```

2. Run the build script:
   ```bash
   chmod +x makepsv
   ./makepsv
   ```

   The generated VPK file will be available in the `cmake-build-psv/` folder.

### Building for Windows

1. Make sure you have installed all the dependencies mentioned above.

2. Run the build script:
   ```bash
   chmod +x makewin
   ./makewin
   ```

   The generated executable will be available in the `build_mingw/` folder.

3. Follow the on-screen instructions to install and run the application on your PS Vita.

## 🎮 Usage

1. Make sure your PC is on and Moonlight is configured
2. Start the application on your PS Vita
3. Select your PC from the list of available devices
4. Enjoy game streaming!

## 📚 Additional Documentation

For more information about PS Vita development with Borealis, see:

- [Borealis guide for PS Vita](https://github.com/xfangfang/borealis/wiki/PS-Vita) - Detailed setup and development instructions
- [Advanced Borealis documentation](https://gist.github.com/xfangfang/305da139721ad4e96d7a9d9a1a550a9d) - Technical information about the framework

## 📝 Notes

- For best performance, a wired network connection on the PC is recommended
- Adjust the quality settings in the app according to your connection
- Some games may require additional configuration on the host PC

## 🤝 Contributing

Contributions are welcome. Please read the contribution guidelines before submitting changes.

## 📄 License

This project is licensed under the Apache 2.0 License. See the [LICENSE](LICENSE) file for details.

## Credits

- Thanks to [Natinusala](https://github.com/natinusala), [xfangfang](https://github.com/xfangfang) and [XITRIX](https://github.com/XITRIX) for [borealis](https://github.com/xfangfang/borealis), the UI framework that makes this project possible.

- Special thanks to:
  - [MetalfaceScout](https://github.com/MetalfaceScout) for the tap implementation and important fixes for the Vita version.
  - [xyzz](https://github.com/xyzz) for the original Moonlight port for PS Vita, which laid the foundation for this client.
  - The [moonlight-stream](https://github.com/moonlight-stream/moonlight-common-c) team for their library, which enables connectivity with Sunshine and GeForce Experience.

---

Developed with ❤️ for the PS Vita community