#include "ApiClient.h"  //dołączenie pliku naglowkowego
#include <httplib.h>              // Do łączenia się z API GIOŚ
#include <nlohmann/json.hpp>     // Biblioteka do obsługi JSON
#include <iostream>
#include <fstream>

using json = nlohmann::json;     // Skrót (zamiast całej nazwy wystarcza json)

//Pierwsze metody w kodzie są odpowiedzialne za poprawne pobranie danych dzięki API


/// Pobiera surową odpowiedź JSON ze wszystkimi stacjami.
std::string ApiClient::getAllStationsRaw() {  //zawierać bedzie wszystkie dane stacji pomiarowych
    httplib::Client cli(baseUrl.c_str());                     // Tworzy klienta HTTP
    auto res = cli.Get("/pjp-api/rest/station/findAll");      // Żądanie GET

    if (res && res->status == 200) {                            //Sprawdza czy nie ma błedu
        return res->body;                                     // Zwraca treść odpowiedzi
    }
    else {
        std::cout << "Brak odpowiedzi z serwera!\n";
        return "{}";                                          // Zwraca pusty JSON
    }
}

/// Parsuje listę stacji z JSON do obiektów Station.
std::vector<Station> ApiClient::getAllStations() { //zwraca liste stacji jako wektor obiektów Station
    std::string data = getAllStationsRaw(); //pobiera surowy JSON
    std::vector<Station> stations;  //tworzy wektor obiektow typu Station

    try {
        json parsed = json::parse(data);                      // Parsuje JSON
        for (const auto& s : parsed) { //petla przechodzi przez wszystkie elementy JSON parsed
            Station station;
            station.id = s.value("id", -1);                   // Bezpieczne odczytywanie pól
            station.name = s.value("stationName", "Brak nazwy");
            station.province = s["city"]["commune"].value("provinceName", "Nieznany");  //wojewodztwa lub nieznany
            stations.push_back(station);                      // Dodaje stacje do listy
        }
    }
    catch (const std::exception& e) { //wychwytuje błąd parsowania JSON
        std::cerr << "Błąd JSON: " << e.what() << "\n";
    }

    return stations; //zwraca liste stacji
}

/// Pobiera ID czujników dla wybranej stacji.
std::vector<int> ApiClient::getSensorIdsForStation(int stationId) { // Funkcja pobierająca ID czujników dla danej stacji
    httplib::Client cli(baseUrl.c_str());
    std::string path = "/pjp-api/rest/station/sensors/" + std::to_string(stationId); //tworzy sciezke do czujnikow
    std::vector<int> sensorIds; //wektor na id  czujnikow

    auto res = cli.Get(path.c_str());                         // Zapytanie o czujniki GET

    if (res && res->status == 200) { //sprawdzanie czy odpowiedz jest poprawna
        try {
            auto jsonData = json::parse(res->body); //parsowanie do obiektu
            for (const auto& sensor : jsonData) //petla po wszystkich
                sensorIds.push_back(sensor.value("id", -1));  // Dodaj ID czujnika
        }
        catch (...) {
            std::cerr << "Błąd parsowania czujników.\n";
        }
    }

    return sensorIds;
}

/// Pobiera pomiary ze wszystkich czujników danej stacji.
std::vector<Measurement> ApiClient::getMeasurementsForStation(int stationId) { //pobiera wszystkie pomiary dla danej stacji 
    std::vector<Measurement> results; //wektor na ppomiary stacji
    std::vector<int> sensorIds = getSensorIdsForStation(stationId);
    httplib::Client cli(baseUrl.c_str());

    for (int sensorId : sensorIds) { //iteracja po ID wszystkich czujnikow danej stacji
        std::string path = "/pjp-api/rest/data/getData/" + std::to_string(sensorId);
        auto res = cli.Get(path.c_str()); //zapytanie do API dla czujnika

        if (res && res->status == 200) { //sprawdzenie czy ok
            try {
                auto jsonData = json::parse(res->body); //parsowanie 
                std::string paramName = jsonData.value("key", "Nieznany"); //(key = PM10, Co, NO2)

                for (const auto& val : jsonData["values"]) { //petla przez kazdy obiekt
                    if (!val["value"].is_null()) { //sprawdza czy pole nie jest puste
                        Measurement m;
                        m.name = paramName; //nazwa parametru
                        m.date = val.value("date", "brak daty"); 
                        m.value = val.value("value", 0.0);
                        results.push_back(m); //dodanie pomiaru o wynikow
                    }
                }
            }
            catch (...) {
                std::cerr << "Błąd danych pomiarowych.\n"; 
            }
        }
    }

    return results;
}

