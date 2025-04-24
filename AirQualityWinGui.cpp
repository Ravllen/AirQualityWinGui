/// \file
/// \brief Aplikacja do monitorowania jakości powietrza w Polsce.
/// \details Program pobiera dane z API GIOŚ, umożliwia analizę i rysowanie wykresu stężenia mierników.

#include <windows.h>    //biblioteka od GUI
#include <string>
#include <vector>
#include <set>          //potrafi automatycznie usuwac duplikaty
#include <sstream>      //budowanie tekstow w pamieci
#include <iomanip>      
#include <algorithm>    //w projekcie tym do sortowania pomiarow wedlug daty
#include "ApiClient.h"

#define IDC_COMBO_STATIONS     1001     //lista rozwijana stacji
#define IDC_COMBO_METRICS      1002     //lista rozwijana miernikow
#define IDC_BUTTON_ANALYZE     1003     //przycisk "Pokaż analize"
#define IDC_EDIT_ANALYSIS      1005     //pole tekstowe z wynikami analizy
#define IDC_BUTTON_CHART       1006     //przycisk pokaż wykres
#define IDC_EDIT_START_DATE    1007     //pole edycji daty poczatkowej
#define IDC_EDIT_END_DATE      1008     //pole edycji daty koncowej

/// \brief Obiekt do komunikacji z API GIOŚ.
ApiClient api;      //tworzy obiekt API

/// \brief Lista dostępnych stacji pomiarowych.
std::vector<Station> stations;  //tworzy wektor do przechowywania

/// \brief Lista wszystkich pomiarów dla wybranej stacji.
std::vector<Measurement> allMeasurements;   //przechowuje dla dane dla stacji

/// \brief Lista dostępnych nazw mierników (np. PM10, PM2.5).
std::vector<std::string> availableMetrics;  //przechowuje liste miernikow

/// \brief Konwertuje std::string (UTF-8) na std::wstring (Unicode).
/// \param str Tekst wejściowy w UTF-8.
/// \return Tekst jako std::wstring w formacie Unicode.
std::wstring stringToWstring(const std::string& str) {  //funkcja konwertująca std::string (UTF-8) na std::wstring (Unicode)
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0); // oblicza długość znakow potrzebnego na wstring (UTF-8 → Unicode)
    std::wstring wstr(len, L'\0');      // tworzy pusty wstring o odpowiedniej długości, wypełniony zerami
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], len);    // konwertuje ciąg znaków UTF-8 na Unicode i zapisuje do wstr
    return wstr;    //zwraca przekonwertowany ciąg
}

/// \brief Sprawdza, czy dana data mieści się w zakresie podanym przez start i end.
/// \param date Data pomiaru.
/// \param start Data początkowa.
/// \param end Data końcowa.
/// \return true, jeśli data mieści się w zakresie.
bool isWithinRange(const std::string& date, const std::string& start, const std::string& end) {     // sprawdza czy data mieści się w zakresie start–end
    return (start.empty() || date >= start) && (end.empty() || date <= end);        // true jeśli data jest w zakresie lub jeśli brak ograniczenia (puste pola)
}

/// \brief Filtruje pomiary na podstawie nazwy miernika i zakresu dat.
/// \param metric Nazwa miernika (np. PM10).
/// \param startDate Data początkowa.
/// \param endDate Data końcowa.
/// \return Wektor przefiltrowanych pomiarów.
std::vector<Measurement> FilterMeasurements(const std::string& metric, const std::string& startDate, const std::string& endDate) {      // Funkcja filtruje pomiary wg miernika i daty
    std::vector<Measurement> filtered;      // Wektor na przefiltrowane pomiary
    for (const auto& m : allMeasurements)   // Iteracja po wszystkich pomiarach stacji
        if (m.name == metric && isWithinRange(m.date, startDate, endDate))      // Sprawdza nazwę miernika i czy mieści się w zakresie dat
            filtered.push_back(m);      // Dodaj pomiar do listy wyników
    std::sort(filtered.begin(), filtered.end(), [](const Measurement& a, const Measurement& b) {        // Sortuj pomiary rosnąco według daty
        return a.date < b.date;
        });
    return filtered;  //zwraca pomiary
}

/// \brief Wyświetla komunikat o pracy w trybie offline.
void ShowOfflineWarning() {     // Funkcja pokazuje komunikat o trybie offline
    MessageBox(NULL,            //tworzy okno z komunikatem ostrzeżenia
        L"Nie udało się pobrać danych z internetu.\nZostaną użyte dane z lokalnej bazy.\n\nTryb offline: niektóre funkcje mogą być ograniczone.",
        L"Tryb offline",
        MB_ICONWARNING | MB_OK);    //typ okna: ikonka ostrzezenia i przycisk ok
}

