# Pushable v20 — usypianie fizyki

Oryginalna pętla AST czekała 0.2 s, a następnie wyłączała symulację po pojedynczym odczycie prędkości liniowej < 1 cm/s. Nie uwzględniała obrotu ani czasu stabilności. To mogło zamrozić obiekt podczas przechyłu.

Po zmianie puszczony obiekt pozostaje fizyczny. O jego uśpieniu decyduje Chaos, uwzględniający ruch bryły, zamiast pojedynczej próbki prędkości AST. Uśpienie nie zmienia obiektu w nieruchomą bryłę: fizyczny kontakt lub impuls może go wybudzić. Nie jest wymagany kontakt całej powierzchni z ziemią — obiekt może też naturalnie oprzeć się na innej przeszkodzie.

Podczas chwytu symulację wyłącza AST PRZED przyłączeniem do postaci. Puszczenie ponownie ją włącza. Ikony i triggery chwytu są przywracane od razu, bez czekania na zatrzymanie. Zachowano ograniczenie ruchu w głębi sceny oraz naturalne opadanie/przechył w płaszczyźnie gry.

## Test użytkownika
1. Uruchom grę ponownie. Pchnij ławkę/stół na przeszkodę, aż stanie ukośnie, i puść. Powinien móc dokończyć opadanie lub oprzeć się fizycznie, zamiast zostać zamrożony przy pierwszym kontakcie.
2. Po uspokojeniu wbiegnij w niego szybko z obu stron. Sprawdź, czy ponownie się porusza.
3. Uderz bronią, która rzeczywiście przekazuje impuls fizyczny. Sam komunikat obrażeń bez kontaktu/impulsu nie porusza bryły.
4. Ponownie złap E zarówno obiekt stojący, jak i niedawno puszczony. Sprawdź pchanie, ciągnięcie i spację do wspinania.
5. Powtórz dla skrzyni, beczki i sześcianu AST. Nie trzeba publikować POI ponownie.

Test automatyczny sprawdza: fizyka po puszczeniu, sen bez wyłączenia symulacji, wybudzenie impulsem, wyłączenie na czas chwytu i ponowne włączenie po puszczeniu. Rzeczywiste kolizje z graczem i animacje pozostają do testu ręcznego użytkownika.

Backup: Saved/POICheckpoints/Before-v20-push.zip.
Checkpoint zawiera też zmieniony oryginalny PullPushObject.uasset: Saved/POICheckpoints/POI-v20-push-sleep.zip.

Weryfikacja końcowa: kompilacja Build-v20.log — Succeeded. Kontrola grafów potwierdziła brak ścieżki zamrażania po spadku prędkości i wyłączenie symulacji przed attachmentem. Sirroh.POI.PushSupport — Success dla beczki, skrzyni i ławki: podparcie, sen z aktywną symulacją, wybudzenie impulsem, chwyt i ponowne puszczenie. Test czeka na zakończenie asynchronicznej kompilacji meshy przed badaniem fizyki. Zachowanie przewracającego się stołu i zderzenie z graczem wymaga testu użytkownika.
