#include "pch.h"
#include "lennox_engineMain.h"
#include "Common\DirectXHelper.h"

using namespace LennoxEngine;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;

//Szablon aplikacji DirectX 12 jest udokumentowany na stronie https://go.microsoft.com/fwlink/?LinkID=613670&clcid=0x415

// �aduje i inicjuje elementy zawarto�ci aplikacji podczas �adowania aplikacji.
lennox_engineMain::lennox_engineMain()
{
	// TODO: zmie� ustawienia czasomierza, je�li chcesz u�y� czego� innego ni� domy�lny tryb zmiennego kroku czasu.
	// np. dla logiki aktualizacji ze sta�ym krokiem czasu 60 kl./s u�yj wywo�ania:
	/*
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0 / 60);
	*/
}

// Tworzy i inicjuje programy renderuj�ce.
void lennox_engineMain::CreateRenderers(const std::shared_ptr<DX::DeviceResources>& deviceResources)
{
	// TODO: zast�p to inicjowaniem zawarto�ci aplikacji.
	m_sceneRenderer = std::unique_ptr<Sample3DSceneRenderer>(new Sample3DSceneRenderer(deviceResources));

	OnWindowSizeChanged();
}

// Aktualizuje stan aplikacji raz na ramk�.
void lennox_engineMain::Update()
{
	// Aktualizuj obiekty sceny.
	m_timer.Tick([&]()
	{
		// TODO: zast�p to funkcjami aktualizacji zawarto�ci aplikacji.
		m_sceneRenderer->Update(m_timer);
	});
}

//Renderuje bie��c� ramk� zgodnie z bie��cym stanem aplikacji.
// Zwraca warto�� true, je�li ramka zosta�a wyrenderowana i jest gotowa do wy�wietlenia.
bool lennox_engineMain::Render()
{
	// Nie pr�buj renderowa� czegokolwiek przed pierwsz� aktualizacj�.
	if (m_timer.GetFrameCount() == 0)
	{
		return false;
	}

	// Renderuj obiekty sceny.
	// TODO: zast�p to funkcjami renderowania zawarto�ci aplikacji.
	return m_sceneRenderer->Render();
}

// Aktualizuje stan aplikacji w przypadku zmiany rozmiaru okna (np. zmiany orientacji urz�dzenia)
void lennox_engineMain::OnWindowSizeChanged()
{
	// TODO: zast�p to zale�nym od rozmiaru inicjowaniem zawarto�ci aplikacji.
	m_sceneRenderer->CreateWindowSizeDependentResources();
}

// Powiadamia aplikacj� o jej wstrzymaniu.
void lennox_engineMain::OnSuspending()
{
	// TODO: zast�p to logik� wstrzymywania aplikacji.

	// Zarz�dzanie okresem istnienia procesu mo�e zako�czy� wstrzymane aplikacje w dowolnym momencie, dlatego
	// warto zapisa� stan, aby umo�liwi� ponowne uruchomienie aplikacji w ostatnim stanie przed jej zamkni�ciem.

	m_sceneRenderer->SaveState();

	// Je�li aplikacja korzysta z �atwych do odtworzenia alokacji pami�ci wideo,
	// rozwa� zwolnienie pami�ci, aby udost�pni� j� innym aplikacjom.
}

// Powiadamia aplikacj� o tym, �e nie jest ju� wstrzymana.
void lennox_engineMain::OnResuming()
{
	// TODO: zast�p to logik� wznawiania aplikacji.
}

// Powiadamia modu�y renderowania, �e nale�y zwolni� zasoby urz�dzenia.
void lennox_engineMain::OnDeviceRemoved()
{
	// TODO: zapisz wymagany stan aplikacji lub programu renderuj�cego i zwolnij program renderuj�cy
	// i jego zasoby, kt�re nie s� ju� prawid�owe.
	m_sceneRenderer->SaveState();
	m_sceneRenderer = nullptr;
}
