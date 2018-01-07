#pragma once

#include <ppltasks.h>	// Dla create_task

namespace DX
{
	inline void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
		{
			// Ustaw punkt przerwania w tym wierszu w celu przechwytywania błędów interfejsu API systemu Win32.
			throw Platform::Exception::CreateException(hr);
		}
	}

	// Funkcja odczytująca asynchronicznie zawartość pliku binarnego.
	inline Concurrency::task<std::vector<byte>> ReadDataAsync(const std::wstring& filename)
	{
		using namespace Windows::Storage;
		using namespace Concurrency;

		auto folder = Windows::ApplicationModel::Package::Current->InstalledLocation;

		return create_task(folder->GetFileAsync(Platform::StringReference(filename.c_str()))).then([](StorageFile^ file)
		{
			return FileIO::ReadBufferAsync(file);
		}).then([](Streams::IBuffer^ fileBuffer) -> std::vector<byte>
		{
			std::vector<byte> returnBuffer;
			returnBuffer.resize(fileBuffer->Length);
			Streams::DataReader::FromBuffer(fileBuffer)->ReadBytes(Platform::ArrayReference<byte>(returnBuffer.data(), fileBuffer->Length));
			return returnBuffer;
		});
	}

	// Konwertuje długość wyrażoną w pikselach niezależnych od urządzenia (DIP) na długość wyrażoną w pikselach fizycznych.
	inline float ConvertDipsToPixels(float dips, float dpi)
	{
		static const float dipsPerInch = 96.0f;
		return floorf(dips * dpi / dipsPerInch + 0.5f); // Zaokrąglij do najbliższej liczby całkowitej.
	}

	// Przypisz nazwę do obiektu, aby pomóc w debugowaniu.
#if defined(_DEBUG)
	inline void SetName(ID3D12Object* pObject, LPCWSTR name)
	{
		pObject->SetName(name);
	}
#else
	inline void SetName(ID3D12Object*, LPCWSTR)
	{
	}
#endif
}

// Nazywanie funkcji pomocniczej dla klasy ComPtr<T>.
// Przypisuje nazwę zmiennej jako nazwę obiektu.
#define NAME_D3D12_OBJECT(x) DX::SetName(x.Get(), L#x)
