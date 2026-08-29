#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SHT4x.h>
#include <BH1750.h>
#include <bluefruit.h>
#include <nrf_gpio.h>
#include <nrf_rtc.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <Adafruit_SPIFlash.h>

#define QUICK_DEBUG 1
#define DEBUG_PRINT 1
#define SUNRISE_DETECTION 0

// Capteurs
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
BH1750 lightMeter(0x23);

// Pins
#define SOIL_MOISTURE_PIN A1
#define SOIL_POWER_PIN D10 // Alimentation capteur sol (high drive)
#define BATTERY_PIN A2
#define BOOST_CONTROL_PIN D8 // MOSFET AO3401 gate — LOW = boost ON, HIGH = boost OFF
#define SDA_PIN 4
#define SCL_PIN 5

// Calibration
#define SOIL_DRY 3500
#define SOIL_WET 1260

// Intervalles adaptatifs (secondes)
#define INTERVAL_MIN 600   //  10 min
#define INTERVAL_BASE 3600 //   1 heure
#define INTERVAL_MAX 14400 //   4 heures

// Seuils de changement pour ajuster la fréquence
#define LUX_DELTA_HIGH 500.0f // → 10 min
#define LUX_DELTA_MED 100.0f  // → 30 min
#define LUX_DELTA_LOW 10.0f   // → 1h (sinon 2h)
#define SOIL_DELTA_HIGH 10    // → 10 min (arrosage détecté)
#define SOIL_DELTA_MED 5      // → 30 min
#define TEMP_DELTA_HIGH 3.0f  // → 30 min
#define NIGHT_LUX 10.0f       // seuil nuit (lux)
#define NIGHT_MULTIPLIER 4.0f // multiplicateur nuit

// Détection lever de soleil
#if SUNRISE_DETECTION
#define NIGHT_HISTORY_SIZE 3   // nb de nuits pour la moyenne
#define NIGHT_ESTIMATE 36000UL // estimation initiale : 10h
#define SUNRISE_MARGIN 900UL   // 15 min de marge avant lever
#define LOOP_OVERHEAD_S 2UL    // overhead mesure + BLE TX
#endif

// BTHome Service
BLEService bthomeService = BLEService(0xFCD2);

// Device info
#define BTHOME_DEVICE_TYPE 1
#define FW_MAJOR 0
#define FW_MINOR 0
#define FW_PATCH 1

// QSPI Flash (mise en deep power-down pour économiser ~30µA)
static Adafruit_FlashTransport_QSPI flashTransport;

// Deep sleep
static SemaphoreHandle_t sleepSemaphore;

// Valeurs précédentes pour l'algorithme adaptatif
static float prevLux = -1.0f;
static int prevSoil = -1;
static float prevTemp = -999.0f;

// Moyenne mobile exponentielle batterie (alpha = 1/20 ≈ 0.05)
#define BATTERY_EMA_ALPHA 0.05f
static float batteryEma = -1.0f; // -1 = non initialisée

// Détection jour/nuit et estimation lever de soleil
#if SUNRISE_DETECTION
enum DayState
{
  UNKNOWN,
  DAY,
  NIGHT
};
static DayState dayState = UNKNOWN;
static uint32_t timeInNight = 0; // secondes écoulées depuis le coucher
static uint32_t nightHistory[NIGHT_HISTORY_SIZE] = {0};
static uint8_t nightHistoryIdx = 0;
static uint8_t nightHistoryCount = 0;

static uint32_t avgNightDuration()
{
  if (nightHistoryCount == 0)
    return NIGHT_ESTIMATE;
  uint32_t sum = 0;
  uint8_t n = min(nightHistoryCount, (uint8_t)NIGHT_HISTORY_SIZE);
  for (uint8_t i = 0; i < n; i++)
    sum += nightHistory[i];
  return sum / n;
}
#endif

