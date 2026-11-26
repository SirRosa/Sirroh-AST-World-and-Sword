# POI v0.5 — checkpoint 2026-11-09

## Zapisane zmiany
- Zaimportowano 10 grafik tła i 10 grafik platform; przypisano je do warsztatu i tabel modułów.
- Naprawiono kierunek spline w adapterze AST bloku oraz w oryginalnych krawędziach warsztatu i DT_ShaftModules (20 aktorów, 40 elementów tabeli).
- Kontroler PWG przekazuje rozpoczęcie parkouru przez Spację do oryginalnego komponentu AST również ze stanu Ground. Zachowuje blokady plecaka, interakcji, ragdolla i pauzy.
- Dodano test rzeczywistej funkcji CheckLedgeDirection AST dla obu stron bloku, także bez kolizji fizycznej.
- Analyzer Blueprintów obejmuje teraz grafy interfejsów i grafy zagnieżdżone, potrzebne do diagnozy AST.

## Potwierdzone
Build-v05.log: kompilacja Editor Win64 Development zakończona Succeeded.
Parkour-controller-v05/probe.json: passed=true. Kontroler uruchomił wejście na platformę 150 cm, postać osiągnęła X=109.39, Z=235.63 i stan Walking/Ground. Test korzysta z rzeczywistej postaci i kontrolera PWG na mapie warsztatu. Geometria próby była tymczasowo izolowana w PIE; nie zmieniono układu zapisanej mapy w celu przejścia testu.

Parkour-hang-v05.json: na krawędzi 220 cm postać przeszła Ground → HANGING_LEDGE (Flying), a po SirrohParkour(true) wróciła Ground/Walking na wysokość kapsuły 88.15 cm. Zwis utrzymywał się po zakończeniu animacji.
Tests-v05.log oraz POI-v05-tests/index.json: ObjectPersistence, Planner, ShaftKit, StreamingPersistence — wszystkie Success; proces testów zakończony kodem 0. ShaftKit sprawdza również prawdziwy interfejs kierunku AST po obu stronach bloku.

## Użycie i ograniczenia
Otwórz /Game/POIKit/L_POI_Workshop_AST i uruchom Play. Instrukcja: POI_AST_INSTRUKCJA.md.
Stare światy zawierają stare kopie modułów: testuj tabelę w nowym świecie.
PNG zachowują przezroczyste marginesy; fizyczny rozmiar platformy określa Settings/Size.
Nie należy traktować testu pojedynczej przeszkody jako sprawdzenia wszystkich połączeń losowego szybu. Cały losowy przebieg wymaga testu przejścia.

## Odtwarzanie
Saved/POICheckpoints/POI-v05-parkour.zip zawiera ścieżki względem katalogu projektu: Source, Content/POIKit, skompilowany moduł i zmieniony BlueprintAnalyzer oraz raporty. Przed odtworzeniem zamknij edytor i zachowaj własne późniejsze zmiany. Nie nadpisuj ich bez porównania.
