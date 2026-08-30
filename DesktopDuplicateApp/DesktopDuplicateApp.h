#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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

		m_duplicateEngine = new D3D11DuplicateEngine();
		if (!m_duplicateEngine)
		{
			Shutdown();
			return false;
		}

		// Capture and ImageView own different D3D11 devices and immediate
		// contexts. Each context is used by only its own thread, while keyed
		// mutex synchronization protects the shared texture across devices.
		if (!m_duplicateEngine->SetImmediateContextGateEnabled(false) ||
			!m_duplicateEngine->SetCaptureOutputMode(CaptureOutputMode::SharedTexture))
		{
			Shutdown();
			return false;
		}

		if (!m_duplicateEngine->Initialize())
		{
			Shutdown();
			return false;
		}

		m_duplicateEngine->SetFrameCaptureCallback(FrameCallbackThunk, this);

		if (!m_duplicateEngine->StartThread())
		{
			Shutdown();
			return false;
		}

		m_running = true;
		return true;
	}

	void Run()
	{
		std::string cmd;
		MSG message = {};

		while (m_running)
		{
			while (::PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
			{
				if (message.message == WM_QUIT)
				{
					m_running = false;
					break;
				}

				::TranslateMessage(&message);
				::DispatchMessage(&message);
			}

			if (!m_running)
				break;

			if (_kbhit())
			{
				std::getline(std::cin, cmd);
				if (cmd == "quit")
				{
					m_running = false;
				}
			}

			::Sleep(10);
		}
	}

	void RequestStop()
	{
		m_running = false;
	}

	void Shutdown()
	{
		m_running = false;

		if (m_duplicateEngine)
		{
			m_duplicateEngine->Shutdown();
			delete m_duplicateEngine;
			m_duplicateEngine = nullptr;
		}

		if (m_imageView)
		{
			delete m_imageView;
			m_imageView = nullptr;
		}

	}

private:
	static void FrameCallbackThunk(void* userData)
	{
		DesktopDuplicateApp* self = static_cast<DesktopDuplicateApp*>(userData);
		if (self)
		{
			self->OnFrameCallback();
		}
	}

	void OnFrameCallback()
	{
		if (!m_imageView || !m_duplicateEngine)
			return;

		HANDLE sharedHandle = m_duplicateEngine->GetSharedTextureHandle();
		if (sharedHandle)
		{
			m_imageView->UpdateSharedTexture(sharedHandle);
		}
	}

private:
	bool m_running = false;
	D3D11DuplicateEngine* m_duplicateEngine = nullptr;
	D3D11ImageView* m_imageView = nullptr;
};
