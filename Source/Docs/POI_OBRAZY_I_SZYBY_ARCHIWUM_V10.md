# Obrazy i własny szyb — instrukcja od początku do końca

## Co jest czym

**L_POI_Workshop_AST to warsztat do budowania i testowania wzorców. Sam zapis levelu nie dodaje nowego POI do generatora.**

Przepływ pracy wygląda tak:

1. Układasz platformy, ściany i oryginalne obiekty AST w levelu.
2. Podpinasz elementy pod jeden aktor **POIShaft** reprezentujący pojedynczy moduł.
3. Przez **Export Section** zapisujesz moduł jako wiersz DataTable.
4. Inny aktor POIShaft, z włączonym **Assemble Modules**, składa cały szyb z wierszy tej tabeli.
5. Generator PWG korzysta z tabeli przy tworzeniu **nowego świata**.

Wiersz tabeli jest zapisaną kopią ustawień i położeń elementów. Późniejsze przesunięcie platformy w warsztacie nie zmieni tabeli, dopóki ponownie nie wykonasz eksportu.

## 1. Import własnych PNG

W Content Browser utwórz np. foldery `POIKit/Art/MyBackgrounds`, `MyPlatforms` i `MyTops`. Przeciągnij PNG do odpowiedniego folderu lub użyj Import. Nie importuj plików ZIP — wcześniej je rozpakuj.

Dla tego zestawu używaj **PNG RGB/RGBA, 8 bitów na kanał**. RGBA oznacza obraz z kanałem przezroczystości. 256×256, 512×512 i inne rozmiary są dopuszczalne; obrazy nie muszą być kwadratowe. Przycięcie pustych marginesów działa obecnie dla źródła BGRA8, do którego Unreal standardowo importuje takie PNG. PNG 16-bitowe mogą wymagać wcześniejszego zapisania jako 8-bitowe.

Otwórz zaimportowaną teksturę i sprawdź:

| Ustawienie tekstury | Zalecana wartość |
|---|---|
| sRGB | Włączone dla kolorowych ilustracji. |
| Compression Settings | Default. |
| Compress Without Alpha / Compression No Alpha | Wyłączone, jeśli obraz zawiera przezroczystość. |
| Texture Group | World jako punkt wyjścia. |
| Mip Gen Settings | FromTextureGroup. |
| Filter | Default. Nearest tylko jeśli świadomie chcesz ostre piksele pixel-artu. |
| Never Stream | Pozostaw domyślnie wyłączone. |

Nie ustawiaj Normalmap, Masks ani Grayscale dla zwykłych kolorowych obrazów. Jeśli domyślna kompresja wyraźnie psuje mały rysunek, można wypróbować UserInterface2D (RGBA), ale zużywa ona więcej pamięci; nie jest konieczna do działania platform.

W podglądzie tekstury sprawdź kanał Alpha: kształt powinien być nieprzezroczysty, a tło przezroczyste. Biały lub czarny kolor tła sam w sobie nie oznacza przezroczystości. Zapisz teksturę.

Materiał bloków jest **Masked, Two Sided, Unlit**. Zachowuje wycięty kształt PNG i pokazuje obraz z obu stron. Nie służy do miękkich, półprzezroczystych efektów jak szkło lub dym.

## 2. Przypisanie grafiki do platformy

1. Przeciągnij **BP_ClimbPlatform** z `Content/POIKit` do levelu.
2. Zaznacz całego aktora. W Details rozwiń **POI Kit → Settings**; możesz wyszukać `Image`.
3. **Image**: wybierz kolorową boczną grafikę platformy.
4. **Top Image**: wybierz obraz powierzchni mostu/desek. Ten sam obraz zostanie użyty na górze i spodzie.
5. **Material**: ustaw **M_KitImageFit** z `Content/POIKit`.
6. **Tint**: biały, z alfą 1, jeśli nie chcesz przyciemniać ilustracji.
7. **Fit Image To Alpha**: włącz. Materiał rozciąga obszar zajęty przez rysunek do wymiarów danej powierzchni. Poszarpane narożniki i otwory wewnątrz grafiki nadal pozostają przezroczyste.
8. **Size**: ustaw wymiary w centymetrach: X = szerokość na ekranie, Y = głębokość, Z = grubość/wysokość bryły. Przykład: `(240,100,40)`.
9. Kliknij **Refresh Block**, jeśli obraz nie odświeżył się po zmianie. Zapisz level.

**Nie przypisuj PNG do Materials → Element 0 komponentu Visual.** Blok odtwarza swój materiał dynamiczny z Settings; ręczne zmiany slotu komponentu mogą zostać zastąpione.

