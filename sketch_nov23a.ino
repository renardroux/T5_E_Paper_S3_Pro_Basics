/**
 * Touch Coordinates Display on E-Paper
 * Touch sensor, home button, and BOOT button (GPIO 0) with display updates
 * 
 * NOTE: IO48 button cannot be used because the PCA9535 is owned by the display
 * power control. Using BOOT button (GPIO 0) instead with Button2 library.
 */
#include <Wire.h>
#include <SPI.h>
#include <Arduino.h>
#include "TouchDrvGT911.hpp"
#include <FastEPD.h>
#include <Button2.h>

// Touch sensor pins
#define SENSOR_SDA  39
#define SENSOR_SCL  40
#define SENSOR_IRQ  3
#define SENSOR_RST  9

// BOOT button (alternative to IO48)
#define BOOT_BUTTON 0

// GT911 Interrupt modes
#ifndef LOW_LEVEL_QUERY
#define LOW_LEVEL_QUERY 2
#endif

TouchDrvGT911 touch;
FASTEPD epaper;
Button2 bootButton;

int16_t x[5], y[5];

// Flags
volatile bool homeButtonPressed = false;
volatile bool bootButtonPressed = false;

// Button2 callback for BOOT button
void bootButtonHandler(Button2& btn) {
    Serial.println("BOOT button pressed (Button2 callback)!");
    bootButtonPressed = true;
}

void setup()
{
    Serial.begin(115200);
    delay(3000);
    
    Serial.println("Starting setup...");
    
    // Check PSRAM
    if (psramInit()) {
        Serial.println("PSRAM initialized");
    } else {
        Serial.println("PSRAM failed");
    }

    // Initialize e-paper display
    epaper.initPanel(BB_PANEL_EPDIY_V7);
    epaper.setPanelSize(960, 540);
    epaper.setRotation(90); // Portrait orientation
    
    // Clear display and show initial message
    epaper.setMode(BB_MODE_1BPP);
    epaper.clearWhite(true);
    epaper.setFont(FONT_12x16);
    epaper.setTextColor(BBEP_BLACK);
    epaper.setCursor(10, 30);
    epaper.println("Touch Coordinate Display");
    epaper.setCursor(10, 50);
    epaper.println("Touch the screen...");
    epaper.fullUpdate(true);

    // Initialize touch sensor
    touch.setPins(SENSOR_RST, SENSOR_IRQ);
    
    if (!touch.begin(Wire, GT911_SLAVE_ADDRESS_L, SENSOR_SDA, SENSOR_SCL)) {
        Serial.println("Failed to find GT911!");
        while (1) {
            delay(1000);
        }
    }
    
    Serial.println("GT911 touch sensor ready!");
    
    touch.setHomeButtonCallback([](void *user_data) {
        Serial.println("Home button pressed!");
        homeButtonPressed = true;
    }, NULL);
    
    touch.setInterruptMode(LOW_LEVEL_QUERY);
    
    // Setup BOOT button (GPIO 0) with Button2 library
    // activeLow=true because BOOT button pulls to ground when pressed
    bootButton.begin(BOOT_BUTTON, INPUT_PULLUP, true);
    bootButton.setPressedHandler(bootButtonHandler);
    bootButton.setDebounceTime(50);
    
    Serial.println("BOOT button initialized on GPIO 0");
    Serial.println("Ready! Touch screen, press home button, or press BOOT button (GPIO 0).");
}

void loop()
{
    static unsigned long lastUpdate = 0;
    static bool wasPressed = false;
    
    // CRITICAL: Call Button2 loop to process button events
    bootButton.loop();
    
    // Check home button
    if (homeButtonPressed) {
        homeButtonPressed = false;
        displayHomeButtonMessage();
    }
    
    // Check BOOT button pressed flag
    if (bootButtonPressed) {
        bootButtonPressed = false;
        Serial.println("Processing BOOT button press...");
        displayBootButtonMessage();
    }
    
    // Check touch
    bool pressed = touch.isPressed();
    
    if (pressed) {
        uint8_t touched = touch.getPoint(x, y, touch.getSupportTouchPoint());
        
        if (touched > 0 && !wasPressed) {
            wasPressed = true;
            
            if (millis() - lastUpdate > 300) {
                Serial.print(millis());
                Serial.print("ms X[0]:");
                Serial.print(x[0]);
                Serial.print(" Y[0]:");
                Serial.println(y[0]);
                
                updateDisplay(touched);
                lastUpdate = millis();
            }
        }
    } else {
        if (wasPressed) {
            wasPressed = false;
        }
    }
    
    delay(10); // Reduced delay for better button responsiveness
}

