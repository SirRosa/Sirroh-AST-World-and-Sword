# POI v0.2 — 2026-10-30

Aktualny checkpoint zastępuje opis ograniczeń v0.1 w zakresie trwałości obiektów. Podstawy modułów i terenu opisuje POI_V01_CHECKPOINT.md.

## Uruchomienie

Otwórz /Game/POIPrototype/L_POI_Demo. Play uruchamia demonstrację z domyślnym latającym pawnem UE (nie postacią AST). E przełącza najbliższy obiekt, F5 zapisuje układ i stan, F9 wczytuje. Klawisze są opt-in przez Enable Demo Input na generatorze; domyślnie wyłączone poza demonstracją. Nie zmieniono startowej mapy projektu.

Zawartość: DT_POIExamples, mapy Modules/L_POI_Corridor, L_POI_Up, L_POI_Down, materiał M_POIImage. Korytarz zawiera drzwi, dźwignię wskazującą drzwi, pojemnik z dwoma tokenami oraz światło. Domyślny seed generuje 8 modułów na maksymalnie 3 piętrach. W edytorze generator pokazuje uproszczoną geometrię; właściwe moduły są streamowane w Play.

Do PNG: POIProp → Visual Mesh = engine Plane, Visual Material = M_POIImage, Image Texture = własna tekstura. Obróć Visual Transform o 90° wokół X, aby płaszczyzna była pionowa. Collision Mode = Decoration daje tło bez kolizji. Solid i Physics używają osobnego boxa. Światło włączasz Light Enabled. To wspólny szablon; można utworzyć własne Blueprinty pochodne.

## Trwałość

UPOIStateComponent można dodać także do własnego aktora/Blueprintu AST. Nadaj LocalId unikalne w całej mapie modułu. Generator dokłada namespace instancji modułu. Brak/duplikat ID wyłącza trwałość całej wadliwej mapy i ustawia komunikat Status.

Zapis obejmuje stan aktywacji, pozostałe tokeny pojemnika, opcjonalny transform względem modułu, usunięcie aktora oraz jawnie wskazane proste właściwości aktora (Saved Actor Properties). On State Restored pozwala Blueprintowi odtworzyć prezentację. Nie zapisujemy referencji do obiektów, pełnego stanu animacji ani prędkości fizyki. Pozycja fizyki jest zapisywana, ruch jest podejmowany od odtworzonej pozycji.

Stan jest przechwytywany przed wyładowaniem mapy i przy Save Layout; odtwarzany po pokazaniu mapy. Zapis na dysk jest jawny (F5/Save Layout), bez automatycznego zapisu przy zamknięciu. Generate Preview tworzy nowy układ i czyści jego stan w pamięci. Używaj osobnych slotów dla różnych generatorów/światów.

APOIInteractiveProp ma tryby Door, Lever, Container, ReleasedPhysics. Są to natywne prototypy, nie ukończone adaptery animowanych obiektów AST. Targets działają tylko w obrębie tej samej instancji mapy. Interact wymaga aktora użytkownika w zasięgu. Take Loot usuwa token i zwraca sukces; podłączenie do ekwipunku wymaga jeszcze adaptera. Nie ma gotowego UI lootu ani przenoszenia tokenów do ekwipunku przez E.

## Co pozostało

Integracja animacji/interakcji AST, GASP+AST i platformy jednostronne; autorskie układy rozgałęzione i sekrety; automatyczne rozmieszczanie POI oraz rezerwacje przestrzeni/roślinności; wycinanie tuneli; multiplayer; bariera wejścia przed gotowością streamowanej kolizji; pełny UI i zapis ekwipunku; pomiary wydajności na docelowej postaci.

Demo i testy są funkcjonalnym prototypem. Nie wykonano ręcznego przejścia postacią AST ani wizualnej oceny renderingu. Testy działają z NullRHI.

## Narzędzia i weryfikacja

Tools/create_poi_demo.py utworzył assety przez PythonScriptPlugin w osobnym procesie UE; plugin włączono tylko parametrem uruchomienia, nie zmieniając .uproject. Komunikat końcowy SetupDemo.log: POI_DEMO_SETUP_SUCCESS. Skrypt nie nadpisuje istniejących assetów.

Silnik do kompilacji: D:/Sirro/UE 5.8.1/UE_5.8. Finalny build Development Editor Win64 zakończony Succeeded; log: Saved/POICheckpoints/Build-v02-final.log.

Testy: Sirroh.POI.Planner (100 seedów, snapshoty, stampy), Sirroh.POI.ObjectPersistence (aktorzy, dźwignia, drzwi, loot), Sirroh.POI.StreamingPersistence (rzeczywiste wczytanie/wyładowanie/wczytanie modułu). Końcowy wynik i archiwum zapisane poniżej po zakończeniu procesu testów.

Checkpoint v0.1: Saved/POICheckpoints/POI-v01-compiled.zip. Kopia przed zmianami: Before-POI-20261030-012604.zip. Końcowe archiwum v0.2 zawiera Source, Content/POIPrototype, .uproject, log kompilacji, testy i skompilowany DLL modułu; nie jest kopią całego projektu ani całego Content. Przy odtwarzaniu zamknij UE i zachowaj strukturę ścieżek w katalogu projektu.

Wynik koncowy: 3/3 testy Success, zero niepowodzen, exit code 0. Archiwum: Saved/POICheckpoints/POI-v02-compiled.zip. Ostatni odczyt limitu przed archiwizacja: 92% wykorzystania okna 5h.

Raport UE rozbija wynik na succeeded=2 i succeededWithWarnings=1 (failed=0). Ostrzezenie dotyczy sprzatania tymczasowego swiata testu streamingu bez jawnego EndPlay; wszystkie asercje tego testu przeszly. Do uporzadkowania w kolejnej sesji.
