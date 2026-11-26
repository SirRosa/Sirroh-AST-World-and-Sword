# Pochodnia, beczka i pudło — AST

Assety do umieszczenia w levelu:

| Przedmiot | Ścieżka Content Browser |
| --- | --- |
| Pochodnia / Light Template | `/Game/POIKit/CarryProps/BP_POI_Light_Template` |
| Ta sama pochodnia, bazowy Blueprint | `/Game/POIKit/CarryProps/BP_AST_Torch` |
| Beczka | `/Game/POIKit/CarryProps/BP_AST_Barrel` |
| Pudło | `/Game/POIKit/CarryProps/BP_AST_Box` |

Po uruchomieniu edytora: **POI → Przedmioty do podnoszenia**. Wstawiany obiekt pojawia się przy zaznaczonym aktorze, a bez zaznaczenia w `(0,0,100)`. Ustaw pozycję i obrót pochodni na ścianie zwykłymi narzędziami transformacji. Operację dodania można cofnąć. Istniejące mapy i stare dekoracyjne lampy nie są automatycznie podmieniane.

Wstawione przedmioty są nieruchome do momentu podniesienia. AST Loot używa istniejącego kontaktu animacji podnoszenia i przekazuje tę samą instancję do natywnej części `BPC_Inventory`. Przytrzymanie i puszczenie Q korzysta z obecnego ładowania rzutu, tak jak miecz. Po wyrzuceniu działa fizyka. Pudło wymaga dwóch wolnych rąk. Beczka zajmuje lewą rękę i zachowuje oryginalną pozę noszenia pod ramieniem. Żaden z tych przedmiotów nie jest bronią. Pochodnia może zajmować lewą lub prawą rękę (zamiana przez podwójne F) i korzysta z walki jednoręcznej po wejściu w postawę bojową Alt. Przedmioty nie trafiają do slotów plecaka.

`CarryPose` zawiera przeniesione pozy bezruchu, ruchu i kucania, kość chwytu oraz transformację oryginalnego socketu. Natywny most pozy AST nakłada je na animację lokomocji. Pochodnia używa oryginalnej leworęcznej pozy Lyra, a w prawej ręce jej odbicia w przestrzeni szkieletu z korektą pozy referencyjnej; walka przejmuje kontrolę nad ręką w postawie bojowej. Sterowanie ruchem i kucaniem odczytuje komponent Mover.

Ogień używa `/Game/VFX/Fire/Systems/NS_Fire_Master` przez istniejący `ASTFireReactiveComponent`. Widoczny płomień korzysta również z obecnego renderera ASTFireFreeFlame. Ukryta, pozbawiona kolizji sfera o średnicy 6 cm ogranicza jego źródło do końcówki; model pochodni nie jest powierzchnią emisji. Emisja Niagara ma jeden punkt na czubku (`FlameTip`, domyślnie lokalne Z=47 cm). Wyłączono ogień powierzchniowy, zwęglanie całego modelu i automatyczne wygaszanie. Światło tworzy i steruje nim wyłącznie komponent ognia; nie ma oddzielnej lampy pochodni. Beczka i pudło nie mają dodanego ognia.

Kontrola ręczna w istniejącym levelu POI (nie była automatyzowana):

1. Wstaw Light Template na ścianie, beczkę i pudło na podłodze. Zapisz fragment standardową funkcją POI i sprawdź jego wystąpienie w świecie.
2. Podejdź z pustymi rękami i podnieś pochodnię przez E / Loot. Sprawdź chwyt w bezruchu, chodzie i kucaniu oraz ogień wyłącznie na czubku.
3. Wciśnij Alt, wykonaj zwykłe ataki bronią, a potem opuść postawę bojową. Sprawdź powrót pozy noszenia i płynne podążanie ognia.
4. Przytrzymaj i puść Q. Pochodnia ma pozostać zapalona, spaść na podłoże i dać się ponownie podnieść po zatrzymaniu.
5. Powtórz podnoszenie, chodzenie, kucanie i Q dla beczki oraz pudła. Sprawdź oburęczny chwyt pudła i chwyt beczki pod lewym ramieniem. Niesienie pudła blokuje podnoszenie kolejnego przedmiotu.
6. Sprawdź, że zwykły proceduralny miecz nadal można podnieść, użyć do walki i wyrzucić. Jeśli używasz multiplayera, sprawdź te działania również na kliencie.

Kopia zapasowa zmienionych istniejących plików: `Saved/CarryProps/BeforeIntegration`. Raport assetów: `Saved/CarryProps/integration-assets.json`. Log kompilacji: `Saved/CarryProps-build.log`. Raport testów technicznych: `Saved/CarryProps/Tests/index.json`. Testy techniczne nie zastępują oceny wizualnej podczas rozgrywki.

Poprawka po teście użytkownika: ekwipunek w BP_POITest_PWG oraz warsztacie nie czeka na zapis nieistniejącego generatora świata. Ogień punktowy tworzy język płomienia także na źródle bez kolizji. Zaktualizowano wszystkie cztery Blueprinty bez zapisywania map użytkownika.

