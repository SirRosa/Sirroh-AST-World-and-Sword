# v26 — kontakt, chwyt, pięści i wahadło

## Co zmieniono
- Kontakt przy biegu: limit prędkości impulsu 90 cm/s dla mas do 25 kg, malejący odwrotnie do masy powyżej 25 kg. To ograniczenie impulsu, nie wymuszona prędkość. Ciężka skrzynia pozostaje trudna do ruszenia. Ciągły nacisk nadal ma budżet 100 N; nie zwiększono siły pchania E ani nie zmniejszono mas globalnie.
- AST nie puszcza już aktywnie pchanego przedmiotu tylko dlatego, że pomocniczy trace nie znalazł pod nim podłoża. Native drive nadal przerywa chwyt, gdy postać oddali się o ponad 100 cm od punktu zapamiętanego względem przedmiotu. Blokada innego przedmiotu ogranicza wejście ruchu zamiast wymuszać teleport.
- Pięści AST: ograniczony impuls z pomiaru ruchu dłoni, raz na obiekt w sekwencji ataku. Obsługa także w POIWorkshopGameMode. Oknem kontaktu jest obecnie środkowe 15–75% montażu, z progiem ruchu dłoni do przodu; wymaga sprawdzenia na używanych animacjach. Brak impulsu od samego pozostawania dłoni przy obiekcie.
- WeaponHUD: GetText wywołuje bezpieczny odczyt. Przy braku Pawn zwraca pusty tekst, zamiast odczytywać zdrowie z None.

## Gotowy test
Otwórz Content Browser → Content/POIKit → L_POI_Test_Pendulum. Naciśnij Play. Mapa ma sterowanie Sirroh, trzy pushables i ciągle napędzane wahadło. Nie wymaga ponownego publikowania pomieszczeń. Nie zmieniano wcześniejszych map.
1. Sprint w ławkę, beczkę i skrzynię. Porównaj niewielką reakcję lekkich i cięższych obiektów.
2. E: wepchnij jeden obiekt w drugi. Przy zatrzymaniu przez przeszkodę chwyt powinien pozostać, dopóki postać dosięga przedmiotu. Zmień kierunek i ciągnij.
3. Bez broni wykonaj zwykły atak AST pięścią. Sprawdź kontakt z przedmiotem i brak reakcji przy zamachu poza jego zasięgiem.
4. Podejdź do wahadła po prawej: złap kołyszącą się skrzynię, zwisaj z boku, wejdź na nią, puść i ponownie złap. Obserwuj podążanie dłoni i przechylenie pod obciążeniem.
5. E na wiszącej skrzyni: pchanie/ciągnięcie, puszczenie i Space do wspinania. To szczególnie ważny test kontaktu interakcji AST z przegubem fizycznym.
6. Obserwuj Output Log: nie powinien wracać spam Get CurrentHealth z WeaponHUD.

## Dodawanie wahadła do własnego POI
W Content/POIKit/Underground/Actors są BP_Pendulum oraz BP_Pendulum_Test. Przeciągnij asset na poziom. Punkt aktora jest miejscem zawieszenia. Ustaw go nad planowaną skrzynią. Zachowaj skalę (1,1,1) i obrót (0,0,0); długość zmieniaj przez Length (cm).
- Length: odległość zawieszenia do przegubu skrzyni, domyślnie 300 cm.
- Payload Class: domyślnie istniejący BP_Pushable_U_Crate, z oryginalną interakcją i krawędziami AST. Skrzynia powstaje dopiero po uruchomieniu gry.
- Payload: opcjonalnie wskaż istniejący fizyczny aktor w tej samej mapie. Ustaw jego górę przy dolnym końcu łańcucha. W tym trybie narzędzie go nie przesuwa i nie niszczy przy usuwaniu wahadła.
- Initial Speed: początkowy impuls ruchu (cm/s), domyślnie 100; ustaw 0, by startować nieruchomo.
- Test Motor: stałe okresowe pobudzanie do testów. W BP_Pendulum_Test jest włączone, w zwykłym BP_Pendulum wyłączone. Test Force Newtons domyślnie 15 N.

Pierwsza wersja ma sześć szarych segmentów wizualnych na jednym sztywnym nośniku i osobną fizyczną skrzynię. Są dwa ciała i dwa przeguby; segmenty nie mają własnych colliderów. To wahadło z napiętym nośnikiem, nie pełna wiotka lina. Nie obsługuje jeszcze owijania, zrywania ani zwisających luźnych ogniw. Obciążenie postaci to na razie 80 kg przykładane do miejsca na skrzyni podczas stania/zwisu. Nie zwiększa masy samej skrzyni.

## Multiplayer i wydajność
Fizyka wahadła działa na authority. Poza nią klient odbiera położenie nośnika (20 Hz z interpolacją), a payload używa replikacji ruchu aktora. Nie jest to jeszcze przetestowany wieloosobowy system predykcji interakcji AST. Testów sieciowych nie wykonano. Przy wielu wahadłach potrzebny będzie budżet aktywności/relevancy; w tej wersji w pobliżu sprawdzane jest obciążenie przez postacie.

## Weryfikacja
Build-v26.log — kompilacja. Assets-v26.log — patch i zapis AST/HUD/Blueprintów wahadła. Tests-v26.log — testy PushSupport i MovingTraversal. Testy obejmują granice impulsów, zasięg chwytu, inicjalizację obu przegubów i wcześniejsze regresje ruchomej krawędzi. Nie zastępują testów animacji, zderzeń i multiplayer w grze. Testy przez sterowanie pulpitem nie były wykonywane.

Poprzednie zadania widoczności i profilowania całego PWG nadal pozostają poza tym checkpointem.

Stan końcowy: kompilacja v26 zakończona powodzeniem. Aktualizacja AST/HUD oraz utworzenie mapy zakończone. MovingTraversal przeszedł. Rozszerzony test PushSupport zgłosił ensure wywołania BeginPlay w przygotowaniu wahadła; poprawiono test na DispatchBeginPlay i ponownie skompilowano. Po tej ostatniej poprawce test nie został powtórzony z powodu kończącego się limitu. Nie oznaczamy pełnego przebiegu v26 jako zaliczonego. Następny krok: uruchomić automatyzację PushSupport i MovingTraversal oraz ręczne testy opisane powyżej.
