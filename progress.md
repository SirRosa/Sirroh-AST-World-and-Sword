# Visual Foundation — progress

Zakres: pakiet Sirroh AST Visual Foundation AI, kroki 00–03. Gałąź feature/visual-foundation-v1. Main pozostaje bez zmian do akceptacji wizualnej.

## 00 — Audyt i baseline (w toku)
- [x] Przeczytano wszystkie 13 dokumentów i przejrzano 9 referencji; priorytetem jest separacja planów bez podnoszenia czerni.
- [x] Odczytano aktualny AGENTS.md (blob 0d881048627876f265c325563117d10317a6c21f), HEAD main ec277fff3ac25a6dcc6fad75e6ad462dfc05ea89, drzewo ad98d0c68517d9171ad3e62bebc935c2b4b8d2c1.
- [x] Backup backup/visual-foundation-before-20261126 potwierdzony na tym samym commicie.
- [x] Lokalne hashe bez filtrów zgodne z GitHub dla PWGEnvironmentV2.h (23024f79f5d0868cb4f7f2d65446a1e06527e054), PWGEnvironmentV2.cpp (abdfca5365cdab68315a3eccddf66f5ba8edd2b0), SirrohDebugSubsystem.cpp (e03d12e042cad4452d1f288430fa9ff3ab5bb35c).
- [x] Drive: istniejący Content/Python/pwg_depth_atmosphere.py (4238 B), lokalny materiał Legacy (17250 B); lokalny zapis PWGv2_asd.sav dostępny. Nie publikowano save ani referencji.
- [x] Audyt źródeł: jeden GlobalDepthAtmosphere, priority 20; w Legacy parametry z Settings, origin z Pawn, mgła podziemna x0.3. F7 ma osobne cele refleksji, Undo/Reset/JSON; cel bez flagi bWorld nie wymaga DebugRebuild. Generator przy wczytaniu kopiuje zapisane Settings. Strefy podziemi: kod chroni dwa grywalne pasma (-LaneSpacing i 0), ale cały system ma trzy; bezpieczny grading musi chronić również trzecie, kosztem części bliskiego tła. Backdrop Y=75+Bulge (bulge do ok. 240 cm), maska Y=-LaneSpacing-115, SolidEarth Y=min(BackgroundDepth,LaneSpacing+450).
- [x] Read-only commandlet: AUDIT_EXIT=0, rzeczywisty Legacy graph zgodny ze skryptem, 13 scalar + 3 vector, WorldPosition, custom HLSL -> Emissive, after tonemapping, blendable priority -10. Logi Saved/ChatGPT/VisualFoundation_20261126/asset_audit.txt i asset_audit.log.
- [x] Lokalny PWGv2Generator.cpp zgodny (673ea12741a8887a3b18cccd39696e0ecbe0056e). DefaultGame.ini zawsze cookuje /Game/Systems/Procedural_World_Generator. SHA256 Legacy: 565DB3F82136F15033C3F6B4652C8D26B1F1BF214AF1D85B64AAE956F64F3F62; SHA256 asd: 834A48D3BDA5D8D0D081C590E50C14A38B050B1D2BADB6A5AE15AA4F9A0B022A.
- [ ] Runtime Settings w grze, oświetlenie, dokładna kolejność względem innych passów i translucencji.
- [ ] Aktualne screenshoty baseline i pomiary CPU/GPU median/p95; obrazy z pakietu są historyczne, nie stanowią testu A/B.

## 01 — Profile wizualne
- [x] Implementacja C++: transient Legacy / Underground_Stylized, niezależne od FPWGv2WorldRow; zachowanie override przy ponownym GenerateWorld na tym samym generatorze. Nowy aktor/mapa wraca do Legacy.
- [x] Implementacja resolvera, sanitization, fallback, strefa stabilna względem generatora +/- (LaneSpacing + 50 + padding), test jednostkowy Sirroh.PWG.VisualProfile (Success).

## 02 — Depth Atmosphere V2
- [x] Kod przełączania jednego aktywnego MID oraz wersjonowany skrypt authoringu osobnego V2. Shader wyłącznie multiplies/lerps istniejący kolor, bez addytywnego wypełnienia i zmian Default Lit; czerń pozostaje czernią. Ochrona jasnych pikseli, blend-in do V2. Legacy przełączany natychmiast; powierzchnia zawsze Legacy.
- [x] Asset utworzony lokalnie z D3D12: VISUAL_FOUNDATION_V2_SAVED, ASSET_AUTHOR_EXIT=0, SHA256 D9AFA66C19389C4D69A2BA6121FDCF40C0A1F4A2F740EC3021DE0741EF2E5820.
- [ ] Cook, rendering w gameplay i A/B.

## 03 — F7 i QA
- [x] Implementacja F7: WIZUALIA / Visual Profiles, tryb A/B, runtime status, edycja kolorów, reset i eksport JSON profilu bez DebugRebuild. Istniejący F7 nadal otwiera sandbox świata zgodnie z dotychczasowym zachowaniem (save na dysku nie jest modyfikowany).
- [x] Kontrolowane wdrożenie (hashy staging/backup/deployed PASS); BUILD_EXIT=0, TEST_EXIT=0. Cztery testy Unreal zwróciły Success (szczegóły niżej).
- [ ] Akceptacja wizualna użytkownika.

