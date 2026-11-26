# Pushable v21 — wspinaczka po przewróceniu

## Wdrożony zakres
Oryginalny PullPushObject AST otrzymał komponent POIPushTraversal. Dziedziczą go beczka, skrzynia, ławka i inne dzieci tej klasy. Nie trzeba ponownie publikować map POI.

Komponent wykorzystuje cztery istniejące aktory ParkourActor i ich oryginalne spline. Po obrocie mesha ustawia je na obrysie tej z sześciu ścian lokalnego bounding boxu, która jest najbardziej zwrócona ku górze. Obsługuje bok i spód po przewróceniu; uwzględnia rozmiar i skalę. Nie dodaje 24 nowych aktorów na obiekt. Geometria odpowiada pudełku otaczającemu mesh, więc w przypadku nieregularnej ławki lub zaokrąglonej beczki wymaga sprawdzenia dopasowania w grze.

## Kolizja i ruch celu
Wybrano dopuszczony wariant zatrzymania fizyki na czas wspinaczki. Po rozpoczęciu traversal AST zatrzymuje fizykę wyłącznie aktualnego pushable. Po zejściu, zakończeniu lub przeskoczeniu na inny cel przywraca symulację i zapamiętane prędkości. Nie ma globalnego wyłączania fizyki.

To NIE jest jeszcze pełna obsługa wspinaczki po poruszających się platformach. Obecne SetWarpTargets AST zapisuje pozycje świata. Dalsza wersja wymaga śledzenia lokalnego punktu uchwytu względem celu także podczas zwisu/przeskoków, aktualizacji Motion Warping i zgodnej obsługi Mover oraz kolizji. Samo ignorowanie kolizji nie zapewnia takiego działania.

## Test ręczny
1. Uruchom ponownie świat. Przewróć beczkę na bok, odczekaj, następnie spróbuj vaultu i chwytu/wspinania z obu stron.
2. Powtórz ze skrzynią i ławką, również po obrocie na przeciwny bok lub do góry nogami.
3. Spróbuj złapać obiekt, który jeszcze lekko się przesuwa. Powinien zatrzymać się na czas traversal, a postać nie powinna go wyrzucić i wspinać się w pustkę.
4. Puść krawędź lub przeskocz na inny obiekt. Poprzedni powinien odzyskać fizykę. Sprawdź także zakończenie vaultu i wejście na wierzch.
5. Ponownie pchaj E i sprawdź, że zachowane są ciągnięcie tyłem, zwalnianie spacją oraz wybudzanie po uderzeniu.

Kontrole automatyczne obejmują położenie spline'ów na sześciu orientacjach oraz wcześniejsze testy fizyki. Nie zastępują testu animacji w grze. Nie wykonywano sterowania komputerem.

Weryfikacja: test Sirroh.POI.PushSupport — Success (trzy modele, sześć orientacji górnej powierzchni, testy podparcia i snu/wybudzenia). Końcowa kompilacja po dodaniu zwalniania ochrony przy ragdoll — Succeeded. Nie testowano automatycznie animacji ani chwytu przez gracza. Zatrzymanie celu obejmuje AST w kontrolerze Sirroh; nie deklarujemy pełnej obsługi innych kontrolerów ani ruchomych platform.
