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

#define DEBUG_PRINT 0

// Capteurs
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
BH1750 lightMeter(0x23);

// Pins
#define SOIL_MOISTURE_PIN A1
#define SOIL_POWER_PIN D10   // Alimentation capteur sol (high drive)
#define BATTERY_PIN A2
#define SDA_PIN 4
#define SCL_PIN 5

// Calibration
#define SOIL_DRY 3500
#define SOIL_WET 1260

// Intervalles adaptatifs (secondes)
#define INTERVAL_MIN   600    //  10 min
#define INTERVAL_BASE  3600   //   1 heure
#define INTERVAL_MAX   14400  //   4 heures

// Seuils de changement pour ajuster la fréquence
#define LUX_DELTA_HIGH   500.0f  // → 10 min
#define LUX_DELTA_MED    100.0f  // → 30 min
#define LUX_DELTA_LOW     10.0f  // → 1h (sinon 2h)
#define SOIL_DELTA_HIGH   10     // → 10 min (arrosage détecté)
#define SOIL_DELTA_MED     5     // → 30 min
#define TEMP_DELTA_HIGH    3.0f  // → 30 min
#define NIGHT_LUX         10.0f  // seuil nuit (lux)
#define NIGHT_MULTIPLIER   4.0f  // multiplicateur nuit

// BTHome Service
BLEService bthomeService = BLEService(0xFCD2);

// Device info
#define BTHOME_DEVICE_TYPE  1
#define FW_MAJOR  0
#define FW_MINOR  0
#define FW_PATCH  1

// QSPI Flash (mise en deep power-down pour économiser ~30µA)
static Adafruit_FlashTransport_QSPI flashTransport;

// Deep sleep
static SemaphoreHandle_t sleepSemaphore;

// Valeurs précédentes pour l'algorithme adaptatif
static float prevLux      = -1.0f;
static int   prevSoil     = -1;
static float prevTemp     = -999.0f;

uint32_t computeNextInterval(float lux, int soil, float temp) {
  uint32_t interval = INTERVAL_MAX;

  if (prevLux >= 0.0f) {
    // Contribution lux
    float luxDelta = fabsf(lux - prevLux);
    if      (luxDelta > LUX_DELTA_HIGH) interval = min(interval, (uint32_t)600);
    else if (luxDelta > LUX_DELTA_MED)  interval = min(interval, (uint32_t)1800);
    else if (luxDelta > LUX_DELTA_LOW)  interval = min(interval, (uint32_t)INTERVAL_BASE);
    else                                interval = min(interval, (uint32_t)7200);

    // Contribution sol
    int soilDelta = abs(soil - prevSoil);
    if      (soilDelta > SOIL_DELTA_HIGH) interval = min(interval, (uint32_t)600);
    else if (soilDelta > SOIL_DELTA_MED)  interval = min(interval, (uint32_t)1800);

    // Contribution température
    if (fabsf(temp - prevTemp) > TEMP_DELTA_HIGH) interval = min(interval, (uint32_t)1800);
  } else {
    interval = INTERVAL_BASE;  // première mesure
  }

  // Modificateur nuit : lux très bas → multiplier, plafonné à INTERVAL_MAX
  if (lux < NIGHT_LUX) {
    interval = min((uint32_t)((float)interval * NIGHT_MULTIPLIER), (uint32_t)INTERVAL_MAX);
  }

  interval = constrain(interval, (uint32_t)INTERVAL_MIN, (uint32_t)INTERVAL_MAX);

  prevLux  = lux;
  prevSoil = soil;
  prevTemp = temp;

#if DEBUG_PRINT
  Serial.print("Next interval: ");
  Serial.print(interval / 60);
  Serial.println(" min");
#endif

  return interval;
}

extern "C" void RTC2_IRQHandler(void) {
  if (NRF_RTC2->EVENTS_COMPARE[0]) {
    NRF_RTC2->EVENTS_COMPARE[0] = 0;
    NRF_RTC2->TASKS_STOP = 1;
    NVIC_DisableIRQ(RTC2_IRQn);
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(sleepSemaphore, &woken);
    portYIELD_FROM_ISR(woken);
  }
}