Weryfikacja: kompilacja AST_V1_18Editor zakończona powodzeniem; Sirroh.Items.CarryProps zakończony Success. Test ładuje rzeczywisty BP_POITest_PWG i sprawdza gotowość ekwipunku, pozy/chwyty, rezerwację i przekazanie przedmiotu, zapis klasy, wyrzucenie z fizyką, lokalizację Niagara/światła oraz utworzenie języka płomienia i geometrii dla pochodni i Light Template. Jest to test bez UI, z NullRHI; nie potwierdza wyglądu w wyrenderowanej rozgrywce ani pełnego przebiegu klawisz E → animacja AST. Kontrola ręczna pozostaje zgodnie z AGENTS.md po stronie użytkownika.

Loot w widoku 2D: wybór i zasięg natywnych przedmiotów używają wyłącznie X/Z. Oś Y nie blokuje wyboru ani nie uruchamia zmiany pasa postaci. Test przeszkód i cel IK są rzutowane na płaszczyznę gracza. Test CarryProps sprawdza selektor każdego z czterech Blueprintów przy Y=-1500/0/+1500 cm oraz odrzucenie przedmiotu poza zasięgiem X/Z.

Poprawki ikony, chwytu i przycięcia (25.11.2026):
- Natywne przedmioty, w tym miecze, używają oryginalnego AST InteractionWidget w przestrzeni ekranu. Widoczność obejmuje również wewnętrzne elementy widgetu i klawisz E; znacznik znika po podniesieniu. Wyboru dokonuje lokalny gracz, z zachowaniem pierwszeństwa bliższej interakcji AST.
- Pochodnia jest ustawiana po złożeniu końcowej pozy postaci. Punkt chwytu trafia w środek widocznej dłoni, a aktor zostaje odłączony od ukrytego szkieletu animacji. Obrót zachowuje overlay noszenia; podczas walki sterowanie przejmuje system broni.
- Przy starcie poziomu przygotowywane są oba chwyty i pozy reakcji Motion Core, również gdy główny rig został już przygotowany przy wyborze stylu. W przebiegu testowym koszt tej inicjalizacji wyniósł 3830,2 ms; nie oznacza to pomiaru czasu rzeczywistej interakcji E. Koszt przeniesiono na inicjalizację poziomu.
- Kompilacja zakończona powodzeniem. Sirroh.Items.CarryProps: Success, obejmuje rzeczywiste widgety Slate, pozycję pochodni po wykonaniu końcowego mostu pozy i gotowość obu chwytów przed podniesieniem. Test działa bez renderowania GPU; wygląd w rozgrywce i odczuwalną płynność należy sprawdzić ręcznie. Map użytkownika nie zapisywano.


Domknięcie poprawek (25.11.2026):
- Poza pochodni jest odbijana w przestrzeni całego szkieletu, z korektą różnicy między pozami referencyjnymi. Poza miecza nie dokłada się do noszenia poza walką. Chwyt, zapis/odtworzenie i wybór strony overlay śledzą slot dłoni, także po zamianie przez podwójne F.
- Materiały postaci GrimModular, GrimModularV7, GrimModularV10 i GrimGirl były Unlit: zmieniono je na DefaultLit, a dotychczasowy kolor przeniesiono do BaseColor. Wyzerowano emisję także w SimpleMan. Kopie oryginałów: `Saved/CarryProps/BeforeLightingFix`; skrypt: `Source/Tools/fix_character_lighting.py`. UEFN pozostawiono bez zmian, ponieważ już reagował na światło.
- PWG: kolizja wypukła powierzchni jest ustawiana po utworzeniu sekcji siatki, co zapobiega jej wielokrotnemu gotowaniu. Materiały klatek trawy są wyszukiwane raz na biome/klatkę, dane ekologii raz na kolumnę, a partie instancji używają kluczy obiektów zamiast generowanych nazw tekstowych. Nie zmniejszano gęstości roślinności.
- Największy zmierzony skok pochodził z budowania bloków wspinaczkowych szybu (ponad 600 ms naraz). Odległe szyby w chunkach doczytywanych asynchronicznie kolejkują bloki i przetwarzają je z budżetem 2 ms na klatkę, minimum jeden blok. Pojedyncze tworzenie Blueprintu może przekroczyć budżet. Początkowe/bliskie fragmenty pozostają synchroniczne; podejście na 600 cm od obrysu szybu kończy kolejkę od razu, aby zachować kompletną trasę. Usunięcie fragmentu anuluje jego kolejkę.
- Uśpione POI nie przeszukują wszystkich komponentów co 0,25 s, jeśli liczba aktorów w poziomach nie zmieniła się. Przedmioty należące do postaci nie są usypiane.

Pomiar `Sirroh.PWG.StreamingCost` jest testem bez UI i GPU, na kopii świata. Mierzy synchroniczny koszt generowania/streamingu i osobno kończy oraz mierzy wszystkie kolejki budowy szybów. Nie jest pomiarem FPS ani pełnego późniejszego ładowania map POI. Test wymaga opcjonalnego fixture w `Saved/SaveGames/PWGv2_CodexStreamingProbe.sav`; bez niego zgłasza pominięcie. Oryginalny zapis użytkownika nie jest modyfikowany.
