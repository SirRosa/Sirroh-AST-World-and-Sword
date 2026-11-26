# POI v0.1 — checkpoint, 2026-10-30

## Zakres

Pierwszy fundament C++ w module Sirroh_Reimagineg_v3. Nie jest to jeszcze cały system opisany w rozmowie.

- POIPlanner: deterministyczny generator głównej trasy X/Z na jednym pasie Y; ważony wybór, dopasowanie typów połączeń i wysokości, wymagane wyjście i opcjonalne osiągnięcie najwyższego piętra. Ograniczony backtracking, brak częściowego układu po porażce. Kolejność wierszy katalogu nie wpływa na wynik.
- POIModuleRow: DataTable z geometrią, wymiarami komórki, wejściem/wyjściem, wagą, środowiskiem, kategorią i miękką referencją do mapy.
- POIGenerator: przyciski Generate Preview / Clear Preview; przykładowe korytarze i schody z instancji sześcianu; własne mapy wczytywane asynchronicznie w Play według odległości od gracza, maksymalnie dwa nowe żądania co 0,25 s, z histerezą usuwania.
- POIModuleAuthor: widoczne granice i strzałki, walidacja metadanych, Export Definition aktualizujący wskazany DataTable. Asset tabeli trzeba następnie zapisać.
- Save Layout / Load Layout: snapshot wybranych modułów, pozycji, wymiarów i lokalnych ID; nie losuje ponownie po zmianie katalogu. Nie zapisuje stanu interaktywnych obiektów.
- POIProp: wspólny szablon mesh/material + niezależna kolizja box, tryby Decoration/Solid/Physics, opcjonalne światło bez cieni domyślnie. Bez Tick. PNG wymaga na razie materiału na mesh płaszczyzny, nie automatycznego importera PNG.
- PWG v2: Initial POI Terrain Stamps — spłaszczanie i przesuwanie wysokości z płynnym przejściem. Do 64 obszarów, stabilna kolejność Priority/Id. Zapisywane przy tworzeniu nowego świata; istniejące światy zachowują swoje zapisane ustawienia.

## Pierwszy podgląd bez assetów

1. Otwórz projekt w UE 5.8. W pustej mapie dodaj klasę POIGenerator (Place Actors lub C++ Classes → Sirroh_Reimagineg_v3 → POI).
2. Zachowaj skalę 1,1,1. Pozostaw Use Example Catalog i domyślne ustawienia (8 modułów, 3 piętra, seed 12345).
3. Kliknij Generate Preview. Zobaczysz korytarze i schody z kolizją. Komunikat Status opisuje wynik.
4. Zmień seed i wygeneruj ponownie. Przy niespełnialnych wymaganiach poprzedni udany podgląd pozostaje, a Status informuje o błędzie.
5. Umieść start gracza ponad początkiem trasy i przetestuj Play. Trasa ma początek przy lokalnym X=0, Y=0, Z=StartFloor*FloorHeight.
6. Save Layout zapisuje do osobnego slotu POI_Prototype_Layout. Nadaj różne sloty różnym pomieszczeniom. Load Layout odtwarza snapshot. Przy odtwarzaniu przed Play wyłącz Generate On Begin Play, aby nie zastąpić snapshotu nowym losowaniem.

Przykładowe schody służą sprawdzeniu układu, nie są gotowymi wyzwaniami GASP/AST.

## Własny moduł

1. Utwórz i zapisz małą mapę bez World Partition. Nie umieszczaj w niej generatora PWG ani POIGeneratora, PlayerStart lub własnego GameMode; powinna zawierać tylko zawartość modułu.
2. Dodaj POIModuleAuthor dokładnie na 0,0,0, bez obrotu i skalowania.
3. Ustaw unikalne ModuleId, wymiary i EntryFloor/ExitFloor. Domyślna komórka: X=400 cm, piętro=300 cm, szerokość pasa Y=300 cm. Wszystkie aktywne moduły muszą mieć wymiary komórki zgodne z generatorem.
4. Wejście leży na X=0, wyjście na X=WidthCells*CellWidth. Oba są na Y=0. Wysokość to odpowiednio EntryFloor/ExitFloor*FloorHeight. Zbuduj i ręcznie przetestuj trasę między strzałkami. Oznaczone piętra muszą być mniejsze niż HeightCells.
5. Ustaw Definition.Level na zapisaną mapę. Utwórz DataTable ze strukturą POIModuleRow i wybierz go jako Target Catalog.
6. Validate Module sprawdza metadane i położenie znacznika, nie rzeczywistą osiągalność skoku. Export Definition dodaje/zastępuje wiersz o nazwie ModuleId. Zapisz tabelę i mapę.
7. W POIGenerator wyłącz Use Example Catalog, przypisz tabelę i generuj. W edytorze widzisz uproszczony układ; właściwe mapy pojawią się w Play.
8. W buildzie pakowanym uwzględnij źródłowe mapy w Packaging → List of Maps to Include. Soft reference nie zastępuje weryfikacji cooking.

