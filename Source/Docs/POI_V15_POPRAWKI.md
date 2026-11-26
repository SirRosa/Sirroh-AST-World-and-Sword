# v15 — pushable, ściany tuneli, widoczność i aktywność POI

Stan po wznowieniu 15.11.2026: migracja 11 map zakończona; testy automatyczne PushSupport, PublishedLevels, RoomChains i ShaftTerrain przeszły. Test PushSupport potwierdza poprawną wysokość wyniku zapytania o podłoże również przy początkowym nakładaniu kolizji; nie jest testem całej animowanej interakcji AST. Końcowy checkpoint: `Saved/POICheckpoints/POI-v15-push-sight.zip`.

## Zakres zmian

Pushable nadal używają oryginalnego PullPushObject i BPC_Interaction AST, wraz z animacjami, warpami i IK. Poprawiono kolejność inicjalizacji aktora i strony chwytu przed montażem animacji. Wybór interakcji w sidescrollerze odrzuca boczne anchory odwrócone od gracza. Wymiary pomocniczej kolizji i testu podłoża wynikają teraz z rozmiaru obiektu. Specjalna obsługa początkowego nakładania kolizji zapobiega przekazywaniu punktu startowego jako nowej podłogi; ignorowane są również aktory podrzędne pchanego obiektu i postaci. Ta część dotyczy także oryginalnego sześcianu AST.

Trzy kopie meshy POI mają pivot wycentrowany zgodnie z założeniem AST. Aktorzy w dotychczasowych mapach zostają przesunięci o odpowiadający mu offset, aby zachować położenie obrazu w świecie. Migracja ma znacznik i nie przesuwa obiektów ponownie przy kolejnym uruchomieniu. Oryginalne meshe ZL_retroCaves pozostają bez zmian. Przy ręcznym ustawianiu nowego aktora użyj przyciągania do podłoża (End) — pivot znajduje się teraz w środku bryły.

Grafiki Background dla poziomych fragmentów Underground są nakładane na zakrzywioną tylną ścianę tunelu. W świecie płaska grafika tych Background jest ukryta, ale granice edycji i kolizja pozostają. Warsztat nadal pokazuje płaski Background do wygodnego budowania. Szyby zachowują dotychczasowe tła. Losowanie korzysta z puli przypisanej do motywu fragmentu, deterministycznie dla zapisanego świata.

Dodano ciemnozieloną osłonę miejsc odciętych od widoku postaci. Widoczność jest liczona w płaszczyźnie X/Z za pomocą 192 promieni co 0,1 sekundy, bez SceneCapture. Podłoga, sufit i obiekty blokujące kanał Visibility zasłaniają przestrzeń za sobą. Nie jest to symulacja oświetlenia ani pełna widoczność 3D. Drzwi muszą mieć kolizję na Visibility; widoczne szczeliny lub prześwit nad drzwiami mogą przepuszczać widok. Mocno obrócona kamera debugowa wyłącza osłonę — oceniaj ją z normalnej kamery gracza.

Odległe POI mają wyłączane ticki aktorów i komponentów oraz rendering. Są budzone w pobliżu gracza. Nie są usuwane, więc ta optymalizacja nie resetuje lokalnego stanu drzwi czy przedmiotów. Kolizja i fizyka pozostają; nie jest to jeszcze pełny system usypiania AI, timerów i symulacji wszystkich obiektów.

## Test ręczny — wykonuje użytkownik

1. Otwórz L_Pushable_U i przetestuj beczkę, skrzynię, ławkę oraz oryginalny sześcian AST. Podejdź z obu stron, naciśnij E, odczekaj bez ruchu, następnie pchaj/pociągnij A/D i puść obiekt. Sprawdź dłonie od pierwszej klatki chwytu oraz wysokość podstawy przed ruchem, w ruchu i po puszczeniu.
2. Powtórz na świecie PWG uruchomionym przez Main Menu. Użyj `SirrohPOIChain 0`. Sprawdź także pochyłość i krawędź podłoża. Jeżeli obiekt nadal podnosi się, podaj jego nazwę, stronę chwytu i czy dzieje się to w warsztacie, PWG czy obu.
3. Obejrzyj tło w Light_U/Pushable_U w świecie: grafika ma iść po zaokrąglonej ścianie, bez przecinającej ją płaszczyzny. Porównaj ze świadomie płaskim podglądem warsztatu.
4. Ze zwykłej kamery sprawdź powierzchnię, szyb, Underground oraz przejście przez drzwi. Przestrzeń za podłogą/sufitem ma być ciemnozielona. Sprawdź przełączenie zamknięte/otwarte drzwi oraz chwyt krawędzi przy granicy ciemności.
5. Zapisz i ponownie wczytaj świat. Istniejące plany ciągów nie wymagają nowego seeda; do sprawdzenia całej generacji możesz dodatkowo utworzyć nowy świat.

## Porównanie wydajności i diagnostyka

W konsoli:

- `stat unit` — czasy Frame, Game, Draw, GPU; zapisz wartości podczas postoju i biegu tą samą trasą.
- `stat game` — koszty logiki i ticków.
- `poi.Dormancy 0` / `poi.Dormancy 1` — porównanie bez/z usypianiem POI. Poczekaj sekundę po zmianie.
- `poi.Sight 0` / `poi.Sight 1` — porównanie bez/z zasłanianiem widoku.

Porównuj ten sam świat, pozycję i kamerę, po zakończeniu ładowania. Nie stwierdzono jeszcze pomiarem, że liczba aktorów była jedyną przyczyną spadków FPS. Kompilacja i testy strukturalne nie mierzą wydajności rozgrywki.

Następne etapy optymalizacji: profil Unreal Insights podczas przejścia między chunkami; osobne budżety wczytywania i aktywacji; trwały zapis stanu przed odładowaniem fragmentu; instancjonowanie dekoracji; limit świateł i cieni; aktywacja AI, pułapek i fizyki według odległości oraz pomieszczenia. Nie należy wyłączać symulacji wrogów czy odładowywać zniszczalnych obiektów bez mechanizmu zapisu ich stanu.

## Kopie bezpieczeństwa

Przed zmianą assetów zapisano `Saved/POICheckpoints/Before-v15-assets.zip`, obejmujący także oryginalny BPC_Interaction. Końcowy checkpoint v15 zawiera zmodyfikowany BPC_Interaction, kod, assety POI, bibliotekę po kompilacji i raporty. Przy przywracaniu checkpointu należy przywrócić razem kod/DLL oraz asset BPC_Interaction — Blueprint korzysta z nowych funkcji pomocniczych C++.