## 04–06
Nie rozpoczęto. Wymagają akceptacji poprzedniego etapu.

### Wartości eksperymentalne (startowe, niezatwierdzone wizualnie)
Mode Legacy (domyślnie); Strength 1; NeutralPadding 10 cm; BackRange 150 cm; FrontRange 130 cm; BackgroundTint (0.76,0.84,1); FogStrength 0.65; Desaturation 0.55; ForegroundTint (0.78,0.66,0.90); ForegroundDarkness 0.55; BlendSeconds 0.25 s. Brak nowych oparów. Brak additive lift i nowej pikselizacji.
Zachowanie wszystkich pasm ma pierwszeństwo: prostokątne ściany Y=75 i część płytkich naturalnych ścian mogą pozostać neutralne. Dodatkowa semantyka/geometria wymaga osobnej akceptacji.
Status: kod na feature branch, wdrożony i skompilowany lokalnie, cztery testy techniczne zaliczone. Obraz i pomiar niepotwierdzone. Nie merge'ować.

Wdrożenie: pierwsze polecenie przekroczyło limit Windows przed uruchomieniem; wersjonowany skrypt deploy_visual_foundation_v1.ps1 używa krótkiego launchera i przypiętego SHA. Nie oznacza to błędu implementacji.

### Wynik wdrożenia 2026-11-26
- Kod: 202c3a6c0cb4b60b11e578925eafe8c25ff2ea61, launcher: ae32450203289bbcc18f4b9d5f5c813643db7d33.
- LOCAL_PREFLIGHT, STAGING_HASHES, BACKUPS, DEPLOYED_HASHES: PASS.
- ASSET_AUTHOR_EXIT=0; BUILD_EXIT=0 (AST_V1_18Editor Win64 Development, UE 5.8.1); TEST_EXIT=0 (-nullrhi).
- Sirroh.POI.RoomChains: Success (09:56:17 UTC).
- Sirroh.PWG.SpawnGeometryAudit: Success (09:56:28 UTC).
- Sirroh.PWG.StreamingCost: Success (09:56:31 UTC).
- Sirroh.PWG.VisualProfile: Success (09:56:31 UTC).
- LEGACY_AND_ASD_SHA256=UNCHANGED. Audyt geometrii użył kopii zapisów i nowych seedów testowych; nie wykonywano regeneracji oryginalnego zapisu.
- Uwaga: skrypt wdrożeniowy zakończył się kodem 1 na etapie raportowania, ponieważ nazwa zmiennej $matches kolidowała z automatyczną zmienną PowerShell $Matches; komunikat FAILED=No verified success for SpawnGeometryAudit jest sprzeczny z czterema rzeczywistymi rekordami Test Completed. Zmieniono nazwę kolekcji na $testLines. Nie uruchamiano ponownego wdrożenia; log źródłowy zachowano bez zmian.
- Logi: https://drive.google.com/drive/folders/13qsCtlcg4qhONRFqdunV4PoaJW-JGff7 (Saved/ChatGPT/VisualFoundation_20261126; author.log, build.log, test.log, result.txt; backup_202c3a6).
- Test ręczny: uruchomić nowo skompilowany edytor, wczytać istniejący świat, w podziemiach F7 → WIZUALIA / Visual Profiles → Mode = Underground_Stylized. Porównać z Legacy przy tej samej kamerze i świetle. Zmiany parametrów live, reset i eksport; bez DebugRebuild. Restart edytora wymagany po zmianach C++, nie przy przełączaniu profilu. Legacy jest domyślne i stanowi natychmiastowe wycofanie eksperymentu. Przy pełnym rollbacku przy zamkniętym edytorze przywrócić 4 stare pliki C++ z backupu, usunąć wyłącznie dodany test, authoring script i nowy asset po weryfikacji hashy, ponownie skompilować; nie dotykać save.
- Otwarte bramki: rzeczywisty baseline Legacy w gameplay (screeny, Settings, CPU/GPU median/p95), porównanie A/B V2, ręczna obsługa F7, cook i pełne potwierdzenie istniejącego zapisu w rozgrywce. Bez akceptacji nie włączać domyślnie V2, nie rozpoczynać 04–06 i nie merge'ować do main.

