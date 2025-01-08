#include <Wire.h>
#include <Adafruit_GFX.h>
#include <GxEPD2_BW.h>
#include <BH1750.h>
#include <EEPROM.h>
#include <map>
#include <esp_adc_cal.h>
#include <string>
#include <vector>

// Define constants for pins, EEPROM size, and inactivity timeout
#define EINK_CS 9
#define EINK_DC 15
#define EINK_RST 0
#define EINK_BUSY 4
#define BTN_UP 37
#define BTN_DOWN 39
#define BTN_OK 38
#define BTN_POWER 27
#define POWER_PIN 12
#define LED_PIN 10
#define EEPROM_SIZE 64
#define INACTIVITY_TIMEOUT 20000

// Initialize display and light meter objects
GxEPD2_BW<GxEPD2_154_M09, GxEPD2_154_M09::HEIGHT> display(GxEPD2_154_M09(EINK_CS, EINK_DC, EINK_RST, EINK_BUSY));
BH1750 lightMeter(0x23);

// Declare global variables for light meter status, mode, and settings
bool lightMeterOk = true;
bool crazyMode = false;
float lux = 0;
double ev = 1;
int menuSelection = 0;
bool editMode = false;
bool needsUpdate = true;
float selectedAperture = 2.8;
float selectedShutterSpeed = 1.0;
std::string selectedShutterSpeedString = "1s";
int selectedISO = 400;

// Define enumerations for different modes
enum Mode
{
  APERTURE_PRIORITY,
  SHUTTER_PRIORITY
};
Mode currentMode = APERTURE_PRIORITY;
unsigned long lastActivity = 0;

// Define vectors for shutter speeds, apertures, and ISO values
const std::vector<float> shutterSpeeds = {30.0f, 15.0f, 8.0f, 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f, 0.0667f, 0.0333f, 0.0167f, 0.008f, 0.004f, 0.002f, 0.001f};
const std::vector<std::string> shutterSpeedsString = {"30s", "15s", "8s", "4s", "2s", "1s", "1/2", "1/4", "1/8", "1/15", "1/30", "1/60", "1/125", "1/250", "1/500", "1/1000"};
const std::vector<float> apertures = {1.4f, 1.8f, 2.0f, 2.8f, 3.5f, 4.0f, 5.6f, 8.0f, 11.0f, 16.0f, 22.0f};
const std::vector<int> isoValues = {50, 100, 200, 400, 800, 1600};

// Function to get battery voltage
float getBatVoltage()
{
  analogSetPinAttenuation(35, ADC_11db);
  esp_adc_cal_characteristics_t *adc_chars =
      (esp_adc_cal_characteristics_t *)calloc(
          1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12,
                           3600, adc_chars);
  uint16_t ADCValue = analogRead(35);

  uint32_t BatVolmV = esp_adc_cal_raw_to_voltage(ADCValue, adc_chars);
  float BatVol = float(BatVolmV) * 25.1 / 5.1 / 1000;
  free(adc_chars);
  return BatVol;
}

// Function to find the closest shutter speed
float closestShutter(float shutterSpeed)
{
  Serial.print("Function called with shutter: ");
  Serial.println(shutterSpeed);
  size_t closestIndex = 0;
  float minDifference = std::abs(shutterSpeeds[0] - shutterSpeed); // Différence minimale initiale

  if (shutterSpeed > shutterSpeeds[0])
  {
    Serial.println(shutterSpeeds[0]);
    return shutterSpeeds[0];
  }
  else if (shutterSpeed < shutterSpeeds[shutterSpeeds.size() - 1])
  {
    Serial.println(shutterSpeeds[shutterSpeeds.size() - 1]);
    return shutterSpeeds[shutterSpeeds.size() - 1];
  }
  else
  {

    for (size_t i = 1; i < shutterSpeeds.size(); ++i)
    {
      float difference = std::abs(shutterSpeeds[i] - shutterSpeed);
      if (difference < minDifference)
      {
        minDifference = difference;
        closestIndex = i;
      }
    }
  }
  Serial.print("Closest index shutter: ");
  Serial.println(shutterSpeeds[closestIndex]);
  return shutterSpeeds[closestIndex]; // Retourne l'ouverture la plus proche
}

// Function to find the closest aperture
float closestAperture(float aperture)
{
  Serial.print("Function called with aperture: ");
  Serial.println(aperture);
  size_t closestIndex = 0;
  float minDifference = std::abs(apertures[0] - aperture); // Différence minimale initiale

  if (aperture < apertures[0])
  {
    Serial.println(apertures[0]);
    return apertures[0];
  }
  else if (aperture > apertures[apertures.size() - 1])
  {
    Serial.println(apertures[apertures.size() - 1]);
    return apertures[apertures.size() - 1];
  }
  else
  {
    for (size_t i = 1; i < apertures.size(); ++i)
    {
      float difference = std::abs(apertures[i] - aperture);
      if (difference < minDifference)
      {
        minDifference = difference;
        closestIndex = i;
      }
    }
  }
  Serial.print("Closest index aperture: ");
  Serial.println(apertures[closestIndex]);
  return apertures[closestIndex]; // Retourne l'ouverture la plus proche
}

