# Poprawka crasha eksportu POI — v11 fix

Przyczyna potwierdzona w Saved/Logs/AST_V1_18.log: EditorAssetLibrary.duplicate_asset pozostawiał skopiowany UWorld z flagą Standalone w pamięci. Następne LoadMap kończyło się błędem EditorServer.cpp:2544, World Memory Leaks. Dotychczasowy test commandletu nie sprawdzał tej samej ścieżki kontroli pamięci co zwykły edytor.

Zmiana w Content/Python/poi_authoring.py:

- Kopia jest jawnie zapisywana, a następnie zwalniana przez EditorLoadingAndSavingUtils.unload_packages przed LoadMap.
- Po zwolnieniu sprawdzamy, czy UWorld kopii rzeczywiście zniknął. Jeśli nie, eksport zwraca błąd zamiast otwierać mapę.
- Wszystkie kopie tworzone są, gdy źródłowy level pozostaje aktywny. Dzięki temu ponowny odczyt źródła podczas kopiowania drugiego fragmentu nie tworzy kolejnego świata Standalone.
- Referencje Pythona do świata i aktorów są zwalniane przed zmianą mapy.
- Jeśli przygotowanie kopii nie powiedzie się przed zmianą mapy, skrypt pozostawia bieżącą mapę otwartą i nie wywołuje LoadMap w obsłudze błędu.

Test commandletu: eksport 5 fragmentów i ponowny odczyt transformacji oraz referencji — PASS (Roundtrip-v11-fix.log). Test zwykłego trybu edytora przez ExecutePythonScript z NullRHI, bez Play, bez sterowania komputerem: PASS, POI_V11_ROUNDTRIP_OK w Editor-export-v11-fix.log, brak World Memory Leaks. Obejmuje pięć zapisów, zmiany map, powrót do źródła i zachowanie transformacji oraz wewnętrznej referencji aktora.

Zmiany dotyczą Pythona; DLL i parkour nie wymagają zmian. Kontrola Development Editor: Build-v11-fix.log. Po aktualizacji zamknij i ponownie otwórz edytor, aby przeładować moduł Pythona. Użyj tej samej pozycji POI → Shaft: Surface - Underground → Zapisz wszystkie fragmenty z Background. Nie trzeba odbudowywać ani ponownie tworzyć mapy użytkownika.

Checkpoint: Saved/POICheckpoints/POI-v11-fix-export-crash.zip; skrypt Tools/checkpoint_poi_v11_fix.py.
