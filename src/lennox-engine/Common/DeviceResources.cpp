#include "pch.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"

using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml::Controls;
using namespace Platform;

namespace DisplayMetrics
{
	// Ekrany o wysokiej rozdzielczość mogą wymagać bardzo dużo mocy procesora GPU i energii baterii na potrzeby renderowania.
	// Na przykład telefony z ekranami o wysokiej rozdzielczości mogą mieć krótki czas pracy baterii, jeśli
	// gry będą próbować renderować 60 klatek na sekundę przy pełnej rozdzielczości.
	// Decyzję o renderowaniu w pełnej rozdzielczości na wszystkich platformach i dla wszystkich typów urządzeń
	// należy podejmować z rozwagą.
	static const bool SupportHighResolutions = false;

	// Domyślne progi definiujące ekran o „wysokiej rozdzielczości”. Jeśli progi
	// zostaną przekroczone i parametr SupportHighResolutions ma wartość false, wymiary będą skalowane
	// o 50%.
	static const float DpiThreshold = 192.0f;		// 200% standardowego ekranu.
	static const float WidthThreshold = 1920.0f;	// 1080 pikseli szerokości.
	static const float HeightThreshold = 1080.0f;	// 1080 pikseli wysokości.
};

// Stałe używane do obliczania obrotów ekranu.
namespace ScreenRotation
{
	// Obrót wokół osi Z o 0 stopni
	static const XMFLOAT4X4 Rotation0(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// Obrót wokół osi Z o 90 stopni
	static const XMFLOAT4X4 Rotation90(
		0.0f, 1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// Obrót wokół osi Z o 180 stopni
	static const XMFLOAT4X4 Rotation180(
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// Obrót wokół osi Z o 270 stopni
	static const XMFLOAT4X4 Rotation270(
		0.0f, -1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);
};

// Konstruktor zasobów urządzenia.
DX::DeviceResources::DeviceResources(DXGI_FORMAT backBufferFormat, DXGI_FORMAT depthBufferFormat) :
	m_currentFrame(0),
	m_screenViewport(),
	m_rtvDescriptorSize(0),
	m_fenceEvent(0),
	m_backBufferFormat(backBufferFormat),
	m_depthBufferFormat(depthBufferFormat),
	m_fenceValues{},
	m_d3dRenderTargetSize(),
	m_outputSize(),
	m_logicalSize(),
	m_nativeOrientation(DisplayOrientations::None),
	m_currentOrientation(DisplayOrientations::None),
	m_dpi(-1.0f),
	m_effectiveDpi(-1.0f),
	m_deviceRemoved(false)
{
	CreateDeviceIndependentResources();
	CreateDeviceResources();
}

// Konfiguruje zasoby, które nie zależą od urządzenia Direct3D.
void DX::DeviceResources::CreateDeviceIndependentResources()
{
}

// Konfiguruje urządzenie Direct3D i zapisuje w nim dojścia oraz kontekst urządzenia.
void DX::DeviceResources::CreateDeviceResources()
{
#if defined(_DEBUG)
	// Jeśli projekt należy do kompilacji debugowanej, włącz debugowanie za pośrednictwem warstw zestawu SDK.
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
		}
	}
#endif

	DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgiFactory)));

	ComPtr<IDXGIAdapter1> adapter;
	GetHardwareAdapter(&adapter);

	// Utwórz obiekt urządzenia interfejsu API Direct3D 12
	HRESULT hr = D3D12CreateDevice(
		adapter.Get(),					// Adapter sprzętowy.
		D3D_FEATURE_LEVEL_11_0,			// Minimalny poziom funkcji, który może obsługiwać ta aplikacja.
		IID_PPV_ARGS(&m_d3dDevice)		// Zwraca utworzone urządzenie Direct3D.
		);

#if defined(_DEBUG)
	if (FAILED(hr))
	{
		// Jeśli inicjowanie się nie powiedzie, wróć do urządzenia WARP.
		// Aby uzyskać więcej informacji na temat platformy WARP, zobacz: 
		// https://go.microsoft.com/fwlink/?LinkId=286690

		ComPtr<IDXGIAdapter> warpAdapter;
		DX::ThrowIfFailed(m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

		hr = D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_d3dDevice));
	}
