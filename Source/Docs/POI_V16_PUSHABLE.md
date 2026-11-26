# Pushable v16 — chwyt, ciągnięcie i zwalnianie

## Zmiany
- Oryginalny BPC_Interaction AST aktualizuje geometrię chwytu również przy zerowej prędkości. Nie trzeba ruszać, aby uruchomić pierwszą aktualizację IK. Pozostawiono osobny próg interpolacji obrotu podczas ruchu.
- Sirroh rozpoznaje faktycznie przyłączony pushable niezależnie od ogólnego stanu interakcji. Kierunek patrzenia jest zapamiętywany przy chwycie; przeciwny input przesuwa postać tyłem.
- Spacja zwalnia obiekt oryginalną funkcją AST przed obsługą traversal, zarówno w kontrolerze, jak i w producencie wejścia Mover.
- Podczas chwytu ruch postaci jest ograniczony do zapamiętanej płaszczyzny Y. Fizyka pushable ma zablokowaną translację Y oraz obrót X/Z; ruch i opadanie w płaszczyźnie X/Z pozostają możliwe. To nie chroni przed upadkiem w prawdziwą dziurę w podłodze.
- Ograniczenia otrzymują trzy warianty oraz pushable wykryte w opublikowanych POI, również oryginalny sześcian AST. Nie trzeba ponownie publikować pomieszczeń.

## Test ręczny
Uruchom świat z Main Menu, z kontrolerem Sirroh. Sprawdź beczkę, skrzynię, ławkę i sześcian AST.
1. Podejdź z lewej, wciśnij E i nie ruszaj przez 2 sekundy. Dłonie powinny osiąść na obiekcie bez dodatkowego kroku. Powtórz z prawej.
2. Trzymając obiekt, idź A, następnie D, potem ponownie A. Postać powinna zachować zwrot do obiektu i naprzemiennie pchać/ciągnąć bez obrotu obiektu o 180 stopni.
3. Podczas pchania wciśnij spację przy krawędzi możliwej do wspięcia. Obiekt powinien zostać zwolniony przed ruchem postaci w górę. Sprawdź też spację w spoczynku.
4. Pchaj, ciągnij i puść obiekt blisko granicy głębokości tunelu. Nie powinien uciekać w głąb ani w stronę kamery. Sprawdź, czy po puszczeniu nadal normalnie opada na podłogę.
5. Ponownie złap ten sam obiekt po wspinaczce i po puszczeniu — chwyt i kierunek powinny zostać ustalone od nowa.

Nie wykonywano testów rozgrywki przez sterowanie komputerem. Kompilacja, kontrola grafu Blueprint i testy automatyczne nie potwierdzają jakości animacji; służy temu powyższy test. Multiplayer nie był weryfikowany.

## Odzyskanie
Kopia assetów sprzed zmian: Saved/POICheckpoints/Before-v16-push.zip.
Checkpoint źródeł, assetów i DLL: Saved/POICheckpoints/POI-v16-push-input.zip.

Weryfikacja: kompilacja AST_V1_18Editor Win64 Development zakończona powodzeniem; kontrola grafu PUSH_V16_ASSETS_READY; 4/4 testy Sirroh.POI (PushSupport, PublishedLevels, RoomChains, ShaftTerrain) zakończone Success. Testy nie obejmują animacji chwytu ani sterowania w rozgrywce.
