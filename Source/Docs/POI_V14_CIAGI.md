# v14 — 10 pól na template i pierwsze ciągi pomieszczeń

## Założenia i zakres

Docelowy system składa ręcznie zaprojektowane fragmenty (Entrance, Parkour, Pushable, Light, Empty, później Ambush, Trap, Treasure, Hidden itd.) w pokoje i lochy. Aktorzy zachowują mechaniki AST i referencje wewnątrz fragmentu. Teren dopasowuje się do zaplanowanych POI. Grafiki pochodzą z pul; autor edytuje mapę i publikuje przez menu POI.

Pierwsza implementacja ciągów jest pozioma: **Entrance + 4–7 fragmentów + Entrance**. Nie skaluje fragmentów. Szerokość każdego Background wyznacza długość fragmentu; sąsiednie pola stykają się krawędziami. Podłoga na obu końcach musi być na dolnej granicy Background, a przejście wolne. Gracz musi móc przejść przez każdy fragment od lewej do prawej. Drzwi mogą znajdować się w Entrance. Umieszczenie drzwi w samym środku pustego pola nie tworzy automatycznie ścian wokół nich.

Kategorie losowane są w tasowanych zestawach, żeby liczniejsza pula jednej kategorii nie wypierała pozostałych. W obrębie kategorii losowany jest wariant. Początek i koniec są Entrance. Fragmenty w jednym ciągu mają tę samą wysokość Background. Inne wysokości tworzą osobne zgodne zestawy i wymagają pasującego Entrance. Brak Entrance lub brak części środkowych oznacza brak ciągu.

Wielopoziomowe pokoje z Up/Down, walidacja osiągalności skoków, rozgałęzienia, sekrety i reguły walki pozostają kolejnymi etapami. Sam tag Parkour nie dowodzi, że ręcznie ułożona trasa jest przechodnia — testuje ją autor.

## Mapy: po 10 fragmentów każda

Folder: `Content/POIKit/Underground/Templates`.

- L_Pushable_U
- L_Entrance_U
- L_Light_U
- L_Empty_U
- L_Parkour_U

Każda mapa zawiera 10 pól w rzędzie wzdłuż X. Istniejące pola zachowano, brakujące skopiowano jako punkty wyjścia do własnych wariantów. Nie są to 50 różnych, ukończonych projektów poziomów.

Jeśli widzisz tylko jedno pole: w World Outliner wpisz `Background`, zaznacz wyniki i naciśnij **F** w viewporcie. Obejmiesz kamerą cały zestaw. Nad polami są duże podpisy `01 / 10` ... `10 / 10`.

Edytuj pola w tej samej mapie albo jej kopii. Zachowuj nazwy kategorii Background, np. `Pushable_U_06_Background`. Do dodatkowych odmian kopiuj całe pole z jego Background. Nie nakładaj granic na siebie.

Publikacja: **POI → Underground → Zapisz wszystkie fragmenty z Background**. Aktualny zestaw 50 pól został opublikowany na start, więc pierwszy test nie wymaga ręcznego publikowania. Po własnej edycji opublikuj zmienioną mapę ponownie.

## Test przez Main Menu

1. Uruchom Main Menu tak jak zwykle i utwórz **nowy świat z nową nazwą**, z podziemiami włączonymi. Stare zapisy zachowują wcześniejszy plan.
2. Wejdź postacią do świata.
3. Otwórz konsolę Unreal klawiszem tyldy (`~`) i wpisz **`SirrohPOIChain 0`**.
4. Komenda wczyta otoczenie pierwszego ciągu i przeniesie postać przed jego pierwsze drzwi. Podczas aktywnej interakcji AST najpierw ją zakończ.
5. Idź w prawo. Sprawdź oba Entrance, części środkowe, styki podłogi i tła, pchanie, drzwi, oświetlenie i platformy.
6. `SirrohPOIChain 1`, `SirrohPOIChain 2` itd. wybierają kolejne ciągi. Brak indeksu daje komunikat, nie tworzy sztucznego pokoju.
7. Wyjdź i wczytaj ten sam świat: układ ciągu ma pozostać taki sam. Nowy seed może wylosować inne warianty i kolejność.

To używa normalnego świata, GameMode i controllera Sirroh. Komenda jest skrótem do ręcznych testów, nie osobną demonstracyjną mapą. Może chwilowo zatrzymać grę przy wymuszonym wczytaniu celu.

## Teren, zapis i wydajność

Generator rezerwuje ciąg przed budową chunków, omija szyby, sprawdza miejsce pomiędzy warstwami, wyrównuje podłogę i sufit do POI oraz łagodnie łączy je z otoczeniem. Stare losowe przeszkody, skały i miecze nie są dodawane wewnątrz ciągu. Zapis świata zawiera fragmenty, pozycje i granice ciągów. Publikowanie zmian nie przebudowuje istniejących zapisów.

Mapy fragmentów korzystają z dotychczasowego streamingu chunków; cały loch nie jest wczytywany jednocześnie. Wymuszony teleport testowy celowo czeka na wczytanie map. Pula Dirt i PF_Planks nadal losuje grafiki. Ukończony test strukturalny lub kompilacja nie potwierdzają płynności ani poprawności interakcji podczas gry — te testy wykonuje użytkownik.

## Zakończenie i weryfikacja

- Kompilacja AST_V1_18Editor Win64 Development: Succeeded.
- Automatyczne testy RoomChains, PublishedLevels, ShaftTerrain: Success.
- Rzeczywiste tabele świata: seed 123 — 17 ciągów / 127 fragmentów; seed 847 — 20 / 142; seed 2026 — 13 / 95.
- Wszystkie pięć template'ów: 10 pól i 10 publikacji na kategorię. Weryfikacja katalogu potwierdziła istnienie opublikowanych map.
- Kopia poprzednich map: `Saved/POICheckpoints/Templates-before-v14.zip`.
- Checkpoint kodu, assetów, biblioteki po kompilacji i logów: `Saved/POICheckpoints/POI-v14-room-chains.zip`.

Nie uruchamiano ręcznie sterowanej rozgrywki ani testów przez automatyzację pulpitu. Teleport testowy oraz interakcje w wygenerowanym ciągu wymagają sprawdzenia przez użytkownika.
