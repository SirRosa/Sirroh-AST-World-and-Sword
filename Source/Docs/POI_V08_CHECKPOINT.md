# Checkpoint v0.8 — 2026-11-10

## Wdrożone zmiany
- Przeskok: reset natywnego stanu Hop/Reach po przekazaniu i zakończeniu akcji; punkt docelowy odsunięty od końców spline; układ sterowania zwisu zgodny z kamerą boczną.
- Tło: fizyczna kolizja Solid na mapie, w tabelach i presecie BP_BackWall. Wybór Braced/Free dla chwytu Automatic uwzględnia fizyczne podparcie za nogami, w tym ścianę tła.
- Usunięto aktywną tylną krawędź AST. Przód i dwa boki zachowane.
- Dwa naciśnięcia S do 0,3 s uruchamiają zejście przez aktualną platformę OneWay. Pomijana jest tylko ta platforma i tylko dla tej kapsuły. Nowa Spacja pozwala ponownie szukać chwytu.
- Coyote time 0,2 s korzysta z natywnego impulsu skoku Mover i dotychczasowej prędkości skoku.
- Grafika platform jest maskowana w miejscu sylwetki gracza przez CustomStencil=254. Włączono r.CustomDepth=3. Dotyczy M_KitImageFit.

## Weryfikacja
Kompilacja: Saved/POICheckpoints/Build-v08.log. Aktualizacja materiału i 38 bloków: Assets-v08.json oraz Assets-v08.log. Brak testów gry przez komputer. Ręczne testy użytkownika: TESTY_AST_V08.md. Poprawka zapętlenia wymaga potwierdzenia na zgłoszonym przypadku.

## Odtwarzanie
POI-v08-parkour.zip zawiera Source, Content/POIKit, Config, AGENTS.md, binaria i raporty z zachowaniem ścieżek względem projektu. Przed odtwarzaniem porównaj późniejsze własne zmiany. Kopia sprzed prac: POI-v08-before.zip.
