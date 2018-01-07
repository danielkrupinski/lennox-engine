#include "pch.h"
#include "App.h"

#include <ppltasks.h>

using namespace LennoxEngine;

using namespace concurrency;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::System;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;

using Microsoft::WRL::ComPtr;

//Szablon aplikacji DirectX 12 jest udokumentowany na stronie https://go.microsoft.com/fwlink/?LinkID=613670&clcid=0x415

// G³ówna funkcja s³u¿y tylko do inicjowania klasy IFrameworkView.
[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^)
{
	auto direct3DApplicationSource = ref new Direct3DApplicationSource();
	CoreApplication::Run(direct3DApplicationSource);
	return 0;
}

IFrameworkView^ Direct3DApplicationSource::CreateView()
{
	return ref new App();
}

App::App() :
	m_windowClosed(false),
	m_windowVisible(true)
{
}

// Pierwsza metoda wywo³ywana podczas tworzenia elementu IFrameworkView.
void App::Initialize(CoreApplicationView^ applicationView)
{
	// Zarejestruj procedury obs³ugi zdarzeñ dotycz¹ce cyklu ¿ycia aplikacji. Ten przyk³ad obejmuje element Activated, aby
	// mo¿e uaktywniæ element CoreWindow i rozpocz¹æ renderowanie w oknie.
	applicationView->Activated +=
		ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(this, &App::OnActivated);

	CoreApplication::Suspending +=
		ref new EventHandler<SuspendingEventArgs^>(this, &App::OnSuspending);

	CoreApplication::Resuming +=
		ref new EventHandler<Platform::Object^>(this, &App::OnResuming);
}

// Element wywo³ywany w przypadku utworzenia lub odtworzenia obiektu CoreWindow.
void App::SetWindow(CoreWindow^ window)
{
	window->SizeChanged += 
		ref new TypedEventHandler<CoreWindow^, WindowSizeChangedEventArgs^>(this, &App::OnWindowSizeChanged);

	window->VisibilityChanged +=
		ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &App::OnVisibilityChanged);

	window->Closed += 
		ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(this, &App::OnWindowClosed);

	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	currentDisplayInformation->DpiChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnDpiChanged);

	currentDisplayInformation->OrientationChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnOrientationChanged);

	DisplayInformation::DisplayContentsInvalidated +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnDisplayContentsInvalidated);
}

// Inicjuje zasoby sceny lub ³aduje wczeœniej zapisany stan aplikacji.
void App::Load(Platform::String^ entryPoint)
{
	if (m_main == nullptr)
	{
		m_main = std::unique_ptr<lennox_engineMain>(new lennox_engineMain());
	}
}

// Ta metoda jest wywo³ywana po uaktywnieniu okna.
void App::Run()
{
	while (!m_windowClosed)
	{
		if (m_windowVisible)
		{
			CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

			auto commandQueue = GetDeviceResources()->GetCommandQueue();
			PIXBeginEvent(commandQueue, 0, L"Update");
			{
				m_main->Update();
			}
			PIXEndEvent(commandQueue);

			PIXBeginEvent(commandQueue, 0, L"Render");
			{
				if (m_main->Render())
				{
					GetDeviceResources()->Present();
				}
			}
			PIXEndEvent(commandQueue);
		}
		else
		{
			CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
		}
	}
}

// Element wymagany przez element IFrameworkView.
// Zakoñcz zdarzenia, które nie powoduj¹ wywo³ania elementu Uninitialize. Zostanie on wywo³any, jeœli element IFrameworkView
// klasa jest niszczona, gdy aplikacja dzia³a na pierwszym planie.
void App::Uninitialize()
{
}

// Procedury obs³ugi zdarzeñ cyklu ¿ycia aplikacji.

void App::OnActivated(CoreApplicationView^ applicationView, IActivatedEventArgs^ args)
{
	// Element Run() zostanie uruchomiony po uaktywnieniu elementu CoreWindow.
	CoreWindow::GetForCurrentThread()->Activate();
}