uint32_t computeNextInterval(float lux, int soil, float temp)
{
  uint32_t interval = INTERVAL_MAX;

  if (prevLux >= 0.0f)
  {
    // Contribution lux
    float luxDelta = fabsf(lux - prevLux);
    if (luxDelta > LUX_DELTA_HIGH)
      interval = min(interval, (uint32_t)600);
    else if (luxDelta > LUX_DELTA_MED)
      interval = min(interval, (uint32_t)1800);
    else if (luxDelta > LUX_DELTA_LOW)
      interval = min(interval, (uint32_t)INTERVAL_BASE);
    else
      interval = min(interval, (uint32_t)7200);

    // Contribution sol
    int soilDelta = abs(soil - prevSoil);
    if (soilDelta > SOIL_DELTA_HIGH)
      interval = min(interval, (uint32_t)600);
    else if (soilDelta > SOIL_DELTA_MED)
      interval = min(interval, (uint32_t)1800);

    // Contribution température
    if (fabsf(temp - prevTemp) > TEMP_DELTA_HIGH)
      interval = min(interval, (uint32_t)1800);
  }
  else
  {
    interval = INTERVAL_BASE; // première mesure
  }

  // Modificateur nuit : lux très bas → multiplier, plafonné à INTERVAL_MAX
  if (lux < NIGHT_LUX)
  {
    interval = min((uint32_t)((float)interval * NIGHT_MULTIPLIER), (uint32_t)INTERVAL_MAX);
  }

#if SUNRISE_DETECTION
  // Détection transitions jour/nuit
  bool isNight = (lux < NIGHT_LUX);
  if (dayState == NIGHT && !isNight)
  {
    // Nuit → Jour : enregistrer la durée totale de la nuit
    nightHistory[nightHistoryIdx] = timeInNight;
    nightHistoryIdx = (nightHistoryIdx + 1) % NIGHT_HISTORY_SIZE;
    if (nightHistoryCount < NIGHT_HISTORY_SIZE)
      nightHistoryCount++;
    dayState = DAY;
  }
  else if (dayState != NIGHT && isNight)
  {
    // Jour/UNKNOWN → Nuit
    dayState = NIGHT;
    timeInNight = 0;
  }
  else if (dayState == UNKNOWN && !isNight)
  {
    dayState = DAY;
  }

  // Plafond nuit : éviter de dépasser le lever de soleil estimé
  if (dayState == NIGHT)
  {
    uint32_t avg = avgNightDuration();
    if (timeInNight < avg)
    {
      uint32_t timeUntilSunrise = avg - timeInNight + SUNRISE_MARGIN;
      interval = min(interval, timeUntilSunrise);
    }
    else
    {
      // Dépassé la durée moyenne → vérifier fréquemment
      interval = min(interval, (uint32_t)INTERVAL_MIN);
    }
  }
#endif

  interval = constrain(interval, (uint32_t)INTERVAL_MIN, (uint32_t)INTERVAL_MAX);

  prevLux = lux;
  prevSoil = soil;
  prevTemp = temp;

#if DEBUG_PRINT
  Serial.print("Next interval: ");
  Serial.print(interval / 60);
  Serial.println(" min");
#endif

  return interval;
}

extern "C" void RTC2_IRQHandler(void)
{
  if (NRF_RTC2->EVENTS_COMPARE[0])
  {
    NRF_RTC2->EVENTS_COMPARE[0] = 0;
    NRF_RTC2->TASKS_STOP = 1;
    NVIC_DisableIRQ(RTC2_IRQn);
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(sleepSemaphore, &woken);
    portYIELD_FROM_ISR(woken);
  }
}

void deepSleep(uint32_t seconds)
{
    Serial.flush();

  // RTC2 est libre (RTC0=SoftDevice, RTC1=FreeRTOS)
  // LFCLK déjà démarré par Bluefruit.begin()
  NRF_RTC2->TASKS_STOP = 1;
  NRF_RTC2->TASKS_CLEAR = 1;
  NRF_RTC2->PRESCALER = 4095;    // 32768/4096 = 8 Hz
  NRF_RTC2->CC[0] = seconds * 8; // ticks pour la durée demandée
  NRF_RTC2->INTENSET = RTC_INTENSET_COMPARE0_Msk;
  NRF_RTC2->EVTENSET = RTC_EVTENSET_COMPARE0_Msk;
  NVIC_SetPriority(RTC2_IRQn, 6);
  NVIC_ClearPendingIRQ(RTC2_IRQn);
  NVIC_EnableIRQ(RTC2_IRQn);
  NRF_RTC2->TASKS_START = 1;

  // Active le mode deep sleep ARM — FreeRTOS idle utilisera WFE en mode deep
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

  // Bloque le task loop ; l'idle task dort jusqu'au réveil RTC2
  xSemaphoreTake(sleepSemaphore, portMAX_DELAY);

  SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    Serial.flush();
}