// Function to convert shutter speed to string
std::string convertShutterToString(float shutterSpeed)
{
  size_t index = 0;
  for (size_t i = 0; i < shutterSpeeds.size(); ++i)
  {
    if (shutterSpeeds[i] == shutterSpeed)
    {
      index = i;
      break;
    }
  }
  return shutterSpeedsString[index];
}

// Function to calculate exposure value (EV)
double calculateEV(float lux)
{
  double n = 0.32;
  double sv = log2(selectedISO * n);
  double nc = n * 340;
  double iv = log2(lux / nc);
  Serial.print("EV: ");
  Serial.println(iv + sv);
  return iv + sv;
}

// Function to calculate aperture based on EV, ISO, and shutter speed
float calculateAperture(double ev, int iso, float shutterSpeed)
{
  double sv = log2(iso / 100.0); // Sensibilité ISO en log2
  Serial.print("ISO: ");
  Serial.println(iso);
  Serial.print("SV: ");
  Serial.println(sv);
  Serial.print("Aperture result: ");
  Serial.println(sqrt(shutterSpeed * pow(2, ev + sv)));
  return sqrt(shutterSpeed * pow(2, ev + sv)); // Retourne l'ouverture N
}

// Function to calculate shutter speed based on EV, ISO, and aperture
float calculateShutterSpeed(double ev, int iso, float aperture)
{
  float sv = log2(iso / 100.0f); // Sensibilité ISO en log2
  Serial.print("ISO: ");
  Serial.println(iso);
  Serial.print("SV: ");
  Serial.println(sv);
  Serial.print("Shutter speed result: ");
  Serial.println((aperture * aperture) / pow(2, ev + sv));
  return (aperture * aperture) / pow(2, ev + sv); // Retourne le temps de pose t
}

// Function to recalculate parameters based on the current mode
void recalculateParameter()
{
  if (currentMode == APERTURE_PRIORITY)
  {
    selectedShutterSpeed = calculateShutterSpeed(ev, selectedISO, selectedAperture);
    Serial.print("Calculated Shutter Speed: ");
    Serial.println(closestShutter(selectedShutterSpeed));
    selectedShutterSpeed = closestShutter(selectedShutterSpeed);
    Serial.print("Closest Shutter: ");
    Serial.println(selectedShutterSpeed);
  }
  else
  {
    selectedAperture = calculateAperture(ev, selectedISO, selectedShutterSpeed);
    Serial.print("Calculated Aperture: ");
    Serial.println(selectedAperture);
    selectedAperture = closestAperture(selectedAperture);
    Serial.print("Closest Aperture: ");
    Serial.println(selectedAperture);
  }
}

// Function to save settings to EEPROM
void saveSettingsToEEPROM()
{
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(0, selectedISO);
  EEPROM.put(4, selectedAperture);
  EEPROM.put(8, currentMode);
  EEPROM.put(12, selectedShutterSpeed);
  EEPROM.put(16, ev);
  EEPROM.commit();
}

// Function to load settings from EEPROM
void loadSettingsFromEEPROM()
{
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, selectedISO);
  EEPROM.get(4, selectedAperture);
  int mode;
  EEPROM.get(8, mode);
  currentMode = static_cast<Mode>(mode);
  int shutterIndex;
  EEPROM.get(12, selectedShutterSpeed);
  EEPROM.get(16, ev);
}

// Function to render the menu on the display
void renderMenu(bool invertDisplay = false)
{
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(invertDisplay ? GxEPD_BLACK : GxEPD_WHITE);
    display.setCursor(0, 10);
    display.setTextSize(2);
    display.setTextColor(invertDisplay ? GxEPD_WHITE : GxEPD_BLACK);
    display.print("Light Meter: ");
    lightMeterOk ? display.println("OK") : display.println("NOK");
    display.setCursor(0, 30);
    // BATTERY ? display readAnalog(35) for testing
    float battery = getBatVoltage();
    display.print("Bat: ");
    display.print(battery);
    display.print("V ");
    // map battery voltage from 3.3v - 4.3v to 0-100%
    float batteryLevel = map(battery, 3.3f, 4.3f, 0.0f, 100.0f);
    display.print((int)batteryLevel);
    display.println("%");

    display.setCursor(0, 60);
    display.print("EV Measured: ");
    display.println(ev, 0);
    for (int i = 0; i < 4; i++)
    {
      display.setCursor(0, 90 + i * 25);
      if (i == menuSelection)
        display.print(editMode ? "* " : "> ");
      else if (i == 3)
        display.print("");
      else
        display.print("- ");
      switch (i)
      {
      case 0:
        display.print("Mode: ");
        display.println(currentMode == APERTURE_PRIORITY ? "Aperture" : "Shutter");
        break;
      case 1:
        display.print("ISO: ");
        display.println(selectedISO);
        break;
      case 2:
        if (currentMode == APERTURE_PRIORITY)
        {
          display.print("Aperture:");
          display.println(selectedAperture);
        }
        else
        {
          display.print("Shutter:");
          display.println(convertShutterToString(selectedShutterSpeed).c_str());
        }
        break;
      case 3:
        if (currentMode == SHUTTER_PRIORITY)
        {
          display.setTextSize(3);
          display.print(" Aprt:");
          display.println(selectedAperture);
          Serial.print("Closest Aperture: ");
          Serial.println(selectedAperture);
        }
        else
        {
          display.setTextSize(3);
          display.print(" Sht:");
          display.println(convertShutterToString(selectedShutterSpeed).c_str());
          Serial.print("Closest Shutter: ");
          Serial.println(selectedShutterSpeed);
        }
      }
    }
  } while (display.nextPage());
}

