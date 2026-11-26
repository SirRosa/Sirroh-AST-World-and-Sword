# POI — nowy sposób pracy (v11)

Nie dodajesz aktora POIShaft. Nie otwierasz DataTable. Pracujesz w zwykłym levelu i zapisujesz fragmenty przez menu **POI** na górnym pasku edytora levelu.

## 1. Otwórz własną kopię template

1. Uruchom ponownie edytor po aktualizacji. W Content Browser otwórz **Content → POIKit**.
2. Znajdź mapę **L_POI_Template**, kliknij ją prawym przyciskiem i wybierz **Duplicate**. Nazwij kopię np. `L_MojeSzyby_01`.
3. Otwórz kopię dwuklikiem. Wszystkie następne kroki wykonujesz w edytorze levelu, nie w tabeli.

Możesz również użyć kopii swojego **L_POI_Workshop_AST** — eksport obsługuje oba levele. Oryginalnego warsztatu nie przebudowano.

W template jest pięć oddzielnych pól wyznaczonych przez Background:

| Pole | Co budujesz |
|---|---|
| ENTRANCE_01 | Górę szybu / wejście z wyższej warstwy |
| MIDDLE_01, MIDDLE_02, MIDDLE_03 | Trzy różne warianty środkowego fragmentu |
| EXIT_01 | Dół szybu / wyjście do niższej warstwy |

To pięć **fragmentów**, nie pięć pełnych szybów. Generator wybiera dół, dokłada losowe środki, kończy górą. Kolejne kopie levelu dodają kolejne warianty.

## 2. Zbuduj fragmenty

Przesuwaj, obracaj i skaluj platformy. Dodawaj zwykłe aktory z Content Browser: Beam, szafki, pickables, Cover itd. Edytuj spline’y oryginalnych aktorów AST tak jak w Showcase.

Obiekty nie muszą być podpięte pod żadnego specjalnego aktora. Eksport zachowuje ich klasę, transformację, spline’y, ustawienia oraz odwołania między aktorami zapisanymi w tym samym fragmencie.

**Co trafia do zapisu:** obiekty, których punkt położenia/pivot leży wewnątrz prostokąta Background w osiach X/Z, do 2000 cm od Y=0. Background też trafia do zapisu. Obiekt przecięty granicą należy do pola zawierającego jego pivot — utrzymuj całą geometrię w granicach, aby fragmenty się nie zderzały.

Nie obracaj Background wyznaczającego granice; pozostałe obiekty możesz obracać. Domyślne pole ma 600 × 300 cm. Dla szybkiego składania szybów pozostaw te wymiary jednakowe we wszystkich wariantach. Eksport obsługuje szerokość 400–1200 cm i wysokość 150–3000 cm.

Floor do testów, PlayerStart, światła, napisy, kamery i stare pomocniki POIShaft nie są częścią opublikowanego POI. Obiekt oznaczony Actor Tag `POIExclude` również jest pomijany. Obiekty zależne od aktora w innym polu umieść razem w jednym polu; nie buduj połączeń między oddzielnymi POI. Logikę umieszczaj w aktorach, nie w Level Blueprint warsztatu.

## 3. Zapisz do generatora — jedna pozycja menu

Na **górnym pasku okna levelu** otwórz:

**POI → Shaft: Surface - Underground → Zapisz wszystkie fragmenty z Background**

Gotowe. Narzędzie zapisuje level roboczy, tworzy osobne mapy fragmentów, rejestruje je i wraca do Twojego levelu. Nie ustawiasz katalogu ani generatora.

Pozostałe przejścia są obok, w tym samym menu:

- Underground - Caves
- Caves - Dungeon
- Dungeon - Catacombs

Wybranie jednego przejścia publikuje tylko do niego. Aby wykorzystać zestaw w innym przejściu, wybierz odpowiednią pozycję i zapisz ponownie.

**Tylko jeden fragment:** zaznacz jego Background, następnie wybierz w tym samym menu `Zapisz zaznaczony Background jako: Gora / wejscie`, `Srodek` albo `Dol / wyjscie`. W ten sposób możesz nadać rolę niezależnie od nazwy.

Przy zapisie wszystkich pól rola wynika z nazwy Background: `ENTRANCE`, `MIDDLE`, `EXIT`. Inna nazwa oznacza środek. Zostaw nazwy z template, jeśli nie potrzebujesz tego zmieniać.

Ponowny zapis tego samego pola z tej samej mapy aktualizuje jego pozycję w katalogu nowych światów. Nowa kopia mapy dodaje nowe warianty. Samo usunięcie obiektu z levelu nie usuwa wcześniej opublikowanego wariantu — użyj ponownego zapisu pola, żeby zaktualizować jego zawartość.

