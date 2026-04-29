# DesktopDuplicateApp
DirectX 11 desktop duplication capture and image-view application

# Info
Windows C++ application for capturing the desktop through the D3D11 Desktop Duplication pipeline and displaying captured frames through the D3D11 image-view module.

This project wires together the shared Core utilities, the D3D11 duplicate engine, and the D3D11 image viewer into a small executable application. It initializes COM, starts the capture engine thread, receives captured frame callbacks, and updates the image view with the latest shared texture handle.

# Features
- D3D11 desktop duplication capture integration
- D3D11 shared texture display through D3D11ImageView
- Frame capture callback handling
- Capture thread startup and shutdown flow
- Console control handler for graceful shutdown
- Debug DirectX live-object reporting in debug builds
- Modular integration with shared Core and D3D11 engine modules

# Dependencies
- Core
- D3D11DuplicateEngine
- D3D11ImageView
- D3D11Engine
- D3D11UIFramework
- D3D11ImageIO
- D3D11EngineInterface
- Windows SDK / DirectX 11
- C++20
- MSVC v143 (Visual Studio 2022)

# Build Environment
- C++20
- MSVC v143 (Visual Studio 2022)
- Windows 10/11
- WindowsTargetPlatformVersion 10.0
- Debug / Release
- Win32 / x64

# Project Structure
- DesktopDuplicateApp/ : application source and Visual Studio project files
- Icons/ : SVG icon assets used by the UI/view modules
- Shaders/ : compiled shader objects used by the rendering/image pipeline
- DesktopDuplicateApp.sln : Visual Studio solution that references the application and shared modules

# Repository Layout
This project expects DesktopDuplicateApp and the shared Module projects to be placed under the same parent development root.

Example:

```text
Private Dev/
+-- Application/
|   +-- DesktopDuplicateApp/
+-- Module/
    +-- Core/
    +-- D3D11DuplicateEngine/
    +-- D3D11ImageView/
    +-- D3D11Engine/
    +-- D3D11UIFramework/
    +-- D3D11ImageIO/
    +-- D3D11EngineInterface/
```

The Visual Studio solution references the shared module projects through relative paths such as `../../Module/Core/Core/Core.vcxproj` and `../../Module/D3D11DuplicateEngine/D3D11DuplicateEngine/D3D11DuplicateEngine.vcxproj`.

# Run
Open `DesktopDuplicateApp.sln` with Visual Studio 2022, build the desired configuration, and run `DesktopDuplicateApp`.

While the app is running, type `quit` in the console window to stop the capture loop gracefully.

# Notes
- The shared Core and D3D11 modules are managed as sibling repositories/projects under the `Module` directory.
- The app initializes the capture engine on output index `0`.
- The default image-view window rectangle is initialized as `1920 x 900`.
- Debug builds report DirectX live objects during shutdown.
