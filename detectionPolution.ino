#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <WebServer.h>
#include <LiquidCrystal_I2C.h>

#define ONE_WIRE_BUS 4  // Pin de lecture pour le capteur de température
#define TDS_PIN 34      // Pin pour capteur TDS
#define TURBIDITY_PIN 35 // Pin pour capteur Turbidité

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
LiquidCrystal_I2C lcd(0x27, 20, 4);  // Écran LCD avec adresse 0x27

const char* ssid = "ADOC 4G";  // Nom du réseau Wi-Fi
const char* password = "0102030405"; // Mot de passe Wi-Fi

WebServer server(80);  // Serveur web sur le port 80

// Variables pour la lecture des capteurs
float temperature = 0;
int tdsValue = 0;
int turbidityValue = 0;

void handleRoot();
void handleData();
void handleHistory();





// Structure pour enregistrer les données dans l'EEPROM
struct DataEntry {
  uint32_t timestamp;
  float temperature;
  int tds;
  int turbidity;
};

const int EEPROM_SIZE = 4096;
const int ENTRY_SIZE = sizeof(DataEntry);
const int MAX_ENTRIES = EEPROM_SIZE / ENTRY_SIZE;
int currentIndex = 0;

// Fonction pour lire les capteurs
void readSensors() {
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0);
  tdsValue = analogRead(TDS_PIN);
  turbidityValue = analogRead(TURBIDITY_PIN);
}

// Fonction pour enregistrer les données dans l'EEPROM
void saveData() {
  DataEntry entry;
  entry.timestamp = millis();  // Timestamp actuel
  entry.temperature = temperature;
  entry.tds = tdsValue;
  entry.turbidity = turbidityValue;

  int addr = currentIndex * ENTRY_SIZE;
  EEPROM.put(addr, entry);
  EEPROM.commit();

  currentIndex++;
  if (currentIndex >= MAX_ENTRIES) currentIndex = 0;  // Réinitialise à zéro si l'EEPROM est pleine
}

// Fonction pour mettre à jour l'affichage LCD
void updateLCD() {
  lcd.clear();

  // Déterminer l'état de l'eau
  String waterStatus = "";
  if (tdsValue < 300 && turbidityValue < 1000) {
    waterStatus = "It's clear";
  } else if (tdsValue < 600 || turbidityValue < 2000) {
    waterStatus = "It's cloudy";
  } else {
    waterStatus = "It's dirty";
  }

  // Affichage sur l'écran LCD
  lcd.setCursor(0, 0);
  lcd.print("Status: ");
  lcd.print(waterStatus);

  lcd.setCursor(0, 1);
  lcd.print("Temp: ");
  lcd.print(temperature, 1);
  lcd.print(" C");

  lcd.setCursor(0, 2);
  lcd.print("TDS: ");
  lcd.print(tdsValue);
  lcd.print(" ppm");

  lcd.setCursor(0, 3);
  lcd.print("Turb: ");
  lcd.print(turbidityValue);
  lcd.print(" NTU");
}

