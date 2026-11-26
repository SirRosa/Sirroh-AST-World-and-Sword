# AGENTS.md — Sirroh AST / World & Sword

Instrukcja dla każdego nowego agenta pracującego nad projektem. Najpierw przeczytaj całość, sprawdź aktualny stan repozytorium i dopiero realizuj zadanie użytkownika. Nie traktuj historycznych commitów jako nakazu cofnięcia nowszych zmian.

## 1. Projekt i zasady projektowania

- Sirroh AST (AST_V1_18, Unreal Engine 5.8.1): gra 2.5D, z oprawą dążącą do Rain World, interaktywnością i reaktywną fizyką inspirowaną Noitą oraz kierunkowym systemem walki.
- Kamera idealnie z boku, perspektywa ortograficzna. Świat, postacie i fizyka działają w 3D. Nie zmieniaj tego założenia bez uzgodnienia.
- Gracz porusza się po trzech pasmach głębokości. Są one szczegółem implementacji: gracz nie ma ich widzieć ani być zmuszony do rozumienia ich istnienia. Interakcje, podnoszenie, wspinaczka, walka i obsługa obiektów mają być możliwe niezależnie od aktualnego pasma. Nie rozwiązuj problemów przez wymaganie od gracza ręcznego dopasowania pasma do obiektu.
- Priorytety: spójność wizualna, fizyczna interaktywność, brak softlocków, wydajność, kompatybilność istniejących systemów AST, parkouru, POI i streamingu. Nie zastępuj działających systemów uproszczonymi atrapami.
- Zmiany proceduralne mają działać dla różnych seedów i istniejących zapisów. Nie modyfikuj formatu zapisu, nie kasuj światów ani nie regeneruj ich bez potrzeby.

## 2. Dostęp i podstawowy workflow

Kolejność pracy: **Google Drive → GitHub → MCPENROUZ**.

| Zasób | Lokalizacja |
|---|---|
| Repozytorium bieżącego projektu | https://github.com/SirRosa/Sirroh-AST-World-and-Sword |
| Synchronizowany projekt na Google Drive | https://drive.google.com/drive/folders/1TmyJl3JdbRR_Y4uXNlSGXM3OOjqbxbnT |
| Projekt na komputerze | D:\0 Sirro\Unreal Projects\AST_V1_18 |
| Plik projektu | AST_V1_18.uproject |
| Właściwy silnik | D:\0 Sirro\UE 5.8.1\UE_5.8 |
| Moduł C++ | Source/Sirroh_Reimagineg_v3 |
| Dokumentacja i narzędzia | Source/Docs, Source/Tools |

Użytkownik udostępnił konektory GitHub (w tym GitHub Actions), Google Drive i MCPENROUZ. Najpierw odkryj dostępne narzędzia i ich rzeczywiste schematy; nie zakładaj braku dostępu. Jeżeli po sprawdzeniu występuje błąd połączenia lub uprawnień, podaj konkretny błąd.

1. Odczytuj aktualny kod z GitHuba; pełny snapshot Source został zapisany w commicie 41f1c8f. Google Drive jest źródłem plików projektu, assetów, aktualizacji lokalnych oraz logów. GitHub jest roboczą kopią kodu, nie automatycznie aktualnym odbiciem dysku. Przed zmianą sprawdź rozbieżności i SHA.
2. Jeśli potrzebnego pliku nie ma w repo lub jest nieaktualny, odczytaj/skopiuj go z Drive do GitHuba (z zachowaniem ścieżki i oryginału). Nie zużywaj MCP na przeglądanie każdego CPP. Używaj odczytu zakresów linii, drzewa Git i dokumentacji. Przy pobieraniu plików przez konektor sprawdź także zwrócone załączniki/plik roboczy, jeśli tekst odpowiedzi jest pusty.
3. Wykonuj właściwą pracę na GitHubie, po backupie. Przy wielu plikach preferuj jeden spójny commit (blob/tree/commit/ref lub odpowiedni dostępny mechanizm). Weryfikuj diff i faktyczny SHA. Actions mogą służyć do automatyzacji/testów dostępnych na runnerze; nie są zamiennikiem lokalnej kompilacji z właściwym UE.
4. Prześlij na komputer tylko zmienione pliki, najlepiej z URL przypiętych do pełnego SHA commita. Skrypt przed zapisem weryfikuje wszystkie stare i nowe hashe (git hash-object --no-filters), pobiera wszystkie pliki do stagingu, tworzy lokalne kopie, dopiero potem wdraża całość. Przy rozbieżności przerwij, zachowaj zmiany użytkownika i ustal różnicę. Zachowaj kodowanie/CRLF, nie nadpisuj całego Source.
5. Kompiluj lokalnie przez MCPENROUZ, wykonuj adekwatne testy, odczytuj logi i poprawiaj rzeczywiste błędy. Nie deklaruj zakończonej implementacji bez sprawdzenia wdrożenia/kompilacji. Test headless nie zastępuje testu wizualnego.
6. Repo zawiera przede wszystkim kod. Nie wrzucaj całego wielogigabajtowego projektu, binariów, sekretów, licencjonowanych assetów czy zapisów na publiczny GitHub. Duże pliki i binarne backupy trzymaj w synchronizowanym projekcie; Git LFS tylko kiedy rzeczywiście potrzebny.