void deepSleep(uint32_t seconds) {
  // RTC2 est libre (RTC0=SoftDevice, RTC1=FreeRTOS)
  // LFCLK déjà démarré par Bluefruit.begin()
  NRF_RTC2->TASKS_STOP = 1;
  NRF_RTC2->TASKS_CLEAR = 1;
  NRF_RTC2->PRESCALER = 4095;      // 32768/4096 = 8 Hz
  NRF_RTC2->CC[0] = seconds * 8;   // ticks pour la durée demandée
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
}

void setup() {
#if DEBUG_PRINT
  Serial.begin(115200);
  delay(1000);
  Serial.println("Plant Sensor BTHome Starting...");
#endif

  // LEDs XIAO éteintes (active low, consomment si flottantes)
  pinMode(LED_RED,   OUTPUT); digitalWrite(LED_RED,   HIGH);
  pinMode(LED_GREEN, OUTPUT); digitalWrite(LED_GREEN, HIGH);
  pinMode(LED_BLUE,  OUTPUT); digitalWrite(LED_BLUE,  HIGH);

  // Sémaphore pour le deep sleep
  sleepSemaphore = xSemaphoreCreateBinary();

  // Init I2C
  Wire.begin();
  Wire.setClock(100000);

  // Init SHT40
  if (!sht4.begin()) {
#if DEBUG_PRINT
    Serial.println("SHT40 non détecté!");
#endif
  } else {
#if DEBUG_PRINT
    Serial.println("SHT40 OK");
#endif
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
  }

  // Init BH1750
  if (!lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE)) {
#if DEBUG_PRINT
    Serial.println("BH1750 non détecté!");
#endif
  } else {
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
    NRF_GPIO_PIN_NOSENSE
  );
  digitalWrite(SOIL_POWER_PIN, LOW);

  // QSPI flash en deep power-down (~0.5µA au lieu de ~30µA en standby)
  // 0xB9 = commande Deep Power-Down standard des flash SPI
  flashTransport.begin();
  flashTransport.runCommand(0xB9);
  flashTransport.end();

  // Init BLE (démarre aussi le LFCLK nécessaire pour RTC2)
  Bluefruit.autoConnLed(false);  // empêche Bluefruit de contrôler LED_BLUE
  Bluefruit.begin();
  Bluefruit.setName("Plant Sensor");
  Bluefruit.setTxPower(4);

  // Start BTHome service
  bthomeService.begin();

#if DEBUG_PRINT
  Serial.println("Setup terminé!");
#endif
}

void sendBTHomeData(uint8_t soil, float temp, float humidity, float lux, uint8_t battery) {
  // BTHome v2 payload
  uint8_t payload[31];
  uint8_t idx = 0;

  // BTHome header
  payload[idx++] = 0x40;  // v2, unencrypted

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
  serviceData[0] = 0xD2;  // BTHome UUID low byte
  serviceData[1] = 0xFC;  // BTHome UUID high byte
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

void loop() {
  static bool firstBoot = true;
  if (firstBoot) {
    firstBoot = false;
    deepSleep(60);  // Attend 1 minute au premier démarrage
  }

#if DEBUG_PRINT
  Serial.println("\n--- Lecture ---");
#endif

  // Battery
  int batteryRaw = analogRead(BATTERY_PIN);
  float voltage = (batteryRaw / 4095.0) * 3.0;
  int batteryPercent = map((int)(voltage * 100), 200, 300, 0, 100);
  batteryPercent = constrain(batteryPercent, 0, 100);
#if DEBUG_PRINT
  Serial.print("Bat: ");
  Serial.print(voltage);
  Serial.print("V (");
  Serial.print(batteryPercent);
  Serial.println("%)");
#endif

  // Soil - allume le capteur, attend stabilisation, lit, éteint
  digitalWrite(SOIL_POWER_PIN, HIGH);
  delay(50);
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

  if (sht4.getEvent(&humidity, &temp)) {
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

  // BH1750 - déclenche une mesure unique (auto power-down après)
  lightMeter.configure(BH1750::ONE_TIME_HIGH_RES_MODE);
  delay(200);
  float lux = lightMeter.readLightLevel();
#if DEBUG_PRINT
  Serial.print("Lux: ");
  Serial.println(lux);
#endif


  // Broadcast BTHome
  sendBTHomeData(soilMoisture, temperature, humidite, lux, batteryPercent);

  // Deep sleep adaptatif via RTC2
  deepSleep(computeNextInterval(lux, soilMoisture, temperature));
}
