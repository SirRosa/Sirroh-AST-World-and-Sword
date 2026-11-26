# Corridory Underground — pierwsze template'y (v13)

Aktualizacja: wersja v14 rozszerza wszystkie mapy do 10 pól i dodaje składanie ciągów. Aktualne uruchamianie i testy opisuje `POI_V14_CIAGI.md`; poniżej pozostaje opis obiektów i materiałów z v13.

## Gdzie zacząć

1. Otwórz projekt po kompilacji. Otwórz Content Drawer: Ctrl+Space.
2. Przejdź do `Content/POIKit/Underground/Templates`.
3. Zduplikuj wybraną mapę (prawy przycisk na asset mapy → Duplicate). Nadaj własną nazwę i otwórz kopię dwuklikiem.
4. Każdy osobny Background oznacza jeden fragment POI. Buduj w jego obrysie. Podpisy nad fragmentami pomagają je rozróżnić.

| Mapa | Zawartość |
|---|---|
| L_Pushable_U | 5 fragmentów: beczka; skrzynia; ławka; beczka + skrzynia; skrzynia + ławka |
| L_Entrance_U | 5 fragmentów z oryginalnymi drzwiami AST, w różnych pozycjach |
| L_Light_U | 5 fragmentów z pochodnią, różne pozycje i wysokości |
| L_Empty_U | 1 pusty fragment z tłem i podłożem — do kopiowania i dekoracji |
| L_Parkour_U | 1 fragment z trzema platformami AST takimi jak w szybach |

To proste układy startowe do własnej edycji. Pięć pól na mapie to pięć niezależnych wariantów, a nie pięć części jednego pokoju. Do dodatkowych wariantów najprościej zduplikować mapę. Można też skopiować całe pole razem z Background i przesunąć wszystkie jego obiekty w X; granice Background nie mogą się nakładać.

## Dodawanie i edycja obiektów

W `Content/POIKit/Underground/Actors` są:

- `BP_Pushable_U_Barrel`, `BP_Pushable_U_Crate`, `BP_Pushable_U_Bench` — przeciągnij asset do levelu. To dzieci oryginalnego AST PullPushObject: zachowują jego logikę, interakcje, cztery strony chwytu, warpy, triggery oraz cztery krawędzie parkouru. Anchory są dopasowane do wymiarów poszczególnych meshy. Oryginalne assety AST pozostają bez zmian. Kopie meshy z prostą kolizją znajdują się w `Underground/Meshes`.
- `BP_Door_U` — dziecko oryginalnego DoorActor AST, bez zmiany mechaniki drzwi.
- `BP_Torch_U` — model pochodni z Point Light. Zaznacz aktora, potem komponent `TorchLight`: możesz zmienić Intensity, Light Color i Attenuation Radius. Domyślnie ciepłe światło, zasięg 650 cm, bez cieni i bez Niagara. Światło jest częścią aktora i zapisuje się razem z nim do POI.

Platformy kopiuj z `L_Parkour_U` lub własnego warsztatu szybów. Zachowują dotychczasowe ustawienia wspinaczki i jednostronnej kolizji. Możesz dodawać inne kompletne aktory AST, np. Beam, szafki lub dekoracje.

Przesuwaj i obracaj **całego aktora**, nie jego pojedynczy warp/trigger. Trzy gotowe pushable najlepiej testować najpierw przy Scale 1. Zmiana samego mesha na zupełnie inny nie dopasowuje automatycznie odziedziczonych anchorów; przygotowane trzy warianty mają je już dopasowane.

Background ma wyznaczać granice i pozostać bez rotacji. Eksport wybiera obiekty według położenia ich pivotu wewnątrz Background w X/Z. Nie zostawiaj pivotu dekoracji poza polem. Podpisy, PlayerStart i wspólna testowa podłoga `Floor` nie trafiają do publikacji. Lokalny blok `..._Ground` jest częścią fragmentu.

