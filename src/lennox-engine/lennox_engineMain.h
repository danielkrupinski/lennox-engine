#pragma once

#include "Common\StepTimer.h"
#include "Common\DeviceResources.h"
#include "Content\Sample3DSceneRenderer.h"

// Renderuje zawartość Direct3D na ekranie.
namespace lennox_engine
{
	class lennox_engineMain
	{
	public:
		lennox_engineMain();
		void CreateRenderers(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void Update();
		bool Render();

		void OnWindowSizeChanged();
		void OnSuspending();
		void OnResuming();
		void OnDeviceRemoved();

	private:
		// TODO: zastąp własnymi modułami renderowania zawartości.
		std::unique_ptr<Sample3DSceneRenderer> m_sceneRenderer;

		// Renderowanie czasomierza pętli.
		DX::StepTimer m_timer;
	};
}