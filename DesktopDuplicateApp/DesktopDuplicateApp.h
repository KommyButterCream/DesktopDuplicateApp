#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <string>
#include <iostream>
#include <conio.h>

#include "../../../Module/D3D11DuplicateEngine/D3D11DuplicateEngine/D3D11DuplicateEngine.h"
#include "../../../Module/D3D11DuplicateEngine/D3D11DuplicateEngine/CommonTypes.h"
#include "../../../Module/D3D11ImageView/D3D11ImageView/D3D11ImageView.h"

class DesktopDuplicateApp
{
public:
	DesktopDuplicateApp() = default;
	~DesktopDuplicateApp()
	{
		Shutdown();
	}

	bool Initialize()
	{
		DWORD windowStyle = WS_VISIBLE | WS_OVERLAPPEDWINDOW;

		m_imageView = new D3D11ImageView();
		if (!m_imageView)
			return false;

		if (!m_imageView->Initialize(GetDesktopWindow(), RECT(0, 0, 1920, 900), windowStyle))
		{
			Shutdown();
			return false;
		}

		// 뷰어 창이 이 앱의 유일한 창이다. 사용자가 닫기 버튼을 누르면
		// 그것이 곧 종료 요청이므로, 통지를 받아 메시지 루프를 끝낸다.
		m_imageView->SetCloseHandler(ViewerCloseCallback, this);

		m_duplicateEngine = new D3D11DuplicateEngine();
		if (!m_duplicateEngine)
		{
			Shutdown();
			return false;
		}

		// 캡처와 ImageView 는 서로 다른 D3D11 디바이스와 immediate context 를
		// 쓴다. 각 컨텍스트는 자기 스레드만 쓰므로 게이트가 필요 없고,
		// 디바이스를 건너는 동기화는 키드 뮤텍스가 맡는다.
		//
		// 프레임 풀을 공유로 만들어 뷰어가 슬롯을 직접 읽게 한다. 슬롯이
		// 여러 장이라, 뷰어가 한 장을 읽는 동안에도 캡처가 다음 장에 쓸 수 있다.
		if (!m_duplicateEngine->SetImmediateContextGateEnabled(false) ||
			!m_duplicateEngine->SetFramePoolSharable(true))
		{
			Shutdown();
			return false;
		}

		if (!m_duplicateEngine->Initialize())
		{
			Shutdown();
			return false;
		}

		// 슬롯 핸들은 여기서 한 번만 넘긴다. 이후 콜백은 번호만 주고받는다.
		if (!RegisterFramePoolWithViewer())
		{
			Shutdown();
			return false;
		}

		m_duplicateEngine->SetFrameCaptureCallback(FrameCallback, this);

		// 미리 열어 둔 핸들은 풀이 다시 만들어지는 순간 전부 무효가 된다.
		// 그 사실을 알 수 있는 유일한 통로가 이 통지다. 등록하지 않으면
		// 해상도 변경이나 디바이스 재생성 후에 화면이 조용히 얼어붙는다.
		m_duplicateEngine->SetCaptureEventCallback(CaptureEventCallback, this);

		// 캡처 스레드를 띄우기 전에 세운다. 이 뒤로 콜백이 뷰어를 만진다.
		::InterlockedExchange(&m_viewerAlive, TRUE);

		if (!m_duplicateEngine->StartThread())
		{
			Shutdown();
			return false;
		}

		::InterlockedExchange(&m_running, TRUE);
		m_nextStatsTick = ::GetTickCount64() + STATS_INTERVAL_MS;
		return true;
	}

