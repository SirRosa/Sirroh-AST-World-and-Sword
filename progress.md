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
- [x] Implementacja resolvera, sanitization, fallback, strefa stabilna względem generatora +/- (LaneSpacing + 50 + padding), test jednostkowy Sirroh.PWG.VisualProfile (jeszcze nieuruchomiony).

## 02 — Depth Atmosphere V2
- [x] Kod przełączania jednego aktywnego MID oraz wersjonowany skrypt authoringu osobnego V2. Shader wyłącznie multiplies/lerps istniejący kolor, bez addytywnego wypełnienia i zmian Default Lit; czerń pozostaje czernią. Ochrona jasnych pikseli, blend-in do V2. Legacy przełączany natychmiast; powierzchnia zawsze Legacy.
- [ ] Weryfikacja assetu, cook, rendering i A/B.

## 03 — F7 i QA
- [x] Implementacja F7: WIZUALIA / Visual Profiles, tryb A/B, runtime status, edycja kolorów, reset i eksport JSON profilu bez DebugRebuild. Istniejący F7 nadal otwiera sandbox świata zgodnie z dotychczasowym zachowaniem (save na dysku nie jest modyfikowany).
- [ ] Kontrolowane wdrożenie, build, testy resolvera i regresji.
- [ ] Akceptacja wizualna użytkownika.

## 04–06
Nie rozpoczęto. Wymagają akceptacji poprzedniego etapu.

### Wartości eksperymentalne (startowe, niezatwierdzone wizualnie)
Mode Legacy (domyślnie); Strength 1; NeutralPadding 10 cm; BackRange 150 cm; FrontRange 130 cm; BackgroundTint (0.76,0.84,1); FogStrength 0.65; Desaturation 0.55; ForegroundTint (0.78,0.66,0.90); ForegroundDarkness 0.55; BlendSeconds 0.25 s. Brak nowych oparów. Brak additive lift i nowej pikselizacji.
Zachowanie wszystkich pasm ma pierwszeństwo: prostokątne ściany Y=75 i część płytkich naturalnych ścian mogą pozostać neutralne. Dodatkowa semantyka/geometria wymaga osobnej akceptacji.
Status: kod na feature branch; lokalny asset, kompilacja, testy, obraz i pomiar jeszcze niepotwierdzone. Nie merge'ować.
