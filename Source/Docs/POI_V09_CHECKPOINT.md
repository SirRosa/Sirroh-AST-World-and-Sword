# Checkpoint v0.9 — 2026-11-10

Przywrócono M_KitImageFit: Masked/TwoSided/Unlit, ImageTexture alpha bezpośrednio do Opacity Mask, dopasowanie UVRect i Tint zachowane. Całkowicie usunięto węzły SceneTexture maskowania z v0.8. Nie zmieniano ani nie usuwano przypisanych PNG ani geometrii mapy.

Weryfikacja bez GUI: Material-v09.json i Material-v09.log. Odtworzenie grafu i zapis materiału zakończone poprawnie, brak komunikatów błędu kompilacji materiału. Brak testów gry. Kod C++ bez zmian; używana jest udana kompilacja v0.8.

Ważne: rysowanie postaci przed platformami nie jest rozwiązane tą poprawką. Usunięto wadliwą próbę z v0.8, by przywrócić obrazy. Użytkownik potwierdził pozostałe zachowania ruchu v0.8.

Aktualna instrukcja: POI_OBRAZY_I_SZYBY_KROK_PO_KROKU.md.
Archiwum: Saved/POICheckpoints/POI-v09-parkour.zip. Kopia materiału sprzed poprawki: POI-v09-before-material.zip.
