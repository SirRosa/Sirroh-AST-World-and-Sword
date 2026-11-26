# Visual Foundation — progress

Zakres: pakiet Sirroh AST Visual Foundation AI, kroki 00–03. Gałąź feature/visual-foundation-v1. Main pozostaje bez zmian do akceptacji wizualnej.

## 00 — Audyt i baseline (w toku)
- [x] Przeczytano wszystkie 13 dokumentów i przejrzano 9 referencji; priorytetem jest separacja planów bez podnoszenia czerni.
- [x] Odczytano aktualny AGENTS.md (blob 0d881048627876f265c325563117d10317a6c21f), HEAD main ec277fff3ac25a6dcc6fad75e6ad462dfc05ea89, drzewo ad98d0c68517d9171ad3e62bebc935c2b4b8d2c1.
- [x] Backup backup/visual-foundation-before-20261126 potwierdzony na tym samym commicie.
- [x] Lokalne hashe bez filtrów zgodne z GitHub dla PWGEnvironmentV2.h (23024f79f5d0868cb4f7f2d65446a1e06527e054), PWGEnvironmentV2.cpp (abdfca5365cdab68315a3eccddf66f5ba8edd2b0), SirrohDebugSubsystem.cpp (e03d12e042cad4452d1f288430fa9ff3ab5bb35c).
- [x] Drive: istniejący Content/Python/pwg_depth_atmosphere.py (4238 B), lokalny materiał Legacy (17250 B); lokalny zapis PWGv2_asd.sav dostępny. Nie publikowano save ani referencji.
- [x] Audyt źródeł: jeden GlobalDepthAtmosphere, priority 20; w Legacy parametry z Settings, origin z Pawn, mgła podziemna x0.3. F7 ma osobne cele refleksji, Undo/Reset/JSON; cel bez flagi bWorld nie wymaga DebugRebuild. Generator przy wczytaniu kopiuje zapisane Settings. Strefy podziemi: kod chroni dwa grywalne pasma (-LaneSpacing i 0), ale cały system ma trzy; bezpieczny grading musi chronić również trzecie, kosztem części bliskiego tła. Backdrop Y=75+Bulge (bulge do ok. 240 cm), maska Y=-LaneSpacing-115, SolidEarth Y=min(BackgroundDepth,LaneSpacing+450).
- [ ] Zweryfikować faktyczny graf materiału, runtime i kolejność renderingu: uruchomiono read-only commandlet; logi Saved/ChatGPT/VisualFoundation_20261126.
- [ ] Aktualne screenshoty baseline i pomiary CPU/GPU median/p95; obrazy z pakietu są historyczne, nie stanowią testu A/B.

## 01 — Profile wizualne
- [ ] Transient Legacy / Underground_Stylized, niezależne od zapisów.
- [ ] Resolver, walidacja, stabilna strefa i fallback.

## 02 — Depth Atmosphere V2
- [ ] Osobny asset, jeden aktywny pass, zachowane rzeczywiste światło i czerń.
- [ ] Weryfikacja assetu, cook, rendering i A/B.

## 03 — F7 i QA
- [ ] Integracja istniejącego panelu, reset, eksport, odczyt runtime.
- [ ] Kontrolowane wdrożenie, build, testy resolvera i regresji.
- [ ] Akceptacja wizualna użytkownika.

## 04–06
Nie rozpoczęto. Wymagają akceptacji poprzedniego etapu.