void setup()
{
#if DEBUG_PRINT
  Serial.begin(115200);
  delay(1000);
  Serial.println("Plant Sensor BTHome Starting...");
#endif

  // Boost ON en tout premier — BH1750 doit être alimenté pour lightMeter.begin()
  // INPUT_PULLDOWN : pull-up interne tire la gate LOW → P-channel ON dès le reset
  pinMode(BOOST_CONTROL_PIN, INPUT_PULLDOWN);

  // LEDs XIAO éteintes (active low, consomment si flottantes)
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_RED, HIGH);
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_GREEN, HIGH);
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_BLUE, HIGH);

  // Sémaphore pour le deep sleep
  sleepSemaphore = xSemaphoreCreateBinary();

  // Init I2C
  Wire.begin();
  Wire.setClock(100000);

  // Init SHT40
  if (!sht4.begin())
  {
#if DEBUG_PRINT
    Serial.println("SHT40 non détecté!");
#endif
  }
  else
  {
#if DEBUG_PRINT
    Serial.println("SHT40 OK");
#endif
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
  }

  // Init BH1750 (boost ON via INPUT_PULLDOWN ci-dessus)
  if (!lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE))
  {
#if DEBUG_PRINT
    Serial.println("BH1750 non détecté!");
#endif
  }
  else
  {
#if DEBUG_PRINT
    Serial.println("BH1750 OK");
#endif
  }

  // Config ADC
  analogReadResolution(12);
  analogReference(AR_INTERNAL_3_0);

  // Capteur sol en high drive (jusqu'à 15mA) - éteint par défaut
  nrf_gpio_cfg(
      digitalPinToPinName(SOIL_POWER_PIN),
      NRF_GPIO_PIN_DIR_OUTPUT,
      NRF_GPIO_PIN_INPUT_DISCONNECT,
      NRF_GPIO_PIN_NOPULL,
      NRF_GPIO_PIN_H0H1,
      NRF_GPIO_PIN_NOSENSE);
  digitalWrite(SOIL_POWER_PIN, LOW);

  // QSPI flash en deep power-down (~0.5µA au lieu de ~30µA en standby)
  // 0xB9 = commande Deep Power-Down standard des flash SPI
  flashTransport.begin();
  flashTransport.runCommand(0xB9);
  flashTransport.end();

  // Init BLE (démarre aussi le LFCLK nécessaire pour RTC2)
  Bluefruit.autoConnLed(false); // empêche Bluefruit de contrôler LED_BLUE
  Bluefruit.begin();
  Bluefruit.setName("Plant Sensor");
  Bluefruit.setTxPower(4);

  // Start BTHome service
  bthomeService.begin();

#if DEBUG_PRINT
  Serial.println("Setup terminé!");
#endif
}

void sendBTHomeData(uint8_t soil, float temp, float humidity, float lux, uint8_t battery)
{
  // BTHome v2 payload
  uint8_t payload[31];
  uint8_t idx = 0;

  // BTHome header
  payload[idx++] = 0x44; // v2, unencrypted, trigger-based (intervalle adaptatif)

  // Les object IDs doivent être en ordre numérique croissant (spec BTHome)

  // Battery (0x01) - uint8, factor 1, %
  payload[idx++] = 0x01;
  payload[idx++] = battery;

  // Temperature (0x02) - sint16, factor 0.01, °C
  payload[idx++] = 0x02;
  int16_t tempInt = (int16_t)(temp * 100);
  memcpy(&payload[idx], &tempInt, 2);
  idx += 2;

  // Humidity (0x03) - uint16, factor 0.01, %
  payload[idx++] = 0x03;
  uint16_t humidInt = (uint16_t)(humidity * 100);
  memcpy(&payload[idx], &humidInt, 2);
  idx += 2;

  // Illuminance (0x05) - uint24, factor 0.01, lx
  payload[idx++] = 0x05;
  uint32_t luxInt = (uint32_t)(lux * 100);
  payload[idx++] = (luxInt & 0xFF);
  payload[idx++] = ((luxInt >> 8) & 0xFF);
  payload[idx++] = ((luxInt >> 16) & 0xFF);

  // Moisture (0x2F) - uint8, factor 1, %
  payload[idx++] = 0x2F;
  payload[idx++] = soil;

  // Device type (0xF0) - uint16 little endian
  payload[idx++] = 0xF0;
  payload[idx++] = (BTHOME_DEVICE_TYPE & 0xFF);
  payload[idx++] = (BTHOME_DEVICE_TYPE >> 8) & 0xFF;

  // Firmware version (0xF2) - uint24, format patch/minor/major little endian
  payload[idx++] = 0xF2;
  payload[idx++] = FW_PATCH;
  payload[idx++] = FW_MINOR;
  payload[idx++] = FW_MAJOR;

  // Build service data (UUID + payload)
  uint8_t serviceData[33];
  serviceData[0] = 0xD2; // BTHome UUID low byte
  serviceData[1] = 0xFC; // BTHome UUID high byte
  memcpy(&serviceData[2], payload, idx);

  // Stop advertising
  Bluefruit.Advertising.stop();
  Bluefruit.Advertising.clearData();

  // Add service
  Bluefruit.Advertising.addService(bthomeService);

  // Add service data
  Bluefruit.Advertising.addData(BLE_GAP_AD_TYPE_SERVICE_DATA, serviceData, idx + 2);

  // Start advertising
  Bluefruit.Advertising.restartOnDisconnect(false);
  Bluefruit.Advertising.setInterval(160, 160);
  Bluefruit.Advertising.setFastTimeout(1);
  Bluefruit.Advertising.start(1);

#if DEBUG_PRINT
  Serial.println("BTHome broadcasted");
#endif
}