/// \brief Procedura obsługi okna wykresu.
/// \param hwnd Uchwyt do okna.
/// \param msg Typ komunikatu.
/// \param wParam Parametr komunikatu.
/// \param lParam Parametr komunikatu.
/// \return Wynik obsługi komunikatu.
LRESULT CALLBACK ChartWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {  // Procedura obsługi i budowania wykresu
    static std::vector<Measurement>* data; //wskaźnik do danych pomiarów do wykresu
    static std::string metricName;  //nazwa miernika do tytułu wykresu

    if (msg == WM_CREATE) {  //sprawdzenie czy tworzymy okno 
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;       //dostęp do danych przekazanych przy tworzeniu okna
        data = (std::vector<Measurement>*)cs->lpCreateParams;       //pobiera dane pomiarowe przekazane przy tworzeniu okna
        metricName = data->empty() ? "Wykres" : data->at(0).name;   //jesli nie ma miernika to Wykres
        return 0;
    }

    if (msg == WM_PAINT) {      // Obsługa rysowania wykresu
        PAINTSTRUCT ps;     // Struktura do przechowywania info o rysowaniu
        HDC hdc = BeginPaint(hwnd, &ps);    // Start rysowania
        RECT rect;      //struktura przechowująca prostokąt gdzie bedzie rysowany wykres
        GetClientRect(hwnd, &rect);     // Pobierz rozmiar obszaru roboczego okna

        const int padding = 60;     // Margines wewnętrzny od krawędzi okna
        int width = rect.right - rect.left - 2 * padding;   // Szerokość obszaru wykresu (z marginesami)
        int height = rect.bottom - rect.top - 2 * padding;  // Wysokość obszaru wykresu (z marginesami)

        if (data->size() < 2) {     //sprawdza czy wystarczy danych do rysowania wykresu 
            TextOut(hdc, padding, padding, L"Za mało danych do wyświetlenia wykresu.", 37);
            EndPaint(hwnd, &ps);
            return 0;
        }

        double minVal = data->at(0).value, maxVal = data->at(0).value;  // Inicjalizacja wartości min i max
        for (const auto& m : *data) {       // Przejdź po wszystkich pomiarach
            if (m.value < minVal) minVal = m.value;
            if (m.value > maxVal) maxVal = m.value;
        }

        MoveToEx(hdc, padding, padding + height, NULL);     // Ustaw początek osi X
        LineTo(hdc, padding + width, padding + height);     // Rysuj oś X (poziomą)
        LineTo(hdc, padding + width, padding);              // Rysuj oś Y (pionową)

        int count = data->size();       // Liczba punktów pomiarowych
        for (int i = 0; i < count - 1; ++i) {       //rysowanie linii wykresu
            int x1 = padding + (i * width) / (count - 1);   // Pozycja X pierwszego punktu
            int y1 = padding + height - (int)((data->at(i).value - minVal) * height / (maxVal - minVal));       // Pozycja Y pierwszego punktu
            int x2 = padding + ((i + 1) * width) / (count - 1);     // Pozycja X kolejnego punktu
            int y2 = padding + height - (int)((data->at(i + 1).value - minVal) * height / (maxVal - minVal));   // Pozycja Y kolejnego punktu
            MoveToEx(hdc, x1, y1, NULL);    // Ustaw start rysowania linii
            LineTo(hdc, x2, y2);    // Rysuj linię do kolejnego punktu
        }

        for (int i = 0; i <= 5; ++i) {      //rysowanie linii pomocniczych
            int y = padding + i * height / 5;   // Oblicz pozycję Y dla linii siatki
            MoveToEx(hdc, padding, y, NULL);    // Ustaw początek linii siatki
            LineTo(hdc, padding + width, y);    // Rysuj poziomą linię siatki
            double val = maxVal - i * (maxVal - minVal) / 5;    // Oblicz wartość liczbową dla linii siatki
            std::wstringstream ss;  // Strumień do budowy tekstu (na osi Y)
            ss << std::fixed << std::setprecision(1) << val;        //zaokrąglenie
            TextOut(hdc, 5, y - 10, ss.str().c_str(), ss.str().length());   // Wyświetl wartość
        }

        int step = (count > 10) ? count / 10 : 1;   // Co ile punktów pokazywać godziny
        for (int i = 0; i < count; i += step) {     // Pętla do rysowania etykiet czasu na osi X
            int x = padding + (i * width) / (count - 1);        // Oblicz pozycję X dla etykiety godziny
            std::wstring hour = stringToWstring(data->at(i).date.substr(11, 5));        //godzina i data na wstring
            TextOut(hdc, x - 15, padding + height + 5, hour.c_str(), hour.length());    // Wyświetl godzinę
        }

        std::string startDate = data->front().date;     //data pierwszego pomiaru
        std::string endDate = data->back().date;        //data ostatniego pomiaru
        std::wstring title = stringToWstring("Wykres " + metricName + " [" + startDate + " - " + endDate + "]"); // Stwórz tytuł wykresu z nazwą miernika i datami
        TextOut(hdc, padding + 80, 10, title.c_str(), title.length());      // Wyświetl tytuł wykresu

        TextOut(hdc, 10, padding - 30, L"Stężenie [µg/m³]", 17);    // Wyświetl etykietę osi Y (jednostki)
        TextOut(hdc, padding + width / 2 - 15, padding + height + 30, L"Czas", 4);  // Wyświetl etykietę osi X (Czas)

        EndPaint(hwnd, &ps);    //koniec rysowania
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);    // Przekazuje komunikat do domyślnej procedury okna
}