#endif

	DX::ThrowIfFailed(hr);

	// Utwórz kolejkę poleceń.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	DX::ThrowIfFailed(m_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
	NAME_D3D12_OBJECT(m_commandQueue);

	// Utwórz sterty deskryptorów dla widoków obiektów docelowych renderowania i widoków wzorników głębi.
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = c_frameCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	DX::ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
	NAME_D3D12_OBJECT(m_rtvHeap);

	m_rtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));
	NAME_D3D12_OBJECT(m_dsvHeap);

	for (UINT n = 0; n < c_frameCount; n++)
	{
		DX::ThrowIfFailed(
			m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n]))
			);
	}

	// Utwórz obiekty synchronizacji.
	DX::ThrowIfFailed(m_d3dDevice->CreateFence(m_fenceValues[m_currentFrame], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	m_fenceValues[m_currentFrame]++;

	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_fenceEvent == nullptr)
	{
		DX::ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
}

// Te zasoby muszą być odtwarzane przy każdej zmianie rozmiaru okna.
void DX::DeviceResources::CreateWindowSizeDependentResources()
{
	// Zaczekaj na ukończenie wszystkich poprzednich zadań procesora GPU.
	WaitForGpu();

	// Wyczyść wcześniejszą zawartość dotyczącą określonego rozmiaru okna i zaktualizuj śledzone wartości ograniczników.
	for (UINT n = 0; n < c_frameCount; n++)
	{
		m_renderTargets[n] = nullptr;
		m_fenceValues[n] = m_fenceValues[m_currentFrame];
	}

	UpdateRenderTargetSize();

	// Szerokość i wysokość łańcucha wymiany musi być oparta na
	// natywna szerokość i wysokość. Jeśli okno nie jest w natywnym
	// orientacji, konieczne jest odwrócenie wymiarów.
	DXGI_MODE_ROTATION displayRotation = ComputeDisplayRotation();

	bool swapDimensions = displayRotation == DXGI_MODE_ROTATION_ROTATE90 || displayRotation == DXGI_MODE_ROTATION_ROTATE270;
	m_d3dRenderTargetSize.Width = swapDimensions ? m_outputSize.Height : m_outputSize.Width;
	m_d3dRenderTargetSize.Height = swapDimensions ? m_outputSize.Width : m_outputSize.Height;

	UINT backBufferWidth = lround(m_d3dRenderTargetSize.Width);
	UINT backBufferHeight = lround(m_d3dRenderTargetSize.Height);

	if (m_swapChain != nullptr)
	{
		// Jeśli łańcuch wymiany już istnieje, zmień jego rozmiar.
		HRESULT hr = m_swapChain->ResizeBuffers(c_frameCount, backBufferWidth, backBufferHeight, m_backBufferFormat, 0);

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			// Jeśli urządzenie zostało usunięte, należy utworzyć nowe urządzenie i nowy łańcuch wymiany.
			m_deviceRemoved = true;

			// Nie kontynuuj wykonywania tej metody. Zasoby urządzenia zostaną zniszczone i ponownie utworzone.
			return;
		}
		else
		{
			DX::ThrowIfFailed(hr);
		}
	}
	else
	{
		// W przeciwnym razie utwórz nowy element za pomocą tej samej karty, która jest używana przez istniejące urządzenie Direct3D.
		DXGI_SCALING scaling = DisplayMetrics::SupportHighResolutions ? DXGI_SCALING_NONE : DXGI_SCALING_STRETCH;
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

		swapChainDesc.Width = backBufferWidth;						// Dopasuj rozmiar okna.
		swapChainDesc.Height = backBufferHeight;
		swapChainDesc.Format = m_backBufferFormat;
		swapChainDesc.Stereo = false;
		swapChainDesc.SampleDesc.Count = 1;							// Nie używaj próbkowania wielokrotnego.
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = c_frameCount;					// Użyj potrójnego buforowania, aby zminimalizować opóźnienie.
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;	// Wszystkie aplikacje uniwersalne systemu Windows muszą używać elementu _FLIP_ SwapEffects.
		swapChainDesc.Flags = 0;
		swapChainDesc.Scaling = scaling;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

		ComPtr<IDXGISwapChain1> swapChain;
		DX::ThrowIfFailed(
			m_dxgiFactory->CreateSwapChainForCoreWindow(
				m_commandQueue.Get(),								// Łańcuchy wymiany potrzebują odwołania do kolejki poleceń w programie DirectX 12.
				reinterpret_cast<IUnknown*>(m_window.Get()),
				&swapChainDesc,
				nullptr,
				&swapChain
				)
			);

		DX::ThrowIfFailed(swapChain.As(&m_swapChain));
	}

	// Ustaw odpowiednią orientację łańcucha wymiany i wygeneruj
	// Przekształcenia macierzy 3W na potrzeby renderowania w obróconym łańcuchu wymiany.
	// Jawnie określono, aby macierz 3W unikała błędów zaokrąglania.

	switch (displayRotation)
	{
	case DXGI_MODE_ROTATION_IDENTITY:
		m_orientationTransform3D = ScreenRotation::Rotation0;
		break;

	case DXGI_MODE_ROTATION_ROTATE90:
		m_orientationTransform3D = ScreenRotation::Rotation270;
		break;

	case DXGI_MODE_ROTATION_ROTATE180:
		m_orientationTransform3D = ScreenRotation::Rotation180;
		break;

	case DXGI_MODE_ROTATION_ROTATE270:
		m_orientationTransform3D = ScreenRotation::Rotation90;
		break;

	default:
		throw ref new FailureException();
	}

	DX::ThrowIfFailed(
		m_swapChain->SetRotation(displayRotation)
		);

	// Utwórz widoki obiektu docelowego renderowania buforu zapasowego łańcucha wymiany.
	{
		m_currentFrame = m_swapChain->GetCurrentBackBufferIndex();
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
		for (UINT n = 0; n < c_frameCount; n++)
		{
			DX::ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_d3dDevice->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvDescriptor);
			rtvDescriptor.Offset(m_rtvDescriptorSize);

			WCHAR name[25];
			if (swprintf_s(name, L"m_renderTargets[%u]", n) > 0)
			{
				DX::SetName(m_renderTargets[n].Get(), name);
			}
		}
	}

	// Utwórz wzornik i widok głębi.
	{
		D3D12_HEAP_PROPERTIES depthHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		D3D12_RESOURCE_DESC depthResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(m_depthBufferFormat, backBufferWidth, backBufferHeight, 1, 1);
		depthResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		CD3DX12_CLEAR_VALUE depthOptimizedClearValue(m_depthBufferFormat, 1.0f, 0);

		ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
			&depthHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&depthResourceDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(&m_depthStencil)
			));

		NAME_D3D12_OBJECT(m_depthStencil);

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = m_depthBufferFormat;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

		m_d3dDevice->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	}

	// Ustaw całe okno jako element docelowy okienka ekranu renderowania 3W.
	m_screenViewport = { 0.0f, 0.0f, m_d3dRenderTargetSize.Width, m_d3dRenderTargetSize.Height, 0.0f, 1.0f };
}

