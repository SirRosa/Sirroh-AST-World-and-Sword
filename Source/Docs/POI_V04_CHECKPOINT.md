# Checkpoint v0.4 — moduły szybów i oryginalne aktory AST

Data zapisu 2026-11-08.

Kompilacja AST_V1_18Editor Win64 Development zakończyła się powodzeniem. Archiwum POI-v04-AST-modules.zip jest nakładką na ten projekt. Zawiera Source, Content/POIKit, Content/POIPrototype, skompilowany moduł edytora, manifest modułów i wyniki sprawdzeń. Do odtworzenia potrzebne są istniejące zasoby postaci oraz wtyczki AST/GASP tego projektu.

## Gdzie zacząć

Otwórz Content → POIKit → L_POI_Workshop_AST. Instrukcja użytkownika znajduje się w Source/Docs/POI_AST_INSTRUKCJA.md. L_AST_Showcase_PWG to kopia oryginalnej ShowcaseMap z trybem testowym opartym na BP_PWGGameMode.

## Wdrożone

- Osobne role Entrance, Middle i Exit. Losowanie kolejnych środków z wagami i seedem. Biblioteka DT_ShaftModules ma przykłady dla czterech przejść między warstwami.
- Zapis oryginalnych ParkourActor, BeamActor i aktorów spline z AST ParkourSystem wraz z punktami spline oraz ustawieniami. Presety Ledge, Mantle, Vault, BalanceBeam i NarrowPassage są dziećmi oryginalnych Blueprintów.
- Zapis biblioteki i seeda w planie świata PWG. Starsze zapisane receptury nadal korzystają z powtarzanych sekcji.
- BP_POITest_PWG dziedziczy ustawienia postaci i kontrolera po BP_PWGGameMode. Test uruchamia postać przy PlayerStart. Kod tworzenia postaci i instalowania wejścia jest współdzielony z PWG.
- Podpięcie PWGSidescrollerInputComponent odbywa się po utworzeniu komponentów Blueprinta, przed BeginPlay Mover.
- Domyślnie ukryte duże obrysy OneWayProximity.

## Sprawdzenie

Raport AST-POI-Tests obejmuje Planner, ObjectPersistence, StreamingPersistence i ShaftKit. ShaftKit sprawdza oryginalne obiekty GASP/AST, przejście kapsuły od dołu, podparcie z góry, odtworzenie spline BeamActor, zachowanie tagu Beam i kolejność ról w szybie.

Próba Play na L_POI_Workshop_AST zakończyła się powodzeniem. AST_POI_PIE.json potwierdza BP_SirrohCharacter, PWGPlayerController, PWGSidescrollerInputComponent oraz zgodność klas postaci i kontrolera z L_SirrohGameplay otwieranym przez Main Menu. Po restarcie nad oryginalnym BeamActor postać osiągnęła stan BEAM_WALKING.

Próba Play działała bez renderowania obrazu. Nie zweryfikowano wizualnie animacji ani ręcznego przejścia wszystkich kombinacji modułów w obu kierunkach. Każdy własny wariant wymaga próby dosięgania kolejnych chwytów. Walidacja skrajnych punktów nie gwarantuje przechodniości całej sekcji.

Przykłady mają charakter konstrukcyjny. Pozostałą wysokość, na której nie mieści się pełny środek, generator uzupełnia proceduralnymi stopniami przed wejściem. Eksport obsługuje bloki POI i oryginalne aktory spline AST wskazane w instrukcji.