Pracuj w bieżącym chacie z dostępnymi narzędziami. Nie przenoś samowolnie pracy do Codex/Work. Repo i pliki powyżej dotyczą World & Sword; inne repozytoria AST nie są domyślnym celem zmian.

## 3. MCPENROUZ — minimalizuj zużycie

Zasada rozliczania (oryginalny komunikat):
> Only accepted device jobs count as API calls, including reading files and device data. Listing devices and checking an existing operation do not use API calls. Additional packs do not bypass account restrictions.

- Limit jest realny i zależny od konta. W poprzednich sesjach dostępny budżet bywał ograniczony do około 10 calli. Nie zakładaj, że jest to stała aktualna liczba; sprawdzaj dostępny budżet, jeśli narzędzia go podają.
- Listowanie urządzeń i sprawdzanie istniejącej operacji (operation_status, z właściwym operation_id) nie zużywa calli zgodnie z komunikatem. Nie utożsamiaj operation_id z session_id procesu. Odczyt process_output nie musi być rozliczany tak samo — nie spamuj nim.
- Gdy MCP jest jedyną opcją, przygotuj **jeden zbiorczy skrypt PowerShell/Python**: preflight, kontrola hashy, staging, backup, wdrożenie, kompilacja, testy i wynik. Nie wykonuj osobnych wywołań dla każdego pliku, katalogu czy polecenia. Nie przekraczaj limitów długości polecenia; dłuższe skrypty umieszczaj w repo.
- Skrypt zapisuje logi i jednoznaczne statusy w projekcie, np. Saved/ChatGPT/<NazwaZadania>/{result.txt,build.log,test.log}. Google Drive synchronizuje te pliki; odczytuj je konektorem Drive bez MCP. Używaj unikalnych nazw, sprawdzaj daty i aktualność logów. Synchronizacja może być opóźniona.
- Przy trwającej kompilacji sprawdzaj istniejącą operację lub zsynchronizowane logi zamiast uruchamiać nowe zadania. Brak aktualizacji logu nie oznacza zakończenia procesu. Weryfikuj końcowy exit code i treść logu. Nie zgaduj wyniku.
- Poprzedni identyfikator urządzenia nie jest trwałą gwarancją dostępności. Przy rozłączeniu sprawdź listę urządzeń; nie marnuj calli na wielokrotne ponawianie błędnych zleceń.
- Nie uruchamiaj kompilacji z otwartym Unreal Editor ani nie zamykaj użytkownikowi gry/edytora bez uzgodnienia. Jeśli pliki są zablokowane, raportuj blokadę; nie zabijaj procesów na ślepo.

## 4. Backup i możliwość cofnięcia zmian — obowiązkowe

**Przed każdą zmianą CPP, H lub innego istniejącego pliku zrób trwały backup na GitHubie.** Najlepiej utwórz osobny branch backup/<konkretne-zadanie> wskazujący dokładny stan przed zmianą. Jeżeli aktualny plik lokalny różni się od repo, najpierw zabezpiecz rzeczywisty oryginał. Zweryfikuj backup oraz jego SHA przed wdrożeniem. Nie usuwaj starych backupów.