// Określ wymiary obiektu docelowego renderowania i czy będzie on skalowany w dół.
void DX::DeviceResources::UpdateRenderTargetSize()
{
	m_effectiveDpi = m_dpi;

	// Aby wydłużyć czas pracy baterii w przypadku urządzeń z ekranami o wysokiej rozdzielczości, renderuj z użyciem mniejszego obiektu docelowego renderowania
	// i zezwól procesorowi GPU na skalowanie obrazu wyjściowego przy wyświetlaniu.
	if (!DisplayMetrics::SupportHighResolutions && m_dpi > DisplayMetrics::DpiThreshold)
	{
		float width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_dpi);
		float height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_dpi);

		// Gdy urządzenie jest w orientacji pionowej, wysokość jest większa od szerokości. Porównaj
		// większy wymiar z progiem szerokości i mniejszy wymiar
		// z progiem wysokości.
		if (max(width, height) > DisplayMetrics::WidthThreshold && min(width, height) > DisplayMetrics::HeightThreshold)
		{
			// Aby skalować aplikację, zmieniamy obowiązującą wartość DPI. Rozmiar logiczny nie ulega zmianie.
			m_effectiveDpi /= 2.0f;
		}
	}

	// Oblicz wymagany rozmiar docelowy renderowania w pikselach.
	m_outputSize.Width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_effectiveDpi);
	m_outputSize.Height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_effectiveDpi);

	// Uniemożliwiaj tworzenie zawartości DirectX o zerowym rozmiarze.
	m_outputSize.Width = max(m_outputSize.Width, 1);
	m_outputSize.Height = max(m_outputSize.Height, 1);
}