**Utwórz nowy świat przez Main Menu**, aby zobaczyć nowy katalog. Istniejące światy zachowują zapisane wcześniej warianty. Losowanie grafik jest powtarzalne dla danego seeda: ponowne wczytanie tego samego fragmentu nie zmienia jego wyglądu.

## 4. Surface i Dungeon

Tak samo edytujesz zawartość prostokąta Background, następnie wybierasz:

- **POI → Surface → Zapisz zaznaczony Background** — pojedyncze POI na powierzchni;
- **POI → Dungeon → Zapisz zaznaczony Background** — pojedyncze POI w warstwie lochów.

Możesz także zapisać wszystkie pola naraz jako oddzielne warianty. Dolna krawędź Background wyznacza poziom podłoża POI.

Domyślnie generator próbuje umieścić POI co 80 m. Omija szyby i wodę. Pod Surface wyrównuje teren; Dungeon pojawia się tylko, gdy mieści się w wysokości komnaty i ma równe podłoże. To rozmieszczanie samodzielnych POI; nie zastępuje całych komnat lochów ani nie wycina dowolnych tuneli. Obecny eksport używa tych samych ograniczeń wielkości pola co szyb.

## 5. Grafiki — ustawiasz pulę raz

Gotowe zasoby:

- **Content/POIKit/Pools/BG_Bricks** — 10 obrazów Background;
- **Content/POIKit/Pools/PF_Planks** — 10 par: bok platformy + góra/dół.

Przy generowaniu każda platforma losuje parę z PF_Planks, a każdy Background obraz z BG_Bricks. Obrócenie lub skalowanie platformy nie wymaga wybierania nowego obrazu.

**POI → wybrane przejście → Pule grafik tego przejscia** otwiera jego ustawienia:

- `Backgrounds` — wybierasz pulę tła;
- `Platforms` — wybierasz pulę platform.

Domyślnie wszystkie cztery przejścia korzystają z powyższych pul. Surface korzysta z ustawień przejścia 0, Dungeon z przejścia 3.

**Podgląd bez uruchamiania świata:** `POI → wybrane przejście → Podglad: losuj grafiki w otwartym levelu`. To tylko wygląd roboczy; świat losuje własny zestaw według seeda. Podgląd można cofnąć przez Undo.

### Dodawanie nowych PNG

1. Przeciągnij PNG do folderu w Content Browser, np. `Content/POIKit/Art/Nowe`.
2. Dla kolorowych obrazów używaj 8-bitowego PNG RGBA. W teksturze: **sRGB = włączone**, **Compression Settings = Default**, **Compress Without Alpha = wyłączone**. Nie wybieraj Normalmap. Nie usuwaj przezroczystości.
3. Otwórz **BG_Bricks** albo **PF_Planks**. W `Images` kliknij **+**.
4. Dla Background ustaw `Side` na nową teksturę; `Top` pozostaw pusty.
5. Dla platformy ustaw `Side` na obraz boku, `Top` na obraz mostu/górnej powierzchni. Ten sam `Top` zostanie użyty od spodu.
6. Kliknij **Bake Alpha Bounds** i zapisz pulę. Przy zmianie pól przeliczenie wykonuje się też automatycznie. Po ponownym imporcie PNG kliknij przycisk ponownie.

System rozciąga widoczną część PNG do wielkości powierzchni, pomijając przezroczysty margines. Otwory i nieregularny kontur wewnątrz grafiki pozostają przezroczyste.

Nowa pula: zduplikuj BG_Bricks lub PF_Planks w Content Browser, zmień nazwę i zawartość `Images`, następnie wybierz tę pulę w ustawieniach przejścia. Nie przypisujesz jej osobno do każdego POI.

## 6. Krótki test do wykonania

1. Otwórz kopię L_POI_Template; sprawdź, czy na pasku jest menu **POI**.
2. W MIDDLE_02 obróć i przeskaluj platformę, dodaj Beam lub szafkę.
3. Zapisz wszystkie fragmenty dla Surface - Underground. Oczekiwany komunikat: zapisano 5 POI, po czym wracasz do swojego levelu.
4. Przez Play sprawdź wspinaczkę na mapie roboczej; testowy GameMode pochodzi z PWG/Sirroh.
5. Utwórz nowy świat przez Main Menu. Sprawdź mieszanie fragmentów i losowe grafiki, działanie Beam/szafki oraz przejścia na granicy dwóch fragmentów.
6. Wyjdź z obszaru i wróć: grafiki powinny pozostać takie same dla tego świata.

Testy rozgrywki wykonujesz Ty. Weryfikacja automatyczna eksportu i kompilacja nie potwierdzają przechodniości własnoręcznie ułożonego szybu ani zachowania każdego obcego Blueprinta.
