# Plan naprawy fundamentów: fizyka, ruchome cele AST, widoczność

Status: plan jest częściowo wdrożony. Bieżący zakres, ograniczenia i testy opisuje CHECKPOINT_PHYSICS_V24.md oraz wcześniejszy CHECKPOINT_PHYSICS_V23.md. Poniższa analiza dokumentuje punkt wyjścia sprzed wdrożenia. Punkt startowy: checkpoint POI-v21-push-traversal.zip. Użytkownik odrzucił zamrażanie celu wspinaczki. Testy rozgrywki wykonuje użytkownik; dozwolone są analizy, commandlety i testy bez sterowania komputerem.

## Cel i kolejność
1. Krótki pomiar bazowy i izolowane przypadki błędów.
2. Spójna fizyka obiektów i kontakt gracz–obiekt, od chwili spawnu.
3. Pchanie/ciągnięcie bez przenikania i teleportowania bryły.
4. Ruchoma podstawa postaci i pełny cykl wspinaczki AST na ruchomym celu; usunięcie zamrażania.
5. Usunięcie starej kurtyny widoczności i wdrożenie nowej architektury.
6. Kontrola kosztu na zapełnionym świecie, kompilacja, checkpoint, instrukcja testów.

Nie dodawać teraz nowych typów pomieszczeń ani kolejnych interakcji. Każdy etap kończyć osobną kompilacją i checkpointem. Nie deklarować pełnego działania na podstawie samej kompilacji lub testu wywołującego SetSimulatePhysics.

## Co ustalono w kodzie
- POIVisibility.cpp: 192 segmenty / 193 synchroniczne LineTrace na aktualizację, TickInterval=0.1, promienie X/Z na Y gracza, ECC_Visibility, zasięg 3500; proceduralna kurtyna umieszczona tuż przed kamerą, aktualizacja wierzchołków i tworzenie tablic przy każdym przebiegu. To około 1930 zapytań/s, ale liczba zapytań sama nie określa kosztu. Nie ma gradientu ani klasyfikacji lane'ów/dekoracji. Jedna płaska zasłona przesłania różne głębokości. Aktualizowanie również położenia kurtyny w 10 Hz może powodować skoki względem kamery.
- Spawn kurtyny: Private/Procedural_World_Generator/PWGv2Generator.cpp; pole SightCurtain w odpowiadającym nagłówku. Obecny przełącznik diagnostyczny: poi.Sight 0/1.
- POIPushTraversalComponent.cpp: ProtectTarget wyłącza symulację celu, Release przywraca zapisane prędkości. To faktycznie umożliwia zawieszenie spadającej bryły. Usunąć ten model, nie stroić go.
- Ten sam komponent przestawia cztery oryginalne spline'y AST na najwyżej skierowaną ścianę bounding boxu co 0.05 s po zmianie obrotu. Testy v21 obejmują sześć osiowych orientacji, NIE potwierdzają chwytu w trakcie obrotu ani dopasowania do rzeczywistego kształtu beczki/ławki.
- AST BPC_Interaction Grab PullPush Object przypina obiekt do proxy postaci. Eksporty Push-v20/21-patched.json zawierają SetRelativeTransform/Location z bSweep=false. Aktualny model nie gwarantuje ciągłej kolizji bryły z otoczeniem.
- v20 usunął wyłączanie fizyki po pojedynczym odczycie prędkości liniowej; fizyka jest włączana przez stan Released, a wyłączana w Held przed attachmentem. Brak jeszcze potwierdzonej inicjalizacji dynamicznej bryły przy każdym spawnie. Objaw braku fizyki przed pierwszym E jest zgłoszony przez użytkownika; sprawdzić CDO, instancje opublikowanych map i BeginPlay.
- POIShaft::UpdateDormancy wyłącza tick aktorów/komponentów i rendering dalekich POI, ale nie symulację fizyczną. Obiekty aktywne fizycznie nie mogą polegać na wyłączonym komponencie aktualizującym stan. Uwzględnić to przy nowej integracji i profilowaniu.
- UE 5.8 lokalnie ma MotionWarpingComponent::AddOrUpdateWarpTargetFromComponent(... bFollowComponent ...), Mover/MoveLibrary/BasedMovementUtils i przykłady L_BasedMovement, L_PhysicsSimulatedCharacter, Light/Medium/HeavySimulatedBox. Samo istnienie API nie oznacza gotowej integracji AST.
- AST SetWarpTargets w eksporcie BPC_ParkourSystem używa AddOrUpdateWarpTargetFromLocationAndRotation, czyli celów świata. Nie wystarczy zmienić pojedynczego celu mantle: sprawdzić także zwis, hop, drop, IK i podstawę po wylądowaniu.