void updateDisplay(uint8_t touched)
{
    // Clear the content area (leave the title at top)
    epaper.fillRect(0, 70, epaper.width(), epaper.height() - 70, BBEP_WHITE);
    
    // Display touch information
    epaper.setFont(FONT_12x16);
    epaper.setTextColor(BBEP_BLACK);
    
    int yPos = 90;
    char buffer[50];
    
    sprintf(buffer, "Touch Points: %d", touched);
    epaper.setCursor(10, yPos);
    epaper.println(buffer);
    yPos += 25;
    
    for (int i = 0; i < touched && i < 5; i++) {
        sprintf(buffer, "Point %d: X=%d Y=%d", i, x[i], y[i]);
        epaper.setCursor(10, yPos);
        epaper.println(buffer);
        yPos += 25;
    }
    
    // Draw a circle at touch position
    if (touched > 0) {
        // Map touch coordinates (5-539, 7-959) to display (0-540, 0-960)
        int displayX = map(x[0], 5, 539, 5, epaper.width());
        int displayY = map(y[0], 7, 959, 0, epaper.height());
        
        displayX = constrain(displayX, 0, epaper.width() - 1);
        displayY = constrain(displayY, 0, epaper.height() - 1);
        
        epaper.drawCircle(displayX, displayY, 10, BBEP_BLACK);
        epaper.fillCircle(displayX, displayY, 5, BBEP_BLACK);
    }
    
    // Partial update - use X coordinates for rotated screen
    epaper.partialUpdate(true, 0, epaper.width() - 1);
    
    // Reset I2C for touch sensor (CRITICAL for continued operation)
    pinMode(SENSOR_SDA, INPUT_PULLUP);
    pinMode(SENSOR_SCL, INPUT_PULLUP);
    delay(10);
    Wire.begin(SENSOR_SDA, SENSOR_SCL);
    delay(50);
}

void displayHomeButtonMessage()
{
    int boxX = 10;
    int boxY = 400;
    int boxW = epaper.width() - 20;
    int boxH = 100;
    
    epaper.fillRect(boxX, boxY, boxW, boxH, BBEP_WHITE);
    epaper.drawRect(boxX, boxY, boxW, boxH, BBEP_BLACK);
    
    epaper.setFont(FONT_12x16);
    epaper.setTextColor(BBEP_BLACK);
    
    epaper.setCursor(80, 430);
    epaper.print("Home Button Pressed!");
    
    epaper.setCursor(60, 450);
    epaper.print("Press anywhere to continue");
    
    // Use X coordinates for partial update (rotated screen)
    epaper.partialUpdate(true, boxX, boxX + boxW);
    
    // Reset I2C
    pinMode(SENSOR_SDA, INPUT_PULLUP);
    pinMode(SENSOR_SCL, INPUT_PULLUP);
    delay(10);
    Wire.begin(SENSOR_SDA, SENSOR_SCL);
    delay(50);
}

void displayBootButtonMessage()
{
    int boxX = 10;
    int boxY = 250;
    int boxW = epaper.width() - 20;
    int boxH = 100;
    
    epaper.fillRect(boxX, boxY, boxW, boxH, BBEP_WHITE);
    epaper.drawRect(boxX, boxY, boxW, boxH, BBEP_BLACK);
    
    epaper.setFont(FONT_12x16);
    epaper.setTextColor(BBEP_BLACK);
    
    epaper.setCursor(90, 280);
    epaper.print("BOOT Button Pressed!");
    
    epaper.setCursor(100, 300);
    epaper.print("Button detected at ");
    epaper.print(millis());
    epaper.print("ms");
    
    // Use X coordinates for partial update (rotated screen)
    epaper.partialUpdate(true, boxX, boxX + boxW);
    
    // Reset I2C
    pinMode(SENSOR_SDA, INPUT_PULLUP);
    pinMode(SENSOR_SCL, INPUT_PULLUP);
    delay(10);
    Wire.begin(SENSOR_SDA, SENSOR_SCL);
    delay(50);
}