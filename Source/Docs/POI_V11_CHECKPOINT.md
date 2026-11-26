# Checkpoint v11 — prosty zapis POI, 2026-11-11

## Dostarczone

- `Content/Python/init_unreal.py` instaluje menu **POI** w głównym pasku edytora levelu.
- `poi_authoring.py`: eksport dowolnych aktorów w granicach Background przez skopiowanie levelu i usunięcie elementów spoza pola. Zachowuje ustawienia, transformacje, spline’y i wewnętrzne referencje. Źródłowy level jest zapisywany i ponownie otwierany; nie jest filtrowany ani przebudowywany. Eksport wyklucza pomocniki testowe i dzieci ChildActorComponent (te zachowuje ich rodzic).
- `L_POI_Template`: kopia warsztatu bez starych aktorów eksportera i generowanego podglądu, 5 pól Entrance/Middle/Middle/Middle/Exit. Zachowany testowy GameMode PWG/Sirroh. Oryginalny warsztat i mapy użytkownika nie były zapisywane.
- Oddzielne katalogi automatyczne Shaft/Surface/Dungeon; po finalizacji puste, bez testowych wariantów. Nie opublikowano za użytkownika jego nieukończonych wzorców.
- `FPOIShaftRow.SectionLevel`, instancje `ULevelStreamingDynamic` z usuwaniem przy zwalnianiu właściciela. Istniejący eksport Pieces/NativePieces nadal obsługiwany.
- PWG nowego świata automatycznie dołącza katalog użytkownika szybów. Istniejące moduły pozostają w losowaniu jako dotychczasowe warianty. Samodzielne Surface/Dungeon zapisują snapshot i pozycje w WorldSave; domyślnie próba rozmieszczenia co 80 m, bez kolizji z szybami, nad wodą ani w zbyt niskich komnatach. Surface dodaje stempel wyrównujący podłoże; roślinność w obrębie POI jest pomijana.
- BG_Bricks: 10 obrazów. PF_Planks: 10 par bok + góra/dół. Cztery DA_Shaft_0..3 wskazują te pule. Osobne losowanie każdego bloku, deterministyczne względem seeda i indeksu instancji, niezależne od kolejności ładowania. UV alfa wypiekane w DataAsset; dostępne Bake Alpha Bounds i podgląd losowania w menu.
- `/Game/POIKit` dodany do cook directories, żeby katalog, pule i opublikowane mapy nie zniknęły w buildzie.
- Nowa instrukcja: `POI_PROSTY_ZAPIS.md`; stare punkty startowe odsyłają do niej.

## Sprawdzone

- Development Editor Win64: Succeeded, `Saved/POICheckpoints/Build-v11.log`.
- Headless setup: dwie pule po 10 wpisów, cztery profile; `Authoring-v11.json`.
- Headless eksport 5 obszarów i ponowne otwarcie mapy: zachowany obrót, skala, lokalne położenie dowolnego StaticMeshActor oraz referencja Owner do Background z tej samej mapy. Pomocniki wykluczone. `Authoring-roundtrip-v11.json`, PASS.
- Automation `Sirroh.POI.PublishedLevels`: Success. Test pul, powtarzalnego losowania, wczytania mapy jako instancji, obecności bloków z grafiką, zwolnienia instancji. Raport `POI-v11-tests/index.json`.
- Finalizacja: katalogi puste, podgląd grafik na template, menu istnieje w ToolMenus. `Finalize-v11.log`: POI_V11_FINAL_ASSETS_OK.
- Bez sterowania komputerem, bez ręcznych testów rozgrywki. Testy nie stanowią dowodu przechodniości nowych wzorców ani zachowania wszystkich obcych Blueprintów.

## Granice wersji / dalsza praca

- Autorstwo ograniczone do prostokątnego Background: obrót 0, dodatnia skala; szerokość 400–1200, wysokość 150–3000 cm. Pozostałe aktory mogą mieć dowolny transform. Wybór przynależności po pivocie X/Z, Y ±2000. Nie eksportuje Level Blueprint jako niezależnego systemu logiki; używać aktorów.
- Światy zapisują mapy konkretnych wersji. Nie kasować starych Published, jeżeli istnieją sejwy odwołujące się do nich. Zmiany katalogu dotyczą nowych światów; pule wyglądu są wspólne i mogą zmienić wygląd dawnych światów po edycji puli.
- Obecnie brak przycisku wycofania wpisu z katalogu. Ponowny zapis aktualizuje wariant, nowa kopia mapy dodaje wariant. Publikacja nie usuwa wpisów odpowiadających usuniętym/zmienionym nazwom Background.
- Surface/Dungeon to samodzielne POI z domyślnym rozmieszczaniem, nie pełne zastępowanie całej struktury komnat. Składanie szybów ma dotychczasowy mechanizm dopasowania szerokości i proceduralnego uzupełnienia reszty wysokości. Najprościej zachować pola 600×300.
- Zachowanie i trwały stan niestandardowych Blueprintów zależą od ich własnej integracji z zapisem gry. Ten eksport nie dodaje nowego uniwersalnego zapisu stanu skrzynek/lootów.
- Przednia kolejność rysowania postaci vs platformy nadal ma ograniczenie z v09; w tej zmianie nie zmieniano renderowania postaci ani parkouru.

## Odtworzenie

Archiwum `Saved/POICheckpoints/POI-v11-simple-authoring.zip` zawiera Source, Content/POIKit, Content/Python, Config, projekt, DLL modułu, instrukcje i raporty. Skrypt `Tools/checkpoint_poi_v11.py` sprawdza ZIP i zapisuje manifest SHA256. Rozpakowywać do katalogu projektu przy zamkniętym edytorze, po zabezpieczeniu nowszych zmian.
