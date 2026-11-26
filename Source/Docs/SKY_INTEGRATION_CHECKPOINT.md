# Niebo PWG — checkpoint integracji

## Aktualny wynik — 8 listopada 2026

Integracja UDS/UDW z grafiką StyleHex SoftFantasy jest wdrożona. Naprawa ograniczenia ruchu podczas ragdolla jest skompilowana i sprawdzona. SkyRecoveryForced.log potwierdza wymuszony TriggerRagdoll, tryb Ragdoll z plane=0, następnie ExitRagdoll i tryb Walking z plane=1. Test zakończył 2200 klatek, kod wyjścia 0. Nie wystąpiły asercje OriginMax ani RenderBatch.NumIndices. SkyRagdollPlaneFix.log zawiera wcześniejszy udany test z naturalnym upadkiem podczas walki.

Ostatnia kompilacja AST_V1_18Editor Win64 Development zakończyła się Succeeded. Pełny zapis znajduje się w Saved/Logs/SkyFinalBuild.txt. Po niej wykonano końcowy test, bez dalszych zmian w kodzie.

Otwarte ograniczenia pozostają przy pełnym cook/buildzie dystrybucyjnym, który nie był testowany, oraz błędzie edytorskiego narzędzia UDS_OpenEditorUtility podczas ładowania przez -game. Poniższe sekcje zachowują historię diagnozy sprzed naprawy.


## Stan z 6 listopada 2026

Mapa gry to `/Game/Levels/L_SirrohGameplay`. Generator tworzy `APWGEnvironmentV2`. W Initialize aktor uruchamia Ultra Dynamic Sky jako zegar i źródło oświetlenia, Ultra Dynamic Weather jako sterownik pogody oraz BP_Stylized_Sky_Lite_SoftFantasy jako warstwę graficzną. StyleHex otrzymuje czas UDS w godzinach i zachmurzenie z funkcji Get Cloud Coverage 0-10; materiały dysków pozostają materiałami StyleHex. Kopuła i projekcja dysków są dopasowane do kamery ortograficznej. Własne światła, mgła i postprocess StyleHex są wyłączone. PWG zachowuje własną perspektywę powietrzną i kanał oświetlenia roślinności. Przy brakujących paczkach kod zachowuje poprzednią panoramę.

Źródła integracji to `Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2.cpp` i odpowiadający nagłówek. Osobny test deweloperski jest w pliku `PWGEnvironmentV2Verification.cpp`. Surowe archiwa są w katalogu projektu. Rozpakowane paczki znajdują się w `Content/UltraDynamicSky` i `Content/StyleHex_Studio`. Katalogi Blueprintów dodano do ustawień cook w DefaultGame.ini.

## Jak wznowić w PowerShell z katalogu projektu

```powershell
$engine = 'D:\Sirro\UE 5.8.1\UE_5.8\Engine'
$project = (Get-Item '.\AST_V1_18.uproject').FullName
& "$engine\Build\BatchFiles\Build.bat" AST_V1_18Editor Win64 Development $project -WaitMutex
& "$engine\Binaries\Win64\UnrealEditor.exe" $project /Game/Levels/L_SirrohGameplay -game -SirrohSkipMainMenu -RenderOffscreen -ResX=960 -ResY=540 -nosound -unattended -PWGSkyQA -log=SkyFinal.log
```

Kontrolowane wymuszenia czasu i pogody dotyczą wyłącznie procesu testowego. Bez przełącznika PWGSkyQA gra korzysta z animacji UDS i losowej pogody UDW. Zrzuty zapisują się w Saved/Screenshots/WindowsEditor. Test sprawdza bezchmurny dzień, noc, burzę, wywołanie przełącznika podziemi i powrót. Pełny cook i samodzielny build dystrybucyjny wymagają osobnego testu.

## Dowody i otwarte kwestie

Build AST_V1_18Editor Win64 Development zakończył się powodzeniem. SkyResume.log potwierdził odczyt czasu i przejście zachmurzenia z 0.0 do 0.8. Na zrzutach potwierdzono dysk słońca i teksturę księżyca w kadrze. Log gry ładowanej przez edytor z -game raportuje błąd narzędzia edytorskiego UDS_OpenEditorUtility. Oryginalny pakiet narzędzia przywrócono z archiwum po próbie ponownego zapisu.

