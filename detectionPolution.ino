/*
 * Projet de surveillance de la qualité de l'eau
 * Auteur : [zakariyae oraiche]
 * Date : [20/05/2025]
 * Version : 1.0
 * 
 * Matériel requis :
 * - ESP32
 * - Capteur DS18B20 (Température)
 * - Capteur TDS
 * - Capteur de Turbidité
 * - Module RTC DS3231
 * - Écran LCD 20x4 I2C
 */


#include <WiFi.h>                // Pour la connexion WiFi
#include <OneWire.h>             // Protocole pour le capteur de température
#include <DallasTemperature.h>   // Bibliothèque du capteur DS18B20
#include <EEPROM.h>              // Pour le stockage interne
#include <WebServer.h>           // Serveur web intégré
#include <LiquidCrystal_I2C.h>   // Contrôle de l'écran LCD
#include <RTClib.h>              // Pour le module RTC

// Définition des broches
#define ONE_WIRE_BUS 4   // Broche du bus OneWire pour le DS18B20
#define TDS_PIN 34       // Entrée analogique pour le TDS
#define TURBIDITY_PIN 35 // Entrée analogique pour la turbidité

// Initialisation des capteurs
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
LiquidCrystal_I2C lcd(0x27, 20, 4); // Adresse I2C de l'écran
RTC_DS3231 rtc; // Module RTC pour l'horodatage

// Configuration WiFi
const char* ssid = "ADOC 4G";       // Nom du réseau
const char* password = "0102030405"; // Mot de passe WiFi

WebServer server(80); // Création du serveur web sur le port 80

// Structure pour le stockage des données en EEPROM
struct DataEntry {
  uint32_t unixTime;     // Horodatage en secondes (epoch UNIX)
  float temperature;     // Température en °C
  int tds;               // Valeur TDS brute
  int turbidity;         // Valeur turbidité brute
  bool valid = false;    // Marqueur de données valides
};

// Configuration de la mémoire
const int ENTRIES_PER_DAY = 48;    // 48 mesures = 1 par heure pendant 2 jours
const int ENTRY_SIZE = sizeof(DataEntry);
const int EEPROM_SIZE = ENTRIES_PER_DAY * ENTRY_SIZE; // Taille totale nécessaire
int currentIndex = 0; // Index courant pour l'écriture cyclique

// Variables des capteurs
float temperature = 0;
int tdsValue = 0;
int turbidityValue = 0;

/*----------------------------------------------------------
 * Fonction : Lecture des valeurs des capteurs
 *--------------------------------------------------------*/
void readSensors() {
  // Lecture de la température
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0);
  
  // Lecture analogique des autres capteurs
  tdsValue = analogRead(TDS_PIN);
  turbidityValue = analogRead(TURBIDITY_PIN);
}

/*----------------------------------------------------------
 * Fonction : Sauvegarde des données dans l'EEPROM
 *--------------------------------------------------------*/
void saveData() {
  DataEntry entry;
  entry.unixTime = rtc.now().unixtime(); // Horodatage actuel
  entry.temperature = temperature;
  entry.tds = tdsValue;
  entry.turbidity = turbidityValue;
  entry.valid = true;

  // Écriture à la position courante
  int addr = currentIndex * ENTRY_SIZE;
  EEPROM.put(addr, entry);
  EEPROM.commit();

  // Gestion cyclique de l'index
  currentIndex = (currentIndex + 1) % ENTRIES_PER_DAY;
}

/*----------------------------------------------------------
 * Fonction : Mise à jour de l'affichage LCD
 *--------------------------------------------------------*/
void updateLCD() {
  lcd.clear();
  
  // Détermination de l'état de l'eau
  String waterStatus = "";
  if (tdsValue < 300 && turbidityValue < 1000) {
    waterStatus = "It's clear";
  } else if (tdsValue < 600 || turbidityValue < 2000) {
    waterStatus = "It's cloudy";
  } else {
    waterStatus = "It's dirty";
  }

  // Affichage des valeurs
  lcd.setCursor(0, 0);
  lcd.print("Status: " + waterStatus);
  lcd.setCursor(0, 1);
  lcd.print("Temp: " + String(temperature, 1) + " C");
  lcd.setCursor(0, 2);
  lcd.print("TDS: " + String(tdsValue) + " ppm");
  lcd.setCursor(0, 3);
  lcd.print("Turb: " + String(turbidityValue) + " NTU");
}

