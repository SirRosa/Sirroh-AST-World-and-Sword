# v12 — szyby, teren i przedni lane

## Zmiana generacji

Zmiany planu obowiązują **nowo utworzone światy**. Istniejące sejwy zachowują stare położenie i wysokość szybów; nie migrujemy automatycznie terenu pod postacią ani istniejących POI. Nie trzeba ponownie publikować swoich fragmentów.

- Przy włączonych szybach POI powierzchnia prowadzi do Underground. Stare bezpośrednie przejścia surface→caves nie omijają już systemu POI.
- Kolejne przejście underground→caves jest przesunięte o około 18 m w X. Nie wycina dziury dokładnie pod wyjściem z poprzedniego szybu. Jeśli region jest zbyt krótki, żeby zachować 9 m odstępu, ten dodatkowy portal jest pomijany zamiast nakładany.
- Generator najpierw wybiera 4–6 modułów: dół, losowe środki, góra. Wysokość jest sumą ich rzeczywistych wysokości, bez skalowania. Typowe moduły 300 cm dają 12–18 m.
- Dokładna kolejność modułów oraz TopZ/BottomZ trafiają do zapisu świata. Wczytanie nie losuje ponownie konstrukcji.
- Teren powierzchni jest wyrównywany przy górnym końcu. Podłogi podziemi dochodzą do końców POI na pełnej szerokości wejścia/wyjścia. Między punktami dopasowania podłoga łagodnie interpoluje wysokość; poza skrajnymi punktami wraca do profilu bazowego. Ta wersja nie przebudowuje systemu terenu na niszczalny.

## Lane i ruch

- Otwory w podłogach i sufitach obejmują przedni oraz środkowy lane. Tylny lane pozostaje poza otworem.
- Przy wejściu z przodu kontroler przekazuje relokację do Mover: zmienia Y na środek szybu, zachowuje X/Z i aktualizuje płaszczyznę ruchu. Nie wykonuje jej podczas aktywnej wspinaczki lub interakcji AST.
- Warunek uwzględnia promień i wysokość kapsuły, aby nie przenosić postaci stojącej poza otworem albo wysoko nad nim. Po przekazaniu relokacji łapanie AST jest odłożone do kolejnego ticka.
- Fizyczna osłona przedniej strony szybu leży za przednim lane’em, nie pomiędzy nim a środkowym.

## Koszt ładowania

Losowanie grafik po załadowaniu fragmentu wywołuje teraz RefreshAppearance, nie RefreshBlock. Nie przebudowuje spline’ów/aktorów AST ani kolizji i nie resetuje ignorowania platform jednostronnych. Tekstury obu pul i materiał są zatrzymywane w ResidentAssets podczas ładowania świata. To konkretne usunięcie zbędnej pracy; nie jest pomiarem ani gwarancją usunięcia wszystkich spike’ów. Instancje map nadal wczytuje standardowy streaming UE.

## Test użytkownika

1. Uruchom edytor ponownie, utwórz **nowy świat** z nową nazwą przez Main Menu.
2. Sprawdź kilka wejść surface→underground, również w biomie, w którym wcześniej były puste dziury. Powinny mieć szyby POI.
3. Sprawdź dół: koniec modułu EXIT powinien spotkać się z podłogą Underground. Kolejny szyb powinien być przesunięty w bok.
4. Wejdź w otwór, biegnąc po przednim lane’ie. Postać powinna trafić na platformy szybu; podczas spadania przytrzymaj Spację/W, żeby sprawdzić złapanie krawędzi.
5. Powtórz wejście ze środkowego lane’a. Zwykłe przechodzenie obok otworu i skok wysoko nad nim nie powinny przenosić postaci.
6. Wyjdź, wróć i ponownie wczytaj świat: konstrukcja, wysokość oraz grafiki powinny być powtarzalne.
7. Jeśli pozostaną przycięcia, zanotuj, czy występują przy pierwszym wejściu w nowy obszar, czy również przy powrocie. Dalsza optymalizacja wymaga pomiaru; nie należy zgadywać przyczyny.

## Kierunek dla przyszłego terraformowania

Zachowaj osobno bazowy profil wysokości, zapisane modyfikacje gracza oraz punkty dopasowania POI. Nie nadpisuj bazy każdą zmianą terenu. Obecne zapisane końce szybów i stemple terenu pasują do takiego podziału; niszczalna warstwa wymaga osobnego projektu i nie została dodana w tej poprawce.

## Weryfikacja techniczna

Build: Saved/POICheckpoints/Build-v12.log — Succeeded. Testy: POI-v12-tests/index.json, Tests-v12.log — ShaftTerrain Success, PublishedLevels Success. ShaftTerrain sprawdza głębokość wynikającą z modułów, dopasowanie podłoża na końcach, logikę otworów obu lane’ów, warunki relokacji, serializację planu oraz zachowanie aktora AST i kolizji przy zmianie grafiki. PublishedLevels sprawdza streaming i powtarzalne pule. Testy automatyczne nie zastępują testu przechodniości w grze.

Checkpoint: Saved/POICheckpoints/POI-v12-shaft-terrain.zip, Tools/checkpoint_poi_v12.py.