// Fonction pour afficher les données en JSON sur la page web
void handleData() {
  String json = "{";
  json += "\"temperature\":" + String(temperature, 1) + ",";
  json += "\"tds\":" + String(tdsValue) + ",";
  json += "\"turbidity\":" + String(turbidityValue) + ",";
  bool pollution = (tdsValue > 600 || turbidityValue > 2000);
  json += "\"pollution\":" + String(pollution ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

// Fonction pour afficher l'historique des 48h en JSON
void handleHistory() {
  String json = "[";
  unsigned long currentTime = millis();
  for (int i = 0; i < MAX_ENTRIES; i++) {
    DataEntry entry;
    EEPROM.get(i * ENTRY_SIZE, entry);  // Récupère les données de l'EEPROM
    if (entry.timestamp == 0) continue;  // Si pas de données, continue

    // Vérifie si l'entrée est dans les 48 dernières heures (48h = 172800000ms)
    if (currentTime - entry.timestamp < 172800000) {
      json += "{";
      json += "\"time\":" + String(entry.timestamp / 1000) + ",";
      json += "\"temperature\":" + String(entry.temperature, 1) + ",";
      json += "\"tds\":" + String(entry.tds) + ",";
      json += "\"turbidity\":" + String(entry.turbidity);
      json += "},";
    }
  }
  if (json.endsWith(",")) json.remove(json.length() - 1);  // Supprime la dernière virgule
  json += "]"; 
  server.send(200, "application/json", json);  // Envoie l'historique des données
}

// HTML pour la page d'accueil du serveur Web
const char indexHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang='fr'>
<head><meta charset='UTF-8'><title>Surveillance Eau</title>
<style>
body { font-family: Arial; background:#f5f5f5; text-align:center; }
.card { margin:20px auto; background:#fff; padding:20px; border-radius:10px; box-shadow:0 0 10px #ccc; width:80%; }
table { width:100%; display:none; border-collapse:collapse; margin-top:20px; }
th, td { border:1px solid #ccc; padding:8px; }
th { background:#007bff; color:white; }
button { padding:10px 20px; margin-top:20px; }
#alert { color: red; font-weight: bold; }
</style></head>
<body>
<div class='card'>
<h2>Température</h2><p id='temperature'>-- °C</p>
<h2>Turbidité</h2><p id='turbidity'>-- NTU</p>
<h2>TDS</h2><p id='tds'>-- ppm</p>
<p id='alert'></p>
<button onclick='showHistory()'>Afficher Historique</button>
<table id='historyTable'><thead><tr><th>Temps</th><th>Température</th><th>TDS</th><th>Turbidité</th></tr></thead><tbody id='historyBody'></tbody></table>
</div>
<script>
function refreshData() {
  fetch("/data").then(r => r.json()).then(data => {
    document.getElementById("temperature").innerText = data.temperature + " °C";
    document.getElementById("tds").innerText = data.tds + " ppm";
    document.getElementById("turbidity").innerText = data.turbidity + " NTU";
    document.getElementById("alert").innerText = data.pollution === true ? "Alerte Pollution détectée !" : "";
  });
}
function showHistory() {
  fetch("/history").then(r => r.json()).then(data => {
    let table = document.getElementById("historyTable");
    let body = document.getElementById("historyBody");
    body.innerHTML = "";
    data.forEach(row => {
      let tr = document.createElement("tr");
      tr.innerHTML = <td>${row.time}</td><td>${row.temperature}</td><td>${row.tds}</td><td>${row.turbidity}</td>;
      body.appendChild(tr);
    });
    table.style.display = "table";
  });
}
setInterval(refreshData, 5000);
refreshData();
</script>
</body></html>
)rawliteral";

// Déclaration de la fonction handleRoot
void handleRoot() {
  server.send(200, "text/html", indexHTML);
}


// Fonction d'initialisation
void setup() { 
  Serial.begin(115200);                // Initialisation du port série
  WiFi.begin(ssid, password);          // Connexion au réseau Wi-Fi
  EEPROM.begin(EEPROM_SIZE);           // Initialisation de l'EEPROM
  sensors.begin();                     // Initialisation des capteurs de température
  lcd.init();                          // Initialisation de l'écran LCD
  lcd.backlight();                     // Allume le rétroéclairage de l'écran LCD

  while (WiFi.status() != WL_CONNECTED) {  // Attente de la connexion Wi-Fi
    delay(500); 
    Serial.print("."); 
  }

  Serial.println("");
  Serial.print("Connecté ! Adresse IP : ");
  Serial.println(WiFi.localIP());


  server.on("/", handleRoot);           // Route pour la page d'accueil
  server.on("/data", handleData);       // Route pour récupérer les données
  server.on("/history", handleHistory); // Route pour récupérer l'historique
  server.begin();                       // Démarre le serveur Web
  Serial.println("Serveur Web actif");  // Affiche un message de confirmation
}

unsigned long lastSave = 0;  // Variable pour enregistrer les données périodiquement

// Boucle principale
void loop() { 
  server.handleClient();      // Gère les requêtes du serveur Web
  readSensors();              // Lit les capteurs
  updateLCD();                // Met à jour l'affichage LCD
  if (millis() - lastSave > 3600000) {  // Enregistre les données toutes les heures
    saveData(); 
    lastSave = millis();      // Réinitialise le temps de dernière sauvegarde
  }
  delay(1000);  // Attente d'une seconde avant la prochaine lecture
}