/// \brief Tworzy i wyświetla okno wykresu z danymi pomiarowymi.
/// \param data Wektor pomiarów do przedstawienia na wykresie.
void ShowChartWindow(const std::vector<Measurement>& data) {    // Funkcja do wyświetlania okna z wykresem
    WNDCLASS wc = {}; // Zerowanie wszystkich pól struktury klasy okna przed wypełnieniem
    wc.lpfnWndProc = ChartWndProc;      // Ustaw procedurę obsługi zdarzeń okna wykresu
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"ChartWindowClass";     // Ustaw nazwę klasy okna
    RegisterClass(&wc);     // Rejestruje klasę okna w systemi

    HWND hwnd = CreateWindowEx(0, L"ChartWindowClass", L"Wykres pomiarów",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 500,
        NULL, NULL, GetModuleHandle(NULL), (LPVOID)&data);      //tworzy nowe okno, ustawia szerokosc i wysokosc, styl, nadaje tytul

    ShowWindow(hwnd, SW_SHOW);      // Pokaż okno wykresu
    UpdateWindow(hwnd);       // Wymuś odświeżenie okna

    MSG msg;       //przechownie komunikatu
    while (GetMessage(&msg, nullptr, 0, 0)) {   //Petla komunikatow (czeka na komunikat od systemu np. kliknieciie, odswiezenie)
        TranslateMessage(&msg);     //tlumaczy komunikaty klawiatury na czytelne znaki
        DispatchMessage(&msg);      // wysyla komunikat do pordecury ChartWndProc
    }
}

