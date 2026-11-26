# v24 — spokojniejszy kontakt i chwyt ruchomej krawędzi

Zmiany dotyczą zgłoszenia po v23. Mapy i ręcznie edytowane POI nie są przebudowywane. Nie wykonano testów gry przez sterowanie pulpitem.

Weryfikacja końcowa 2026-11-17: kompilacja AST_V1_18Editor Win64 Development — SUCCEEDED; PushSupport oraz MovingTraversal — SUCCESS, bez błędów testów. Archiwum: `Saved/POICheckpoints/POI-v24-contact-and-grip.zip` wraz z manifestem SHA-256. Nie jest to nowa spakowana wersja gry.

## Co zmieniono

1. Zwykłe wbieganie nie przekazuje całego pędu biegu przedmiotowi. Popchnięcie ma budżet 100 N, najwyżej 8 cm/s przyrostu prędkości na kontakt i nie rozpędza przedmiotu w kierunku kontaktu powyżej 15 cm/s. Nie hamuje obiektów rozpędzonych innym źródłem siły. Kolejne trafienia korzystają z czasu, który upłynął, zamiast za każdym razem dostawać pełny impuls. Dla zablokowanej postaci Mover usuwa składową prędkości w stronę przeszkody. Jest to celowa reguła sterowania: resztę uderzenia absorbuje postać/podłoże, a nie idealnie sprężyste zderzenie.
2. Osobny napęd świadomego pchania przez E zachowuje ustawienia v23. Nie zwiększano mas ani nie zamrażano przedmiotów jako sposobu ograniczenia uderzeń.
3. Polityka kontaktu obejmuje wszystkie komponenty postaci, nie tylko kapsułę. Kinematyczne collidery zachowują zapytania i overlap, ale nie uderzają bryłami solvera. Kości symulowane przez PhysicsControl zachowują symulację, lecz poza ragdollem nie przekazują kontaktu solvera do PhysicsBody/WorldDynamic; ruch postaci i kontakt z przedmiotami obsługuje kapsuła/Mover. Oryginalne odpowiedzi są przywracane przy ragdollu i końcu działania komponentu. Wymaga sprawdzenia regresji interakcji z dynamicznymi obiektami i fizycznych reakcji postaci.
4. Pushable dostaje jawny typ kolizji PhysicsBody od BeginPlay, również przed pierwszym E.
5. Przy wyborze krawędzi zapisywana jest rama obiektu z tego momentu. Opóźnione cele AST są przeliczane przez tę ramę na aktualne położenie podstawy, zamiast zakotwiczać stary punkt świata obok przesuniętej beczki. Dotyczy wyboru/hopu przez ASTWithLedge oraz kolejnej akcji na trzymanej krawędzi. Oryginalne animacje i mechanika AST pozostają.

## Weryfikacja

Kompilacja: `Saved/POICheckpoints/Build-v24.log`.
Automatyczne testy: `Sirroh.POI.PushSupport` oraz `Sirroh.POI.MovingTraversal`, raport `Saved/POICheckpoints/POI-v24-tests`.

Rozszerzenia testów: limit popchnięcia przy różnych masach, budżet siły dla 30/60/120 Hz, brak dodatkowego impulsu bez upływu czasu, przesunięcie obiektu między wyborem krawędzi a opublikowaniem celu animacji, zachowanie symulacji animowanego collidera przy wyłączeniu jego kontaktu z przedmiotami, przywrócenie odpowiedzi kolizji. To nie zastępuje testu animacji i kontaktów w rzeczywistym świecie PWG.

## Test dla użytkownika

Uruchom projekt ponownie; nie musisz ponownie publikować POI.

1. Wbiegnij z rozpędu w nieruszaną wcześniej beczkę i skrzynię. Oczekiwane: małe przesunięcie, wyhamowanie postaci, brak wyrzucenia. Przytrzymaj kierunek przez kilka sekund — obiekt nie powinien stale przyspieszać do prędkości sprintu.
2. Powtórz bieg i od razu Space, tak jak w zgłoszeniu. Oczekiwane: dłonie trafiają w aktualną krawędź, a nie miejsce sprzed popchnięcia.
3. Zatrzymaj się, złap krawędź i wejdź na przedmiot. Powtórz na przedmiocie już przesuwającym się oraz przewróconym. Oczekiwane: wspinaczka nie powoduje podskakiwania obiektu; fizyka nadal działa.
4. Sprawdź pchanie/ciągnięcie przez E, puszczenie i ponowny chwyt. To osobny napęd, powinien nadal pozwalać na celowe przesuwanie przedmiotu.
5. Jeżeli masz możliwość wejścia w ragdoll, sprawdź także kontakt ragdolla z przedmiotem po zakończeniu wspinaczki.

Gdy problem pozostaje, krótki film z widocznym początkiem kontaktu jest cenniejszy niż sam zrzut końcowej pozycji. Opcjonalne `poi.PhysicsContactLog 1` pokazuje impulsy od kontrolowanego kontaktu; po krótkiej próbie wyłącz je przez `poi.PhysicsContactLog 0`.

## Niezakończony zakres poprzedniego planu

Nowa widoczność regionów/portali z gradientem, pełne profilowanie wydajności POI oraz pełna walidacja wszystkich przerwań/hopów na dynamicznych podstawach nadal pozostają otwarte. Stara kurtyna nadal nie jest uruchamiana. Nie należy przedstawiać v24 jako zamknięcia całego planu ani jako wizualnie potwierdzonej naprawy wszystkich przypadków fizyki.
