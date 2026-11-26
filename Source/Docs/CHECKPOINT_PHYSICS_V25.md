# v25 — regresja chwytu i impulsy fizyczne

Przyczyna teleportowania: Update PushPullCollision TransformWhileMove w AST nadal zapisywał relative location/rotation korzenia przedmiotu po jego odłączeniu od proxy. Wówczas wartości względne stawały się współrzędnymi świata. Wyłączono dwa konkurujące zapisy; zachowano aktualizacje proxy i IK. Przed inicjalizacją chwytu zapisywana jest poza i prędkość przedmiotu, przy uruchomieniu napędu fizycznego są przywracane.

Wbieganie wykorzystuje zamierzony ruch sweepu, ponieważ prędkość Mover po trafieniu może już wynosić zero. Impulsy mają ograniczenie zależne od masy, 60 cm/s przyrostu i budżet ciągłego nacisku 100 N. Nie zmieniono mas wariantów.

Obie ścieżki kontaktu broni przekazują ograniczony impuls do symulowanych przedmiotów, raz na trafiony obiekt w fazie ataku. Impuls zależy od zmierzonego ruchu i masy broni; nieruchome nakładanie broni nie jest uderzeniem.

Zachowano lokalne cele ruchomej krawędzi i politykę kontaktu postaci podczas wspinaczki. Przedmioty nie są zamrażane na czas wspinaczki.

## Weryfikacja i ograniczenia

Build-v25.log: końcowy wynik kompilacji. Tests-v25.log i POI-v25-tests: PushSupport oraz MovingTraversal. Assets-v25.log: aktualizacja AST i sprawdzenie, że usunięte zapisy nie mają wejść wykonania. Testy obejmują przesunięty o 500 cm proxy przy chwycie, fizyczne impulsy broni, limity kontaktu, opóźnienie celu ruchomej krawędzi. Nie są dowodem poprawności wszystkich animacji w grze. Nie wykonywano testów przez pulpit ani przebudowy map użytkownika.

## Test ręczny

Uruchom ponownie edytor i świat przez Main Menu. Nie publikuj ponownie POI.
1. Na świeżej beczce, skrzyni i ławce: E, chwila bez ruchu, ruch w obie strony, puszczenie i ponowny chwyt. Brak teleportowania; zmiana kierunku przechodzi w ciągnięcie.
2. Wbiegnij sprintem w dotąd nietknięty przedmiot; porównaj mały i cięższy. Oczekiwany ograniczony ruch, bez wystrzelenia. Obiekt przy ścianie może pozostać nieruchomy.
3. Uderz przedmiot bronią. Następnie przyłóż nieruchomą broń: samo nakładanie nie powinno stale odpychać.
4. Rozpędź przedmiot, skocz i złap krawędź; sprawdź wejście na górę, zwis i puszczenie. Chwyt powinien śledzić ruch, bez zamrażania fizyki ani wystrzelenia.
5. Podczas E naciśnij Space: chwyt pushable powinien zostać zwolniony przed wspinaczką.

Opcjonalnie poi.PhysicsContactLog 1 podczas krótkiej próby sprintu, potem poi.PhysicsContactLog 0.

## Nadal otwarte

Pełne testy gry wykonuje użytkownik. Widoczność z gradientem/portalami i profilowanie POI z poprzedniego planu nadal nie są zakończone. Ten checkpoint dotyczy zgłoszonej regresji pushable i kontaktów fizycznych.

Końcowa weryfikacja 2026-11-18: AST_V1_18Editor Win64 Development — Succeeded. Wyniki automatyzacji zapisane w Tests-v25.log. Archiwum POI-v25-push-and-contact-fix.zip zawiera moduł edytora, nie nowy packaged build gry.

Weryfikacja po wznowieniu 2026-11-19: kompilacja Succeeded; PushSupport i MovingTraversal — Success. Test impulsu używa odrębnej sceny dla każdego wariantu (synchroniczny test wykonywał warianty w jednej klatce silnika). Pomiar po impulsie: Barrel 29.995 cm/s, Crate 19.997 cm/s, Bench 47.992 cm/s. Wszystkie obiekty obudzone. Nie jest to pomiar wydajności ani test ręczny gry.
