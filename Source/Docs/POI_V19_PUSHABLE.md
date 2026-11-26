# Pushable v19 — pierwsza animacja chwytu

## Zmiana
Odświeżanie punktów dłoni w oryginalnym AST przeniesiono również przed PIERWSZĄ animację: Play MoveToInteractionMontage w Checking Interaction PullPushObje. Dotychczas punkty odświeżały się dopiero w Play Pullpush Interaction, czyli przy późniejszym StartPullPushState_Montage. Wcześniejszy etap mógł korzystać z domyślnych/starych światowych pozycji IK. Ponowny chwyt bez przesunięcia obiektu maskował ten problem.

Kolejność: ustalenie aktualnej strony i WarpTransform → wybór montażu podejścia → obliczenie czasu startowego → oryginalne Set Interaction IK dla obu rąk → animacja podejścia. Zachowano istniejącą aktualizację przy właściwym chwycie i podczas ruchu. Nie zmieniano fizyki, sond podparcia ani transformacji obiektu.

## Test użytkownika
1. Uruchom ponownie grę. Złap E obiekt po raz pierwszy, patrząc na pierwszą chwilę animacji. Sprawdź beczkę i skrzynię z obu stron.
2. Pchaj, puść, następnie przesuń obiekt zwykłym wbiegnięciem w jego kolizję. Złap ponownie E. To kluczowy test odświeżenia celu.
3. Przejdź od jednego obiektu do drugiego i złap go z przeciwnej strony. Postać nie powinna wyginać ramion i tułowia do poprzedniego celu.
4. Sprawdź, że pchanie/ciągnięcie nadal działa i skrzynia nie jest od razu puszczana.

Nie trzeba ponownie publikować POI. Nie wykonywano testów gry przez sterowanie komputerem. Kontrola grafu potwierdza kolejność wywołań, ale wynik animacji wymaga testu użytkownika.

Kopia przed zmianą: Saved/POICheckpoints/Before-v19-push.zip.
Checkpoint: Saved/POICheckpoints/POI-v19-push-approach.zip.

Weryfikacja: kompilacja Succeeded (Build-v19.log); Blueprint skompilowany i zapisany; kontrola grafu PUSH_V19_ASSETS_READY potwierdziła wykonanie oryginalnego komunikatu IK przed Play MoveToInteractionMontage. Nie powtarzano testów fizyki, ponieważ nie zmieniono jej kodu. Wynik wizualny pozostaje do sprawdzenia w grze.
