# WSPR-Beacon-Controller
WSPR Beacon Controller


Układ składa jest w płytki Wemos D1mini  i opcjonalnego wyświetlacza OLED

Podłączony do beacona WSPR zakupionego z poniższego linku:

https://www.banggood.com/pl/SI5351-WSPR-Transmitter-with-TCXO-and-GPS-for-Stable-Frequency-Sync-Auto-QTH-Locator-23dBm-Output-p-2043151.html

Beacon ten wyposażony jest w nadajnik na SI5351 z TCXO i GPS dla stabilnej synchronizacji częstotliwości o mocy 23dBm.

Po podłaczeniu płytki WEMOS do sterownika beacona zbudowanego na procesorze ATMEGA 328 umozliwia zdalne, poprzez WWW zmiany parametrów pracy:
- częstotliwości
- znaku
- lokatora
- mocy

Kontroler pozwala na ustawienie 5 harmonogramów z możliwością ustawienia dnia, godziny i czestotliwości pozwalając na zmianę częstotliwości w zależności od propagacji. Na stronie WWW pokazuje godzinę wysłania ostatniego stringu do beacona, odebrana ramkę potwierdzającą przyjęcie komendy jak rónież wysłania beaconu WSPR. Na wyświetlaczu OLED po wysłaniu stringu zmieniającego parametry wartości te pokazują się na ekranie. Po 30 sekundach ekran wygasza się, ponowne uruchomienie następuje po naciśnięciu przycisku na porcie GPIO15 zwierany do 3V3. Wszytskie parametry ustawiane na stronie WWW jak i harmonogramy zapisywane sa w pamięci nieulotnej i po uruchomieniu są nadal działające.

Podłączenie:

Beacon WSPR:

![wspr_board](https://github.com/user-attachments/assets/d3cef5c9-f7b3-47c4-b702-5d3626a87345)

<img width="538" height="365" alt="WeMos-D1-Mini-Pinout" src="https://github.com/user-attachments/assets/6a422306-e107-4c8d-b658-128dd6aef174" />


Podłaczenie beacona do WEMOS:

Beacon   ->   WEMOS

RX       ->   GPIO3

TX       ->   GPIO1

GND      ->   GND

+5V      ->   5V

-----------------------------------------------------------------------------------------------

Podłaczenie OLED:

OLED   ->   WEMOS

SDA    ->   GPIO4

SCL    ->   GPIO5

GND    ->   GND

+3V3   ->   3V3

-----------------------------------------------------------------------------------------------

W katalogu ino dwie wersje programu: PL w języku polskim, EN w języku angielskim.

Zainstaluj wymagane biblioteki, kompilowane w arduino ver. 2.3.8

----------------------------------------------------------------------------------------------


# WSPR-Beacon-Controller
WSPR Beacon Controller

The system consists of a Wemos D1mini board and an optional OLED display.

Connected to a WSPR beacon purchased from the following link:

https://www.banggood.com/pl/SI5351-WSPR-Transmitter-with-TCXO-and-GPS-for-Stable-Frequency-Sync-Auto-QTH-Locator-23dBm-Output-p-2043151.html

This beacon is equipped with an SI5351 transmitter with a TCXO and GPS for stable frequency synchronization at 23dBm.

After connecting the WEMOS board to the beacon controller based on the ATMEGA 328 processor, it enables remote, web-based changes to operating parameters:
- frequency
- call sign
- locator
- power

The controller allows you to set 5 schedules with the ability to set the day, time, and frequency, allowing you to change the frequency depending on the propagation. The website displays the time the last string was sent to the beacon, the received frame confirming command acceptance, and the sending of the WSPR beacon. After sending a string changing the parameters, these values ​​appear on the OLED display. After 30 seconds, the screen goes blank; restarting is achieved by pressing a button on the GPIO15 port shorted to 3V3. All parameters set on the website and schedules are saved in non-volatile memory and remain operational after restart.

Connection:

WSPR Beacon:

![wspr_board](https://github.com/user-attachments/assets/d3cef5c9-f7b3-47c4-b702-5d3626a87345)

<img width="538" height="365" alt="WeMos-D1-Mini-Pinout" src="https://github.com/user-attachments/assets/6a422306-e107-4c8d-b658-128dd6aef174" />


Connecting the beacon to WEMOS:

Beacon -> WEMOS

RX -> GPIO3

TX -> GPIO1

GND -> GND

+5V -> 5V

----------------------------------------------------------------------------------------------

OLED connection:

OLED -> WEMOS

SDA -> GPIO4

SCL -> GPIO5

GND -> GND

+3V3 -> 3V3

----------------------------------------------------------------------------------------------

The ino directory contains two versions of the program: PL for Polish and EN for English.

Install the required libraries, compiled in Arduino version 2.3.8.