Górna powierzchnia znajduje się na wysokości pozycji aktora. Bryła rozciąga się w dół o Size.Z. Dla aktora Z=220 i Size.Z=40 góra jest na 220, a spód na 180.

Zmiana Image albo Top Image nie zmienia kształtu kolizji. Kolizja pozostaje prostą bryłą o wymiarach Size. Dla platformy przechodzonej od dołu ustaw **Collision=OneWay**; Solid blokuje ze wszystkich stron; NoPhysicalCollision nie daje podparcia po wejściu.

Standardowy BP_ClimbPlatform ma krawędź przednią i boczne AST. Nie dodawaj na te same miejsca drugiego zestawu niezależnych ParkourActor.

## 3. Przypisanie grafiki tła

1. Przeciągnij **BP_BackWall** do levelu.
2. W Settings ustaw **Image** na obraz ściany i **Material=M_KitImageFit**.
3. **Top Image** zostaw puste; tło nie potrzebuje powierzchni góra/spód.
4. **Tint** ustaw na biały. Dopiero potem przyciemniaj według potrzeb.
5. **Kind=Background**, **Collision=Solid** — tło od v0.8 ma kolizję i może być wykrywane jako podparcie.
6. Przykładowe **Size=(600,1,300)** oznacza obraz szeroki na 600 cm, gruby na 1 cm i wysoki na 300 cm.
7. Dla modułu o wysokości 300 ustaw pozycję tła względem modułu na `(0,45,300)`. Jego dół znajdzie się na Z=0. Położenie w Y dobierz tak, aby ściana była za postacią, a nie przecinała jej drogi.
8. Kliknij Refresh Block i zapisz level.

Nowe PNG nie są automatycznie losowane przez generator. Najprościej tworzysz kilka wariantów modułu z różnymi obrazami i eksportujesz każdy wariant do osobnego wiersza. Generator losuje te wiersze. Nie ma osobnej automatycznej biblioteki losowania nowych plików PNG z folderu.

## 4. Dlaczego w warsztacie jest pięć modułów

| Aktor | Rola |
|---|---|
| ENTRANCE_01 | Jeden wariant górnej części, czyli wejścia do szybu. |
| MIDDLE_01 | Pierwszy wariant środka. |
| MIDDLE_02 | Drugi wariant środka. |
| MIDDLE_03 | Trzeci wariant środka. |
| EXIT_01 | Jeden wariant dolnego zakończenia. |
| PREVIEW_Entrance_RandomMiddles_Exit | Złożony podgląd całego szybu, a nie kolejny moduł do eksportowania. |

Nie potrzebujesz dokładnie pięciu wariantów. Minimum do składania to jeden Entrance, jeden Middle i jeden Exit o zgodnej szerokości i warstwie. Możesz mieć np. 4 wejścia, 20 środków i 3 zakończenia. Role określają miejsce w szybie; nie projektują za Ciebie schodów, otworu czy dekoracji.

## 5. Pierwszy własny moduł — bez nadpisywania przykładów

### Utwórz własny warsztat i tabelę

1. Zapisz kopię levelu przez **Save Current Level As**, np. `L_MojWarsztatSzybow`.
2. W Content Browser utwórz **Data Table** (Add → Miscellaneous → Data Table), wybierając strukturę **POIShaftRow**. Nazwij tabelę **DT_MojeSzyby**. Możesz też zduplikować DT_ShaftModules, ale wtedy jej stare wiersze nadal uczestniczą w losowaniu.
3. W panelu Place Actors wyszukaj **POIShaft** i dodaj aktora. Nazwij go `AUTHOR_Middle_A`.
4. Ustaw Transform aktora: Location `(0,0,0)`, Rotation `(0,0,0)`, Scale `(1,1,1)`.
5. Ustaw **Height=300**, **Assemble Modules=false**, **Build Boundary Walls=false**.
6. W **Recipe** ustaw Role=Middle, Upper Layer=0, Width=600, Step Rise=150, Climbing=AST, Weight=1.
7. W **Export** ustaw Target Table=DT_MojeSzyby i Row Name=`Middle_A_Layer0`.

### Zbuduj geometrię modułu

Dodaj dwie platformy BP_ClimbPlatform i ścianę BP_BackWall. Dla pierwszego prostego modułu użyj tych wartości:

