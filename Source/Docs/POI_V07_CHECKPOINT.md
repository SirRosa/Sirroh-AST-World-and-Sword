# POI v0.7 — 2026-11-09

Zakaz testów gry przez sterowanie komputerem zapisano w głównym AGENTS.md. Skrypt obserwatora v0.6 został wyłączony. Nie wykonywano testów interaktywnych.

## Zmiany
- Korekta skalowania obróconej płaszczyzny grafiki przy niejednorodnej skali aktora.
- Cztery oryginalne aktory krawędzi AST i spline dopasowane do wymiarów platformy.
- Styl Braced nie jest już wykluczany przez sam wybór OneWay; Automatic uwzględnia wysokość bryły i dostępność kolizji.
- Wejście z chwytu własnej fizycznej platformy korzysta z natywnej gałęzi Mantle AST. Oryginalna gałąź Ledge nie obsługuje tego przejścia.
- Przednie chwyty w powietrzu oraz cele przeskoku są wybierane według X/Z, niezależnie od lane w zakresie głębokości 2000 cm; wywoływane są oryginalne Find Air Montage i Find Hop Montage.
- Skok spod platformy nie jest konsumowany przez awaryjną próbę biegu po ścianie.

## Walidacja
Build-v07.log: kompilacja Editor Win64 Development.
POI-v07-tests: cztery testy automatyczne Success, w tym akceptowanie kierunków czterech rzeczywistych krawędzi AST i pełna wysokość grafiki przy skali Z=2.4.
Refresh-v07.json: odświeżone komponenty zapisanej mapy, bez zmieniania pozycji platform.

Pełne zachowanie w grze NIE jest potwierdzone. Ręczne testy pozostawiono użytkownikowi: TESTY_AST_V07.md. Analiza źródłowych grafów i zakres adaptera: AST_ANALIZA_V07.md.

Checkpoint: Saved/POICheckpoints/POI-v07-parkour.zip. Zawiera kod, instrukcje, AGENTS.md, POIKit, binaria oraz raporty. Archiwum używa ścieżek względem katalogu projektu. Przed odtworzeniem należy porównać własne późniejsze zmiany.
