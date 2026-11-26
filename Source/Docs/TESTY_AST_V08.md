# Test ręczny v0.8

Uruchom ponownie edytor po kompilacji, otwórz L_POI_Workshop_AST i naciśnij Play. Testów gry agent nie wykonywał.

1. **Przeskok i bezczynność:** złap platformę, przeskocz na drugą, puść wszystkie klawisze. Postać powinna pozostać w spokojnym zwisie. Następnie sprawdź ruch w obie strony, Spację (wejście) i C (puszczenie). Powtórz blisko lewego i prawego końca platformy.
2. **Tylna krawędź:** aktywne mają być przód i dwa boki. Tył nie powinien oferować chwytu.
3. **Tło i stopy:** spróbuj wejść w ścianę tła. Przy platformie z AST Hang=Automatic porównaj zwis bez ściany za nogami i ze ścianą. Można też wymusić Braced/Free dla porównania; animacje i IK wykonuje oryginalne AST.
4. **Kolejność rysowania:** umieść platformę bliżej kamery niż postać. Jej grafika nie powinna zasłaniać postaci. Sprawdź także włosy i wyposażenie. Mechanizm dotyczy grafik używających M_KitImageFit, a nie każdego obiektu świata.
5. **Podwójne S:** stojąc na platformie OneWay, dwukrotnie naciśnij S w odstępie do 0,3 s. Postać ma opaść przez bieżącą platformę; platformy niżej nadal powinny podpierać. Jedno S nie uruchamia przejścia przez platformę. Solid celowo nie przepuszcza.
6. **Ponowny chwyt:** po podwójnym S, w trakcie spadania naciśnij Spację w zasięgu krawędzi. Bez nowego naciśnięcia postać nie powinna natychmiast ponownie złapać tej samej platformy.
7. **Coyote time:** zejdź z krawędzi i naciśnij Spację tuż po utracie podparcia (do 0,2 s). Powinien nastąpić normalny skok. Po dłuższej przerwie nie powinien. Nie ma dodatkowego skoku po zwykłym skoku ani po celowym opadnięciu przez platformę.

Jeśli przeskok nadal się zapętla, zapisz: kierunek, wciśnięte klawisze, czy po puszczeniu klawiszy pętla trwa, oraz ustawienia obu platform (Size, Scale, AST Hang, Collision). Krótkie nagranie tej jednej próby będzie najbardziej przydatne.
