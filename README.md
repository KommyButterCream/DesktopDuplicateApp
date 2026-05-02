# DesktopDuplicateApp

DXGI Desktop Duplication API 기반의 Windows 데스크톱 캡처 뷰어 애플리케이션입니다.

이 저장소는 `D3D11DuplicateEngine`으로 데스크톱 프레임을 캡처하고, 캡처 결과로 생성된 D3D11 shared texture handle을 `D3D11ImageView`에 전달해 화면에 표시하는 작은 실행 앱입니다. 캡처 엔진, 렌더링 엔진, 이미지 뷰어는 sibling module 프로젝트로 분리되어 있고, 이 앱은 해당 모듈들을 조립해 실제 캡처 루프를 실행합니다.

## 주요 기능

- DXGI Desktop Duplication API 기반 데스크톱 캡처
- D3D11 shared texture를 통한 GPU-side 프레임 전달
- 별도 캡처 스레드 시작/종료
- 프레임 캡처 callback에서 최신 shared handle을 뷰어에 반영
- Win32 message loop와 console input을 함께 처리하는 실행 루프
- `quit`, `Ctrl+C`, 콘솔 종료 이벤트를 통한 graceful shutdown
- Debug 빌드에서 DirectX live object 리포트 출력

## 동작 구조

```text
DesktopDuplicateApp
  |
  +-- D3D11ImageView 초기화
  |     +-- 표시용 D3D11 device/window 생성
  |
  +-- D3D11DuplicateEngine 초기화
  |     +-- output index 0 대상 캡처 준비
  |     +-- frame callback 등록
  |     +-- 캡처 스레드 시작
  |
  +-- OnFrameCallback
        +-- SharedCaptureData에서 최신 shared texture handle 확인
        +-- D3D11ImageView::UpdateSharedTexture(handle) 호출
```

현재 앱의 callback에는 NVENC 인코딩을 붙일 수 있는 위치가 주석으로 남아 있습니다. 실제 표시 경로는 shared texture handle을 이미지 뷰어로 넘기는 방식입니다.

## 빌드 환경

- Windows 10/11
- Visual Studio 2022
- MSVC v143
- Windows SDK 10.0
- C++20
- DirectX 11 / DXGI
- 지원 구성: `Debug|Win32`, `Release|Win32`, `Debug|x64`, `Release|x64`

## 의존 모듈

솔루션과 프로젝트는 아래 sibling module 프로젝트를 상대 경로로 참조합니다.

- `Core`
- `D3D11DuplicateEngine`
- `D3D11ImageView`
- `D3D11Engine`
- `D3D11UIFramework`
- `D3D11ImageIO`
- `D3D11EngineInterface`

앱 프로젝트가 직접 참조하는 핵심 프로젝트는 `Core`, `D3D11DuplicateEngine`, `D3D11ImageView`입니다. 나머지 D3D11 관련 모듈은 이미지 뷰어와 렌더링 파이프라인에서 함께 사용됩니다.

## 저장소 배치

이 프로젝트는 application 저장소와 shared module 저장소가 같은 개발 루트 아래에 있는 구조를 기대합니다.

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

예를 들어 솔루션은 다음과 같은 상대 경로를 사용합니다.

```text
..\..\Module\Core\Core\Core.vcxproj
..\..\Module\D3D11DuplicateEngine\D3D11DuplicateEngine\D3D11DuplicateEngine.vcxproj
..\..\Module\D3D11ImageView\D3D11ImageView\D3D11ImageView.vcxproj
```

## 실행 방법

1. Visual Studio 2022에서 `DesktopDuplicateApp.sln`을 엽니다.
2. 원하는 configuration/platform을 선택합니다. 일반적으로 `Debug|x64` 또는 `Release|x64`를 사용합니다.
3. 빌드 후 `DesktopDuplicateApp`을 실행합니다.
4. 콘솔에 `quit`을 입력하거나 `Ctrl+C`를 눌러 종료합니다.

## 프로젝트 구성

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

- `main.cpp`: COM 초기화, console control handler 등록, 앱 lifecycle 실행
- `DesktopDuplicateApp.h`: 이미지 뷰어/캡처 엔진 초기화, message loop, frame callback, shutdown 처리
- `Icons/`: UI/view module에서 사용하는 SVG 아이콘 리소스
- `Shaders/`: 렌더링/image pipeline에서 사용하는 compiled shader object 리소스

## 구현 메모

- 기본 캡처 대상은 output index `0`입니다.
- 기본 이미지 뷰어 영역은 `1920 x 900`으로 초기화됩니다.
- `SharedCaptureData`는 `SRWLOCK`으로 shared handle과 새 프레임 플래그를 보호합니다.
- Debug 빌드에서는 종료 시 `Core::DirectX::DxReportLiveObjects()`를 호출합니다.
