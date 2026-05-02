# DesktopDuplicateApp

A Windows desktop capture viewer built on the DXGI Desktop Duplication API.

This repository contains a small executable app that captures desktop frames through `D3D11DuplicateEngine` and displays them by passing the captured D3D11 shared texture handle to `D3D11ImageView`. The capture engine, rendering engine, and image viewer live in sibling module projects; this app wires those modules together into a runnable capture loop.

## Features

- Desktop capture through the DXGI Desktop Duplication API
- GPU-side frame handoff through D3D11 shared textures
- Dedicated capture thread startup and shutdown
- Frame callback handling for forwarding the latest shared handle to the viewer
- Win32 message loop combined with console input handling
- Graceful shutdown through `quit`, `Ctrl+C`, or console close events
- DirectX live-object reporting in debug builds

## Runtime Flow

```text
DesktopDuplicateApp
  |
  +-- Initialize D3D11ImageView
  |     +-- Create the display D3D11 device/window
  |
  +-- Initialize D3D11DuplicateEngine
  |     +-- Prepare capture for output index 0
  |     +-- Register the frame callback
  |     +-- Start the capture thread
  |
  +-- OnFrameCallback
        +-- Read the latest shared texture handle from SharedCaptureData
        +-- Call D3D11ImageView::UpdateSharedTexture(handle)
```

The current callback includes a placeholder for adding NVENC encoding later. The active display path forwards the shared texture handle to the image viewer.

## Build Environment

- Windows 10/11
- Visual Studio 2022
- MSVC v143
- Windows SDK 10.0
- C++20
- DirectX 11 / DXGI
- Supported configurations: `Debug|Win32`, `Release|Win32`, `Debug|x64`, `Release|x64`

## Dependencies

The solution and project reference the following sibling module projects through relative paths.

- `Core`
- `D3D11DuplicateEngine`
- `D3D11ImageView`
- `D3D11Engine`
- `D3D11UIFramework`
- `D3D11ImageIO`
- `D3D11EngineInterface`

The app project directly references `Core`, `D3D11DuplicateEngine`, and `D3D11ImageView`. The other D3D11 modules are included in the solution and are used by the image-view and rendering pipeline.

## Repository Layout

This project expects the application repository and shared module repositories to be placed under the same development root.

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

For example, the solution uses relative paths such as:

```text
..\..\Module\Core\Core\Core.vcxproj
..\..\Module\D3D11DuplicateEngine\D3D11DuplicateEngine\D3D11DuplicateEngine.vcxproj
..\..\Module\D3D11ImageView\D3D11ImageView\D3D11ImageView.vcxproj
```

## Running

1. Open `DesktopDuplicateApp.sln` in Visual Studio 2022.
2. Select the desired configuration/platform. `Debug|x64` or `Release|x64` is typically recommended.
3. Build and run `DesktopDuplicateApp`.
4. Type `quit` in the console or press `Ctrl+C` to stop the app.

## Project Structure

```text
DesktopDuplicateApp/
+-- DesktopDuplicateApp.sln
+-- DesktopDuplicateApp/
|   +-- main.cpp
|   +-- DesktopDuplicateApp.h
|   +-- DesktopDuplicateApp.vcxproj
+-- Icons/
+-- Shaders/
```

- `main.cpp`: Initializes COM, registers the console control handler, and runs the app lifecycle.
- `DesktopDuplicateApp.h`: Initializes the image viewer and capture engine, runs the message loop, handles frame callbacks, and performs shutdown.
- `Icons/`: SVG icon resources used by the UI/view modules.
- `Shaders/`: Compiled shader object resources used by the rendering/image pipeline.

## Implementation Notes

- The default capture target is output index `0`.
- The default image-view rectangle is initialized to `1920 x 900`.
- `SharedCaptureData` protects the shared handle and new-frame flag with an `SRWLOCK`.
- Debug builds call `Core::DirectX::DxReportLiveObjects()` during shutdown.