void loop()
{

  // Battery EN PREMIER — au repos, avant boost et I2C (évite les fluctuations sous charge)
  int batteryRaw = analogRead(BATTERY_PIN);
  float voltage = (batteryRaw / 4095.0) * 3.0;
  int batteryRawPercent = map((int)(voltage * 100), 200, 300, 0, 100);
  batteryRawPercent = constrain(batteryRawPercent, 0, 100);
  if (batteryEma < 0.0f)
    batteryEma = (float)batteryRawPercent; // init
  else
    batteryEma = (1.0f - BATTERY_EMA_ALPHA) * batteryEma + BATTERY_EMA_ALPHA * batteryRawPercent;
  int batteryPercent = constrain((int)(batteryEma + 0.5f), 0, 100);

  // Boost ON — OUTPUT LOW (P-channel ON)
  pinMode(BOOST_CONTROL_PIN, OUTPUT);
  digitalWrite(BOOST_CONTROL_PIN, LOW);
  delay(20); // stabilisation boost + BH1750 power-on reset

  // Réinitialiser I2C (SDA/SCL relâchés avant le sleep précédent)
  Wire.begin();
  Wire.setClock(100000);

  static bool firstBoot = true;
#if SUNRISE_DETECTION
  static uint32_t lastInterval = 0;
#endif

  if (firstBoot)
  {
    firstBoot = false;
#if !QUICK_DEBUG
    deepSleep(60); // Attend 1 minute au premier démarrage
#endif
  }

#if SUNRISE_DETECTION
  // Accumule le temps passé en nuit (pour le cap lever de soleil)
  if (dayState == NIGHT)
  {
    timeInNight += lastInterval + LOOP_OVERHEAD_S;
  }
#endif

#if DEBUG_PRINT
  Serial.println("\n--- Lecture ---");
  Serial.print("Bat: ");
  Serial.print(voltage);
  Serial.print("V (");
  Serial.print(batteryPercent);
  Serial.println("%)");
#endif

  // Soil - allume le capteur, attend stabilisation, lit, éteint
  digitalWrite(SOIL_POWER_PIN, HIGH);
  delay(100);
  int soilRaw = analogRead(SOIL_MOISTURE_PIN);
  digitalWrite(SOIL_POWER_PIN, LOW);
  int soilMoisture = map(soilRaw, SOIL_DRY, SOIL_WET, 0, 100);
  soilMoisture = constrain(soilMoisture, 0, 100);
#if DEBUG_PRINT
  Serial.print("Sol raw: ");
  Serial.println(soilRaw);
  Serial.print("Sol: ");
  Serial.print(soilMoisture);
  Serial.println("%");
#endif


  // SHT40
  sensors_event_t humidity, temp;
  float temperature = 0;
  float humidite = 0;

  if (sht4.getEvent(&humidity, &temp))
  {
    temperature = temp.temperature;
    humidite = humidity.relative_humidity;
#if DEBUG_PRINT
    Serial.print("Temp: ");
    Serial.print(temperature);
    Serial.println("°C");
    Serial.print("RH: ");
    Serial.print(humidite);
    Serial.println("%");
#endif
  }

  // BH1750
  lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE);
  lightMeter.configure(BH1750::ONE_TIME_HIGH_RES_MODE);
  delay(200);
  float lux = lightMeter.readLightLevel();

  // Relâcher SDA/SCL avant d'éteindre le boost (évite back-power du BH1750 via I2C)
  NRF_TWIM0->ENABLE = TWIM_ENABLE_ENABLE_Disabled << TWIM_ENABLE_ENABLE_Pos;
  pinMode(SDA_PIN, INPUT);
  pinMode(SCL_PIN, INPUT);
  // Boost OFF — INPUT_PULLUP (pull-up interne tient la gate HIGH, P-channel OFF)
  pinMode(BOOST_CONTROL_PIN, INPUT_PULLUP);
#if DEBUG_PRINT
  Serial.print("Lux: ");
  Serial.println(lux);
#endif

  // Broadcast BTHome
//#if !QUICK_DEBUG
  sendBTHomeData(soilMoisture, temperature, humidite, lux, batteryPercent);
//#endif

  // Deep sleep adaptatif via RTC2
#if SUNRISE_DETECTION
  lastInterval = computeNextInterval(lux, soilMoisture, temperature);

#if QUICK_DEBUG
  lastInterval = 10;
#endif
  // Add small delay so logs are flushed before deep sleep (important pour la détection du lever de soleil)
  delay(1);
  deepSleep(lastInterval);
#else
  // Add small delay so logs are flushed before deep sleep (important pour la détection du lever de soleil)
  delay(1);
  #if QUICK_DEBUG
    deepSleep(30);
  #else
  deepSleep(computeNextInterval(lux, soilMoisture, temperature));

  #endif
#endif
}
