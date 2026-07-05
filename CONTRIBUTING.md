# Contributing to K0WLY Two-Way CW Keyer

Thank you for your interest in contributing! This is an open hardware
project by K0WLY, licensed under CERN-OHL-W v2 and CC BY 4.0.

## How to Contribute

### Reporting Issues
- Use GitHub Issues to report bugs or suggest improvements
- Include your hardware version, firmware version, and a clear description
- For keyer timing issues, include your WPM setting and what behavior you observed

### Submitting Changes

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-improvement`)
3. Make your changes
4. Test thoroughly on actual hardware before submitting
5. Submit a Pull Request with a clear description of what changed and why

### Contribution Areas

- **Firmware** — Bug fixes, new features, performance improvements
- **Hardware** — Schematic corrections, PCB improvements, BOM updates
- **Documentation** — Corrections, clarifications, translations
- **Testing** — Verified builds on different hardware configurations

## Attribution

By contributing to this project you agree that your contributions
will be licensed under the same licenses as the project:

- Hardware and firmware: CERN-OHL-W v2
- Documentation: CC BY 4.0

All contributions must retain attribution to K0WLY as the original author.

## Code Style

- Follow the existing code style in `main.cpp`
- Comment clearly — future builders will read this code
- Keep the keyer task on Core 1 and WiFi/display on Core 0
- Do not introduce blocking operations into the keyer task

## Contact

For questions, reach out via GitHub Issues or on the amateur radio
bands — K0WLY operates from Grid DN40, Saratoga Springs, Utah.

73 de K0WLY
