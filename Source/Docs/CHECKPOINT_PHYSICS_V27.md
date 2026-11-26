# v27 — zatrzymywanie pchającej postaci i uruchamianie kontaktu pięści

## Zmiany
Ogranicznik wejścia pchania otrzymywał znak A/D zamiast kierunku ruchu w świecie. Przy ScreenRightDirection=-X jego znak był odwrócony względem obliczonego opóźnienia przedmiotu. Teraz zarówno wejście, jak i opóźnienie są liczone w osi świata.

Samo osłabienie wejścia nie usuwało prędkości Mover. Aktywny napęd pushable rejestruje teraz OnPostMovement: postać nie może wyprzedzić punktu chwytu w kierunku nacisku o więcej niż 12 cm, a na granicy jej prędkość nie może przekraczać prędkości przedmiotu. Można natychmiast zmienić kierunek i ciągnąć. Napęd przedmiotu dostaje intencję kierunku (docelowo 160 cm/s), dzięki czemu próba pchania trwa również po zatrzymaniu postaci; siła maksymalna pozostała 800 N. Nie zwiększano zasięgu interakcji ani mas. Nie zmieniano kolizji wspinaczki.

Graf AST wywołuje ObserveASTMelee bezpośrednio z wyjścia Play Montage (Mover Actor). Mover odtwarza montaż poprzez swoją symulację, więc wyszukanie aktywnej instancji natychmiast mogło niczego nie znaleźć. Obserwator zapamiętuje teraz żądany montaż i czeka do 0,75 s na jego faktyczny start. Dopiero wtedy inicjalizuje śledzenie dłoni i listę trafionych obiektów. Utrzymano ograniczone impulsy oraz jedno trafienie przedmiotu na montaż. Złagodzono kierunkowy filtr ruchu, by pionowy cios typu uppercut nie wymagał dużej składowej do przodu.

## Test po ponownym uruchomieniu edytora
Nie trzeba ponownie publikować POI ani przebudowywać map.
1. Złap skrzynię przez E i pchaj w ścianę przez kilka sekund. Postać powinna zatrzymać się z przedmiotem i zachować chwyt.
2. Powtórz w przeciwną stronę, następnie wpychając obiekt w drugi pushable. Przy zmniejszeniu oporu oba powinny móc ruszyć. Zmień kierunek — ciągnięcie powinno działać bez ponownego E.
3. Puść przedmiot, włącz `poi.UnarmedContactLog 1`, wykonaj cios pięścią przy beczce/ławce. Log powinien zawierać `POI fists started montage=...`, a przy fizycznym kontakcie `POI fist contact target=...`. Potem wyłącz przez `poi.UnarmedContactLog 0`.
4. Jeżeli nie ma ruchu, rozróżnij: brak wpisu aktywacji, komunikat timeout, aktywacja bez kontaktu, kontakt bez ruchu. To wskaże konkretny dalszy etap do sprawdzenia, bez zgadywania masy.
5. Sprawdź również Space podczas pchania oraz chwyt kołyszącej się skrzyni na L_POI_Test_Pendulum.

## Weryfikacja i checkpoint
Kompilacja: Saved/POICheckpoints/Build-v27.log. Automatyzacja: Tests-v27.log i POI-v27-tests. Dodano sprawdzenie zatrzymania i odwrócenia ruchu w obu kierunkach świata oraz ograniczenia prędkości względem wolnego przedmiotu. Nie testowano gameplay przez sterowanie pulpitem. Testy matematyczne/regresyjne nie są potwierdzeniem wszystkich animacji pięści w grze.

Pozostają ograniczenia v26: wahadło jest prototypem z prostym nośnikiem; pełne testy multiplayer, nowa widoczność i profilowanie całego PWG nie są zakończone.
