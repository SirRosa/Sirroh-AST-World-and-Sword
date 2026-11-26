# Pushable v17 — regresja przy chwytaniu

Poprzednia poprawka v16 zmieniała FirstMoveWhenPush z progu prędkości 15 cm/s (kwadrat 225) na warunek zawsze prawdziwy. Ten warunek uruchamia nie tylko IK, lecz także CalculateBoxTransform i zmianę względnych transformacji kolizji pomocniczej oraz obiektu. Przywrócono próg AST 225. W gałęzi spoczynku wywoływana jest wyłącznie oryginalna aktualizacja IK z zachowaniem sprawdzenia lokalnego sterowania. Nie uruchamia ona sekwencji przesuwania obiektu.

ConfigurePushPlane nie przebudowuje już ograniczenia Chaos, jeśli ustawienia są prawidłowe. Blokada głębokości, ciągnięcie tyłem i zwalnianie przed skokiem pozostają.

## Test użytkownika
1. Uruchom ponownie grę z Main Menu. Zacznij od obiektu, który nie został wyrzucony podczas wcześniejszego testu (najlepiej nowy świat).
2. Podejdź, naciśnij E, odczekaj 2 sekundy bez ruchu. Obiekt nie powinien przeskoczyć ani zostać wyrzucony.
3. Puść obiekt i ponownie złap z tej samej strony. Powtórz 5 razy, następnie z drugiej strony. Sprawdź, czy przed obiektem nie została niewidzialna przeszkoda.
4. Popchnij i pociągnij A/D, puść, podejdź ponownie. Sprawdź beczkę, skrzynię i oryginalny sześcian AST.
5. Sprawdź spację podczas pchania — obiekt ma zostać na podłodze.

Nie trzeba ponownie publikować POI. Już uszkodzone pozycje obiektów w uruchomionej sesji nie zostaną automatycznie cofnięte. Testy gry pozostawiono użytkownikowi; nie wykonywano sterowania komputerem.

Kompilacja: Build-v17.log. Kontrola assetów i grafu: Assets-v17.log, Push-v17-patched.json.
Kopia sprzed poprawki: Saved/POICheckpoints/Before-v17-push.zip.

Weryfikacja: Build-v17.log — Succeeded; kontrola zapisanego grafu — oba progi AST 225, osobna gałąź bezruchu do IK i zachowana ścieżka IK podczas ruchu; Sirroh.POI.PushSupport — Success. Nie jest to potwierdzenie poprawności animacji ani chwytu w rozgrywce.