| Element | Pozycja względem modułu (X,Y,Z) | Settings → Size |
|---|---|---|
| Platforma A | `(-180,0,150)` | `(240,100,40)` |
| Platforma B | `(180,0,300)` | `(240,100,40)` |
| Tło | `(0,45,300)` | `(600,1,300)` |

Platformy: Kind=AST, Collision=OneWay, Both AST Sides=true. Przypisz boczne i górne obrazy jak wyżej. Tło: Kind=Background, Collision=Solid. Ten układ jest prostym przykładem danych mieszczącym się w regułach eksportu; jego przejście sprawdź postacią.

**W World Outliner przeciągnij każdy z trzech elementów na AUTHOR_Middle_A, aby był jego bezpośrednim dzieckiem.** Sama grupa edytora lub folder w Outlinerze nie zastępuje podpięcia aktorów. Podpięcie rozpoznasz po możliwości rozwinięcia POIShaft i zobaczenia jego dzieci.

Po podpięciu sprawdź pozycje. Przy module w `(0,0,0)` wartości lokalne i światowe są takie same. Gdy moduł przesuniesz np. do X=1000, platformy powinny przesunąć się razem z nim; ich pozycje światowe będą X=820 i X=1180.

### Ważne ograniczenia eksportu

- Aktor POIShaft: zerowy obrót i skala `(1,1,1)`.
- Bloki POI eksportowane w module: również zerowy obrót i skala `(1,1,1)`. **Wymiary ustawiaj w Settings → Size.**
- Jeśli wcześniej rozciągnąłeś blok skalą, przelicz Size: `nowy Size = stary Size × Scale`, osobno dla każdej osi, a potem ustaw Scale na `(1,1,1)`. Przykład: Size.Z=40 i Scale.Z=2,4 → Size.Z=96, Scale.Z=1. Pozycja aktora nadal oznacza górę.
- Pozycja Y każdego bloku względem modułu musi być od -50 do +50.
- Pozycja górnej powierzchni Z musi mieścić się od 0 do Height.
- Bloki muszą mieścić się w szerokości modułu: przy Width=600 obszar X wynosi od -300 do +300, z uwzględnieniem połowy szerokości bloku.
- Potrzebny jest przynajmniej jeden blok wspinaczki na wysokości nie większej niż Step Rise oraz jeden dokładnie przy górze modułu. Dla przykładu są to Z=150 i Z=300.
- Moduł może zawierać maksymalnie 100 eksportowanych elementów. Samo przejście pomiędzy nimi wymaga testu w grze.

### Zapisz moduł

1. Zaznacz AUTHOR_Middle_A, nie pojedynczą platformę.
2. Kliknij **Export Section**.
3. Sprawdź pole **Status**. Powinno informować o eksporcie. Jeśli jest błąd, tabela nie dostała nowego poprawnego modułu; sprawdź ograniczenia powyżej.
4. Otwórz DT_MojeSzyby i sprawdź wiersz Middle_A_Layer0. Powinien zawierać elementy w Pieces oraz Use Custom Section=true.
5. Zapisz **tabelę i level**.

Nie klikaj Build Shaft na ręcznie poprawianym module, żeby go „zapisać”. Build Shaft generuje geometrię z Recipe; Export Section zapisuje Twoją geometrię do tabeli. Do oglądania efektu tabeli używaj osobnego aktora podglądu.

## 6. Dodaj wejście, zakończenie i kolejne środki

Powtórz powyższy proces dla osobnych aktorów POIShaft z własnymi bezpośrednimi dziećmi. Możesz kopiować elementy istniejącego modułu, ale po kopiowaniu sprawdź, czy są podpięte pod nowego autora, a nie starego.

Na pierwszą próbę zachowaj Height=300, Width=600 i Upper Layer=0 we wszystkich modułach:

| Nazwa autora | Recipe → Role | Row Name w tej samej tabeli |
|---|---|---|
| AUTHOR_Entrance_A | Entrance | Entrance_A_Layer0 |
| AUTHOR_Middle_A | Middle | Middle_A_Layer0 |
| AUTHOR_Middle_B | Middle | Middle_B_Layer0 |
| AUTHOR_Exit_A | Exit | Exit_A_Layer0 |

Środek B może mieć inne grafiki lub inny rozkład platform, ale zachowaj punkty połączenia na dole i górze. Wejście i zakończenie projektujesz tak, aby dało się przejść między szybem a otoczeniem. System nie tworzy automatycznie drzwi ani otworu na podstawie samej nazwy Entrance/Exit.

Eksportuj i zapisz każdy wiersz osobno. Jeśli użyjesz istniejącej Row Name, eksport zastąpi ten wiersz. Weight=0 wyłącza wiersz z losowania; większa dodatnia waga zwiększa jego szansę wśród pasujących modułów tej samej roli.

