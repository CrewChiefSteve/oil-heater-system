#ifndef PINS_H
#define PINS_H

// SPI Bus (Shared by all MAX31855K thermocouples)
#define SPI_SCK         18
#define SPI_MISO        19
// Note: MAX31855K is read-only, no MOSI needed

// Chip Select pins for MAX31855K thermocouples
#define CS_TIRE_IN      5   // Inside tire probe
#define CS_TIRE_MID     6   // Middle tire probe
#define CS_TIRE_OUT     7   // Outside tire probe
#define CS_BRAKE        15  // Brake rotor probe

// Status LED
#define RGB_LED         48  // WS2812B RGB LED (single pixel)

// Power management
#define VBAT_ADC        1   // Battery voltage ADC input
#define CHRG_STAT       2   // TP4056 charge status pin

// Audio feedback
#define BUZZER          47  // Buzzer for alerts (optional)

#endif // PINS_H
