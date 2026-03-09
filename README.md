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

Kontroler pozwala na ustawienie 5 harmonogramów z możliwością ustawienia dnia, godziny i czestotliwości pozwalając na zmianę częstotliwości w zależności od propagacji. Na stronie WWW pokazuje godzinę wysłania ostatniego stringu do beacona, odebrana ramkę potwierdzającą przyjęcie komendy jak rónież wysłania beaconu WSPR. Na wyświetlaczu OLED po wysłaniu stringu zmieniającego parametry wartości te pokazują się na ekranie. Po 30 sekundach ekran wygasza się, ponowne uruchomienie następuje po naciśnięciu przycisku. Wszytskie parametry ustawiane na stronie WWW jak i harmonogramy zapisywane sa w pamięci nieulotnej i po uruchomieniu są nadal działające.

Podłączenie:

Wemos D1: 

