#pragma once

#include <wrl.h>

namespace DX
{
	// Klasa pomocnicza używana na potrzeby synchronizacji animacji i symulacji.
	class StepTimer
	{
	public:
		StepTimer() : 
			m_elapsedTicks(0),
			m_totalTicks(0),
			m_leftOverTicks(0),
			m_frameCount(0),
			m_framesPerSecond(0),
			m_framesThisSecond(0),
			m_qpcSecondCounter(0),
			m_isFixedTimeStep(false),
			m_targetElapsedTicks(TicksPerSecond / 60)
		{
			if (!QueryPerformanceFrequency(&m_qpcFrequency))
			{
				throw ref new Platform::FailureException();
			}

			if (!QueryPerformanceCounter(&m_qpcLastTime))
			{
				throw ref new Platform::FailureException();
			}

			// Zainicjuj maksymalną różnicę równą 1/10 sekundy.
			m_qpcMaxDelta = m_qpcFrequency.QuadPart / 10;
		}

		// Pobierz czas, który upłynął od poprzedniego wywołania funkcji Update.
		uint64 GetElapsedTicks() const						{ return m_elapsedTicks; }
		double GetElapsedSeconds() const					{ return TicksToSeconds(m_elapsedTicks); }

		// Pobierz całkowity czas od uruchomienia programu.
		uint64 GetTotalTicks() const						{ return m_totalTicks; }
		double GetTotalSeconds() const						{ return TicksToSeconds(m_totalTicks); }

		// Pobierz całkowitą liczbę aktualizacji od czasu uruchomienia programu.
		uint32 GetFrameCount() const						{ return m_frameCount; }

		// Pobierz bieżącą szybkość klatek.
		uint32 GetFramesPerSecond() const					{ return m_framesPerSecond; }

		// Określ, czy ma być używany tryb stałych czy zmiennych kroków czasu.
		void SetFixedTimeStep(bool isFixedTimestep)			{ m_isFixedTimeStep = isFixedTimestep; }

		// Określ częstotliwość wywoływania funkcji Update w trybie stałych kroków czasu.
		void SetTargetElapsedTicks(uint64 targetElapsed)	{ m_targetElapsedTicks = targetElapsed; }
		void SetTargetElapsedSeconds(double targetElapsed)	{ m_targetElapsedTicks = SecondsToTicks(targetElapsed); }

		// Format liczby całkowitej reprezentuje czas, używając 10 000 000 impulsów na sekundę.
		static const uint64 TicksPerSecond = 10000000;

		static double TicksToSeconds(uint64 ticks)			{ return static_cast<double>(ticks) / TicksPerSecond; }
		static uint64 SecondsToTicks(double seconds)		{ return static_cast<uint64>(seconds * TicksPerSecond); }

		// Po celowym przerwaniu chronometrażu (na przykład blokującej operacji we/wy)
		// wywołaj ten element, aby zapobiec próbie użycia przez logikę stałych kroków czasu zestawu wyrównywania 
		// Wywołania funkcji Update.

		void ResetElapsedTime()
		{
			if (!QueryPerformanceCounter(&m_qpcLastTime))
			{
				throw ref new Platform::FailureException();
			}

			m_leftOverTicks = 0;
			m_framesPerSecond = 0;
			m_framesThisSecond = 0;
			m_qpcSecondCounter = 0;
		}

		// Zaktualizuj stan czasomierza, wywołując określoną funkcję Update odpowiednią liczbę razy.
		template<typename TUpdate>
		void Tick(const TUpdate& update)
		{
			// Wykonaj zapytanie dotyczące bieżącego czasu.
			LARGE_INTEGER currentTime;

			if (!QueryPerformanceCounter(&currentTime))
			{
				throw ref new Platform::FailureException();
			}

			uint64 timeDelta = currentTime.QuadPart - m_qpcLastTime.QuadPart;

			m_qpcLastTime = currentTime;
			m_qpcSecondCounter += timeDelta;

			// Ogranicz bardzo duże różnice czasu (np. po wstrzymaniu w debugerze).
			if (timeDelta > m_qpcMaxDelta)
			{
				timeDelta = m_qpcMaxDelta;
			}

			// Przekonwertuj jednostki funkcji QPC na format impulsów kanonicznych. Przepełnienie nie jest możliwe ze względu na poprzednie ograniczenie.
			timeDelta *= TicksPerSecond;
			timeDelta /= m_qpcFrequency.QuadPart;

			uint32 lastFrameCount = m_frameCount;

			if (m_isFixedTimeStep)
			{
				// Logika aktualizacji stałych kroków czasu

				//Jeśli aplikacja zbliża się do docelowego czasu trwania (w ciągu 1/4 milisekundy), włącz ograniczenie
				// zegara, aby dokładnie dopasować wartość do wartości docelowej. Zapobiega to drobnym i mniej ważnym błędom
				// przed nagromadzeniem. Bez tego ograniczenia gra żądająca 60 klatek na sekundę
				// stała aktualizacja z włączoną synchronizacją w pionie na wyświetlaczu NTSC 59,94 doprowadzi do
				// takiej liczby niewielkich błędów, która spowoduje pominięcie ramki. Lepszym rozwiązaniem jest zaokrąglenie 
				// małe odchylenia do zera, aby zachować poprawne działanie.

				if (abs(static_cast<int64>(timeDelta - m_targetElapsedTicks)) < TicksPerSecond / 4000)
				{
					timeDelta = m_targetElapsedTicks;
				}

				m_leftOverTicks += timeDelta;

				while (m_leftOverTicks >= m_targetElapsedTicks)
				{
					m_elapsedTicks = m_targetElapsedTicks;
					m_totalTicks += m_targetElapsedTicks;
					m_leftOverTicks -= m_targetElapsedTicks;
					m_frameCount++;

					update();
				}
			}
			else
			{
				// Logika aktualizacji zmiennych kroków czasu.
				m_elapsedTicks = timeDelta;
				m_totalTicks += timeDelta;
				m_leftOverTicks = 0;
				m_frameCount++;

				update();
			}

			// Śledź bieżącą szybkość klatek.
			if (m_frameCount != lastFrameCount)
			{
				m_framesThisSecond++;
			}

			if (m_qpcSecondCounter >= static_cast<uint64>(m_qpcFrequency.QuadPart))
			{
				m_framesPerSecond = m_framesThisSecond;
				m_framesThisSecond = 0;
				m_qpcSecondCounter %= m_qpcFrequency.QuadPart;
			}
		}

	private:
		// Źródłowe dane chronometrażu używają jednostek funkcji QPC.
		LARGE_INTEGER m_qpcFrequency;
		LARGE_INTEGER m_qpcLastTime;
		uint64 m_qpcMaxDelta;

		// Pochodne dane chronometrażu używają formatu impulsów kanonicznych.
		uint64 m_elapsedTicks;
		uint64 m_totalTicks;
		uint64 m_leftOverTicks;

		// Składowe do śledzenia szybkości klatek.
		uint32 m_frameCount;
		uint32 m_framesPerSecond;
		uint32 m_framesThisSecond;
		uint64 m_qpcSecondCounter;

		// Składowe do konfigurowania trybu stałych kroków czasu.
		bool m_isFixedTimeStep;
		uint64 m_targetElapsedTicks;
	};
}