## Iteracja naprawcza — 26.11.2026
Użytkownik odrzucił V1 wizualnie (A/B screenshoty, chronione +/-260 cm, mix 1). Backup d07e745fd5a35cbb586a48c379574b2c38f42b3d na backup/visual-foundation-before-semantic-20261126. Main nietknięty.
- [x] Kod: domyślny Underground_Stylized; Legacy nadal dostępne, powierzchnia Legacy. Sekcje tylne 5–9,13–16,33+ oraz maska przekroju 29–32 otrzymują osobne stencil-only proxies 201/202, bez kolizji/cieni/main-pass, z kontrolą widoczności/cullingu i lifecycle chunków. Nie klasyfikujemy geometrii z Y. SolidEarth i pozostałe obiekty nie otrzymują ambient lift. Projekt już ma r.CustomDepth=3; 254 jest używane przez gracza, nie ruszamy go.
- [x] Kod/shader: rzeczywisty gain/contrast, lokalny ambient floor wyłącznie na widocznej ścianie, osobne tint i atmosfera, ochrona jasnych pikseli, gameplay gain/contrast domyślnie neutralne, diagnostyka cyan/magenta/green. Parametry w F7; demo domyślnie. Ambient można wyłączyć. Nie zmieniamy fizycznego światła ani cieni; floor jest artystyczną korektą po tonemap, nie symulacją światła. Proxy obejmują sekcje PWG, nie wszystkie natywne aktory POI.
- [x] Lokalny asset i kompilacja: authoring i staging PASS; build 1 EXIT=6 (C2487, C4458), poprawki f80b083/c696188, build 2 EXIT=0. Stary asset i save zabezpieczone. Nowy SHA256 D736F40A523F9DB74F43F5B12C3C6AC8B4332B0A38E6433922C1698064DC8F34.
- [x] Regresje po build 2: TEST_EXIT=0; VisualProfile, SpawnGeometryAudit, StreamingCost, RoomChains 4/4 Success.
- [x] Real RHI D3D12 A/B i diagnostyka: 6 PNG odczytanych i ocenionych (szczegóły niżej).

- Test renderowania przygotowany w 5712a39: kopia asd jako VF_SemanticProbe_20261126, real RHI, stała kamera, porównanie bez pochodni oraz z BP_AST_Torch i zatrzymanym tickiem jej komponentów. Loguje kamerę i źródła point light; brak zatwierdzenia do czasu odczytu rzeczywistych PNG.
- Logi iteracji: https://drive.google.com/drive/folders/1JwIbANeDUzAwUxo-KC9h5rSparVHxxiV .

### Wyniki renderu semantycznego — 26.11.2026
- Kod wdrożony: c696188a624426779a2370a7b166dd91aa1d60ac. RENDER_EXIT=0; RENDER_CAPTURES=PASS; LEGACY_V1_AND_ORIGINAL_SAVES=UNCHANGED. Użytkownik nie musi tworzyć świata; restart gry/edytora ze względu na C++.
- Wszystkie klatki 1600x900, kamera X=-42506.83 Y=-1840 Z=2161.21, P=-0.06 Yaw=88.54, OrthoWidth=1520. Kopia świata asd, Mountains/Shininglands, warstwa 1. Para bez pochodni: brak pobliskich aktywnych point lights; drobny ruch postaci 1.88 cm w osi X, 0.10 cm w Z. Geometria i kamera nie przebudowywane. Para z rzeczywistą BP_AST_Torch: identyczna pozycja gracza, point light 0.792, zasięg 453.097 cm, CastShadows=1, X=-42135.11 Y=-29.93 Z=1994.34; tick komponentów pochodni zamrożony tylko na potrzeby A/B.
- Render folder: https://drive.google.com/drive/folders/1yvfnpatXbs2i8vZL4_1vHQG4ES84fN2e ; 01 Legacy dark, 02 Semantic dark, 03 masks, 04 ambient off, 05 Legacy torch, 06 Semantic torch.
- Maski rzeczywiste: cyan rear 32.96% kadru, magenta cutaway 53.70%, green pozostałe 13.34%; sylwetka gracza zielona. Pomiar luminancji Rec.709 z PNG, tylna ściana wg cyan maski erodowanej 6px, wyłączono kolumny 430–779 (gracz/pochodnia): mediana Legacy 2.85/255, Semantic 31.93/255, ambient off 19.56/255. W parze z pochodnią poza jej bezpośrednim otoczeniem te same mediany. Cutaway median 0/255 we wszystkich ujęciach. Nie jest to pomiar fizycznego oświetlenia ani wydajności GPU.
- Wynik: zauważalna separacja planów bez globalnego liftu; pochodnia i postać nadal widoczne, źródło światła z CastShadows=1 zachowane. Ambient off nadal wzmacnia istniejące niezerowe światło; nie jest automatycznym blackoutem. Przy zerowej składowej sceny wyłączenie ambientu pozostawia czerń. Widać ostrą pionową granicę tła po prawej (w Legacy również istnieje, ale jest mniej widoczna); wymaga oceny, nie ustalono przyczyny. Brak pełnej weryfikacji natywnych POI, przejść pasm, translucencji ognia, obsługi F7 przez człowieka, cook i kosztu GPU proxy. NeutralPadding nie kontroluje już maski, FrontRange nie wpływa na nowy shader; pozostawione dla kompatybilności. Preset demo przyjęty eksperymentalnie.
- [ ] Akceptacja wizualna użytkownika; ocena granicy tła, dodatkowe sceny i GPU timings. Etapy 04–06 nie rozpoczęte, bez dekoracji i bez merge do main.