- Dodatkowo zapisz lokalne kopie zmienianych plików w Saved/ChatGPT/<zadanie>. Dla .umap/.uasset wykonaj kopię binarną przed edycją, z kontrolą sumy; przechowuj ją lokalnie/na Drive. Nie publikuj binariów bez potrzeby.
- Małe, celowane zmiany. Nie modyfikuj innych systemów „przy okazji”. Przy usuwaniu elementu sprawdź wszystkie miejsca generowania, zależności, kolizje i interakcje.
- W przypadku regresji wróć do konkretnej sprawdzonej wersji lub wykonaj ukierunkowany revert. Nie resetuj na siłę main ani lokalnych zmian użytkownika. Backup obejmuje również testy i nagłówki.
- W końcowym raporcie podaj link do rzeczywistego commita i backupu, wynik wdrożenia, kompilacji i testów, a także niezweryfikowane aspekty.

## 5. Kompilacja i znane pułapki narzędziowe

Właściwy build w PowerShell (zweryfikowany wcześniej):
~~~powershell
$p='D:\0 Sirro\Unreal Projects\AST_V1_18'
$e='D:\0 Sirro\UE 5.8.1\UE_5.8'
$u="$p\AST_V1_18.uproject"
& "$e\Engine\Build\BatchFiles\Build.bat" AST_V1_18Editor Win64 Development "-Project=$u" -WaitMutex -NoHotReloadFromIDE
$rc=$LASTEXITCODE
~~~
Przechwytuj stdout/stderr do logu, zapisuj BUILD_EXIT i przerywaj testy przy błędzie. Nie myl kompilacji kodu z pakowaniem gry.

- **Dwie ścieżki silnika:** kompilator korzystał jednocześnie z nagłówków z D:\0 Sirro\UE 5.8.1\UE_5.8 oraz D:\Sirro\UE 5.8.1\UE_5.8. Prawidłowa jest pierwsza. Rozwiązaniem było odizolowanie/przeniesienie nieaktualnych plików pośrednich, poprawienie kolejności include oraz ponowny build bez XGE/UBA. Przy nawrocie sprawdź EngineAssociation, ścieżki UBT, odpowiedzi kompilatora i cache. Nie usuwaj całego projektu, silnika ani Content. Tryb bez XGE/UBA (-NoXGE -NoUBA, o ile obsługiwane przez bieżący UBT) jest awaryjny; nowsze buildy z XGE standalone przechodziły.
- **Kolejność include UE:** PWGv2Generator.cpp musi zaczynać się od własnego nagłówka PWGv2Generator.h, przed Misc/ScopeExit.h (7de4dfd). Stosuj reguły UBT/IWYU.
- **TObjectPtr:** w range-for po TArray<TObjectPtr<T>> nie używaj automatycznie auto*; stosuj jawny typ wskaźnika lub .Get() zależnie od kontenera (d69bb36).
- **Shadowing AActor:** parametr bHidden w setterze APOIShaft kolidował z polem klasy bazowej. Zmieniono go na bHideLayer (70e8598).
- Ostrzeżenie XGE „License not activated” może oznaczać tryb standalone, nie błąd kompilacji. Decyduje końcowy Result i exit code.
- Python commandlet: przekazuj ścieżkę skryptu z ukośnikami / (D:/0 Sirro/...), ponieważ backslash przed 0 bywa interpretowany jako escape. VertexColor w MaterialEditingLibrary łącz przez domyślny pin (''); nazwa RGB nie działa dla tego węzła.
- Testy Unreal mogą długo inicjalizować Zen/DDC; sprawdzaj log projektu, nie uruchamiaj równoległych instancji tylko dlatego, że stdout jest jeszcze pusty.

Przykładowy test headless po udanym buildzie:
~~~powershell
& "$e\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $u '-ExecCmds=Automation RunTests Sirroh.PWG.SpawnGeometryAudit' '-TestExit=Automation Test Queue Empty' -unattended -nop4 -nullrhi -nosplash -stdout -FullStdOutLogOutput
~~~
Weryfikuj Test Completed / Result={Success}, faktycznie znalezione i wykonane testy oraz exit code. Dobieraj też Sirroh.PWG.StreamingCost i Sirroh.POI.RoomChains oraz inne testy związane z zadaniem. Testy z -nullrhi nie potwierdzają rzeczywistego obrazu; do zmian renderingu wymagaj także testu wizualnego.

## 6. Stan projektu i rozwiązane regresje (26.11.2026)

Ostatni stan przed utworzeniem tej instrukcji: d6276b269ef4d6a29b2d61304eeb9e559443ee0b. Użytkownik potwierdził, że ostatnia poprawka cullingu prawdopodobnie rozwiązała problem.

