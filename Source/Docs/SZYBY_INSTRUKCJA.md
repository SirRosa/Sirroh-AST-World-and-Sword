# Szyby w AST — od pierwszego bloku do świata PWG

Ten dokument opisuje warsztat v0.3. Dla obecnego podziału na wejście, losowane środki i dół oraz oryginalnych obiektów AST zacznij od [instrukcji v0.4](<D:/0 Sirro/Unreal Projects/AST_V1_18/Source/Docs/POI_AST_INSTRUKCJA.md>).

## Uruchomienie warsztatu

1. Otwórz projekt AST_V1_18 w jego Unreal Engine 5.8. W Content Browser przejdź do Content → POIKit i otwórz L_ShaftWorkshop.
2. W World Outliner znajdziesz cztery szyby o wysokości 900 cm oraz krótszy EDIT_ME_300cm_Section. Kliknij Play w trybie Selected Viewport, a następnie kliknij widok gry. W sterowaniu bocznym A/D porusza postacią, spacja skacze. Chwyt i puszczenie krawędzi korzystają z akcji przypisanych w projekcie postaci. Escape wraca do edycji.
3. Do pierwszej próby wybierz krótki szyb. Sprawdź podejście z obu stron, skok spod belki, lądowanie na niej, chwyt krawędzi oraz powrót na dół. Każdy ręczny wariant wymaga takiej próby z docelową postacią.

## Pojedynczy blok

Przeciągnij Blueprint z POIKit do widoku. W Details → Settings ustaw Size w centymetrach. X to szerokość w kierunku ruchu, Y to głębokość, Z to wysokość. Uchwyt aktora leży pośrodku górnej powierzchni. Zmieniaj Size zamiast skali aktora. Face Positive X wybiera prawą lub lewą krawędź chwytu AST. Ustaw ją w stronę, z której gracz podchodzi.

| Preset | Zastosowanie |
|---|---|
| BP_GASP_Solid / BP_AST_Solid | Pełna przeszkoda fizyczna z krawędzią odpowiedniego systemu. |
| BP_GASP_NoCollision / BP_AST_NoCollision | Postać przenika przez blok. Krawędź pozostaje dostępna dla zapytań traversal. Do stania po zakończeniu animacji potrzebna jest osobna podpora. |
| BP_GASP_OneWay / BP_AST_OneWay | Belka do skoku od dołu i lądowania z góry. |
| BP_Both_OneWay | Jedna belka z krawędziami obu systemów. |
| BP_SideWall | Fizyczna ściana boczna. |
| BP_BackWall | Dekoracyjne tło bez kolizji. Ustaw za torem ruchu, po stronie +Y. |

Aby nałożyć grafikę, zaimportuj PNG do Content Browser, zaznacz blok i przeciągnij zaimportowaną teksturę do Settings → Image. Rozmiar obrazu w świecie ustalasz przez Size.X i Size.Z. Kolizję pełnej ściany ustala prostopadłościan Size, również tam, gdzie obraz ma przezroczyste piksele. Możesz też ustawić obrazy dla całego szybu przez Platform Image, Side Image i Back Image w Recipe i nacisnąć Build Shaft.

## Własny wariant i zapis w tabeli

1. Zrób kopię mapy warsztatu przez Save Current Level As. Zaznacz EDIT_ME_300cm_Section. Jego pozycja wyznacza środek dolnego wyjścia. Zachowaj zerowy obrót i skalę 1. Wysokość tej sekcji to 300 cm.
2. W Recipe wybierz Upper Layer z tabeli poniżej. Width określa szerokość szybu, Step Rise maksymalny pionowy odstęp generowanych stopni. Build Shaft tworzy układ startowy. Ponowne użycie zastępuje wygenerowane bloki, więc ręczne poprawki wyeksportuj przed przebudową.
3. Rozwiń aktora w Outlinerze. Przesuwaj stopnie w płaszczyźnie X/Z. Aby dodać własny blok, przeciągnij jego Blueprint do mapy, a następnie w Outlinerze przeciągnij aktora bloku bezpośrednio na aktora szybu. Samo ustawienie bloku wewnątrz szybu nie powoduje eksportu. Automatyczne ściany i tło są pomijane przy eksporcie i odtwarzane przy budowie.
4. Pierwszy chwyt umieść w zasięgu Step Rise od dołu. Ostatnią platformę umieść na wysokości sekcji, np. Z lokalne 300 cm. Sprawdź, czy postać dosięga następnego chwytu także na połączeniu dwóch powtórzeń. Dla przejścia w dół pozostaw wolne miejsce obok platform. Bottom Exit Clearance zostawia otwór przy dolnym końcu ścian bocznych.
5. W Target Table wybierz DT_ShaftVariants. W Row Name wpisz nową nazwę, np. Studnia_Deski_01. Export Section zapisuje pod tą nazwą lub zastępuje istniejący wiersz. Sprawdź pole Status, otwórz tabelę i zapisz ją przez Ctrl+S. Zapisz też mapę. Do tabeli trafiają ustawienia i względne pozycje bloków.
6. Generator powtarza pełne sekcje bez rozciągania, a na pozostałej wysokości dodaje stopnie. Sekcja może mieć 150–3000 cm i do 100 elementów. Przy dużych wysokościach obowiązuje limit 500 kopiowanych bloków.

| Upper Layer | Przejście | Gotowy wiersz |
|---|---|---|
| 0 | powierzchnia → podziemia | Surface_Underground |
| 1 | podziemia → jaskinie | Underground_Caves |
| 2 | jaskinie → lochy | Caves_Dungeon |
| 3 | lochy → katakumby | Dungeon_Catacombs |

## Użycie w PWG

Otwórz Content → POIKit → L_PWG_Shafts i uruchom Play. Mapa ma własny tryb gry z nazwą świata ShaftKit_FirstWorld. W generatorze PWGv2 opcja Use Kit Shafts włącza zestaw. Shaft Variants pozwala wskazać inną tabelę typu POIShaftRow. Domyślnie generator szuka DT_ShaftVariants w POIKit. Plan świata ustala miejsca przejść, a tabela wybiera wygląd przejścia między sąsiednimi warstwami. Bezpośredni skrót z powierzchni do jaskiń korzysta ze starszego rozwiązania.

Aby wymusić własny wariant, ustaw Weight na 1 dla swojego wiersza i na 0 dla pozostałych wierszy tej samej warstwy. Zapisz tabelę. Uruchom **nowy świat z nową nazwą zapisu**. Plan zapisuje wybrany wariant przy tworzeniu świata. Jeśli menu gry wskazuje wcześniej wybrany świat, utwórz nowy również w menu. Do sprawdzenia wariantu bez generowania świata użyj mapy warsztatu.

## Odtworzenie checkpointu

Zamknij edytor. Rozpakuj archiwum na kopię tego samego projektu, zachowując strukturę katalogów od katalogu z plikiem .uproject. Checkpoint zawiera moduł edytora Win64 Development, źródła i zasoby zestawu. Wymaga istniejących zasobów postaci, GASP i wtyczki AST z projektu. Otwórz L_ShaftWorkshop. W Session Frontend → Automation uruchom Sirroh.POI.ShaftKit, aby sprawdzić zapytania obu systemów i kierunkową kolizję kapsuły.