Nie twórz modułów wymagających bocznych połączeń lub połączeń wstecz: v0.1 generuje jedną trasę biegnącą w dodatnim X. Łączenie pustych typów lub niezgodnych typów jest odrzucane. Category jest metadanymi; nie ma jeszcze reguł rozmieszczenia skarbów/sekretów.

## Teren PWG

Na APWGv2Generator dodaj Initial POI Terrain Stamps przed utworzeniem NOWEGO nazwanego świata. Współrzędne są lokalne dla PWG, w cm. Przykład:

- Id=CabinFoundation, Center=(2000,0), HalfExtent=(400,400), BlendDistance=600, Operation=Flatten, Height=<lokalne Z progu>, Priority=0.
- Id=CabinPit, Center=(2000,1200), HalfExtent=(200,200), BlendDistance=600, Operation=Offset, Height=-200, Priority=1.

Flatten ustawia bezwzględną wysokość w środku obszaru; Offset dodaje różnicę do dotychczasowej wysokości. Większy Priority działa później. Nie modyfikujemy plików istniejących światów przy zmianie Initial Stamps. Stare zapisy bez pola stampów działają jak wcześniej.

To mechanizm korekty wysokości, nie automatyczny wybór miejsca chatki. Nie wycina tuneli, nie rezerwuje przestrzeni, nie wyłącza roślinności i nie sprawdza maksymalnego nachylenia przejścia. Ustaw szerokie BlendDistance i testuj kolizję; mesh terenu ma skończoną rozdzielczość próbkowania.

## Granice tej wersji / następna praca

- Brak adapterów drzwi/dźwigni/pojemników AST, brak mostka GASP+AST i platform OneWay.
- Brak zapisu stanów drzwi, lootu i fizyki. Własne mapy po wyładowaniu i ponownym załadowaniu odtwarzają stan początkowy. Nie używać jeszcze do trwałego lootu.
- Brak rozgałęzionych komnat, sekretów, portali pomiędzy warstwami i automatycznego rozmieszczania POI.
- Streaming to prototyp lokalny dla gracza 0; brak multiplayer, priorytetów ładowania i bariery wejścia zanim kolizja mapy będzie gotowa. Dystans wyprzedzenia należy przetestować.
- Przykładowa geometria jest ładowana w całości (do 64 modułów). Streaming dotyczy własnych map.
- Zmiana źródłowej mapy zmienia jej zawartość także dla zapisanych układów: snapshot zabezpiecza wybór modułów i ich metadane, nie archiwizuje plików map.
- Potrzebny ręczny test w edytorze: przejście postacią, źródłowe mapy, wyładowanie/powrót, teren. Automatyczny test nie zastępuje tego odbioru.

## Kompilacja i odtworzenie

Używaj silnika D:\Sirro\UE 5.8.1\UE_5.8. Kopia D:\0 Sirro\UE 5.8.1\UE_5.8 użyła artefaktów odwołujących się do pierwszej lokalizacji i dała duplikaty definicji nagłówków.

```powershell
& 'D:\Sirro\UE 5.8.1\UE_5.8\Engine\Build\BatchFiles\Build.bat' AST_V1_18Editor Win64 Development '-Project=D:\0 Sirro\Unreal Projects\AST_V1_18\AST_V1_18.uproject' -WaitMutex -NoHotReloadFromIDE -NoXGE
```

Poza nowymi plikami POI i integracją stampów poprawiono pierwsze include w czterech istniejących plikach: SirrohASTNPCComponent.cpp, DirectionalCombatComponent.cpp, SirrohASTInventoryComponent.cpp, PWGGameMode.cpp. Wymagane przez kompilator; bez zmian logiki tych klas.

Kopia przed zmianami: ../Saved/POICheckpoints/Before-POI-20261030-012604.zip. To kopia Source, nie Content ani pełnego projektu. Końcowy checkpoint i wynik testów zostaną dopisane po kompilacji.

Weryfikacja v0.1: Development Editor Win64 — Succeeded. Sirroh.POI.Planner — Success, exit 0; 100 seedow, zapis pamieciowy i stampy terenu. Checkpoint skompilowanej v0.1: ../Saved/POICheckpoints/POI-v01-compiled.zip. Cztery nowe pliki POIStateComponent/POIInteractiveProp rozpoczynaja v0.2 i nie sa czescia tego archiwum.
