# Checkpoint v0.10 — 2026-11-10

Zmiany w kontrolerze Sirroh:
- Chwyt w powietrzu wymaga trzymania Spacji/W (lub przycisku skoku gamepada). Wyłączono konkurencyjny automatyczny Air Parkour Check AST w trybie Sirroh, aby nie przechwytywał innego celu bez intencji gracza. Natywne funkcje AST do doboru animacji i ruchu pozostają używane.
- Wybór celu uwzględnia kierunek A/D lub ruchu z rozpędu, przednie i skierowane w stronę podejścia boczne krawędzie. Cel przed postacią ma pierwszeństwo przed neutralnym celem bezpośrednio nad nią; w tej samej grupie wybierany jest bliższy w X/Z. Cele za kierunkiem ruchu są odrzucane.
- W+Spacja podczas zwisu najpierw wywołuje natywne wejście na bieżącą fizyczną platformę. Dalszy chwyt jest próbą awaryjną, gdy AST nie rozpocznie wejścia. Trwająca animacja nie jest restartowana.

Weryfikacja: Build-v10.log. Testy ręczne: TESTY_AST_V10.md. Nie wykonywano sterowania komputerem ani testów gry.
Materiał i mapa pozostają z v0.9; maskowanie postaci przed platformami nadal wyłączone.
Checkpoint: Saved/POICheckpoints/POI-v10-parkour.zip.
