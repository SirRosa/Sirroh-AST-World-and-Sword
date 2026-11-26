# v28 — ponowna diagnoza pchania i pięści

Poprzednie testy v27 przeszły, lecz użytkownik potwierdził nadal występujące błędy. Nie traktujemy wyników izolowanych testów jako potwierdzenia gameplay.

## Zmiany
- Limit chwytu działa także przy zerowym wejściu. Zatrzymuje bezwładność/root motion postaci po obu stronach punktu chwytu, zamiast wyłączać się, gdy RequestedAxis=0. Nie zwiększono zasięgu interakcji ani siły napędu.
- Test pchania obejmuje zarejestrowany callback OnPostMovement rzeczywistego komponentu Mover, nie tylko pomocniczą funkcję liczącą limit.
- Adapter fizycznych pięści jest instalowany na postaci Sirroh razem z jej wejściem ruchu. Obserwuje aktywny montaż wskazany przez MeleeMontageResult AST także bez wywołania z konkretnej gałęzi Blueprintu. W spoczynku sprawdza stan 20 razy na sekundę; w trakcie montażu pracuje co klatkę.
- Odczyt dłoni ma zależność tick od mesha. Usunięto filtr kierunku całego aktora oraz arbitralne okno 15–75% montażu. Fizyczny kontakt wymaga ruchu dłoni w stronę trafionej powierzchni. Nieruchome nakładanie dłoni, ruch oddalający od powierzchni oraz skok dłoni ponad 80 cm w klatce nie powodują impulsu. Każdy obiekt dostaje najwyżej jeden impuls na montaż.
- Testy używają rzeczywistego SweepMultiByObjectType przeciw fizycznym wariantom pushable, z obu stron, i sprawdzają brak powtórnego impulsu.

Korekta poprzedniej diagnozy: PlayMoverMontage w użytej wersji silnika może od razu odtworzyć animację; odroczony jest ruch montażu. Samo oczekiwanie dodane w v27 nie dowodziło przyczyny problemu.

## Test ręczny po ponownym uruchomieniu projektu
1. E na skrzyni, pchanie w ścianę przez kilka sekund, puszczenie kierunku, ponowny nacisk i zmiana kierunku. Powtórz w lewo oraz w prawo, potem z drugim pushable jako przeszkodą.
2. Po puszczeniu E uderz beczkę pięścią z lewej, potem z prawej strony. Sprawdź także ławkę.
3. Jeśli nadal brak reakcji, włącz `poi.UnarmedContactLog 1` przed próbą. Zanotuj obecność `POI fists started`, `POI fist contact` albo `timed out`; następnie wyłącz `poi.UnarmedContactLog 0`. Nie trzeba otwierać edytora tylko po to, dopóki nie planujesz testu.
4. Sprawdź Space z pchania i wspinaczkę, aby wykryć regresję zwalniania chwytu.

Nie trzeba ponownie publikować POI. Zmiany dotyczą kodu; map i Blueprintów użytkownika nie przebudowywano.

## Weryfikacja
Build-v28.log oraz Tests-v28.log w Saved/POICheckpoints. Testy gry przez sterowanie pulpitem nie były wykonywane. Obserwacja pełnego ataku/animacji w świecie pozostaje do potwierdzenia przez użytkownika. Pozostałe otwarte zadania z v26 (widoczność, profilowanie PWG, pełna weryfikacja sieciowa wahadła) pozostają otwarte.

Dodatkowo: śledzenie dłoni korzysta z końcowej pozy SirrohPoseBridge (z uwzględnieniem proceduralnych nadpisań kości), z zależnością tick od tego komponentu. Stan montażu nadal pochodzi z AST. Pozwala to uniezależnić miejsce trafienia od niewidocznego mesha źródłowego.

Zasięg aktywnego chwytu jest teraz liczony przez GetClosestPointOnCollision do rzeczywistej powierzchni przedmiotu (fallback do bounds tylko jeśli collider nie obsługuje zapytania). Limit pozostaje 100 cm. Obrót dużej skrzyni nie porównuje już pozycji postaci z odległym punktem obracającym się wraz z bryłą.