/// \brief Procedura obsługi głównego okna aplikacji.
/// \param hwnd Uchwyt do okna.
/// \param msg Typ komunikatu.
/// \param wParam Parametr komunikatu.
/// \param lParam Parametr komunikatu.
/// \return Wynik obsługi komunikatu.
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) { //odpowiada za wszystkie zdarzenia glownego okna, tworzenie przyciskow kontrolek
    static HWND hComboStations, hComboMetrics, hButtonAnalyze, hButtonChart, hEditAnalysis, hEditStartDate, hEditEndDate;   //deklaracja zmiennych

    switch (msg) {  //rozpoczęcie obslugi roznych typow komunikatow
    case WM_CREATE:
        //lista rozwijana stacji
        hComboStations = CreateWindow(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            50, 30, 500, 200, hwnd, (HMENU)IDC_COMBO_STATIONS, NULL, NULL);
        //lista rozwijana miernikow
        hComboMetrics = CreateWindow(L"COMBOBOX", NULL, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            50, 70, 500, 200, hwnd, (HMENU)IDC_COMBO_METRICS, NULL, NULL);
        //pole do wpisania daty od
        CreateWindow(L"STATIC", L"Data od:", WS_CHILD | WS_VISIBLE, 50, 100, 80, 20, hwnd, NULL, NULL, NULL);
        hEditStartDate = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", NULL, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            150, 100, 200, 20, hwnd, (HMENU)IDC_EDIT_START_DATE, NULL, NULL);
        //pole do wpisania daty do
        CreateWindow(L"STATIC", L"Data do:", WS_CHILD | WS_VISIBLE, 50, 130, 80, 20, hwnd, NULL, NULL, NULL);
        hEditEndDate = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", NULL, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            150, 130, 200, 20, hwnd, (HMENU)IDC_EDIT_END_DATE, NULL, NULL);
        //przycisk "Pokaz Analize"
        hButtonAnalyze = CreateWindow(L"BUTTON", L"Pokaż analizę", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            50, 160, 150, 30, hwnd, (HMENU)IDC_BUTTON_ANALYZE, NULL, NULL);
        //Przycisk "Pokaz wykres"
        hButtonChart = CreateWindow(L"BUTTON", L"Pokaż wykres", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            220, 160, 150, 30, hwnd, (HMENU)IDC_BUTTON_CHART, NULL, NULL);
        //Pole tekstowe z wynikami analizy
        hEditAnalysis = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", NULL,
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            50, 210, 500, 200, hwnd, (HMENU)IDC_EDIT_ANALYSIS, NULL, NULL);

        stations = api.getAllStations();    // Pobierz listę wszystkich stacji z API GIOŚ
        if (stations.empty()) { //Jesli nie udalo sie pobrac z API to wczytujemy z bazy
            stations = api.loadStationsFromFile("stations.json");
            ShowOfflineWarning();
        }
        else {      //Gdy pobranie się powiodlo
            api.saveStationsToFile(stations, "stations.json");
        }
        
        for (const auto& s : stations) { //Petla ktora wypełnia liste nazwami i wojewodztwami danej stacji
            std::wstring name = stringToWstring(s.name + " (" + s.province + ")");
            SendMessage(hComboStations, CB_ADDSTRING, 0, (LPARAM)name.c_str());
        }
        SendMessage(hComboStations, CB_SETCURSEL, 0, 0);  //domyślnie ustawia pierwszą stacje
        break;

    case WM_COMMAND:    //Obsługa zdarzeń kontrolek (np. zmiana wyboru, kliknięcie przycisku)
        if (LOWORD(wParam) == IDC_COMBO_STATIONS && HIWORD(wParam) == CBN_SELCHANGE) {      // Obsługa zdarzenia zmiany wyboru stacji
            int idx = SendMessage(hComboStations, CB_GETCURSEL, 0, 0);   // Pobierz indeks wybranej stacji
            if (idx < 0) break;     // Jeśli nic nie wybrano, przerwij obsługę
            int stationId = stations[idx].id;       // Pobierz ID wybranej stacji
            allMeasurements = api.getMeasurementsForStation(stationId); // Pobierz pomiary dla wybranej stacji z API
            if (allMeasurements.empty()) {  // Wczytaj dane lokalnie jesli nie udalo sie z API
                allMeasurements = api.loadMeasurementsFromFile(std::to_string(stationId), "dane.json");
                ShowOfflineWarning();   // Pokaż komunikat, że działamy w trybie offline
            }
            else {      //jesli API działa
                api.saveMeasurementsToFile(allMeasurements, std::to_string(stationId), "dane.json");    // Zapisz pobrane dane do pliku
            }

            std::set<std::string> unique;   //unikalne nazwy merników
            for (const auto& m : allMeasurements) unique.insert(m.name);    // Zbierz unikalne nazwy mierników
            availableMetrics.assign(unique.begin(), unique.end());  // Przenieś unikalne mierniki do wektora

            SendMessage(hComboMetrics, CB_RESETCONTENT, 0, 0);  // Wyczyść listę mierników w comboboxie
            for (const auto& m : availableMetrics)
                SendMessage(hComboMetrics, CB_ADDSTRING, 0, (LPARAM)stringToWstring(m).c_str());    // Dodaj każdy miernik do listy
            SendMessage(hComboMetrics, CB_SETCURSEL, 0, 0);      // Ustaw pierwszy miernik jako domyślnie wybrany
        }

        if (LOWORD(wParam) == IDC_BUTTON_ANALYZE || LOWORD(wParam) == IDC_BUTTON_CHART) {  //sprawdza czy uzytkownik kiknal w jeden z dwoch przyciskow
            int mIdx = SendMessage(hComboMetrics, CB_GETCURSEL, 0, 0);      // Pobierz indeks wybranego miernika
            if (mIdx < 0 || mIdx >= availableMetrics.size()) break;     //sprawdza czy uzytkownik na pewno wybral jakis miernik
            char startBuf[32], endBuf[32];      //bufory na daty
            GetWindowTextA(hEditStartDate, startBuf, 32);       // Pobierz tekst z pola "Data od"
            GetWindowTextA(hEditEndDate, endBuf, 32);       // Pobierz tekst z pola "Data do"

            auto filtered = FilterMeasurements(availableMetrics[mIdx], startBuf, endBuf);       // Filtrowanie danych wg miernika i zakresu dat

            if (LOWORD(wParam) == IDC_BUTTON_ANALYZE) {     //sprawdza czy uzytkownik klkinal analize
                if (filtered.size() < 2) {      //Sprawdza czy wystarczy danych
                    SetWindowTextA(hEditAnalysis, "Za mało danych.");
                    break;
                }

                double sum = 0, min = filtered[0].value, max = filtered[0].value;       //srednia, min, max
                std::string dmin = filtered[0].date, dmax = filtered[0].date;           //daty do min i max
                for (const auto& m : filtered) {  //przygotowanie powyzszych danych do analizy
                    sum += m.value;
                    if (m.value < min) { min = m.value; dmin = m.date; }
                    if (m.value > max) { max = m.value; dmax = m.date; }
                }

                std::string firstDate = filtered.front().date;  //data pierwszego pomiaru
                std::string lastDate = filtered.back().date;    //data ostatniego pomiaru  
                std::string trend;
                //Oblicza tendencję zmian stężenia na podstawie pierwszego i ostatniego pomiaru
                if (filtered.front().value < filtered.back().value)
                    trend = "Tendencja: wzrostowa ";
                else if (filtered.front().value > filtered.back().value)
                    trend = "Tendencja: malejąca ";
                else
                    trend = "Tendencja: brak zmian";

                std::ostringstream oss;
                oss << std::fixed << std::setprecision(2);
                oss << "Analiza - " << availableMetrics[mIdx] << "\r\n"
                    << "Zakres: " << firstDate << " - " << lastDate << "\r\n"
                    << "Średnia: " << sum / filtered.size() << " µg/m^3\r\n"
                    << "Min: " << min << " (" << dmin << ")\r\n"
                    << "Max: " << max << " (" << dmax << ")\r\n"
                    << trend << "\r\n";

                SetWindowTextA(hEditAnalysis, oss.str().c_str());   //Wyświetl analizę w polu tekstowym
            }
            else {
                if (filtered.size() >= 2) ShowChartWindow(filtered);// Jeśli kliknięto „Pokaż wykres” i są dane, otwórz okno
            }
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0); break;      // Zakończ aplikację, wyślij komunikat WM_QUIT
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);    // Domyślna obsługa komunikatów nieobsłużonych
}

