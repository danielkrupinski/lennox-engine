#include "pch.h"
#include "lennox_engineMain.h"
#include "Common\DirectXHelper.h"

using namespace LennoxEngine;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;

//Szablon aplikacji DirectX 12 jest udokumentowany na stronie https://go.microsoft.com/fwlink/?LinkID=613670&clcid=0x415

// £aduje i inicjuje elementy zawartoœci aplikacji podczas ³adowania aplikacji.
lennox_engineMain::lennox_engineMain()
{
	// TODO: zmieñ ustawienia czasomierza, jeœli chcesz u¿yæ czegoœ innego ni¿ domyœlny tryb zmiennego kroku czasu.
	// np. dla logiki aktualizacji ze sta³ym krokiem czasu 60 kl./s u¿yj wywo³ania:
	/*
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0 / 60);
	*/
}

// Tworzy i inicjuje programy renderuj¹ce.
void lennox_engineMain::CreateRenderers(const std::shared_ptr<DX::DeviceResources>& deviceResources)
{
	// TODO: zast¹p to inicjowaniem zawartoœci aplikacji.
	m_sceneRenderer = std::unique_ptr<Sample3DSceneRenderer>(new Sample3DSceneRenderer(deviceResources));

	OnWindowSizeChanged();
}

// Aktualizuje stan aplikacji raz na ramkê.
void lennox_engineMain::Update()
{
	// Aktualizuj obiekty sceny.
	m_timer.Tick([&]()
	{
		// TODO: zast¹p to funkcjami aktualizacji zawartoœci aplikacji.
		m_sceneRenderer->Update(m_timer);
	});
}

//Renderuje bie¿¹c¹ ramkê zgodnie z bie¿¹cym stanem aplikacji.
// Zwraca wartoœæ true, jeœli ramka zosta³a wyrenderowana i jest gotowa do wyœwietlenia.
bool lennox_engineMain::Render()
{
	// Nie próbuj renderowaæ czegokolwiek przed pierwsz¹ aktualizacj¹.
	if (m_timer.GetFrameCount() == 0)
	{
		return false;
	}

	// Renderuj obiekty sceny.
	// TODO: zast¹p to funkcjami renderowania zawartoœci aplikacji.
	return m_sceneRenderer->Render();
}

// Aktualizuje stan aplikacji w przypadku zmiany rozmiaru okna (np. zmiany orientacji urz¹dzenia)
void lennox_engineMain::OnWindowSizeChanged()
{
	// TODO: zast¹p to zale¿nym od rozmiaru inicjowaniem zawartoœci aplikacji.
	m_sceneRenderer->CreateWindowSizeDependentResources();
}

// Powiadamia aplikacjê o jej wstrzymaniu.
void lennox_engineMain::OnSuspending()
{
	// TODO: zast¹p to logik¹ wstrzymywania aplikacji.

	// Zarz¹dzanie okresem istnienia procesu mo¿e zakoñczyæ wstrzymane aplikacje w dowolnym momencie, dlatego
	// warto zapisaæ stan, aby umo¿liwiæ ponowne uruchomienie aplikacji w ostatnim stanie przed jej zamkniêciem.

	m_sceneRenderer->SaveState();

	// Jeœli aplikacja korzysta z ³atwych do odtworzenia alokacji pamiêci wideo,
	// rozwa¿ zwolnienie pamiêci, aby udostêpniæ j¹ innym aplikacjom.
}

// Powiadamia aplikacjê o tym, ¿e nie jest ju¿ wstrzymana.
void lennox_engineMain::OnResuming()
{
	// TODO: zast¹p to logik¹ wznawiania aplikacji.
}

// Powiadamia modu³y renderowania, ¿e nale¿y zwolniæ zasoby urz¹dzenia.
void lennox_engineMain::OnDeviceRemoved()
{
	// TODO: zapisz wymagany stan aplikacji lub programu renderuj¹cego i zwolnij program renderuj¹cy
	// i jego zasoby, które nie s¹ ju¿ prawid³owe.
	m_sceneRenderer->SaveState();
	m_sceneRenderer = nullptr;
}
