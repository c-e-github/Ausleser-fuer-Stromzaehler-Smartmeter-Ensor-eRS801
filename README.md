# Ausleser fuer Stromzaehler/Smartmeter Ensor eRS801
Lesen von Zeit, bezogener elektrischer Leistung und eingespeister elektrischer Leistung über die P1 Kundenschnittstelle des Smartmeters.

Das Projekt dient zum Auslesen eines Ensor eRS801 Smartmeters (Stromzähler)
und zum Senden der Daten per LoRa an eine Gegenstelle.

Hardware:  LILYGO TTGO ESP32 LoRa32 V2.1.6
![Mikroprozessor](https://github.com/c-e-github/Ausleser-fuer-Stromzaehler-Smartmeter-Ensor-eRS801/blob/main/pics/TTGO_ESP32_LoRa_V2_pinout_pinmap.jpg)


In der Spezifikation des P1 Port des Smartmeters steht:
"5.7.2 Data line specification
Due to the use of optocouplers, the “Data” line must be designed as an OC (Open Collector) output, the Data line must be logically inverted."

Die meisten ESP32 Mikrocontroller haben Hardware-Unterstützung für die Invertierung des UART RX-Signals, aber offensichtlich hat mein Liligo ESP32-Board dies nicht - der Befehl "uart_set_line_inverse(UART_NUM_1, UART_SIGNAL_RXD_INV);" führte nicht dazu, dass die Signale korrekt eingelesen wurden. Daher habe ich eine kleine Hardware-Lösung zur Signal-Umkehr hinzufügen müssen:

Schaltplan Signal-Umkehrer:
![Signal-Umkehrer](https://github.com/c-e-github/Ausleser-fuer-Stromzaehler-Smartmeter-Ensor-eRS801/blob/main/pics/schaltplan-mit-transistor.jpg)
 
Fertige Lösung mit 3D-gedrucktem Gehäuse:
![Gehaeuse](https://github.com/c-e-github/Ausleser-fuer-Stromzaehler-Smartmeter-Ensor-eRS801/blob/main/pics/e450-Leser-gh.jpg)

