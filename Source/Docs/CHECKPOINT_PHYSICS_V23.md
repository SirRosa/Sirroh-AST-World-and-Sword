# Checkpoint v23 — fizyka pchania i ruchoma podstawa AST

To częściowe wdrożenie `PLAN_PHYSICS_TRAVERSAL_VISIBILITY.md`, nie zakończenie całego planu. Mapy użytkownika nie były przebudowywane. Testy gry przez sterowanie komputerem nie były wykonywane.

Stan zweryfikowany 2026-11-17: kompilacja AST_V1_18Editor Win64 Development — SUCCEEDED; aktualizacja assetów — kod wyjścia 0; PushSupport i MovingTraversal — SUCCESS. Końcowy raport potwierdza masy instancji 40 / 60 / 25 kg. Checkpoint: `Saved/POICheckpoints/POI-v23-physics-moving-base.zip`; manifest zawiera SHA-256. To kompilacja modułu edytora, nie nowa spakowana wersja gry.

## Zmiany

- Domyślne masy: beczka 40 kg, skrzynia 60 kg, ławka 25 kg; pozostałe pushable bez nadpisania masy 80 kg. Jawne nadpisania Physics → Mass są zachowywane. Wcześniejsze masy z geometrii wynosiły odpowiednio 1544,88 / 3546,90 / 716,32 kg.
- Pushable uruchamia fizykę w BeginPlay, przed pierwszym E. Zachowuje ograniczenie do płaszczyzny, CCD i limit prędkości depenetracji 200 cm/s. Usypianie pozostaje w solverze.
- Kapsuła Sirroh używa kolizji query dla ruchu Mover. Nie jest już nieskończenie ciężkim ciałem solvera popychającym inne bryły. Kontakt boczny przekazuje ograniczony impuls obliczony z mas obu ciał i ich prędkości względnej. Zmiana prędkości gracza trafia do Mover. Nie dotyczy ragdolla ani kontaktów pionowych.
- Oryginalny AST nadal wybiera chwyt, animacje i IK. Na końcu inicjalizacji chwytu `BeginPhysicalPush` odłącza bryłę od proxy postaci, ponownie włącza jej symulację i uruchamia poziomy napęd siłowy. Proxy przestaje być drugim colliderem. Początkowa skala obiektu jest zachowywana.
- Napęd ogranicza prędkość celu do 200 cm/s i siłę do 80000 kg·cm/s² (800 N). Nie ma napędu pionowego. Gdy obiekt zostaje w tyle, wejście ruchu postaci maleje; przy 12 cm opóźnienia ruch w tym kierunku jest blokowany. Można zmienić kierunek. Utrata chwytu kończy napęd.
- Usunięto zamrażanie obiektu oraz przywracanie zapisanej prędkości z v21. Wspinaczka nie wywołuje już SetSimulatePhysics(false).
- Cele FrontLedge, BackLedge i VaultLedge oryginalnego AST są wiązane z lokalnymi punktami ruchomej podstawy przez natywny Motion Warping. BackFloor pozostaje celem podłoża w świecie, zamiast jechać z przeszkodą.
- Zwis poza animowaną akcją korzysta z natywnej podstawy Mover oraz jej danych w blackboardzie, aktualizowanych po kroku fizyki. Nie dodano niezależnego tickowego teleportowania postaci.
- Krawędzie pushable nie są przełączane na inną ścianę boxu w trakcie trzymania przez gracza.
- PWG nie tworzy starej kurtyny widoczności. Jej promienie i przebudowa siatki nie działają w nowym uruchomieniu świata. **Nowej maski widoczności jeszcze nie ma: głębsze warstwy mogą być ponownie widoczne.**

## Zakres kontroli automatycznej

`Sirroh.POI.PushSupport`: trzy warianty, sześć orientacji krawędzi, fizyka przed E, sen/budzenie, odłączenie od proxy bez przesunięcia, dynamiczna bryła w trakcie chwytu, ograniczenie wejścia przy oporze, koniec napędu po stanie release, bilans pędu i energii funkcji impulsu.

`Sirroh.POI.MovingTraversal`: rzeczywisty Blueprint Sirroh, oryginalne komponenty AST/Mover/MotionWarping, translacja i obrót celu, niezależny BackFloor, brak wyłączenia fizyki, podstawienie podstawy i jej danych do Mover, sprzątanie celu po zakończeniu.

