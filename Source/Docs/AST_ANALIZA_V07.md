# Integracja AST — analiza v0.7

Oryginalny komponent: /AST_ParkourSystem/Blueprints/ActorComponent/BPC_ParkourSystem.

## Fakty z grafów źródłowych
- Check Parkour wybiera zachowanie według Parkour State i DoingTraversalAction.
- Select Hang Parkour Type ma podłączone wyjścia Vault i Mantle; wyjście Ledge nie prowadzi do wejścia na platformę.
- Find Air Montage obsługuje chwyt w powietrzu; Find Hop Montage dobiera animację przejścia między chwytami. Obie funkcje korzystają z oryginalnych danych AST, chooserów i PerformTraversalAction.
- CheckLedgeDirection opiera się na prawej stronie spline i kierunku podejścia.
- LedgeMovement, In Corner Check, Out Corner Check i Hop Check należą nadal do oryginalnego komponentu.
- BeamActor pozostaje oryginalnym aktorem AST ze spline i tagiem Beam.

## Zmiany adaptera Sirroh
- Pełny obwód oryginalnych ParkourActor, poprawne normalne każdej strony, długość spline zgodna z rozmiarem fizycznym.
- Podczas wejścia z własnej fizycznej platformy tymczasowo udostępniana jest natywna gałąź Mantle; po wywołaniu przywracany jest typ Ledge.
- W powietrzu wyszukiwana jest przednia krawędź w X/Z; różnica Y między lane nie decyduje o wyborze celu. Zakres głębokości zapytania: 2000 cm w każdą stronę.
- Wybrany aktor, punkt i normalna przekazywane są jako oryginalne S_LedgeInfo do Find Air Montage lub Find Hop Montage. Nie ma własnych animacji, interpolacji wspinania ani zamiennika maszyny stanów AST.
- Braced/Free są w AST wartościami 0/1. Automatic adaptera wybiera Braced dla bryły o fizycznej wysokości co najmniej 120 cm, jeśli ma kolizję fizyczną. Można wymusić właściwy styl ustawieniem AST Hang. To klasyfikacja geometrii, nie zamiennik IK AST.

## Granice weryfikacji
Kompilacja i testy strukturalne nie potwierdzają pełnego przejścia w grze. Chwyt od spodu, wejście, kolejne chwyty, narożniki, podparcie nóg i lane wymagają testu użytkownika według TESTY_AST_V07.md. Nie wykonujemy testów przez sterowanie komputerem.

## Źródłowe połączenia Select Hang Parkour Type
- NewEnumerator1 → Find Vault Location
- NewEnumerator2 → Find Vault Location