/*----------------------------------------------------------
 * Fonction : Gestion de la page web principale
 *--------------------------------------------------------*/
void handleRoot() {
  // Envoi de la page HTML complète avec CSS intégré
  String html = R"rawliteral(
 
  <!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>AquaMonitor - Qualité de l'eau</title>
    <style>
        /* Styles de base */
        body {
            font-family: 'Segoe UI', sans-serif;
            margin: 0;
            padding: 20px;
            background: linear-gradient(135deg, #f0f9ff 0%, #e6f4ff 100%);
            min-height: 100vh;
        }

        /* Carte principale */
        .dashboard {
            max-width: 800px;
            margin: 2rem auto;
            background: rgba(255, 255, 255, 0.95);
            border-radius: 20px;
            padding: 30px;
            box-shadow: 0 10px 30px rgba(0, 0, 0, 0.1);
            backdrop-filter: blur(10px);
        }

        /* En-tête */
        .header {
            text-align: center;
            margin-bottom: 2rem;
        }

        .header h1 {
            color: #2c3e50;
            font-size: 2.5em;
            margin: 0;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 10px;
        }

        .header h1 i {
            color: #3498db;
        }

        /* Cartes de mesures */
        .metrics {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 20px;
            margin-bottom: 2rem;
        }

        .metric-card {
            background: #fff;
            padding: 25px;
            border-radius: 15px;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.05);
            transition: transform 0.3s ease;
        }

        .metric-card:hover {
            transform: translateY(-5px);
        }

        .metric-value {
            font-size: 2.2em;
            font-weight: 600;
            color: #3498db;
            margin: 10px 0;
        }

        .metric-label {
            color: #7f8c8d;
            font-size: 1.1em;
        }

        /* Bouton */
        .btn {
            background: linear-gradient(135deg, #3498db 0%, #2980b9 100%);
            color: white;
            border: none;
            padding: 15px 30px;
            border-radius: 50px;
            font-size: 1.1em;
            cursor: pointer;
            transition: all 0.3s ease;
            display: flex;
            align-items: center;
            gap: 10px;
            margin: 0 auto;
        }

        .btn:hover {
            transform: scale(1.05);
            box-shadow: 0 5px 15px rgba(52, 152, 219, 0.4);
        }

        /* Tableau historique */
        .history-table {
            width: 100%;
            border-collapse: collapse;
            margin-top: 2rem;
            background: white;
            border-radius: 15px;
            overflow: hidden;
            box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
        }

        .history-table th,
        .history-table td {
            padding: 15px;
            text-align: left;
        }

        .history-table th {
            background: #3498db;
            color: white;
            font-weight: 600;
        }

        .history-table tr:nth-child(even) {
            background-color: #f8f9fa;
        }

        /* Animations */
        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(20px); }
            to { opacity: 1; transform: translateY(0); }
        }

        .animated {
            animation: fadeIn 0.6s ease-out;
        }

        /* Responsive */
        @media (max-width: 768px) {
            .metrics {
                grid-template-columns: 1fr;
            }
            
            .dashboard {
                margin: 1rem;
                padding: 20px;
            }
            
            .metric-value {
                font-size: 1.8em;
            }
        }
    </style>
    <!-- Icônes Font Awesome -->
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css">
</head>
<body>
    <div class="dashboard animated">
        <div class="header">
            <h1><i class="fas fa-water"></i>Surveillance en Temps Réel</h1>
        </div>

        <div class="metrics">
            <div class="metric-card">
                <div class="metric-label"><i class="fas fa-thermometer-half"></i> Température</div>
                <div class="metric-value" id="temp">--°C</div>
            </div>
            
            <div class="metric-card">
                <div class="metric-label"><i class="fas fa-flask"></i> TDS</div>
                <div class="metric-value" id="tds">-- ppm</div>
            </div>
            
            <div class="metric-card">
                <div class="metric-label"><i class="fas fa-tint"></i> Turbidité</div>
                <div class="metric-value" id="turb">-- NTU</div>
            </div>
        </div>

        <button class="btn" onclick="loadHistory()">
            <i class="fas fa-history"></i>
            Afficher l'historique
        </button>

        <div id="history"></div>
    </div>

    <script>
        function updateData() {
            fetch('/data')
            .then(r => r.json())
            .then(data => {
                document.getElementById('temp').textContent = `${data.temperature.toFixed(1)}°C`;
                document.getElementById('tds').textContent = `${data.tds} ppm`;
                document.getElementById('turb').textContent = `${data.turbidity} NTU`;
            });
        }

        function loadHistory() {
            fetch('/history')
            .then(r => r.json())
            .then(data => {
                let html = `
                    <table class="history-table">
                        <thead>
                            <tr>
                                <th>Date</th>
                                <th>Température</th>
                                <th>TDS</th>
                                <th>Turbidité</th>
                            </tr>
                        </thead>
                        <tbody>`;

                data.forEach(row => {
                    const date = new Date(row.time * 1000);
                    html += `
                        <tr>
                            <td>${date.toLocaleDateString()} ${date.toLocaleTimeString()}</td>
                            <td>${row.temp.toFixed(1)}°C</td>
                            <td>${row.tds} ppm</td>
                            <td>${row.turb} NTU</td>
                        </tr>`;
                });

                html += `</tbody></table>`;
                document.getElementById('history').innerHTML = html;
            });
        }

        setInterval(updateData, 1000);
        updateData();
    </script>
