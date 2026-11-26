# Test ręczny v0.10

Uruchom ponownie edytor po kompilacji. Mapa i grafiki nie zostały zmienione.

1. Rozpędź się w lewo, trzymaj A i Spację, skacz do bliskiej lewej krawędzi przy drugiej platformie nad głową. Chwyt ma preferować kierunek w lewo. Powtórz w prawo. Wybór używa rzeczywistego kierunku ruchu ekranowego oraz dopuszcza przednie i odpowiednio skierowane boczne krawędzie.
2. Wisząc na fizycznej platformie, trzymaj W i naciśnij Spację. Postać ma najpierw próbować wejść na aktualną platformę, tak jak po samej Spacji. Dopiero jeśli AST nie rozpocznie wejścia, adapter spróbuje wyższego chwytu. Ponowne naciśnięcie podczas trwającej animacji nie powinno jej restartować.
3. Zejdź z platformy, trzymając tylko A lub D. Bez Spacji i W postać ma swobodnie spaść — bez automatycznego chwytu, także gdy minie krawędź blisko ręki.
4. Powtórz spadanie i przytrzymaj W lub Spację, będąc w zasięgu krawędzi. Chwyt powinien być dostępny. Zwolnienie obu tych klawiszy wyłącza dalsze wyszukiwanie chwytu.
5. Sprawdź podwójne S: samo opadanie nie powinno kończyć się natychmiastowym ponownym chwytem; nowe naciśnięcie Spacji lub W powinno umożliwić chwyt.

Poprawki skompilowano. Testów w grze przez sterowanie komputerem nie wykonano. Przy zgłoszeniu problemu podaj kierunek, trzymane klawisze, czy postać już wisiała, i położenie krawędzi względem postaci.
