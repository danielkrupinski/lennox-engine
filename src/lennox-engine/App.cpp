#include "pch.h"
#include "App.h"

#include <ppltasks.h>

using namespace Lennox;

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

// G��wna funkcja s�u�y tylko do inicjowania klasy IFrameworkView.
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

// Pierwsza metoda wywo�ywana podczas tworzenia elementu IFrameworkView.
void App::Initialize(CoreApplicationView^ applicationView)
{
	// Zarejestruj procedury obs�ugi zdarze� dotycz�ce cyklu �ycia aplikacji. Ten przyk�ad obejmuje element Activated, aby
	// mo�e uaktywni� element CoreWindow i rozpocz�� renderowanie w oknie.
	applicationView->Activated +=
		ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(this, &App::OnActivated);

	CoreApplication::Suspending +=
		ref new EventHandler<SuspendingEventArgs^>(this, &App::OnSuspending);

	CoreApplication::Resuming +=
		ref new EventHandler<Platform::Object^>(this, &App::OnResuming);
}

// Element wywo�ywany w przypadku utworzenia lub odtworzenia obiektu CoreWindow.
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

// Inicjuje zasoby sceny lub �aduje wcze�niej zapisany stan aplikacji.
void App::Load(Platform::String^ entryPoint)
{
	if (m_main == nullptr)
	{
		m_main = std::unique_ptr<LennoxEngine>(new LennoxEngine());
	}
}

// Ta metoda jest wywo�ywana po uaktywnieniu okna.
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
// Zako�cz zdarzenia, kt�re nie powoduj� wywo�ania elementu Uninitialize. Zostanie on wywo�any, je�li element IFrameworkView
// klasa jest niszczona, gdy aplikacja dzia�a na pierwszym planie.
void App::Uninitialize()
{
}

// Procedury obs�ugi zdarze� cyklu �ycia aplikacji.

void App::OnActivated(CoreApplicationView^ applicationView, IActivatedEventArgs^ args)
{
	// Element Run() zostanie uruchomiony po uaktywnieniu elementu CoreWindow.
	CoreWindow::GetForCurrentThread()->Activate();
}

void App::OnSuspending(Platform::Object^ sender, SuspendingEventArgs^ args)
{
	// Zapisz stan aplikacji asynchronicznie po za��daniu odroczenia. Wstrzymanie odroczenia
	// wskazuje, �e aplikacja jest zaj�ta przez operacje wstrzymywania.
	// Odroczenie nie mo�e trwa� niesko�czenie d�ugo. Po oko�o pi�ciu sekundach
	// zostanie wymuszone zako�czenie dzia�ania aplikacji.
	SuspendingDeferral^ deferral = args->SuspendingOperation->GetDeferral();

	create_task([this, deferral]()
	{
		m_main->OnSuspending();
		deferral->Complete();
	});
}

void App::OnResuming(Platform::Object^ sender, Platform::Object^ args)
{
	// Przywr�� zwolnione lub wstrzymane dane albo stan. Domy�lnie dane
	// i stan s� zachowywane w przypadku wznawiania po wstrzymaniu. To zdarzenie
	// nie wyst�puje, je�li dzia�anie aplikacji zosta�o wcze�niej zako�czone.

	m_main->OnResuming();
}

// Procedury obs�ugi zdarze� okna.

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

// Procedury obs�ugi zdarze� wy�wietlania informacji.

void App::OnDpiChanged(DisplayInformation^ sender, Object^ args)
{
	// Uwaga: Warto�� LogicalDpi pobrana w tym miejscu mo�e nie by� zgodna z warto�ci� DPI obowi�zuj�c� dla aplikacji,
	// je�li wykonywane jest skalowanie dla urz�dze� obs�uguj�cych wysok� rozdzielczo��. Po ustawieniu warto�ci DPI w elemencie DeviceResources
	// nale�y zawsze pobiera� t� warto�� za pomoc� metody GetDpi.
	// Wi�cej szczeg��w zawiera plik DeviceResources.cpp.
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
		// Wszystkie odwo�ania do istniej�cego urz�dzenia D3D musz� zosta� zwolnione, aby mo�na by�o
		// utworzy� nowe urz�dzenie.

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