## Etap 1 — pomiar i kontrakt fizyki
Przygotować małą mapę diagnostyczną z właściwym pawnem/controllerem Sirroh: podłoga PWG i zwykła podłoga testowa, skrzynia/beczka/ławka/sześcian AST, po trzy jawne masy, przeszkoda i spadek. Bez dekoracji, widoczności i losowej generacji. Żadnych testów przez sterowanie pulpitem.

Zebrać rzeczywiste wartości z instancji: typ Mover i backend, root collision, masa/inercja, materiał fizyczny, damping, CCD, ustawienia depenetracji, solver, liczba colliderów na bryłę, kolizje dzieci/triggers, ustawienia PWG floor i gotowość collision cookingu. Nie zakładać, że parametry CharacterMovementComponent sterują używanym Mover.

Hipotezy do sprawdzenia, nie ustalone przyczyny: kinematyczna kapsuła jako źródło praktycznie nieograniczonego impulsu; podwójne przekazywanie siły; przenikanie po teleportowanych transformach; zbyt duża depenetracja; brak właściwej podstawy Mover; cienka/asynchronicznie gotowa kolizja terenu; błędny stosunek mas; wiele colliderów tej samej bryły.

Rejestrować kontakt: prędkości względne, normalna, impuls, głębokość penetracji, masy, stan AST, podstawa gracza, symulacja obiektu. Pomiary CPU/Game/Physics/Render/GPU i hitchy w Unreal Insights/CSV. Porównać ten sam seed, trasę i kamerę z poi.Sight 0/1 oraz bez interakcji. Rozdzielić stutter 10 Hz od spadku średniego FPS i spike'ów streamingu.

## Etap 2 — jeden model kontaktu gracza z fizyką
Docelowo obiekty są dynamiczne od spawnu, mogą zasypiać i budzić się przy kontaktach/impulsach bez wcześniejszego E. Krawędzie i triggery mają wyłącznie query, nie dodają sił. Jawne profile masy/materiałów; CCD selektywnie dla szybkich brył, nie jako zamiennik poprawnego ruchu.

Najpierw mały eksperyment zgodności istniejącego AST z natywnym physics-based Mover. Jeśli backend już jest fizyczny, naprawić konfigurację zamiast dodawać drugi solver. Jeśli jest kinematyczny, ocenić migration spike: chód/skok, root motion, AST, lane'y, inventory. Nie przepinać całego projektu bez tego sprawdzenia.

Jeżeli migracja fizycznego Mover łamie AST, wybrać świadomie kontrolowaną integrację kontaktu obecnego Mover: jeden kanał przekazywania impulsu do bryły i reakcji do prędkości gracza, oparty na względnej prędkości normalnej i masie efektywnej, z limitami energii/penetracji. Wyłączyć konkurujące automatyczne siły. Nie dodawać losowego odrzutu ani stałej siły niezależnej od masy. Decyzję backendu zapisać po pomiarze i próbie, nie na podstawie nazwy komponentu.

Kryteria: cięższa bryła wyraźniej hamuje gracza i mniej przyspiesza; brak katapultowania przy sprincie i staniu; brak stałego dodawania energii w spoczynku; impuls budzi obiekt; postać nie musi najpierw naciskać E. Nie wymagać płaskiego przylegania całego mesha do podłoża — stabilne oparcie o przeszkodę jest poprawne.

## Etap 3 — pchanie bez przenikania
Zachować oryginalne wybieranie interakcji, animacje, IK, strony i warpy AST. Zastąpić transport obiektu przez unswept attachment.

Preferowany model: dynamiczna bryła sterowana ograniczonym napędem/constraintem do punktu chwytu. Napęd ma ograniczenie siły i prędkości; nie przenosi przedmiotu przez ściany. Cel chwytu porusza się tylko o dystans dozwolony przez kolizje gracza i bryły; opór ogranicza tempo postaci. Brak natychmiastowego snapu i kumulacji ogromnej energii sprężyny.

Zweryfikować oryginalny AST proxy: pozostawić tylko funkcję potrzebną do zapytań/IK lub usunąć jego fizyczne dublowanie bryły. Obiekt–obiekt i obiekt–podłoga zawsze zachowują kontakt. Ignorowanie kapsuły wyłącznie w ściśle określonym stanie i tylko dla wybranej pary, jeżeli wymaga tego animacja; nigdy globalnie dla PhysicsBody. Zwolnienie bez skoku transformu i sztucznego impulsu.

