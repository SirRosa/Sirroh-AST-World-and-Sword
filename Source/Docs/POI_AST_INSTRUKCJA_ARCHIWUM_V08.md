# Aktualizacja v0.7

Zacznij od TESTY_AST_V07.md. Standardowa platforma to BP_ClimbPlatform z pełnym obwodem oryginalnych krawędzi AST. Szczegóły integracji: AST_ANALIZA_V07.md. Starsze poniższe opisy osobnych przykładów Mantle dotyczą poprzedniej wersji warsztatu.

# POI z oryginalnymi obiektami AST



## Pierwsza próba



Otwórz Content → POIKit → L_POI_Workshop_AST. Naciśnij Play (Selected Viewport) i kliknij widok gry. Mapa używa BP_POITest_PWG, który dziedziczy po BP_PWGGameMode. Dziedziczy jego PlayerPawnClass i PlayerControllerClass, a kod tworzenia postaci i instalowania wejścia Mover jest wspólny z generowanym światem. Start znajduje się przy PlayerStart. Test korzysta z zapisanej tożsamości postaci.



A/D porusza postacią w trybie bocznym. Spacja najpierw prosi AST o dostępną akcję parkour, a bez odpowiedniej przeszkody działa zwykły skok. Podczas wspinaczki żąda następnej akcji parkour. C podczas wspinaczki żąda zejścia/puszczenia. Obsługę i wybór animacji wykonuje AST. NumPad8 przełącza istniejący tryb porównawczy sterowania AST. Do oceny zachowania w PWG używaj domyślnego sterowania bocznego.



Osobna mapa L_AST_Showcase_PWG zawiera kopię oryginalnego ShowcaseMap z trybem testowym PWG. Służy do sprawdzania istniejących przeszkód AST na postaci używanej przez PWG.



## Co przeciągnąć do mapy



| Obiekt w POIKit | Zastosowanie |

|---|---|

| BP_AST_Ledge | Oryginalny ParkourActor ustawiony jako krawędź chwytana. |

| BP_AST_Mantle | Oryginalny ParkourActor ustawiony jako wejście na przeszkodę. |

| BP_AST_Vault | Oryginalny ParkourActor ustawiony jako przeskok. |

| BP_AST_NarrowPassage | Oryginalny aktor przechodzenia przez wąskie przejście, ze spline wyznaczającym trasę. |

| BP_AST_BalanceBeam | Oryginalny BeamActor do chodzenia po belce z balansowaniem. |

| BP_GASP_Solid / BP_GASP_OneWay / BP_GASP_NoCollision | Bloki z obsługą traversal GASP. |

| BP_SideWall | Fizyczna bryła ściany lub platformy, z możliwością podmiany grafiki. |

| BP_BackWall | Obraz tła bez kolizji. |



Nowe presety AST są dziećmi oryginalnych Blueprintów. Możesz też przeciągać bezpośrednio ParkourActor lub BeamActor z zawartości wtyczki. W Content Browser włącz Show Plugin Content i otwórz AST ParkourSystem → Blueprints → Actor.



ParkourActor oznacza miejsce chwytu. Na ShowcaseMap jego odcinki mają kolizję do zapytań ParkourObject. Fizyczną podporę, na której postać ma stanąć, ustaw osobno. Pozwala to użyć tej samej krawędzi nad platformą z pełną kolizją, platformą OneWay albo przy samym zwisie. Dla podpory użyj BP_SideWall i wybierz Settings → Collision. Solid blokuje z każdej strony. OneWay umożliwia skok od dołu i podparcie z góry. NoPhysicalCollision pozostawia bryłę bez podparcia.



BeamActor ma własną geometrię i kolizję. Jego komponenty noszą tag Beam. AST sprawdza ten tag na podłożu pod postacią i korzysta z interfejsu BPI_BeamActor do ruchu po spline. Oryginalny aktor musi pozostać pod stopami postaci. Dodatkowa szeroka podłoga nad belką może zasłonić ją przed tym wykrywaniem.



## Edycja spline krok po kroku