// Ta metoda jest wywoływana w przypadku utworzenia lub odtworzenia obiektu CoreWindow.
void DX::DeviceResources::SetWindow(CoreWindow^ window)
{
	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	m_window = window;
	m_logicalSize = Windows::Foundation::Size(window->Bounds.Width, window->Bounds.Height);
	m_nativeOrientation = currentDisplayInformation->NativeOrientation;
	m_currentOrientation = currentDisplayInformation->CurrentOrientation;
	m_dpi = currentDisplayInformation->LogicalDpi;

	CreateWindowSizeDependentResources();
}

// Ta metoda jest wywoływana w procedurze obsługi zdarzeń dla zdarzenia SizeChanged.
void DX::DeviceResources::SetLogicalSize(Windows::Foundation::Size logicalSize)
{
	if (m_logicalSize != logicalSize)
	{
		m_logicalSize = logicalSize;
		CreateWindowSizeDependentResources();
	}
}

// Ta metoda jest wywoływana w procedurze obsługi zdarzeń dla zdarzenia DpiChanged.
void DX::DeviceResources::SetDpi(float dpi)
{
	if (dpi != m_dpi)
	{
		m_dpi = dpi;

		// Zmiana rozdzielczości DPI wyświetlacza powoduje zmianę rozmiaru logicznego okna (w jednostkach dip), który musi zostać zaktualizowany.
		m_logicalSize = Windows::Foundation::Size(m_window->Bounds.Width, m_window->Bounds.Height);

		CreateWindowSizeDependentResources();
	}
}

// Ta metoda jest wywoływana w procedurze obsługi zdarzeń dla zdarzenia OrientationChanged.
void DX::DeviceResources::SetCurrentOrientation(DisplayOrientations currentOrientation)
{
	if (m_currentOrientation != currentOrientation)
	{
		m_currentOrientation = currentOrientation;
		CreateWindowSizeDependentResources();
	}
}

// Ta metoda jest wywoływana w procedurze obsługi zdarzeń dla zdarzenia DisplayContentsInvalidated.
void DX::DeviceResources::ValidateDevice()
{
	// Urządzenie D3D nie jest już prawidłowe, jeśli domyślna karta zmieniła się od czasu, gdy urządzenie
	// zostało utworzone lub jeśli urządzenie zostało usunięte.

	//Najpierw pobierz identyfikator LUID domyślnej karty z czasu, gdy urządzenie zostało utworzone.

	DXGI_ADAPTER_DESC previousDesc;
	{
		ComPtr<IDXGIAdapter1> previousDefaultAdapter;
		DX::ThrowIfFailed(m_dxgiFactory->EnumAdapters1(0, &previousDefaultAdapter));

		DX::ThrowIfFailed(previousDefaultAdapter->GetDesc(&previousDesc));
	}

	// Następnie pobierz informacje dla bieżącej domyślnej karty.

	DXGI_ADAPTER_DESC currentDesc;
	{
		ComPtr<IDXGIFactory4> currentDxgiFactory;
		DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&currentDxgiFactory)));

		ComPtr<IDXGIAdapter1> currentDefaultAdapter;
		DX::ThrowIfFailed(currentDxgiFactory->EnumAdapters1(0, &currentDefaultAdapter));

		DX::ThrowIfFailed(currentDefaultAdapter->GetDesc(&currentDesc));
	}

	// Jeśli identyfikatory LUID karty nie są zgodne lub jeśli urządzenie zgłasza, że zostało usunięte,
	// musi zostać utworzone nowe urządzenie D3D.

	if (previousDesc.AdapterLuid.LowPart != currentDesc.AdapterLuid.LowPart ||
		previousDesc.AdapterLuid.HighPart != currentDesc.AdapterLuid.HighPart ||
		FAILED(m_d3dDevice->GetDeviceRemovedReason()))
	{
		m_deviceRemoved = true;
	}
}

