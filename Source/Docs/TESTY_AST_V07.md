# Ręczny test AST v0.7

Testy w grze wykonuje użytkownik. Agent nie steruje komputerem w celu testowania gry.

## Przygotowanie
1. Otwórz L_POI_Workshop_AST po ponownym uruchomieniu edytora z nową kompilacją.
2. Używaj BP_ClimbPlatform. Settings: Kind=AST, Both AST Sides=true, AST Ledge=Ledge. Nie dodawaj dodatkowych niezależnych chwytów na tej samej krawędzi.
3. Rozmiar bryły zmieniaj przez Settings → Size. Skalowanie całego aktora też jest obsługiwane, ale pozostaw go pionowo (bez Pitch/Roll).
4. Collision=OneWay przepuszcza od dołu i podpiera z góry. NoPhysicalCollision nie podpiera po wejściu.
5. Zacznij od pojedynczej platformy 240×100×40, z górą na wysokości 220 cm nad podłogą. Niska platforma może nie mieć miejsca na pełny zwis.
6. Mapa używa BP_POITest_PWG i kontrolera Sirroh. Nie przełączaj NumPad8 podczas tej próby.

## Kolejność prób
| Próba | Czynność | Oczekiwany wynik |
|---|---|---|
| Grafika | Ustaw skalę Z aktora np. 2,4 | Widoczna boczna grafika ma wysokość bryły; góra i spód są przy jej skrajach. Poszarpany obrys PNG zachowuje przezroczystość. |
| Chwyt od spodu | Stań pod środkową częścią platformy i skocz | Chwyt przedniej krawędzi przez natywne AST, bez konieczności obchodzenia bocznego końca. |
| Wejście | W zwisie puść kierunki i naciśnij Spację | Natywna animacja wejścia na aktualną fizyczną platformę. |
| Puszczenie | W zwisie naciśnij C | Puszczenie, bez natychmiastowego ponownego chwytu; krótka blokada ponownego automatycznego chwytu. |
| Kolejny chwyt | Dodaj drugą platformę 100–140 cm wyżej; w zwisie W + Spacja | Wybranie wyższego chwytu i przejście przez oryginalną funkcję Find Hop Montage. |
| Chwyt z boku | Druga platforma do 200 cm obok; kierunek A/D + Spacja | Próba przejścia w stronę wskazaną na ekranie. Dobór animacji nadal należy do AST. |
| Narożnik | W zwisie przesuwaj się A/D do końca krawędzi | Przejście obsługiwane przez natywne wykrywanie narożnika AST. |
| Swobodny zwis | Platforma grubości 40 cm, AST Hang=Free | Swobodne nogi. |
| Podparcie | Pełny blok wysokości 220 cm, AST Hang=Braced | Styl z podparciem nóg; IK pozostaje z AST. |
| Lane | Przesuń platformę w osi Y kolejno na inne lane’y; powtórz skok pod nią | Ten sam cel w X/Z pozostaje wykrywany. Połączenie w głębokości wykonuje oryginalny motion warping AST. |

Automatic wybiera Braced przy wysokości bryły co najmniej 120 cm i dostępnej kolizji; w innym wypadku Free. Do porównania stylów wymuś je osobno.

## Gdy próba nie przejdzie
Podaj numer/nazwę próby, czy postać była na ziemi czy w zwisie, ustawienia Collision i AST Hang oraz wysokość/pozycję obu platform. Zrzut panelu Settings i krótkie nagranie pomagają odróżnić brak celu od odrzucenia animacji przez AST. Nie ma potrzeby ręcznego poprawiania spline w standardowym BP_ClimbPlatform.

## Status
Poprawki są przeznaczone do Twojej weryfikacji. Testy automatyczne dotyczą kierunków oryginalnych krawędzi, rozmiarów grafiki, kolizji i danych modułów; nie są potwierdzeniem wszystkich powyższych zachowań w grze.