Dłuższe uruchomienia wykazały utratę precyzji transformacji i późniejszą asercję Slate RenderBatch.NumIndices > 0. Identyczna sekwencja jest w Saved/Logs/AST_V1_18_2-backup-2026.11.05-12.04.30.log przy klasycznej panoramie (sky=1). Do diagnostyki porównać z nowym SkyFinal.log, który próbuje zapisać pozycję kamery co 120 klatek. Nie przypisywać przyczyny konkretnej wtyczce na podstawie samego stosu renderera.

## Wynik ostatniego przebiegu

Końcowy kod zbudowano dla AST_V1_18Editor Win64 Development. W SkyFinal.log zapisano wszystkie pięć stanów testowych. Zrzuty potwierdzają widoczny dysk słońca w dzień, teksturowany księżyc w nocy, deszcz i zakrycie słońca podczas burzy oraz ukrycie i ponowne wyświetlenie nieba i opadów po przełączeniu środowiska. Test wywołał przełącznik podziemi z powierzchni, bez przemieszczania postaci do jaskini.

Po tej sekwencji odtworzono wcześniejszą awarię mapy. Przy klatce 960 kamera miała X=809, Z=934 cm, a przy 1080 już X=73 157 569, Z=396 977 316 cm. Szerokość ortograficzna nadal wynosiła 1520 cm. Później wystąpiła asercja Slate. Przy dalszej diagnostyce śledzić jednocześnie transformacje FollowTarget, kapsuły i siatki postaci oraz wejścia do UpdateCamera, zaczynając przed klatką 960. Na obecnym etapie przyczyna skoku nie jest potwierdzona.

## Wznowienie 8 listopada 2026

SkyCameraDiagnostic.log wykazał, że przed skokiem kamery prędkość postaci w osi Y rosła wykładniczo podczas ragdolla, mimo przypięcia kapsuły do płaszczyzny Y=0. Zmieniono PWGSidescrollerInputComponent.cpp i jego nagłówek. Ragdoll chwilowo wyłącza aktywne ograniczenie płaszczyzny. Po wyjściu z ragdolla, traversal i interakcji ograniczenie wraca w aktualnym położeniu postaci. Obsługiwany jest również ragdoll uruchomiony ręcznie. Przejścia działają także podczas edycji tekstu w panelu debug.

SkyRagdollPlaneFix.log potwierdza wcześniejszy pełny przebieg 2200 klatek z poprawką i normalne zamknięcie z kodem 0. Końcowe współrzędne postaci były około X=-693, Y=-3, Z=1454 cm. W tym przebiegu nie wystąpiły błędy OriginMax ani RenderBatch.NumIndices. Błąd edytorskiego narzędzia UDS_OpenEditorUtility przy -game nadal pojawia się w logu.

Końcowa kompilacja jest zapisywana w Saved/Logs/SkyFinalBuild.txt. Bieżący test zapisuje Saved/Logs/SkyRecoveryFinal.log. Parametr -PWGSkyQARecovery dodatkowo przywraca zdrowie postaci i wywołuje ExitRagdoll w klatce 1700, aby sprawdzić ponowne włączenie ograniczenia płaszczyzny. Log zawiera nazwy trybów ruchu i stan plane. Bez flag testowych te wymuszenia są nieaktywne.

Końcowy test SkyRecoveryFinal.log zakończył się prawidłowo po 2200 klatkach, kod 0. W tym przebiegu postać pozostawała Walking, więc dodano do flagi -PWGSkyQARecovery wymuszenie TriggerRagdoll w klatce 1400 przed ExitRagdoll w klatce 1700. Trwa sprawdzenie w SkyRecoveryForced.log. Kompilacja po tej zmianie zakończyła się Succeeded w 7,49 s (SkyFinalBuild.txt). W razie przerwania należy odczytać ten log i sprawdzić mode=Ragdoll plane=0 oraz późniejsze mode=Walking plane=1 i normalne zakończenie.