Kryteria: skrzynia zatrzymuje się na ścianie i drugiej skrzyni, nie nakłada się na nie; próba pchania ciężkiej bryły ogranicza gracza; przerwanie animacji, skok i utrata kontaktu sprzątają napęd; masy i reakcje nie zmieniają się po pierwszym E.

## Etap 4 — wspinaczka na ruchomym celu
Usunąć freeze/restore zapisanych prędkości z v21 wraz z wdrożeniem zastępstwa. Celem jest nadal dynamiczna lub napędzana platforma. Gracz nie może zatrzymywać spadania przez samo rozpoczęcie animacji.

Wprowadzić wspólny kontekst kontaktu: słaba referencja do komponentu fizycznego, stabilne ID krawędzi, parametr spline'u, lokalny punkt/normalna uchwytu, poprzedni/aktualny transform podstawy, prędkość punktu (liniowa + omega × r), faza AST i stan pary kolizyjnej. Jeden właściciel aktualizacji; nie korygować pozycji równocześnie przez attach, tick teleport i Mover.

- Mantle/vault: cele względne do komponentu przez natywne Motion Warping; sprawdzić semantykę offsetu. Każdy cel osobno: front, back, landing; podłoże za obiektem może należeć do innego komponentu.
- Zwis/przesuwanie: odtwarzać światowy punkt z lokalnego uchwytu po aktualizacji podstawy; delta podstawy przez mechanizmy Mover, nie niezależne SetActorLocation.
- Hop: źródło i cel mają osobne lokalne ramy; przeliczyć cel w trakcie lotu zgodnie z AST. Nie przełączać losowo ID chwytu przez zmianę najwyższej ściany boxu.
- Stanie: poprawna ruchoma podstawa, aktualizacja translacji i obrotu, brak nadmiernego nacisku postaci na lekką bryłę.
- Puszczenie: dziedziczenie prędkości punktu podstawy dokładnie raz. Nie przywracać starej prędkości całej platformy.
- Kolizje: rozwiązać konflikt root motion z kapsułą i konkretną bryłą; zachować kolizje ze światem i sąsiednimi obiektami. Para wraca do kontaktu bez penetracji. Reakcja masy postaci przy zwisie powinna być jawna i ograniczona, bez dwukrotnego naliczania ciężaru.
- Nagły obrót, zniszczenie/unload celu, śmierć, utrata uchwytu: kontrolowane puszczenie, bez softlocku i pozostawionych wyjątków kolizji.

Krawędzie: trwałe lokalne spline'y na odpowiednich stronach mesha + filtrowanie dostępności względem grawitacji i kontaktu. Nie przenosić aktywnie trzymanej krawędzi na przeciwną ścianę. Dla beczki/ławki nie traktować testu bounding boxu jako potwierdzenia zgodności z geometrią. Umożliwić ręczne nadpisanie punktów/typów AST w templatce.

Wydajność: aktualizować kontekst aktywnego gracza/celu w kroku właściwym dla Mover/fizyki; detekcję kandydatów ograniczyć przestrzennie. Śpiące i odległe obiekty bez ciągłego przebudowywania spline'ów. Kilka transformacji aktywnego celu nie jest samo w sobie powodem rezygnacji ze śledzenia; rzeczywisty koszt sprawdzić profilem.

## Etap 5 — nowa widoczność, bez kurtyny przed kamerą
Usunąć APOIVisibility/SightCurtain ze ścieżki generacji oraz stare rysowanie proceduralnej kurtyny. Nie próbować naprawiać modelu przez zwiększanie liczby promieni lub dodanie samego blur.

Rozdzielić trzy rzeczy: widoczność pomieszczeń/warstw, rzeczywistych obiektów gameplay i dekoracji głębi. Lane to pozycja ruchu, nie oddzielny świat widoczności. Równoległe lane'y w tym samym otwartym tunelu nie mają znikać.

Źródło widoczności: przestrzenne regiony tuneli/pomieszczeń oraz portale drzwi/szybów z danych PWG/POI. Otwarty portal przepuszcza widoczność, zamknięte drzwi blokują dalszy obszar; górne/dolne warstwy oddziela teren. W otwartej przestrzeni region może obejmować kilka lane'ów. Przeszkody dynamiczne uwzględniać lokalnie, tylko gdy faktycznie zasłaniają; nie każda dekoracja jest occluderem.

