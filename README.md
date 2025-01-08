# M5CoreInk Light Meter

This project is a light meter application for the M5Stack CoreInk device. It measures the ambient light using a BH1750 sensor and calculates the exposure value (EV) to help photographers determine the correct aperture, shutter speed, and ISO settings for their cameras.

## Installation

1. Clone the repository:
   ```sh
   git clone https://github.com/jujax/m5coreink-lightmeter.git
   ```
2. Open the project in PlatformIO.
3. Connect your M5Stack CoreInk device to your computer.
4. Upload the code to the device.

## Usage

1. Power on the M5Stack CoreInk device.
2. The light meter will initialize and display the current settings.
3. Use the buttons to navigate the menu and adjust the settings:
   - `BTN_UP`: Move up in the menu or increase the selected value.
   - `BTN_DOWN`: Move down in the menu or decrease the selected value.
   - `BTN_OK`: Select the current menu item or toggle edit mode.
   - `BTN_POWER`: Measure the ambient light and update the exposure value (EV).

## Contributing

Contributions are welcome! Please follow these steps to contribute:

1. Fork the repository.
2. Create a new branch for your feature or bugfix.
3. Commit your changes and push the branch to your fork.
4. Create a pull request with a description of your changes.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for more information.