//Tutaj zaczyna się zapisywanie i wczytywanie pliku JSON

/// Zapisuje pomiary do pliku JSON w podkluczu stacji.
bool ApiClient::saveMeasurementsToFile(const std::vector<Measurement>& measurements, const std::string& stationId, const std::string& filename) {
    json allData;

    // Wczytaj plik, jeśli istnieje
    std::ifstream inFile(filename);
    if (inFile.is_open()) {
        try { inFile >> allData; } //wczytanie danych do allData
        catch (...) { allData = json::object(); } //jesli blad pusty obiekt JSON
        inFile.close();
    }

    // Konwersja pomiarów do JSON
    json measurementArray;
    for (const auto& m : measurements) { //iterujemy po wszystkich pomiarach w wektorze
        measurementArray.push_back({
            {"name", m.name},
            {"date", m.date},
            {"value", m.value}
            });
    }

    allData[stationId] = measurementArray;  // Zapisz pod kluczem stacji

    // Zapisz do pliku
    try {
        std::ofstream outFile(filename);
        if (!outFile.is_open()) return false;
        outFile << allData.dump(4); //(dump liczba spaccji do wciecia w pliku)
        return true;
    }
    catch (...) {
        return false;
    }
}

/// Wczytuje pomiary z pliku JSON (po kluczu stacji).
std::vector<Measurement> ApiClient::loadMeasurementsFromFile(const std::string& stationId, const std::string& filename) {
    std::vector<Measurement> measurements; //wektor przechowujaca pomiary z pliku
    json j; //zmienna przechowujaca dane JSON

    try {
        std::ifstream file(filename);
        if (!file.is_open()) return measurements;
        file >> j; //wczytanie danych z plikow do json

        if (!j.contains(stationId)) return measurements; //sprawdza czy klucz istnieje

        for (const auto& m : j[stationId]) { //petla po wszysktich pomiarach
            Measurement meas; //pojedyczny pomiar
            meas.name = m.value("name", "Brak");
            meas.date = m.value("date", "brak daty");
            meas.value = m.value("value", 0.0);
            measurements.push_back(meas); //dodajemy stworzony obiek do measurements
        }
    }
    catch (...) {
        std::cerr << "Błąd wczytywania pomiarów z pliku.\n";
    }

    return measurements;
}

/// Zapisuje listę stacji do pliku JSON.
bool ApiClient::saveStationsToFile(const std::vector<Station>& stations, const std::string& filename) {
    json j; //przechowuje dane JSON dla stacji

    for (const auto& s : stations) { //konwersja na plik json
        j.push_back({
            {"id", s.id},
            {"name", s.name},
            {"province", s.province}
            });
    }

    try {
        //zapis do pliku
        std::ofstream file(filename);
        if (!file.is_open()) return false;
        file << j.dump(4); // formatowanie z wcięciami
        return true;
    }
    catch (...) {
        return false;
    }
}

/// Wczytuje stacje z pliku JSON.
std::vector<Station> ApiClient::loadStationsFromFile(const std::string& filename) {
    std::vector<Station> stations;

    try {
        //proba otwarcia pliku
        std::ifstream file(filename);
        if (!file.is_open()) return stations;

        json j;
        file >> j; //wczytanie zawartosci pliku do j

        for (const auto& s : j) { //iteracja po kazdym elemencie
            Station st; //tworzy zmienna do wypelnienia danymi
            st.id = s.value("id", -1);
            st.name = s.value("name", "Brak");
            st.province = s.value("province", "Nieznany");
            stations.push_back(st); //dodaje obiekt st do wektora stations
        }
    }
    catch (...) {
        std::cerr << "Błąd wczytywania stacji z pliku JSON.\n";
    }

    return stations;
}