Prezentacja: GPU maska/granica zacienienia zastosowana do odpowiednich fragmentów świata, a nie pełnoekranowa nieprzezroczysta płaszczyzna. Preferowany prototyp: regiony/portale + przestrzenna maska w niskiej rozdzielczości, próbkowana z pozycji świata (SceneDepth reconstruction lub wspólna funkcja materiałowa). Jawna klasyfikacja tła/foreground/gameplay, sprawdzenie istniejących stencil values (postać już używa 254) przed użyciem nowych. Krajobraz za sceną i pas trawy przed lane'ami nie mogą być automatycznie wycinane tą samą maską. Przezroczyste materiały wymagają osobnej weryfikacji; SceneDepth nie rozwiązuje ich automatycznie.

Gradient: kontrolowana szerokość przejścia w jednostkach świata, płynne przygaszanie do bardzo ciemnej brudnej zieleni. Nie pokazywać szczegółów zamkniętego pokoju tylko dlatego, że blur przekroczył ścianę. Oddzielić płynny ruch prezentacji co klatkę od rzadszej/zdarzeniowej aktualizacji topologii. Unikać opóźnionego śladu widoczności przy zmianie lane'a/kamery.

Wydajność: topologia aktualizowana po generacji, zmianie drzwi i przejściu regionu; lokalne maski z ograniczonym budżetem i buforami używanymi ponownie. Żadnych przebudów całej siatki i setek synchronicznych promieni co tick. Ocenić koszt GPU, rozdzielczość, overdraw i pasmo; postprocess też nie jest darmowy.

Kryteria: widać wszystkie odsłonięte lane'y, krajobraz i foreground; nie widać warstw przez sufit/podłogę ani pokoju za zamkniętymi drzwiami; drzwi otwierają widok; przejścia mają gradient i nie skaczą z częstotliwością 10 Hz. Osobno sprawdzić szyb między regionami i kamerę debug.

## Testy i zamykanie etapów
Macierz fizyki: light/medium/heavy × sprint/chód/uderzenie × stojący/przechylony/spadający × przed/podczas/po E. Testy zderzeń obiekt–obiekt, stanie na obiekcie, zsuwanie z krawędzi, cienka podłoga PWG, 30/60/120 FPS. Rejestrować penetrację, maksymalne prędkości i energię; limity dobrać do faktycznych jednostek i mas projektu.

Macierz traversal: podstawa przesuwana/obracana/spadająca, wszystkie strony chwytu, zwis/hop/vault/mantle/drop, niszczenie celu. Brak zmiany SimulatePhysics przez wspinaczkę jest warunkiem zaliczenia.

Macierz widoczności: wszystkie lane'y, foreground/background, drzwi otwarte/zamknięte, powierzchnia/podziemia/szyb, poruszanie i zoom. Użytkownik wykonuje test wizualny, agent analizuje raporty i ślady wydajności.

Przed każdym checkpointem: kompilacja odpowiedniego targetu, testy dotyczące zmiany, backup zmienianych assetów AST, dokumentacja migracji istniejących instancji. Nie resetować ręcznie zbudowanych POI. Oznaczyć jasno: potwierdzone automatycznie / do testu użytkownika / jeszcze niewdrożone.

## Gdzie wznowić
- Source/Sirroh_Reimagineg_v3/Private/POI/POICorridorTools.cpp — patche AST i warianty.
- Source/Sirroh_Reimagineg_v3/Private/POI/POIPushTraversalComponent.cpp — usunąć freeze, zastąpić stabilnym lokalnym kontekstem krawędzi.
- Source/Sirroh_Reimagineg_v3/Private/PWG/PWGSidescrollerInputComponent.cpp i PWGPlayerController.cpp — wejście/Mover/wybór AST; nie zmieniać animacji na podróbkę.
- Plugins/AST_InteractionSystem/Content/Actors/PullPushObject.uasset i .../Blueprints/ActorComponent/BPC_Interaction.uasset.
- Saved/POICheckpoints/Push-v21-patched.json, Push-v21-physics.json, v07-BPC_ParkourSystem.json (ten ostatni to starszy eksport, przed edycją odświeżyć).
- Source/Sirroh_Reimagineg_v3/Private/POI/POIVisibility.cpp i Private/Procedural_World_Generator/PWGv2Generator.cpp.
- Lokalne źródła UE: Engine/Plugins/Experimental/Mover, MoverExamples oraz Engine/Plugins/Animation/MotionWarping.

Pierwsze następne działanie: ustalić rzeczywisty backend ruchu postaci i wszystkie źródła impulsów/zmian transformu, wyeksportować aktualne Blueprinty. Dopiero potem wybrać model kontaktu i rozpocząć etap fizyki. Nie zaczynać od kolejnych mnożników siły lub ponownego zamrażania.