## 7. Złóż cały szyb z tabeli

1. Dodaj **nowy POIShaft**, np. w `(6000,0,0)`. Nazwij go `PREVIEW_MojSzyb`.
2. Ustaw Rotation=0 i Scale=1.
3. Włącz **Assemble Modules**.
4. Ustaw **Module Table=DT_MojeSzyby**.
5. Ustaw Recipe → Upper Layer=0 i Width=600.
6. Ustaw Height=1500 i Seed=1. Dla modułów 300 cm zmieści się pięć części.
7. Wyłącz **Build Boundary Walls**, jeśli ściany już projektujesz sam w modułach. Ta opcja dodaje dodatkowe proceduralne ściany, a nie eksportuje Twoje własne.
8. Kliknij **Build Shaft**. Sprawdź Status oraz Built Roles.

Od dołu powinieneś dostać: **Exit, Middle, Middle, Middle, Entrance**. Trzy środki są losowane z pasujących wierszy Middle. Ten sam Seed i ta sama tabela dają ten sam układ; zmiana Seed pozwala zobaczyć inne zestawienie.

Jeśli dodałeś nowy obraz lub przesunąłeś platformę w autorze, najpierw Export Section i zapis tabeli, a dopiero potem Build Shaft na podglądzie. Ręczne poprawki podglądu mogą zostać usunięte przy przebudowie, dlatego poprawiaj moduły źródłowe.

Na start używaj wysokości szybu będącej wielokrotnością wysokości modułu. Przy niepełnym odcinku generator może wstawić proceduralne stopnie.

## 8. Sprawdź przejście i użyj szybu w PWG

Przesuń PlayerStart w pobliże dolnego wejścia do podglądu, nad stabilną podłogą. Ustaw World Settings → GameMode Override na **BP_POITest_PWG**, jeśli korzystasz z nowego pustego levelu. Uruchom Play i sprawdź przejście w górę i dół, kilka seedów oraz miejsca połączeń modułów. Te testy wykonujesz Ty.

W generatorze **PWGv2Generator** ustaw pole **Shaft Variants** na DT_MojeSzyby w konfiguracji używanej przez Twój świat. Tabela przypisana tylko do PREVIEW nie zmienia ustawienia generatora PWG. Puste Shaft Variants powoduje użycie domyślnej DT_ShaftModules.

Następnie utwórz **nowy świat przez Main Menu**. Stare światy przechowują zapisane kopie wybranych modułów i nie dostają automatycznie późniejszych zmian tabeli.

Upper Layer określa przejście:

| Wartość | Przejście |
|---|---|
| 0 | powierzchnia → podziemia |
| 1 | podziemia → jaskinie |
| 2 | jaskinie → lochy |
| 3 | lochy → katakumby |

Dla każdej obsługiwanej warstwy przygotuj zestaw Entrance/Middle/Exit o zgodnej szerokości. Możesz eksportować ten sam projekt kilka razy, zmieniając Upper Layer i Row Name. Niepełny lub niepasujący zestaw może spowodować użycie proceduralnego układu awaryjnego.

## Co obecny eksport rzeczywiście zapisuje

Zapisuje bloki POIKitBlock (w tym BP_ClimbPlatform i BP_BackWall) bezpośrednio podpięte pod POIShaft oraz obsługiwane oryginalne aktory AST ze spline, np. ParkourActor, BeamActor i NarrowPassageActor. Zintegrowanych chwytów BP_ClimbPlatform nie eksportujesz osobno — odtwarzają się z ustawień bloku.

Nie jest to eksport dowolnej zawartości levelu. Zwykłe StaticMeshActor, światła, drzwi, loot i inne Blueprinty nie trafiają automatycznie do wiersza modułu. Na tym etapie pewny zakres tej ścieżki tworzenia POI to modularny szyb z bloków i obsługiwanych obiektów AST. Samo umieszczenie dowolnego obiektu pod POIShaft nie dodaje obsługi jego zapisu.

## Status materiału v0.9

Przywrócono zwykłą alfę PNG w M_KitImageFit, zachowując dopasowanie UV i Tint. Wycofano maskowanie platform przez CustomStencil z v0.8. **Rysowanie postaci przed platformami pozostaje nierozwiązanym/pending punktem; nie traktuj przywrócenia grafik jako potwierdzenia tej funkcji.** Pozostała logika ruchu i wspinaczki nie była zmieniana w tej poprawce.