// Pokaż zawartość łańcucha wymiany na ekranie.
void DX::DeviceResources::Present()
{
	// Pierwszy argument nakazuje zablokowanie infrastruktury DXGI do czasu włączenia synchronizacji w pionie, umieszczając aplikację
	// uśpienie do następnej synchronizacji w pionie. Dzięki temu cykle nie są używane do renderowania
	// ramek, które nigdy nie będą wyświetlane na ekranie.
	HRESULT hr = m_swapChain->Present(1, 0);

	// Jeśli urządzenie zostało usunięte w wyniku zakończenia połączenia lub uaktualnienia sterownika, 
	// musi odtworzyć wszystkie zasoby urządzenia.
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		m_deviceRemoved = true;
	}
	else
	{
		DX::ThrowIfFailed(hr);

		MoveToNextFrame();
	}
}

// Zaczekaj na ukończenie oczekujących zadań procesora GPU.
void DX::DeviceResources::WaitForGpu()
{
	// Zaplanuj polecenie Signal w kolejce.
	DX::ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_currentFrame]));

	// Zaczekaj na przekroczenie ogranicznika.
	DX::ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_currentFrame], m_fenceEvent));
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	// Zwiększ wartość ogranicznika bieżącej ramki.
	m_fenceValues[m_currentFrame]++;
}

// Przygotuj się do renderowania następnej ramki.
void DX::DeviceResources::MoveToNextFrame()
{
	// Zaplanuj polecenie Signal w kolejce.
	const UINT64 currentFenceValue = m_fenceValues[m_currentFrame];
	DX::ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

	// Przejdź do indeksu ramki.
	m_currentFrame = m_swapChain->GetCurrentBackBufferIndex();

	// Sprawdź, czy następna ramka jest gotowa do uruchomienia.
	if (m_fence->GetCompletedValue() < m_fenceValues[m_currentFrame])
	{
		DX::ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_currentFrame], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	// Ustaw wartość ogranicznika następnej ramki.
	m_fenceValues[m_currentFrame] = currentFenceValue + 1;
}

// Ta metoda określa obrót między orientacją natywną urządzenia wyświetlającego i
// bieżąca orientacja wyświetlania.
DXGI_MODE_ROTATION DX::DeviceResources::ComputeDisplayRotation()
{
	DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_UNSPECIFIED;

	// Uwaga: orientacja natywna może być tylko pozioma lub pionowa, mimo że
	// wyliczenie DisplayOrientations ma inne wartości.
	switch (m_nativeOrientation)
	{
	case DisplayOrientations::Landscape:
		switch (m_currentOrientation)
		{
		case DisplayOrientations::Landscape:
			rotation = DXGI_MODE_ROTATION_IDENTITY;
			break;

		case DisplayOrientations::Portrait:
			rotation = DXGI_MODE_ROTATION_ROTATE270;
			break;

		case DisplayOrientations::LandscapeFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE180;
			break;

		case DisplayOrientations::PortraitFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE90;
			break;
		}
		break;

	case DisplayOrientations::Portrait:
		switch (m_currentOrientation)
		{
		case DisplayOrientations::Landscape:
			rotation = DXGI_MODE_ROTATION_ROTATE90;
			break;

		case DisplayOrientations::Portrait:
			rotation = DXGI_MODE_ROTATION_IDENTITY;
			break;

		case DisplayOrientations::LandscapeFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE270;
			break;

		case DisplayOrientations::PortraitFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE180;
			break;
		}
		break;
	}
	return rotation;
}

// Ta metoda uzyskuje pierwszy dostępny adapter sprzętowy obsługujący program Direct3D 12.
// Jeśli nie można znaleźć takiego adaptera, wyrażenie *ppAdapter zostanie ustawione na wartość nullptr.
void DX::DeviceResources::GetHardwareAdapter(IDXGIAdapter1** ppAdapter)
{
	ComPtr<IDXGIAdapter1> adapter;
	*ppAdapter = nullptr;

	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != m_dxgiFactory->EnumAdapters1(adapterIndex, &adapter); adapterIndex++)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// Nie wybieraj adaptera Podstawowy sterownik renderowania.
			continue;
		}

		// Sprawdź, czy adapter obsługuje program Direct3D 12, lecz nie twórz
		// jeszcze rzeczywistego urządzenia.
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}

	*ppAdapter = adapter.Detach();
}
