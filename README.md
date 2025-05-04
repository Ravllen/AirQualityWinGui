Projekt: Air Quality Monitor

Opis:
Aplikacja desktopowa napisana w języku C++ z użyciem biblioteki WinAPI.
Służy do monitorowania jakości powietrza w Polsce poprzez pobieranie danych z API GIOŚ (Głównego Inspektoratu Ochrony Środowiska).
Umożliwia wybór stacji pomiarowej, analizę pomiarów i wizualizację ich na wykresie.

Funkcje:
- Pobieranie listy stacji z API GIOŚ
- Pobieranie danych pomiarowych (np. PM10, PM2.5)
- Tryb offline z danymi lokalnymi
- Analiza: średnia, minimum, maksimum
- Wizualizacja danych na wykresie (WinAPI GDI+)
- Filtracja danych po dacie

Autor: Mateusz Kruk
Data: 2025-22-04

Uruchomienie:
1. Otwórz projekt w Visual Studio 2022 (lub nowszym)
2. Upewnij się, że środowisko obsługuje C++14
3. Zainstaluj zależności przez vcpkg 
4. Zbuduj i uruchom aplikację

Pliki źródłowe:
- AirQualityWinGui.cpp – GUI i logika główna
- ApiClient.cpp/h – obsługa API i plików lokalnych
- dane.json / stations.json – lokalna baza danych 