To testy integracji danych i stanów. Nie dowodzą jakości animacji ani stabilności kontaktów w całej rozgrywce. Wyniki końcowe i log kompilacji znajdują się w `Saved/POICheckpoints/`.

## Test ręczny — bez przebudowy Twoich POI

1. Uruchom projekt po zamknięciu poprzedniej sesji, następnie wejdź do świata z Main Menu. Zacznij od świeżo załadowanego obiektu, bez wcześniejszego E.
2. Wbiegnij z obu stron w beczkę, skrzynię i ławkę. Sprawdź, czy reagują przed pierwszym chwytem oraz czy nie wystrzeliwują. Osobno stań na nich i skocz.
3. Naciśnij E według dotychczasowego sterowania AST i pchaj/ciągnij w obu kierunkach. Sprawdź początek chwytu, ręce, skalę obiektu, puszczenie oraz ponowny chwyt. Jeśli obiekt jest zbyt ciężki dla napędu, podaj jego nazwę i masę z Details → Physics; nie zwiększaj na ślepo siły.
4. Pchaj skrzynię w ścianę i drugi przedmiot przez kilka sekund. Gracz powinien zatrzymać się razem z zablokowanym przedmiotem; obiekty nie powinny przechodzić przez siebie. Sprawdź cofnięcie i puszczenie.
5. Podczas pchania naciśnij Space. Napęd ma zakończyć się przed wspinaczką, a obiekt nie powinien zostać podniesiony przez przywiązanie do postaci.
6. Przewróć beczkę/ławkę. Sprawdź chwyt, zwis, przesuwanie, vault i wejście na górę. Następnie powtórz na przesuwającym się i spadającym obiekcie. Obiekt nie może zastygnąć w powietrzu.
7. Będąc w zwisie, sprawdź przeskok na drugi obiekt, puszczenie i ponowne złapanie. Zgłoś osobno przypadki gwałtownego obrotu podstawy — są istotne dla dalszej walidacji.
8. Obejrzyj świat z normalnej kamery: inne lane'y, krajobraz i trawa przed sceną nie powinny być zasłaniane starą kurtyną. Brak zaciemniania dalszych warstw w tym checkpointcie jest znanym stanem przejściowym.

Opcjonalnie `poi.PhysicsContactLog 1` zapisuje do logu boczne kontakty gracza z masą, prędkością względną i impulsem; po krótkim teście ustaw `poi.PhysicsContactLog 0`. Do zgłoszenia dołącz nazwę obiektu, czynność, stronę ruchu i krótki film. FPS porównuj na tej samej trasie i tym samym seedzie.

## Nadal do wykonania

1. Pełna walidacja dynamicznego cyklu AST: hop między różnymi ruchomymi podstawami, zniszczenie celu, gwałtowny obrót, dziedziczenie prędkości przy wszystkich sposobach puszczenia. Aktualny adapter nie jest dowodem zamknięcia tego etapu.
2. Pomiary kontaktów solvera i strojenie profili mas/siły/tarcia. Ruchomy przedmiot uderzający w nieruchomą postać, kontakty pionowe i obciążenie przy zwisie wymagają osobnej kontroli. Obecny model impulsu obsługuje boczne trafienia zgłoszone przez sweep Mover.
3. Nowa widoczność oparta na regionach/portalach, przestrzennej masce i gradiencie, z rozróżnieniem dekoracji oraz zamkniętych drzwi. Stara kurtyna nie jest bazą nowego rozwiązania.
4. Profil Unreal Insights/CSV streamingu, fizyki i POI oraz bezpieczne zarządzanie aktywnością dynamicznych obiektów. Usunięcie kurtyny usuwa konkretny koszt, ale nie dowodzi naprawy wszystkich spadków FPS.

Wznowienie: przeczytać ten plik, logi Build-v23/Assets-v23/Tests-v23 i raport POI-v23-tests. Zmiany natywne są w POIPhysicalPushComponent, POIMovingTraversalComponent, POIPushTraversalComponent, PWGPhysicsContactComponent; połączenie AST w POICorridorTools. Skrypt `Tools/fix_push_v23.py` tworzy backup i aktualizuje oryginalne assety AST bez zapisywania map.