## Zapis do świata — bez edycji Data Table

1. Zapisz edytowaną kopię mapy (Ctrl+S).
2. Na górnym pasku edytora wybierz **POI → Underground → Zapisz wszystkie fragmenty z Background**.
3. Jeżeli chcesz opublikować tylko jedno pole, zaznacz jego Background i wybierz **Zapisz zaznaczony Background**.
4. Poczekaj na komunikat zakończenia. Powstają osobne mapy publikacji; mapa robocza pozostaje do dalszej edycji.
5. Uruchom **nowy świat PWG** z Main Menu, aby generator uwzględnił nowy katalog. Istniejący zapis świata przechowuje wcześniejszy zestaw POI.

Nazwy Background `Pushable_U_01_Background`, `Entrance_U_01_Background` itd. zapisują też kategorię fragmentu. Gotowe template'y nie zostały automatycznie opublikowane: opublikuj te, które chcesz mieć w świecie.

Na tym etapie Underground umieszcza pojedyncze zapisane fragmenty w pasujących miejscach terenu, z kontrolą miejsca przy szybach, wysokości i płaskości podłoża. Pełne składanie pokojów w ciągi `Entrance → losowe fragmenty → Entrance` jest kolejnym etapem, nie funkcją tej wersji. Obecne mapy przygotowują elementy do tego systemu.

## Losowe obrazy Dirt

10 obrazów z dostarczonego archiwum znajduje się w `Content/POIKit/Art/Dirt`. Pula: `Content/POIKit/Pools/BG_Dirt`. Motyw: `Content/POIKit/Themes/DA_Underground` — Backgrounds = BG_Dirt, Platforms = PF_Planks.

Nie przypisujesz PNG każdemu pokojowi. Zapisane fragmenty Underground korzystają z tego motywu. W edytorze użyj **POI → Underground → Podglad: losuj Dirt**, aby zobaczyć losowanie. **Pule grafik Underground** otwiera ustawienia motywu.

Aby rozszerzyć pulę: zaimportuj PNG do folderu Dirt, pozostaw sRGB włączone, Compression Settings = Default i Compress Without Alpha wyłączone. Otwórz BG_Dirt, dodaj element Images, przypisz teksturę do Side, użyj Bake Alpha Bounds i zapisz asset. W PF_Planks para Side/Top określa bok oraz górę/dół platformy.

Underground używa nowego `M_KitImageLit`: reaguje na pochodnie i ma delikatną bazową jasność. Dotychczasowe motywy szybów zachowują swój materiał. Nie podmieniaj materiału Underground na unlit, jeżeli tło ma reagować na światło.

## Testy do wykonania przez gracza

Mapy mają ustawiony `BP_POITest_PWG`, używany do testów z integracją Sirroh/PWG. Otwórz template i kliknij Play:

1. **Pushable:** podejdź z lewej i prawej do każdej z trzech brył, naciśnij E, sprawdź chwyt i ruch A/D. Sprawdź pozycje dłoni, stóp, kolizję i zakończenie interakcji według sterowania AST. Potem sprawdź drugi przedmiot w polach z dwoma obiektami.
2. **Door:** podejdź do obu stron drzwi, E, otwórz/zamknij i przejdź przez otwór. Sprawdź obrót postaci i brak utknięcia.
3. **Light:** sprawdź, czy pochodnia oświetla tło i obiekty. W razie potrzeby zmniejsz testowe Directional Light. Zmiana Intensity komponentu TorchLight powinna zmieniać lokalną jasność.
4. **Parkour:** sprawdź chwyt, wspięcie, przeskok oraz opuszczenie platformy tak samo jak w szybach.
5. Po publikacji sprawdź te same interakcje w nowym świecie PWG.

Wykonano kompilację i kontrolę struktury assetów bez uruchamiania rozgrywki przez sterowanie komputerem. Kontrola assetów nie zastępuje powyższych testów animacji i interakcji.