1. Zaznacz ParkourActor lub BP_AST_BalanceBeam w World Outliner.

2. W panelu komponentów zaznacz Spline.

3. Kliknij końcowy punkt krzywej w widoku i przesuń go narzędziem Move. To zmienia długość całej krawędzi lub belki.

4. Aby wykonać zakręt, dodaj punkt z menu kontekstowego spline i przesuń go. Sprawdź ustawienie stycznych i płynność krzywej.

5. Ustaw cały aktor przy widocznej krawędzi bryły. Obrót aktora określa orientację chwytu. W przykładach warsztatu krawędzie są ustawione w poprzek głębokości platformy.

6. Sprawdź przejście w Play z obu kierunków. Samo poprawne narysowanie krzywej nie gwarantuje, że postać dosięgnie następnego chwytu.



SplineLength w ParkourActor oraz Length w BeamActor określają podział krzywej na odcinki siatki. Długość całej trasy ustalaj punktami spline. Na oryginalnej ShowcaseMap belka ma około 284 cm, a Length wynosi 5.



LedgeType wybiera Ledge, Vault albo Mantle. ClimbStyle rozróżnia Braced Hang i Free Hang. Dobierz styl do tego, czy postać ma ścianę do podparcia nóg, czy zwisa swobodnie. LedgeDepth jest dodatkowym parametrem AST. Na sprawdzonych przykładach ShowcaseMap wynosił 0, więc zachowaj tę wartość jako punkt wyjścia.



## Wejście, środki i zakończenie



W L_POI_Workshop_AST są oddzielne moduły ENTRANCE_01, MIDDLE_01, MIDDLE_02, MIDDLE_03 oraz EXIT_01. Każdy ma 300 cm wysokości. PREVIEW_Entrance_RandomMiddles_Exit składa z nich szyb o wysokości 1500 cm.



Wybierz moduł w Outlinerze. Recipe → Role określa jego przeznaczenie. Entrance występuje na górze, Middle jest losowane osobno dla kolejnych części, Exit występuje na dole. Upper Layer wybiera przejście między warstwami.



| Upper Layer | Przejście |

|---|---|

| 0 | powierzchnia → podziemia |

| 1 | podziemia → jaskinie |

| 2 | jaskinie → lochy |

| 3 | lochy → katakumby |



1. Zapisz własną kopię mapy.

2. Zduplikuj odpowiedni moduł wraz z jego elementami albo edytuj wybrany przykład. Sprawdź hierarchię w Outlinerze. Bloki i oryginalne aktory AST muszą być bezpośrednio podpięte pod aktora POIShaft.

3. Przesuń belki, edytuj oryginalne spline’y i ustaw własne grafiki na blokach. Zachowaj wspólną szerokość, np. 600 cm. Pozycje są liczone względem modułu, którego Z=0 jest dolnym połączeniem.

4. W Recipe ustaw Role i Upper Layer. W Target Table wybierz DT_ShaftModules. W Row Name wpisz unikalną nazwę, np. Middle_Deski_02_Layer0.

5. Kliknij Export Section. Sprawdź Status i zapisz tabelę. Eksport zapisuje bloki POI oraz ParkourActor/BeamActor wraz z punktami spline i ustawieniami AST. Dodatkowe typy aktorów poza tym zestawem nie są obsługiwane przez ten eksport.

6. Wybierz PREVIEW, ustaw Recipe → Upper Layer, Module Table na DT_ShaftModules, wysokość oraz Seed. Kliknij Build Shaft. Zmiana Seed zmienia losowanie. Ponowna budowa z tym samym Seed i tabelą powtarza układ.

7. Naciśnij Play i sprawdź cały przebieg. PlayerStart możesz przed testem przesunąć do miejsca podejścia do podglądu.



Built Roles pokazuje kolejność budowania od dołu. Dla pięciu części będzie to Exit, Middle, Middle, Middle, Entrance. Weight w tabeli ustala szansę losowania danego wariantu. Wartość 0 wyłącza wiersz.