| Problem | Rozwiązanie / punkt odniesienia |
|---|---|
| Stare platformy PWG w podziemiach, niekompatybilne z nowym parkourem, softlocki | Usunięto stare bloki pomieszczeń oraz switchback slabs i związane stare ledge actory w PWGv2Underground.cpp (fe1ee26). Nie przywracać ich. Zachowano współczesne POI/kit shafts i geometrię tuneli. |
| Śmieciowe stanowiska testowe i NPC na spawnie | Wyłączono automatyczne generowanie fixture'ów AST i treningowego NPC; jawne tryby testowe nadal dostępne (4463176). |
| Pozostałości Default level na spawnie | L_SirrohGameplay był kopią Default. Skrypt Source/Tools/cleanup_pwg_gameplay_map.py usunął 94 zweryfikowane stare aktory z mapy, pozostawił AmbientSound; backup .umap przed zapisem (e4b371d). Nie czyścić innych map/assetów automatycznie. |
| Tunel wystający z ziemi przy spawnie | Interpolacja pierwszej warstwy mogła podciągnąć podłogę nad lokalny teren. Ograniczono zwykły interpolowany floor do Height(X)-340 cm, zachowując autorskie endpointy i podłogi pokojów (7e11248); dodano audyt geometrii (e8a8dbb). |
| Teren/roślinność powierzchni widoczne w podziemiach; tunele widoczne spod powierzchni | Render mask zależny od LayerAt i bUndergroundView; stosowany przy zmianie warstwy i budowie chunków. Ukrywa powierzchnię, skały, roślinność, niebo itp. pod ziemią, odpowiednie cave/POI na powierzchni. Kolizje i fizyka pozostają niezależne; zachowano wejścia (5d63687, 70e8598). |
| Regresja: ziemia znikała również na powierzchni | SolidEarth wydzielony z Backdrop do osobnych sekcji 25–28, zawsze dostępnych do renderowania w obu domenach (z normalnym cullingiem kadru); sekcje autorskich ścian POI przesunięto od 29. Powierzchniowy mesh pozostaje widocznym komponentem, a widoczność zmienia się na rzeczywistych sekcjach draw. Nie przywracaj ukrywania całego komponentu ani całego podziemnego backdropu (d6276b2). |

Ostatni SpawnGeometryAudit przeszedł dla świata asd i dwóch nowych seedów (847, 123): powierzchnia → podziemia → nowy chunk → powierzchnia, kontrola sekcji draw, SolidEarth i niezmienionych kolizji. Ostatnia kompilacja BUILD_EXIT=0. To punkt odniesienia, nie gwarancja braku nowych regresji.

**Maskowanie przekroju podziemi:** sekcje 29–32 to osobna, bezkolizyjna maska pierwszego planu z teksturami WallMaterial odpowiednich warstw; autorskie sekcje POI zaczynają się od 33. Maska jest widoczna wyłącznie pod ziemią, a jej otwory wynikają z rzeczywistych granic pokoi i szybów. Nie łącz jej z SolidEarth (25–28), potrzebnym również na powierzchni. Cieniowanie maski jest wypiekane w vertex color podczas budowy chunka (PWGv2Underground.cpp): EdgeShade=0.85, ShadeRange=100 cm, pełna czerń poza zasięgiem. Dziel maskę w obu osiach (20 cm w pasie gradientu), szczególnie przy pionowych krawędziach szybów; duże, głębokie obszary pozostają jednolicie czarne. Zwykły M_PWGv2_Tiled nie odczytuje vertex RGB, więc maska używa osobnego M_PWGv2_SolidCutaway, z identycznym atlasem i Tint oraz mnożeniem koloru przez VertexColor. Asset generuje Source/Tools/create_pwg_cutaway_material.py; nie modyfikuj zwykłego materiału. Sprawdzaj połączenie grafu i przypisanie parent material, nie tylko dane mesha. Odległość do granic pokoi i szybów jest liczona w X/Z, także z sąsiednich chunków. Zachowaj kolizje i culling. SpawnGeometryAudit sprawdza geometrię maski, otwarte przejścia i widoczność; wspólne krawędzie quadów liczą się jako zasłonięte. Zmiana nie wymaga nowego świata. Uwaga: Unreal może zwrócić TEST_EXIT=0 także przy Result={Fail}; zawsze sprawdzaj wynik testu w logu.

