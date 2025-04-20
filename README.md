# ESP32 Hydroponics Controller

A smart hydroponics system controller using ESP32 and Lilygo 4-Relay board.

## Features

- **Sensor Monitoring**:
  - Liquid level 
  - pH level 
  - TDS/nutrient level 
  - Water temperature

- **Automated Control**:
  - Water pump scheduling
  - Grow light timing
  - pH adjustment (planned)
  - Nutrient dosing (planned)

- **Growth Cycle Management**:
  - Configurable growth stages (seedling, growing, harvesting)
  - Stage-specific settings:
    - Watering schedule
    - Light hours
    - pH ranges

- **Connectivity**:
  - Web interface (ESPAsyncWebServer)
  - WiFi configuration (WiFiManager)
  - MQTT integration for Home Assistant
  - NTP time synchronization

- **Configuration**:
  - Web-based configuration
  - Persistent storage (Preferences)
  - Sensor calibration

## Hardware Requirements

- ESP32 development board
- Lilygo 4-Relay board
- HX710B barometric sensor with 2.5mm silicone tubing for water level
- pH sensor
- TDS sensor - leaks current, haven't used it
- DS18B20 temperature sensor
- 12V water pump
- 12V grow lights - optional
- pH Up/Down solution pumps (planned)
- Nutrients pump (planned)

## Configuration

The system can be configured via the web interface at `http://<device-ip>`.

Default credentials:
- Username: `admin`
- Password: `admin`

Configuration options include:
- WiFi settings
- MQTT server/credentials
- NTP server
- Sensor calibration
- Growth profiles
- Active growth cycle

## Web Interface

The web interface provides:
- Real-time sensor monitoring
- Relay control
- Configuration management
- Growth cycle visualization
- System status

## TODOs / Future Improvements

From code analysis, these features are planned or need improvement:

1. **pH Control**:
   - Implement pH Up/Down pump control
   - Add automatic pH adjustment logic

2. **Nutrient Dosing**:
   - Add food pump control
   - Implement dosing schedule

3. **Hardware Improvements**:
   - Add pin configuration for different boards
   - Fix TDS sensor interference with pH readings

4. **Integration**:
   - Better Home Assistant integration
   - Time zone support

## License

[Apache License 2.0](LICENSE)