Wysokość sekcji musi wynosić 150–3000 cm. Układ powinien mieć chwyt w zasięgu Step Rise od dolnego połączenia oraz chwyt na wysokości górnego połączenia. Walidacja sprawdza te skrajne punkty. Przejście między nimi i łączenie losowych wariantów sprawdzaj postacią. Najłatwiej zacząć od wspólnej wysokości 300 cm. Przy wysokości całego szybu, która nie mieści pełnych modułów, generator wypełnia pozostały odcinek proceduralnymi stopniami przed wejściem.



Build Shaft na pojedynczym module przebudowuje jego wygenerowane elementy. Export Section zapisuje bieżący układ. Własne poprawki zapisz przed przebudową.



## Własna mapa do testowania



Utwórz mapę i ustaw World Settings → GameMode Override na BP_POITest_PWG. Dodaj PlayerStart nad stabilną podłogą, następnie swoje moduły. Kliknij Play. Zachowaj skalę modułów 1 i zerowy obrót aktora POIShaft. Oryginalne aktory AST wewnątrz mogą mieć własny obrót.



Testowy tryb gry ma aktywne POI Test Mode. Korzysta z ręcznie ustawionych obiektów sceny i wspólnego tworzenia postaci PWG. Jego restart postaci wraca do PlayerStart.



## Nowy świat PWG



Generator z pustym polem Shaft Variants szuka najpierw DT_ShaftModules, a następnie starszej DT_ShaftVariants. Jeśli w Twoim generatorze jest przypisana konkretna tabela, ustaw ją na DT_ShaftModules.



Utwórz nowy świat przez Main Menu. Wybrane dane modułów i seed szybu są zapisywane w planie świata, więc późniejsza edycja tabeli dotyczy nowych światów. Generacja miesza wyłącznie moduły odpowiedniej warstwy i zgodnej szerokości. Przy niekompletnej bibliotece używa poprzedniego proceduralnego układu awaryjnego.



Widoczne wcześniej ogromne obrysy OneWayProximity są teraz domyślnie ukryte. Czujnik nadal wykrywa postać potrzebną do obsługi skoku przez platformę od dołu.



## Poprawka v0.5: kierunek krawędzi i grafiki

AST sprawdza stronę podejścia do spline. Jego lokalna prawa strona powinna wskazywać na zewnątrz platformy, w stronę podchodzącej postaci. Odwrócona krawędź może powodować skok przy ścianie bez chwytu.

Dla prostego bloku bez obrotu: krawędź na prawej stronie (+X) ustaw z obrotem Yaw=-90°, prowadząc spline od +Y do -Y; krawędź na lewej stronie (-X) ustaw z Yaw=+90°, prowadząc spline od -Y do +Y. Długość spline powinna odpowiadać głębokości platformy. W blokach ze zintegrowanym adapterem AST użyj Face Positive X — orientacja jest ustawiana automatycznie.

W warsztacie i tabeli DT_ShaftModules poprawiono kierunki przykładowych krawędzi. Już zapisany świat przechowuje własne kopie modułów: do sprawdzenia poprawionych danych utwórz nowy świat.

Dostarczone obrazy znajdziesz w POIKit/Art/Backgrounds oraz POIKit/Art/Platforms. Wszystkie 20 obrazów zostało zaimportowanych i rozdzielonych pomiędzy bloki warsztatu i dane tabel. Aby zmienić obraz, zaznacz blok, wybierz Settings → Image i wskaż teksturę. Obraz jest rozciągany do rozmiaru bloku; przezroczyste marginesy w PNG pozostają częścią obrazu. Wymiary fizycznego bloku określa Settings → Size.


Sprawdzenie v0.5 w Play: kontroler Sirroh uruchamia wejście na platformę (Mantle), chwyt krawędzi (Ledge) i puszczenie chwytu. Warsztat zawiera przykładowe krawędzie Mantle; dla zwisu ustaw LedgeType=Ledge lub użyj BP_AST_Ledge. Przy wyższej pełnej ścianie dobierz Braced Hang, a przy swobodnym zwisie Free Hang. Testy pojedynczych przeszkód nie zastępują przejścia całego zaprojektowanego szybu.
