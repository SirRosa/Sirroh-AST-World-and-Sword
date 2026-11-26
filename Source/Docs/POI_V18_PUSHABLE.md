# Pushable v18 — skrzynia puszczana po chwycie

AST CalculateBoxTransform miał dodatkową sondę podparcia SphereTraceSingle o stałej długości 85 cm. Brak trafienia wywołuje Release PullPush Object. Wysoka skrzynia ma środek ponad tym zasięgiem. Nowy koniec sondy to Start - (0,0,połowa wysokości mesha w świecie + 35 cm). Uwzględnia skalę; zachowuje ograniczony zasięg i zwalnianie obiektu bez podparcia. Nie zmieniono warunku zwalniania.

W Play Pullpush Interaction kolejność to teraz: przypisanie aktora, ustalenie strony, ustawienie IK, start montażu, wywołanie serwerowe montażu. Ma to zapobiegać pierwszej klatce z poprzednimi danymi dłoni. Mechanika i animacje pozostają oryginalne z AST.

## Test ręczny
Uruchom ponownie grę z Main Menu. Nie trzeba publikować pomieszczeń ponownie.
1. Pushable_U_02_crate: E, poczekaj bez ruchu, następnie A/D. Postać nie powinna od razu puszczać skrzyni.
2. Puść i ponownie złap skrzynię z obu stron, również po wcześniejszym pchaniu beczki. Sprawdź pierwszą chwilę ustawienia dłoni.
3. Powtórz dla Pushable_U_01_barrel i ławki; sprawdź ciągnięcie tyłem oraz zwalnianie spacją.
4. Sprawdź krawędź podłogi: po utracie podparcia obiekt nadal powinien być zwalniany. Zasięg sondy nie jest nieograniczony.

Test automatyczny obejmuje sondę podparcia trzech meshy przy skali 1 i 1.5. Nie potwierdza wyglądu dłoni w animacji ani całej interakcji; testy gry wykonuje użytkownik.

Weryfikacja: kompilacja Build-v18.log zakończona Succeeded; zapis assetów PUSH_V18_ASSETS_READY; sprawdzono połączenie nowej sondy i kolejność IK przed montażem w zapisanym grafie. Test Sirroh.POI.PushSupport zakończony Success.