</body>
</html>
 
  )rawliteral";

  server.send(200, "text/html", html);
}

/*----------------------------------------------------------
 * Fonction : Fournit les données actuelles en JSON
 *--------------------------------------------------------*/
void handleData() {
  String json = "{";
  json += "\"temperature\":" + String(temperature, 1) + ",";
  json += "\"tds\":" + String(tdsValue) + ",";
  json += "\"turbidity\":" + String(turbidityValue);
  json += "}";
  server.send(200, "application/json", json);
}

/*----------------------------------------------------------
 * Fonction : Fournit l'historique des 48 dernières heures
 *--------------------------------------------------------*/
void handleHistory() {
  String json = "[";
  bool firstEntry = true;

  // Parcours inversé des données stockées
  for(int i = 0; i < ENTRIES_PER_DAY; i++) {
    int idx = (currentIndex + ENTRIES_PER_DAY - 1 - i) % ENTRIES_PER_DAY;
    DataEntry entry;
    EEPROM.get(idx * ENTRY_SIZE, entry);
    
    if(entry.valid) {
      if(!firstEntry) json += ",";
      json += "{\"time\":" + String(entry.unixTime) + ",";
      json += "\"temp\":" + String(entry.temperature,1) + ",";
      json += "\"tds\":" + String(entry.tds) + ",";
      json += "\"turb\":" + String(entry.turbidity) + "}";
      firstEntry = false;
    }
  }
  json += "]";
  server.send(200, "application/json", json);
}

/*----------------------------------------------------------
 * Fonction d'initialisation
 *--------------------------------------------------------*/
void setup() {
  Serial.begin(115200);
  
  // Initialisation du module RTC
  if (!rtc.begin()) {
    Serial.println("Erreur de connexion au RTC !");
    while(1); // Blocage si échec
  }
  if (rtc.lostPower()) {
    // Réglage automatique à l'heure de compilation
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Initialisation des capteurs
  sensors.begin();
  
  // Configuration de l'écran LCD
  lcd.init();
  lcd.backlight();
  
  // Initialisation de l'EEPROM
  EEPROM.begin(EEPROM_SIZE);

  // Connexion au WiFi
  Serial.print("Connexion à ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnecté !");
  Serial.print("Adresse IP : ");
  Serial.println(WiFi.localIP());

  // Configuration des routes du serveur web
  server.on("/", handleRoot);      // Page principale
  server.on("/data", handleData);  // Données temps réel
  server.on("/history", handleHistory); // Historique
  
  server.begin(); // Démarrage du serveur
}

/*----------------------------------------------------------
 * Boucle principale
 *--------------------------------------------------------*/
void loop() {
  static unsigned long lastSave = 0; // Dernière sauvegarde

  server.handleClient();  // Gestion des requêtes web
  readSensors();          // Acquisition des données
  updateLCD();            // Mise à jour de l'affichage

  // Sauvegarde horaire des données
  if (millis() - lastSave > 3600000) { // Toutes les heures
    saveData();
    lastSave = millis();
  }

  delay(1000); // Pause entre les lectures
}