	void Run()
	{
		std::string cmd;
		MSG message = {};

		while (IsRunning())
		{
			while (::PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
			{
				if (message.message == WM_QUIT)
				{
					RequestStop();
					break;
				}

				::TranslateMessage(&message);
				::DispatchMessage(&message);
			}

			if (!IsRunning())
				break;

			if (_kbhit())
			{
				std::getline(std::cin, cmd);
				if (cmd == "quit")
				{
					RequestStop();
				}
				else if (cmd == "stats")
				{
					PrintStats();
				}
			}

			ServicePoolReregister();

			const ULONGLONG now = ::GetTickCount64();
			if (now >= m_nextStatsTick)
			{
				PrintStats();
				m_nextStatsTick = now + STATS_INTERVAL_MS;
			}

			::Sleep(10);
		}
	}

	void RequestStop()
	{
		::InterlockedExchange(&m_running, FALSE);
	}

	bool IsRunning() const
	{
		return ::InterlockedCompareExchange(const_cast<volatile LONG*>(&m_running), 0, 0) != FALSE;
	}

	void Shutdown()
	{
		::InterlockedExchange(&m_running, FALSE);
		::InterlockedExchange(&m_viewerAlive, FALSE);

		// 엔진을 먼저 세운다. Shutdown 이 캡처 스레드를 정지시키므로,
		// 이 뒤로는 콜백이 오지 않는다.
		if (m_duplicateEngine)
		{
			m_duplicateEngine->Shutdown();
			delete m_duplicateEngine;
			m_duplicateEngine = nullptr;
		}

		// 그다음 뷰어가 들고 있던 슬롯 참조를 놓는다.
		if (m_imageView)
		{
			m_imageView->UnregisterSharedTexturePool();
		}

		if (m_imageView)
		{
			delete m_imageView;
			m_imageView = nullptr;
		}

	}

private:
	static void FrameCallback(void* userData)
	{
		DesktopDuplicateApp* self = static_cast<DesktopDuplicateApp*>(userData);
		if (self)
		{
			self->OnFrameCallback();
		}
	}

	static void ViewerCloseCallback(void* userData)
	{
		DesktopDuplicateApp* self = static_cast<DesktopDuplicateApp*>(userData);
		if (self)
		{
			self->OnViewerClosed();
		}
	}

	// 뷰어 창이 파괴되기 직전에 UI 스레드에서 불린다.
	//
	// 여기서는 깃발만 내린다. 캡처 스레드 정지와 리소스 해제는 Run 이
	// 빠져나온 뒤 Shutdown 이 하던 대로 한다 — 창 메시지를 처리하는 중에
	// 스레드 조인까지 하면 닫기 반응이 그만큼 늦어진다.
	//
	// m_viewerAlive 를 먼저 내리는 이유는, 이 함수가 돌아가는 즉시 창이
	// 파괴되고 뷰어가 정리되기 때문이다. 그 뒤로 캡처 콜백이 뷰어를
	// 건드리지 않게 막는다.
	void OnViewerClosed()
	{
		::InterlockedExchange(&m_viewerAlive, FALSE);
		RequestStop();
	}

	static void CaptureEventCallback(CaptureEventCode code, HRESULT hr, void* userData)
	{
		DesktopDuplicateApp* self = static_cast<DesktopDuplicateApp*>(userData);
		if (self)
		{
			self->OnCaptureEvent(code, hr);
		}
	}

	// 캡처 스레드에서 불린다. 블로킹 작업을 하면 캡처가 그대로 멈춘다.
	void OnCaptureEvent(CaptureEventCode code, HRESULT hr)
	{
		const char* name = "unknown";
		switch (code)
		{
		case CaptureEventCode::AccessLost:      name = "access lost"; break;
		case CaptureEventCode::Reconnecting:    name = "reconnecting"; break;
		case CaptureEventCode::Reconnected:     name = "reconnected"; break;
		case CaptureEventCode::ModeChanged:     name = "mode changed"; break;
		case CaptureEventCode::DeviceRemoved:   name = "device removed"; break;
		case CaptureEventCode::DeviceRecreated: name = "device recreated"; break;
		case CaptureEventCode::Faulted:         name = "faulted"; break;
		default: break;
		}

		printf_s("[capture] %s (hr=0x%08X)\n", name, static_cast<unsigned int>(hr));
		fflush(stdout);

		switch (code)
		{
		case CaptureEventCode::DeviceRecreated:
			// 풀은 아직 다시 만들어지지 않았다. 지금 우리가 들고 있는 텍스처는
			// 사라진 디바이스의 것이므로, 읽지 않도록 먼저 끊는다.
			// 다시 여는 것은 아래 ModeChanged 가 한다.
			::InterlockedExchange(&m_viewerAlive, FALSE);
			if (m_imageView)
			{
				m_imageView->UnregisterSharedTexturePool();
			}
			break;

		case CaptureEventCode::ModeChanged:
			// 풀이 새로 만들어진 직후다. 핸들이 전부 바뀌었으므로 다시 연다.
			//
			// 디바이스 재생성 경로도 결국 여기를 지난다 — 그때 프레임 크기가
			// 0 으로 초기화되어 엔진이 풀을 다시 만들기 때문이다.
			//
			// 앱 루프로 미루지 않고 여기서 바로 하는 이유: 다시 여는 일은
			// OpenSharedResource1 네 번이고 스레드 조인이 없다. 미루면 그동안
			// 화면이 멎는다.
			::InterlockedExchange(&m_poolReregisterFailed, FALSE);
			if (!RegisterFramePoolWithViewer())
			{
				// 표시만 남긴다. 여기서 종료를 부르면 캡처 스레드가 자기
				// 정지를 기다리게 된다.
				::InterlockedExchange(&m_poolReregisterFailed, TRUE);
				break;
			}
			++m_poolReregisterCount;
			::InterlockedExchange(&m_viewerAlive, TRUE);
			break;

		case CaptureEventCode::Faulted:
			// 캡처 스레드가 유휴로 떨어진다. 더 할 수 있는 것이 없다.
			printf_s("[capture] unrecoverable. stopping.\n");
			fflush(stdout);
			::InterlockedExchange(&m_viewerAlive, FALSE);
			RequestStop();
			break;

		default:
			break;
		}
	}

	// 풀 재등록이 실패했으면 더 보여줄 화면이 없다. 캡처 스레드가 아니라
	// 앱 스레드에서 종료를 결정한다.
	void ServicePoolReregister()
	{
		if (::InterlockedExchange(&m_poolReregisterFailed, FALSE) == FALSE)
			return;

		printf_s("[capture] failed to re-open the capture pool. stopping.\n");
		fflush(stdout);
		RequestStop();
	}

	void PrintStats()
	{
		if (!m_duplicateEngine)
			return;

		const CaptureStats stats = m_duplicateEngine->GetStats();
		printf_s("[stats] capture %llu skip=%llu drop=%llu timeout=%llu | "
			"lost=%llu reconnect=%llu devRecreate=%llu badRelease=%llu | "
			"poolReopen=%u lastError=0x%08X\n",
			stats.capturedFrames, stats.skippedFrames, stats.droppedFrames, stats.timeoutCount,
			stats.accessLostCount, stats.reconnectCount, stats.deviceRecreateCount,
			stats.invalidReleaseCount,
			m_poolReregisterCount, static_cast<unsigned int>(stats.lastError));
		fflush(stdout);
	}

	// 엔진이 만들어 둔 슬롯 핸들을 전부 뷰어에 넘긴다.
	// 뷰어가 여기서 한 번 열어 두고, 이후에는 슬롯 번호로만 찾는다.
	bool RegisterFramePoolWithViewer()
	{
		if (!m_imageView || !m_duplicateEngine)
			return false;

		const uint32_t poolCount = m_duplicateEngine->GetFramePoolCount();
		if (poolCount == 0 || poolCount > kMaxFramePoolCount)
			return false;

		HANDLE sharedHandles[kMaxFramePoolCount] = {};
		for (uint32_t i = 0; i < poolCount; ++i)
		{
			sharedHandles[i] = m_duplicateEngine->GetFramePoolSharedHandle(i);
			if (!sharedHandles[i])
				return false;
		}

		return m_imageView->RegisterSharedTexturePool(sharedHandles, poolCount);
	}

	void OnFrameCallback()
	{
		if (!m_imageView || !m_duplicateEngine)
			return;

		// 창이 닫히는 중이면 뷰어를 건드리지 않는다.
		if (::InterlockedCompareExchange(&m_viewerAlive, FALSE, FALSE) == FALSE)
			return;

		// 핸들은 슬롯 번호를 얻으려고 받는다. 실제 픽셀은 뷰어가 자기가 열어
		// 둔 텍스처에서 직접 읽으므로 handle.texture 는 쓰지 않는다.
		CapturedFrameHandle handle = m_duplicateEngine->GetLatestFrameHandle();
		if (handle.slotId < 0)
			return;

		m_imageView->UpdateSharedTexturePoolSlot(static_cast<uint32_t>(handle.slotId));

		// 곧바로 반납한다. 뷰어의 복사는 렌더 스레드에서 나중에 일어나지만,
		// 그 사이 캡처가 같은 슬롯을 덮어쓰더라도 키드 뮤텍스가 둘을 갈라
		// 놓는다. 반납을 미루면 슬롯이 묶여 캡처가 프레임을 버리기 시작한다.
		m_duplicateEngine->ReleaseLatestFrameHandle(handle);
	}

private:
	// 엔진의 풀 크기는 런타임에 묻지만, 콜백에서 힙을 잡지 않으려고
	// 스택 배열로 받는다. 엔진이 이보다 커지면 등록을 거절한다.
	static constexpr uint32_t kMaxFramePoolCount = 16;
	static constexpr ULONGLONG STATS_INTERVAL_MS = 5'000;

	// 여러 스레드가 내린다 — 앱 루프(quit), UI 스레드(창 닫기),
	// 캡처 스레드(복구 불가).
	volatile LONG m_running = FALSE;

	// 뷰어에 프레임을 밀어 넣어도 되는가.
	// 창이 닫히거나 풀이 무효가 되면 내려간다. 캡처 스레드가 읽는다.
	volatile LONG m_viewerAlive = FALSE;

	// 캡처 스레드가 세우고 앱 루프가 소비한다. 종료 판단을 캡처 스레드
	// 밖으로 빼기 위한 것이다.
	volatile LONG m_poolReregisterFailed = FALSE;

	// 풀을 다시 연 횟수. 통계에만 쓴다(캡처 스레드 전용).
	uint32_t m_poolReregisterCount = 0;

	ULONGLONG m_nextStatsTick = 0;

	D3D11DuplicateEngine* m_duplicateEngine = nullptr;
	D3D11ImageView* m_imageView = nullptr;
};