**Oświetlenie ścian podziemi:** tła POI (karty APOIKitBlock i projektowane sekcje PWG od 33) oraz tylne ściany jaskiń/szybów korzystają z osobnych materiałów Default Lit: M_KitImageFit_LitBackground i M_PWGv2_BackgroundLit. Skrypt Source/Tools/create_pwg_lit_backgrounds.py kopiuje oryginalne grafy UV/atlasu/alpha, przekierowuje kolor do Base Color, zeruje Emissive/Metallic/Specular i ustawia Roughness=1. Nie zmieniaj globalnych materiałów platform ani maski SolidCutaway. Nie zastępuj reakcji na prawdziwe światło emisją.

## 7. Mapa kodu i invariants

- Aktywny generator to APWGv2Generator, nie stary APWG_Generator. Szukaj w Source/Sirroh_Reimagineg_v3/{Private,Public}/Procedural_World_Generator. PWGv2Generator.cpp: budowa powierzchni, streaming, zapis/plan; PWGv2Underground.cpp: LayerFloor, LayerCeiling, LayerAt, UpdateCutaway, jaskinie, szyby; PWGv2Decorations.cpp: roślinność, materiały, ApplyChunkLayerVisibility, UpdateGameplayView; PWGv2Plan.cpp i PWGv2AuthoredPOI.cpp: plan i autorskie POI.
- Private/PWG/PWGGameMode.cpp: uruchamianie świata i testowe fixtures; PWGSidescrollerCamera.cpp: kamera i culling kadru; PWGEnvironmentV2.cpp: niebo, pogoda, oświetlenie/atmosfera.
- Private/POI: APOIShaft, APOIKitBlock, autorska geometria, traversal, dormancy i testy. Nie myl starego APWGLedgeActor z obecnym systemem AST ParkourActor/BPI_ParkourActor. Nie twórz zwykłych bloków z kolizją jako zamiennika parkourowych obiektów.
- Render culling, symulacja, kolizja i dormancy to różne mechanizmy. Nie wyłączaj kolizji/ticków tylko po to, żeby coś przestało być widoczne. Pamiętaj o aktorach przenoszonych przez gracza oraz elementach POI tworzonych asynchronicznie.
- Budowa chunków jest kosztowna; unikaj globalnego przebudowywania świata co klatkę. Zachowuj deterministyczność, asynchroniczną budowę kolizji, poprawną własność aktorów na granicach chunków i przywracanie stanu po streamingu.
- Zachowuj istniejący styl graficzny i parametry materiałów. Przy zmianach świateł/ognia nie zastępuj proceduralnych efektów statycznymi atrapami, nie zmieniaj niezwiązanych elementów.
- Przed nową implementacją przeczytaj powiązane aktualne pliki oraz Source/Docs. Historyczne checkpointy mogą być nieaktualne — porównaj z kodem.

## 8. Sposób komunikacji i definicja ukończenia

Użytkownik oczekuje pracy, nie wielokrotnego opisywania planu ani meldunków o nieukończonych etapach. Ogranicz komunikaty do niezbędnego minimum. Nie pytaj o informacje, które już są w repo, Drive lub w poleceniu. Nie zgaduj i nie przedstawiaj hipotezy jako potwierdzonego faktu.

Zadanie jest gotowe do testów użytkownika dopiero po zapisaniu zweryfikowanego commita, wdrożeniu właściwych plików, udanej kompilacji i adekwatnych dostępnych testach. Gdy coś blokuje pracę, podaj dokładną przyczynę, co zostało wykonane i co nie jest zweryfikowane. Nie deklaruj, że efekt wizualny działa, jeśli sprawdzono jedynie testy headless.

- W każdym raporcie końcowym jednoznacznie napisz, czy do testowania zmian trzeba utworzyć nowy świat, czy wystarczy wczytać istniejący zapis (oraz czy konieczny jest restart gry). Jeśli nie masz pewności, zaznacz to.
- Na bieżąco aktualizuj AGENTS.md o trwałe, użyteczne ustalenia, powtarzalne problemy i zweryfikowane rozwiązania. Nie zapisuj przejściowych logów, drobnych iteracji ani redundantnych informacji; dokument ma pozostać zwięzłą instrukcją, nie kroniką rozmów.

**Jeśli rozumiesz zasady pracy, zapytaj, na czym polega twoje zadanie, jeśli jeszcze go nie otrzymałeś.**
