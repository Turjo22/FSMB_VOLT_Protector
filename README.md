<img width="1182" height="830" alt="image" src="https://github.com/user-attachments/assets/1e5b3890-2ca3-4317-9a24-bb1acdfe9d75" />Microcontroller-Based Voltage Protector Device
A complete hardware-software solution designed to protect electrical appliances from sudden power-grid voltage surges, sags, and over-current faults. The system continuously monitors grid parameters using an ATmega32 microcontroller and automatically disconnects the load during fault conditions. It includes a Python-based Graphical User Interface (GUI) for real-time monitoring and threshold configuration over a serial connection.

Features
Over-Voltage Protection: Automatically cuts off the power line if the grid voltage exceeds the user-defined upper threshold.

Under-Voltage Protection: Disconnects the appliance if the grid voltage drops below the safe lower threshold, preventing motor stalling or brownout damage.

Over-Current Protection: Monitors load current in real-time and severs the connection if a short circuit or heavy draw exceeds the safe limit.

Bi-Directional Synchronization: Operating thresholds can be updated directly on the physical device or via the computer GUI. Changes are saved to the microcontroller's EEPROM.

Mutual Exclusion Lock: Prevents simultaneous threshold editing. If the physical device settings menu is open, GUI updates are blocked to ensure data integrity.

Real-Time Telemetry: The GUI displays live RMS voltage, current, fault status, and relay state.

Hardware Components
Microcontroller: ATmega32 (Handles ADC, Timer0, and UART interrupts)

Voltage Sensing: Voltage Transformer (VT) with signal conditioning circuit

Current Sensing: ACS712 Hall-effect current sensor

Display: 16x2 Character LCD (LM016L) for local status output

User Input: 4 tactile push buttons (Settings, Select, Up, Down) for on-device configuration

Protection Mechanism: 12V Relay for load isolation


<img width="1182" height="830" alt="image" src="https://github.com/user-attachments/assets/8948ccfc-f02f-4658-bc1b-66332d73335f" />


Software & Tools
Firmware: Written in C for AVR (utilizes ADC, Timer0, and UART interrupts)

Computer Interface: Python 3 (Tkinter for GUI, pyserial for communication)

Simulation: Proteus Design Suite

Virtualization: com0com (for creating virtual COM port pairs)

System Architecture
The firmware is fully interrupt-driven to maintain real-time responsiveness:

ADC Interrupt: Continuously samples the VT and ACS712 sensors without blocking the main loop.

Timer0 Interrupt: Scans and debounces the physical push buttons.

UART Interrupt: Handles incoming commands from the Python GUI and transmits live telemetry data back to the computer at 9600 baud.

Simulation Setup & Usage
Configure Virtual Ports:

Install and open com0com.

Create a virtual COM port pair (e.g., COM1 and COM2).

Run the Proteus Simulation:

Open the Proteus project file.

Double-click the COMPIM component and set its port to the first half of your virtual pair (e.g., COM2). Ensure the baud rate is set to 9600.

Start the simulation.

Launch the GUI:

Install the required Python packages: pip install pyserial

Open protector_gui.py and ensure DEFAULT_PORT is set to the second half of your virtual pair (e.g., COM1).

Run the Python script: python protector_gui.py

Click Connect in the GUI to establish communication with the simulated microcontroller.

Validation Tests
This project has been successfully simulated and validated against the following test cases:

Load disconnection upon exceeding the over-voltage threshold.

Load disconnection upon dropping below the under-voltage threshold.

Load disconnection upon exceeding the over-current threshold.

Mutual exclusion lock successfully blocking GUI edits while the device menu is active.

Successful EEPROM updates and real-time synchronization between the computer GUI and the microcontroller.