void App::OnSuspending(Platform::Object^ sender, SuspendingEventArgs^ args)
{
	// Zapisz stan aplikacji asynchronicznie po za¿¹daniu odroczenia. Wstrzymanie odroczenia
	// wskazuje, ¿e aplikacja jest zajêta przez operacje wstrzymywania.
	// Odroczenie nie mo¿e trwaæ nieskoñczenie d³ugo. Po oko³o piêciu sekundach
	// zostanie wymuszone zakoñczenie dzia³ania aplikacji.
	SuspendingDeferral^ deferral = args->SuspendingOperation->GetDeferral();

	create_task([this, deferral]()
	{
		m_main->OnSuspending();
		deferral->Complete();
	});
}

void App::OnResuming(Platform::Object^ sender, Platform::Object^ args)
{
	// Przywróæ zwolnione lub wstrzymane dane albo stan. Domyœlnie dane
	// i stan s¹ zachowywane w przypadku wznawiania po wstrzymaniu. To zdarzenie
	// nie wystêpuje, jeœli dzia³anie aplikacji zosta³o wczeœniej zakoñczone.

	m_main->OnResuming();
}

// Procedury obs³ugi zdarzeñ okna.

void App::OnWindowSizeChanged(CoreWindow^ sender, WindowSizeChangedEventArgs^ args)
{
	GetDeviceResources()->SetLogicalSize(Size(sender->Bounds.Width, sender->Bounds.Height));
	m_main->OnWindowSizeChanged();
}

void App::OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args)
{
	m_windowVisible = args->Visible;
}

void App::OnWindowClosed(CoreWindow^ sender, CoreWindowEventArgs^ args)
{
	m_windowClosed = true;
}

// Procedury obs³ugi zdarzeñ wyœwietlania informacji.

void App::OnDpiChanged(DisplayInformation^ sender, Object^ args)
{
	// Uwaga: Wartoœæ LogicalDpi pobrana w tym miejscu mo¿e nie byæ zgodna z wartoœci¹ DPI obowi¹zuj¹c¹ dla aplikacji,
	// jeœli wykonywane jest skalowanie dla urz¹dzeñ obs³uguj¹cych wysok¹ rozdzielczoœæ. Po ustawieniu wartoœci DPI w elemencie DeviceResources
	// nale¿y zawsze pobieraæ tê wartoœæ za pomoc¹ metody GetDpi.
	// Wiêcej szczegó³ów zawiera plik DeviceResources.cpp.
	GetDeviceResources()->SetDpi(sender->LogicalDpi);
	m_main->OnWindowSizeChanged();
}

void App::OnOrientationChanged(DisplayInformation^ sender, Object^ args)
{
	GetDeviceResources()->SetCurrentOrientation(sender->CurrentOrientation);
	m_main->OnWindowSizeChanged();
}

void App::OnDisplayContentsInvalidated(DisplayInformation^ sender, Object^ args)
{
	GetDeviceResources()->ValidateDevice();
}

std::shared_ptr<DX::DeviceResources> App::GetDeviceResources()
{
	if (m_deviceResources != nullptr && m_deviceResources->IsDeviceRemoved())
	{
		// Wszystkie odwo³ania do istniej¹cego urz¹dzenia D3D musz¹ zostaæ zwolnione, aby mo¿na by³o
		// utworzyæ nowe urz¹dzenie.

		m_deviceResources = nullptr;
		m_main->OnDeviceRemoved();

#if defined(_DEBUG)
		ComPtr<IDXGIDebug1> dxgiDebug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
		{
			dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
		}
#endif
	}

	if (m_deviceResources == nullptr)
	{
		m_deviceResources = std::make_shared<DX::DeviceResources>();
		m_deviceResources->SetWindow(CoreWindow::GetForCurrentThread());
		m_main->CreateRenderers(m_deviceResources);
	}
	return m_deviceResources;
}