/// \brief Główna funkcja aplikacji WinAPI.
/// \param hInstance Uchwyt do bieżącej instancji aplikacji.
/// \param hPrevInstance (niewykorzystywany).
/// \param lpCmdLine Argumenty z linii poleceń.
/// \param nCmdShow Parametr określający sposób wyświetlania okna.
/// \return Kod zakończenia programu.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {   // Główna funkcja uruchamiająca aplikację Windows
    SetConsoleOutputCP(CP_UTF8);    // Ustaw kodowanie konsoli na UTF-8
    WNDCLASS wc = { };      // Zainicjalizuj strukturę klasy okna zerami
    wc.lpfnWndProc = WndProc;   //odpowiedzialna za przetwarzanie wszystkich komunikatów 
    wc.hInstance = hInstance;   // Ustaw uchwyt bieżącej aplikacji
    wc.lpszClassName = L"AQClass";  // Ustaw unikalną nazwę klasy okna
    RegisterClass(&wc);     // Zarejestruj klasę okna w systemie
    
    HWND hwnd = CreateWindowEx(0, L"AQClass", L"Air Quality Monitor",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640, 500,
        NULL, NULL, hInstance, NULL);       //tworzy glowne okno aplikacji

    ShowWindow(hwnd, nCmdShow);     // Pokaż główne okno aplikacji na ekranie
    UpdateWindow(hwnd);     // Wymuś natychmiastowe odmalowanie okna (wywołuje WM_PAINT)

    MSG msg;    // Struktura do przechowywania komunikatów systemowych
    while (GetMessage(&msg, nullptr, 0, 0)) {  //czeka na klikniecie
        TranslateMessage(&msg);     //przeksztalca surowe dane z klawiatury na dane tekstowe
        DispatchMessage(&msg);      //wysyla komunikat do funkcji WndProc
    }
    return 0;
}