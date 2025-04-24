#pragma once //zabezpieczenie przed wieloktronym dołączanie pliku

#include <string> //biblioteka tekstów i znaków
#include <vector> //bliblioteka dynamicznej listy

/// Reprezentuje stację pomiarową.
struct Station {   //struktura stacji popmiarowej 
    int id = -1;   //domyślnie brak danych           ///< Unikalny identyfikator stacji
    std::string name; //przechowuje nazwe stacji       ///< Nazwa stacji (np. "Warszawa - Ursynów")
    std::string province; //przechowuje nazwe wojewodztwa    ///< Nazwa województwa (np. "Mazowieckie")
};

/// Reprezentuje pojedynczy pomiar z czujnika.
struct Measurement { //struktura jednego czujnika(PM10,CO2)
    std::string name; //nazwa miernika     ///< Nazwa mierzonego parametru (np. "PM10")
    std::string date; //data i godzina pomiaru      ///< Data i godzina pomiaru (format: "YYYY-MM-DD HH:MM")
    double value = 0.0;  //wartosc zmierzonego parametru                       ///< Wartość pomiaru w µg/m³
};

/// Klasa do komunikacji z API GIOŚ oraz obsługi danych lokalnych.
class ApiClient {  //klasa odpowiedzialna za komunikacje API z GIOŚ, zapisywanie do bazy lokalnej
public:
    /// Pobiera wszystkie stacje jako surowy JSON (string).
    std::string getAllStationsRaw(); //pobieranie surowych danych do JSON

    /// Pobiera listę stacji pomiarowych z API jako obiekty.
    std::vector<Station> getAllStations(); //parsowanie surowych danych na gotowe obiekty Station

    /// Pobiera identyfikatory czujników (sensorów) dla podanej stacji.
    std::vector<int> getSensorIdsForStation(int stationId); //pobiera identyfikatory czujnikow

    /// Pobiera wszystkie dane pomiarowe dla czujników danej stacji.
    std::vector<Measurement> getMeasurementsForStation(int stationId); //

    /// Zapisuje listę stacji do pliku JSON.
    bool saveStationsToFile(const std::vector<Station>& stations, const std::string& filename);

    /// Wczytuje listę stacji z pliku JSON.
    std::vector<Station> loadStationsFromFile(const std::string& filename);

    /// Zapisuje dane pomiarowe (dla jednej stacji) do pliku JSON.
    bool saveMeasurementsToFile(const std::vector<Measurement>& measurements, const std::string& stationId, const std::string& filename);

    /// Wczytuje dane pomiarowe (dla jednej stacji) z pliku JSON.
    std::vector<Measurement> loadMeasurementsFromFile(const std::string& stationId, const std::string& filename);

private:
    const std::string baseUrl = "http://api.gios.gov.pl"; ///< Bazowy adres API GIOŚ
};