// Function to enter sleep mode
void enterSleepMode()
{
  saveSettingsToEEPROM();
  renderMenu(true);
  display.powerOff();
  digitalWrite(POWER_PIN, LOW);
}

// Setup function to initialize the system
void setup()
{
  Serial.begin(115200);
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_POWER, INPUT_PULLUP);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Wire.begin();
  if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE))
  {
    Serial.println("BH1750 sensor not detected, default EV value will be used.");
    lightMeterOk = false;
    crazyMode = true;
  }
  loadSettingsFromEEPROM();

  display.init(0);
  display.setTextWrap(false);
  display.fillScreen(GxEPD_WHITE);
  display.display();
  display.setFullWindow();
  display.firstPage();
  needsUpdate = true;
  lastActivity = millis();

  Serial.print("Loaded ISO: ");
  Serial.println(selectedISO);
  Serial.print("Loaded Aperture: ");
  Serial.println(selectedAperture);
  Serial.print("Loaded Shutter Speed: ");
  Serial.println(selectedShutterSpeed);
  Serial.print("Loaded Mode: ");
  Serial.println(currentMode == APERTURE_PRIORITY ? "Aperture" : "Shutter");
}

// Loop function to handle button presses and update the display
void loop()
{

  if (millis() - lastActivity > INACTIVITY_TIMEOUT)
    enterSleepMode();
  if (digitalRead(BTN_UP) == LOW)
  {
    lastActivity = millis();
    if (!editMode)
      menuSelection = (menuSelection + 2) % 3;
    else
    {
      switch (menuSelection)
      {
      case 1:
      {
        auto it = std::find(isoValues.begin(), isoValues.end(), selectedISO);
        selectedISO = (it != isoValues.begin()) ? *(--it) : isoValues.back();
        break;
      }
      case 2:
      {
        if (currentMode == APERTURE_PRIORITY)
        {
          auto it = std::find(shutterSpeeds.begin(), shutterSpeeds.end(), selectedShutterSpeed);
          selectedShutterSpeed = (it != shutterSpeeds.begin()) ? *(--it) : shutterSpeeds.back();
        }
        else
        {
          auto it = std::find(apertures.begin(), apertures.end(), selectedAperture);
          selectedAperture = (it != apertures.begin()) ? *(--it) : apertures.back();
        }
        break;
      }
      }
      recalculateParameter();
    }
    needsUpdate = true;
    delay(200);
  }
  else if (digitalRead(BTN_DOWN) == LOW)
  {
    lastActivity = millis();
    if (!editMode)
      menuSelection = (menuSelection + 1) % 3;
    else
    {
      switch (menuSelection)
      {
      case 1:
      {
        auto it = std::find(isoValues.begin(), isoValues.end(), selectedISO);
        selectedISO = (it != isoValues.end() - 1) ? *(++it) : isoValues.front();
        break;
      }
      case 2:
      {
        if (currentMode == SHUTTER_PRIORITY)
        {
          auto it = std::find(shutterSpeeds.begin(), shutterSpeeds.end(), selectedShutterSpeed);
          selectedShutterSpeed = (it != shutterSpeeds.end() - 1) ? *(++it) : shutterSpeeds.front();
        }
        else
        {
          auto it = std::find(apertures.begin(), apertures.end(), selectedAperture);
          selectedAperture = (it != apertures.end() - 1) ? *(++it) : apertures.front();
        }
        break;
      }
      }
      recalculateParameter();
    }
    needsUpdate = true;
    delay(200);
  }
  else if (digitalRead(BTN_OK) == LOW)
  {
    lastActivity = millis();
    if (menuSelection == 0)
      currentMode = (currentMode == APERTURE_PRIORITY) ? SHUTTER_PRIORITY : APERTURE_PRIORITY;
    else
      editMode = !editMode;
    needsUpdate = true;
    delay(200);
  }
  else if (digitalRead(BTN_POWER) == LOW)
  {
    lastActivity = millis();
    lux = crazyMode ? random(1.25f, 1280.0f) : lightMeter.readLightLevel();
    Serial.print("Measured Lux: ");
    Serial.println(lux);
    ev = calculateEV(lux);
    needsUpdate = true;
    recalculateParameter();
    delay(200);
  }
  if (needsUpdate)
  {
    renderMenu();
    needsUpdate = false;
  }
}
