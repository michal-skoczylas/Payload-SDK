# Plan implementacji: strumień wideo z payloadu (DJI PSDK 3.9.2)

Ten dokument to Twój przewodnik krok po kroku. Nie zawiera gotowego kodu — zawiera
konkretne kroki, nazwy API, miejsca w repo do przeczytania i sposoby weryfikacji.
Po każdym kroku **program musi się kompilować**. Jeśli nie działa — nie idziesz dalej.

Zanim cokolwiek zrobisz: **przeczytaj cały dokument**. Potem wracaj do niego krok po kroku.

---

## Mapa SDK — co musisz znać na start

### Kluczowe pliki w repo (przeczytaj je teraz)

| Plik | Po co |
|---|---|
| `samples/sample_c++/platform/linux/manifold2/application/application.cpp` | Kolejność inicjalizacji SDK (DjiCore_Init → rejestracje → DjiCore_ApplicationStart) |
| `samples/sample_c++/platform/linux/manifold2/application/main.cpp` | Menu aplikacji i miejsce, gdzie podepniesz swój pipeline |
| `samples/sample_c/module_sample/camera_emu/test_payload_cam_emu_base.c` | Jak zarejestrować handler kamery payloadu (tylko fragmenty) |
| `samples/sample_c/module_sample/camera_emu/test_payload_cam_emu_media.c` | **Jak DJI wysyła strumień** — złoty wzorzec |
| `psdk_lib/include/dji_payload_camera.h` | API kamery: SendVideoStream, SetVideoStreamType, GetVideoStreamState, RegCommonHandler |
| `psdk_lib/include/dji_typedef.h` | Struktura `T_DjiDataChannelState` (przepustowość kanału wideo) |
| `samples/sample_c++/module_sample/liveview/dji_camera_stream_decoder.cpp` | Przykład użycia FFmpeg (dekoder) w tym samym projekcie |
| `samples/sample_c++/platform/linux/manifold2/CMakeLists.txt` | Twoje źródła muszą być tu dodane do listy |

### Kolejność inicjalizacji (z `application.cpp`)

```
DjiCore_Init(userInfo)
    → (przed ApplicationStart) DjiPayloadCamera_SetVideoStreamType(...)   ← WAŻNE
    → rejestracja handlerów kamery: DjiPayloadCamera_RegCommonHandler(...)
    → DjiCore_ApplicationStart()
    → dopiero TERAZ możesz wysyłać wideo
```

**Uwaga krytyczna** (dji_payload_camera.h:803): `SetVideoStreamType` MUSI być
wywołane PRZED `DjiCore_ApplicationStart`, inaczej użyje domyślnego typu i API
zablokuje się do 10s.

### Który format strumienia? (dji_payload_camera.h:129-138)

- `DJI_CAMERA_VIDEO_STREAM_TYPE_H264_CUSTOM_FORMAT` (0)
- `DJI_CAMERA_VIDEO_STREAM_TYPE_H264_DJI_FORMAT` (1) ← **użyj tego**

PSDK 3.9.2 (patrz README.md w korzeniu repo) wspiera **tylko DJI-H264**.
W tym formacie dron sam recoduje strumień; wymaganie: **po każdej ramce H.264
doklej 6 bajtów AUD**: `00 00 00 01 09 10` (wzorzec: `test_payload_cam_emu_media.c:1363-1366`).
Limit bitrate: 8 Mbps.

---

## Krok 0 — Napraw środowisko, przywróć zielony build

### Cel
Masz działający, kompilujący się projekt, zanim dotkniesz nowego kodu.

### Co zrobić
1. W `CameraManager.cpp` znajdź błąd składniowy (literówkę) w destruktorze — backtick
   po `cap_.release();` to nie jest poprawny C++. Usuń go. **To Twój pierwszy bug do znalezienia.**
2. Usuń artefakty, które nie powinny być w repo:
   - `samples/sample_c/module_sample/custom_realsense/` (puste pliki 0 B)
   - `build/appsrc_temp.h264` (śmieci z poprzedniego kodu)
3. Zbuduj projekt:
   ```bash
   cd build
   cmake ..
   make
   ```
4. Sprawdź, czy w `build/bin/` powstał plik wykonywalny `dji_sdk_demo_linux_cxx`.

### Jak sprawdzić, że działa
- `make` kończy się bez błędów.
- (opcjonalnie) `./build/bin/dji_sdk_demo_linux_cxx` pokazuje menu. Bez drona nie
  dojdzie do "Application start", ale menu widzisz.

### Pułapki
- FFmpeg masz w wersji 8.1.1, a sample DJI zakłada 4.x (sprawdzanie wersji w
  `manifold2/CMakeLists.txt:85-97`). Na razie build przechodzi — oznacz to w
  pamięci jako ryzyko, wracamy w Kroku 9.
- Jeśli `make` nie widzi nowych plików: uruchom `cmake ..` ponownie (glob'y są
  skanowane przy konfiguracji).

---

## Krok 1 — Zrozum, jak wygląda strumień wideo w DJI

### Cel
Zanim napiszesz enkoder, musisz zrozumieć format danych, który ma przyjąć `DjiPayloadCamera_SendVideoStream`.

### Co przeczytać
1. `dji_payload_camera.h:834-844` — kontrakt `SendVideoStream`:
   - argument `len` jest typu `uint16_t`, **max 65000 bajtów na jedno wywołanie**,
   - jedno wywołanie = fragment ramki, NIE cała ramka,
   - przed wywołaniem sprawdź `DjiPayloadCamera_GetVideoStreamState` — nie wysyłaj
     więcej niż `realtimeBandwidthLimit`.
2. `test_payload_cam_emu_media.c:1367-1378` — pętla, która tnie ramkę na kawałki
   `DATA_SEND_FROM_VIDEO_STREAM_MAX_LEN` i wysyła po kolei.
3. `test_payload_cam_emu_media.c:1363-1366` — doklejenie AUD dla formatu DJI-H264.
4. `dji_typedef.h:329-344` — `T_DjiDataChannelState` (pola `realtimeBandwidthLimit`,
   `busyState`).

### Koncepcja, którą musisz zapamiętać (po ludzku)

Strumień H.264 to ciąg **jednostek NAL**. Każda ramka = jeden lub więcej NAL
(zazwyczaj SPS/PPS na początku + wiele fragmentów danych). DJI chce:

```
[ramka H.264 (jeden lub kilka NAL)] [AUD: 00 00 00 01 09 10]
```
a całość wysyłasz **w kawałkach ≤ 65000 B**, w odpowiednim rytmie (np. 30 fps).

### Jak sprawdzić, że rozumiesz
Opowiedz komuś / zapisz w notatniku: "czym jest NAL, czym AUD, dlaczego tniemy ramkę".

---

## Krok 2 — Abstrakcja źródła klatek: `IFrameSource` + `FileFrameSource`

### Cel
Oddzielić "skąd biorę klatki" od "co z nimi robię". Dzięki temu na Macu ćwiczysz
na pliku `drone_vid.mp4`, a na Raspberry Pi podepniesz kamerę RealSense bez
zmiany reszty kodu.

### Co zrobić
1. Utwórz `IFrameSource.h` — czysto abstrakcyjny interfejs (klasa z metodami
   czysto wirtualnymi). Zaprojektuj minimalny kontrakt. Sugestia metod:
   - `bool open()` — otwarcie źródła (zwraca czy się udało),
   - `bool readFrame(cv::Mat &frame)` — wypełnia `frame`, zwraca `false` gdy brak,
   - `void close()` — sprzątanie,
   - `int getFps()` — do planowania rytmu wysyłki,
   - `int getWidth()` / `int getHeight()` — do konfiguracji enkodera.
   - Wirtualny destruktor (`virtual ~IFrameSource() = default;`).
2. Utwórz `FileFrameSource.{h,cpp}` implementujący `IFrameSource` na bazie
   `cv::VideoCapture`:
   - `open()` otwiera plik (`cv::CAP_PROP_FRAME_WIDTH/HEIGHT/FPS` do getterów),
   - `readFrame()` robi `cap_ >> frame`, a na końcu pliku przewija na początek
     (pętla testowa ma działać w nieskończoność),
   - nie mieszaj tu enkodera ani PSDK — to ma być "głupie" źródło klatek.
3. Dopisz źródła do listy plików w `manifold2/CMakeLists.txt` (obok istniejącego
   wpisu `custom_camera`).

### Jak sprawdzić, że działa
Napisz tymczasowy `main` (albo mały test w menu), który otworzy `FileFrameSource`
na `drone_vid.mp4` i wypisze N klatek (rozmiar, wymiary, fps). Wywal test po
sprawdzeniu albo zostaw jako `tests/`. Build musi być zielony.

### Pułapki
- Ścieżka do `drone_vid.mp4` — nie koduj na sztywno `/workspaces/...`. Podawaj
  ścieżkę względną względem katalogu, z którego uruchamiasz program, albo przez
  argument/stałą w jednym miejscu.
- `cv::VideoCapture` nie lubi nagłej zmiany rozmiaru pliku — to nie Twój problem
  teraz, po prostu nie zmieniaj pliku w trakcie.

---

## Krok 3 — `H264Encoder`: prawdziwe kodowanie w pamięci (FFmpeg)

### Cel
Klatka BGR → pakiet(ty) H.264 jako ciąg NAL w pamięci. **Bez plików tymczasowych,
bez `cv::VideoWriter`.** To najważniejszy krok całego projektu.

### Dlaczego tak, a nie jak wcześniej
Twój stary `VideoEncoder` zapisywał do pliku i zwracał 1 bajt — dron nie dostałby
obrazu. FFmpeg potrafi kodować w pamięci: wysyłasz klatkę, dostajesz gotowe NAL.

### Przepływ API FFmpeg (zapamiętaj kolejność)
1. `avcodec_find_encoder_by_name("libx264")` — znajdź enkoder. Jeśli `nullptr`,
   fallback: `avcodec_find_encoder(AV_CODEC_ID_H264)`.
2. `avcodec_alloc_context3(codec)` → ustaw parametry:
   - `width`, `height`,
   - `pix_fmt = AV_PIX_FMT_YUV420P`,
   - `time_base = {1, fps}`, `framerate = {fps, 1}`,
   - `bit_rate` (zacznij np. od 4 Mb/s — mieścisz się w 8 Mbps),
   - `gop_size` (np. 2 × fps — ramka kluczowa co ~2 s; DJI lubi regularne I-ramki),
   - `max_b_frames = 0` (baseline, prostsze dla dekoderów),
   - na końcu `avcodec_open2`.
3. Zamiana kolorów (klatka BGR z OpenCV): `cv::cvtColor(BGR → RGB)`, potem
   `sws_getContext` + `sws_scale` do YUV420P. Potrzebujesz bufora `AVFrame`
   (alokujesz `av_frame_alloc`, ustawiasz `format/width/height`, `av_frame_get_buffer`,
   potem `av_frame_make_writable`).
4. Enkodowanie:
   - `avcodec_send_frame(ctx, frame)` — wysyłasz klatkę,
   - `avcodec_receive_packet(ctx, pkt)` w pętli — odbierasz gotowe `AVPacket`.
     `pkt->data` + `pkt->size` to Twój H.264 (może być pusty, `AVERROR(EAGAIN)`
     to normalny stan, że enkoder czeka na więcej klatek — poczytaj).
5. NAL start code: `AVPacket` z `libx264` zawiera 4-bajtowy start code
   `00 00 00 01` — skopiuj bajty do `std::vector<uint8_t>`.

### Wymagana sygnatura
`std::vector<uint8_t> encodeFrame(const cv::Mat &bgrFrame)` — zwraca NAL-y
bieżącej ramki. Wszystko przez wartość (żadnych gołych wskaźników).

### SPS/PPS
Na początku strumienia (i po każdej I-ramce, jeśli chcesz) enkoder wyśle NAL typu
SPS (`0x67`) i PPS (`0x68`). Najprościej: zaakceptuj je tak, jak lecą — DJI i tak
dostaje całą ramkę z poprzedzającymi je NAL-ami. Nie komplikuj na tym etapie.

### Jak sprawdzić, że działa (WERYFIKACJA, bez PSDK i drona!)
Napisz mały program testowy: pobierz 30 klatek z `FileFrameSource`, zakoduj każdą,
**dopisz wynik do pliku `out.h264`** (binarnie). Potem:
```bash
ffprobe out.h264        # pokaże: H.264, rozdzielczość, fps
ffplay out.h264         # zobaczysz obraz (jeśli masz wyświetlacz)
```
Jeśli `ffprobe` nie widzi H.264 — Twój enkoder jest zły i nie ma sensu iść dalej.

### Pułapki
- Zapomniany `av_frame_make_writable` → zwisające piksele.
- Zła kolejność `send/receive` (musisz drainować `receive_packet` aż do `EAGAIN`).
- YUV420P wymaga parzystych wymiarów — wideo DJI jest parzyste, ale pamiętaj.
- `sws_getContext` każdy raz tworzony i zwalniany = marnowanie CPU; przechowuj kontekst.
- Czytaj przykładowy kod dekodera w `liveview/dji_camera_stream_decoder.cpp:81-233`
  — masz tam wzór obsługi `sws_scale` w tym repo.

---

## Krok 4 — `DjiPayloadSender`: framing DJI-H264 + cięcie na kawałki

### Cel
Klasa, która przyjmuje NAL-y ramki i wysyła je do DJI w poprawnym formacie —
albo (w trybie offline) zapisuje do pliku.

### Co zrobić
1. Utwórz `DjiPayloadSender.{h,cpp}` z metodą np. `bool send(const std::vector<uint8_t> &nalData)`.
2. Wewnątrz zaimplementuj (wzór: `test_payload_cam_emu_media.c:1367-1378`):
   - skopiuj dane ramki do bufora,
   - **doklej AUD** `{0x00,0x00,0x00,0x01,0x09,0x10}` (dla formatu DJI-H264),
   - wysyłaj po kawałkach ≤ 65000 B:
     - `DjiPayloadCamera_SendVideoStream(buf + offset, chunkSize)`,
     - sprawdzaj kod powrotu (`DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS`), loguj błędy.
3. **Stub offline (żeby działało bez drona):** zdefiniuj makro, np.
   `DJI_STREAM_SIMULATE`. Gdy jest ustawione, zamiast `SendVideoStream` dopisuj
   kawałki do pliku `stream_out.h264` (fopen/fwrite). Wtedy cały pipeline testujesz
   na Macu. Na Pi wyłączasz makro i wszystko leci do prawdziwego API.
4. Opcjonalnie (Krok 4B): przed wysyłką odczytaj `DjiPayloadCamera_GetVideoStreamState`
   i loguj `busyState`/`realtimeBandwidthLimit`. Na razie wystarczy log.

### Jak sprawdzić, że działa
- Tryb stub: prześlij przez sendera 30 zakodowanych klatek → `ffprobe stream_out.h264`
  musi zobaczyć H.264.
- Bez drona nie możesz przetestować `SendVideoStream` — to normalne.

### Pułapki
- Nie wysyłaj ramki "na raz" jeśli > 65000 B — DJI odrzuci.
- `SendVideoStream` działa tylko w Linux i po `DjiCore_ApplicationStart`.
- Pilnuj wycieku buforów AUD — AUD to 6 bajtów na ramkę, nic strasznego, ale trzymaj
  porządek (stack/member, nie `new`).

---

## Krok 5 — `VideoStreamPipeline`: wątek łączący wszystko

### Cel
Osobny wątek robiący w kółko: `pobierz klatkę → zakoduj → wyślij`, z rytmem 30 fps
i odrzucaniem zaległości (real-time, nie plik).

### Co zrobić
1. `VideoStreamPipeline.{h,cpp}` — klasa trzymająca:
   - `std::unique_ptr<IFrameSource> source`,
   - `std::unique_ptr<H264Encoder> encoder`,
   - `std::unique_ptr<DjiPayloadSender> sender`,
   - wątek `std::thread` + flagi `running_`.
2. Metody: `bool start()` (otwiera źródło, tworzy enkoder, odpala wątek),
   `void stop()` (flaga stop + `join`).
3. Pętla wątku:
   - `source->readFrame(frame)` — jeśli `false`, odczekaj i próbuj dalej,
   - `encoder->encodeFrame(frame)` → NAL-y,
   - `sender->send(nals)`,
   - odmierz czas, by uzyskać `getFps()` klatek/s,
   - **kluczowa zasada real-time:** jeśli wykonanie cyklu zajęło dłużej niż okres
     1/fps, nie śpij dłużej (albo w ogóle pomiń sen) — kamera ma płynąć, nie nadrabiać zaległości.
4. Wyczyść starą logikę: w `main.cpp` usuń zakomentowane `SendVideoStream` i
   blokującą pętlę z `DjiUser_RunCustomVideoStreamSample`; tam ma być tylko
   start/stop pipeline'u.

### Jak sprawdzić, że działa
- Tryb stub: uruchom pipeline na 100 klatkach → `ffprobe stream_out.h264` → H.264.
- Mierz fps logiem co N ramek (jak w Twoim starym kodzie, ale z prawdziwym payloadem).

### Pułapki
- `std::thread` + PSDK: nie wołaj `SendVideoStream` z menu/wątku głównego blokująco
  — stąd osobny wątek.
- Stop musi być miękki: flaga atomowa + `join()`, inaczej wiszący wątek.
- Wyjątki w wątku "umierają" — łap w pętli i loguj.

---

## Krok 6 — Integracja z PSDK (rejestracja + menu)

### Cel
SDK wie, że Twój payload ma kamerę, ma wybrany format DJI-H264 i uruchamia Twój
pipeline z menu.

### Co zrobić
1. W `application.cpp` (lub w kodzie uruchamianym przed `DjiCore_ApplicationStart`,
   patrz `DjiTest_CameraEmuBaseStartService()` w `test_payload_cam_emu_base.c:1252-1366`):
   - wywołaj `DjiPayloadCamera_SetVideoStreamType(DJI_CAMERA_VIDEO_STREAM_TYPE_H264_DJI_FORMAT)`,
   - zarejestruj handler kamery `DjiPayloadCamera_RegCommonHandler(&handler)`,
     wypełniając minimum: `GetSystemState`, `SetMode`, `GetMode` (wzoruj się na
     `test_payload_cam_emu_base.c` — wybierz tylko niezbędne callbacki, nie musisz
     kopiować wszystkiego).
2. W `main.cpp` dopnij opcję menu `v` → `DjiUser_RunCustomVideoStreamSample()`
   (miejsce już masz) → tworzy `VideoStreamPipeline` z `FileFrameSource` i odpala.
3. Po uruchomieniu, w `DjiUser_ApplicationStart`, dodaj log o stanie kanału:
   `DjiPayloadCamera_GetVideoStreamState` (żeby widzieć, że SDK przyjmuje wideo).

### Jak sprawdzić, że działa
- Build zielony.
- Bez drona: pipeline w trybie stub leci bez błędów.
- Na dronie (później): DJI Pilot 2 pokaże widok kamery payloadu.

### Pułapki
- **Kolejność ma znaczenie**: `SetVideoStreamType` przed `DjiCore_ApplicationStart`
  (dji_payload_camera.h:803). Sprawdź, że wołasz to zanim application start się wykona.
- Handler `GetSystemState` zwraca `T_DjiCameraSystemState` — wypełnij pola
  sensownie (np. `DJI_CAMERA_MODE_PHOTO`), pilot opiera na tym decyzje UI.
- Nie usuwaj działających próbek innych modułów — tylko dodajesz.

---

## Krok 7 — Test offline end-to-end (bez drona, bez kamery)

### Cel
Udowodnić, że cały łańcuch `plik → enkoder → framing → wysyłka` produkuje
poprawny H.264, który odtwarza `ffplay`.

### Procedura
1. Zbuduj z `-DDJI_STREAM_SIMULATE` (lub makrem w kodzie).
2. Uruchom, wybierz `v`.
3. Odczekaj kilkadziesiąt klatek.
4. Zatrzymaj program (miękko, przez flagę — nie `kill -9`).
5. Sprawdź:
   ```bash
   ffprobe stream_out.h264
   ffplay stream_out.h264
   ```

### Akceptacja
- `ffprobe` pokazuje kodek H.264 i poprawne fps.
- `ffplay` pokazuje ruchomy obraz (a nie szum).

Jeśli `ffplay` pokazuje zielone/zeszpecone klatki — wróć do Kroku 3 (prawdopodobnie
konwersja kolorów albo brak I-ramek). Ten test to **80% Twojego zadania** — zrób
go naprawdę dobrze.

---

## Krok 8 — Raspberry Pi + RealSense (dopiero jak dostaniesz sprzęt)

### Cel
Ta sama baza, tylko nowe źródło klatek. **Nie zmieniasz enkodera ani sendera.**

### Co zrobić
1. Na RPi (RPi OS 64-bit): zainstaluj zależności jak w kontenerze (OpenCV, FFmpeg)
   + `librealsense2-dev librealsense2-utils` (repo Intela, instrukcja na developer.intelrealsense.com).
2. Napisz `RealsenseFrameSource.{h,cpp}` implementujący `IFrameSource`:
   - `rs2::pipeline` → `rs2::config` (np. 1280×720 @30) → `pipeline.start(config)`,
   - `readFrame()`: `pipeline.wait_for_frames()` → `get_color_frame()` →
     skopiuj do `cv::Mat` (przetwórz format rs2 do BGR — poczytaj o `rs2::video_frame`),
   - `close()` → `pipeline.stop()`.
3. Kompilacja: dodaj `find_package(realsense2)` w CMake i włączaj ten plik tylko
   gdy biblioteka jest (przykład wzorca: jak `if (FFMPEG_FOUND)` w `manifold2/CMakeLists.txt:79-104`).
4. Test na sucho (bez PSDK): mały program grabujący klatki z wypisaniem wymiarów/fps.

### Jak podłączyć do pipeline'u
Zamień konstrukcję `FileFrameSource` na `RealsenseFrameSource` — to cała zmiana.
Reszta: enkoder, framing, wysyłka — bez dotyku.

### Pułapki
- RealSense podłączaj do USB 3.0 (2.0 nie daje wystarczającej przepustowości).
- Wymiary YUV420P muszą być parzyste; dobierz rozdzielczość sensownie do 8 Mbps.

---

## Krok 9 — Integracja z M350 RTK (dopiero ze sprzętem opiekuna)

### Co zrobić
1. **Okablowanie:** E-Port (lub SkyPort) — telemetria po UART, **wideo po Ethernet**.
   Bez sieci `SendVideoStream` nie ma kanału. Sprawdź `CONFIG_HARDWARE_CONNECTION`
   w `dji_sdk_config.h` — dla wideo musi być wariant z `DJI_USE_UART_AND_NETWORK_DEVICE`.
2. **Klucze:** wypełnij prawdziwe dane w `dji_sdk_app_info.h` (masz już pole;
   **nigdy nie commituj kluczy** — trzymaj je tylko lokalnie).
3. Uruchom: dron + DJI Pilot 2 + Twój program (bez `DJI_STREAM_SIMULATE`).
4. W Pilot 2 otwórz widok kamery payloadu → powinieneś zobaczyć obraz.
5. **Bandaż na ryzyko FFmpeg 8.x:** jeśli linkowanie/uruchomienie padnie na FFmpeg,
   zainstaluj na RPi FFmpeg 4.x (zalecane przez DJI) i powtórz build.

### Jak sprawdzić, że działa
- `DjiCore_Init` zwraca success (klucze poprawne).
- `GetVideoStreamState` nie jest w stanie busy przy Twoim bitrate.
- Obraz w DJI Pilot 2.

---

## Krok 10 — Liveview + dokumentacja (na koniec)

### Liveview (odbiór obrazu z drona)
Wzorzec masz już w repo: `samples/sample_c++/module_sample/liveview/`.
- `DjiLiveview_StartH264Stream(position, source, callback)` — zacznij od FPV
  (test_liveview.cpp:80),
- dekoder FFmpeg do podglądu: `dji_camera_stream_decoder.cpp`.

### Dokumentacja
Napisz w `custom_camera/` krótki `README`:
- jak zbudować (wymagane pakiety, komendy),
- jak uruchomić offline (makro stub) i na dronie,
- schemat przepływu danych (ASCII):
  ```
  RealSense/plik → klatka BGR → H264Encoder(NAL) → DjiPayloadSender(AUD+chunk)
        → DjiPayloadCamera_SendVideoStream → dron → DJI Pilot 2
  ```
- lista błędów, na które trafiłeś, i ich rozwiązania.

---

## Checklista końcowa

- [ ] Krok 0: zielony build po naprawie backticka
- [ ] Krok 2: `IFrameSource` + `FileFrameSource`, test N klatek
- [ ] Krok 3: `ffprobe` widzi H.264 z Twojego enkodera
- [ ] Krok 4: stub sendera produkuje `stream_out.h264` z AUD
- [ ] Krok 5: pipeline w wątku, `ffplay` odtwarza
- [ ] Krok 6: `SetVideoStreamType` przed `ApplicationStart`, menu `v` działa
- [ ] Krok 7: test offline end-to-end zaliczony
- [ ] Krok 8 (Pi): `RealsenseFrameSource` grabi klatki
- [ ] Krok 9 (dron): obraz w DJI Pilot 2
- [ ] Krok 10: Liveview + README

---

## Zasady, które Cię uratują

1. **Jeden krok = jeden committ.** Mały, kompilowalny, z sensownym opisem.
2. **Zanim zapytasz o kod — zapytaj o rozumienie.** Ten projekt ma Cię nauczyć.
   Nie kopiuj rozwiązania; prześledź API, przeczytaj wzorce w repo, dopiero potem pisz.
3. **Debuguj od najwęższego ogniwa.** Nie działa obraz? Sprawdź po kolei:
   źródło (klatki się pojawiają?) → enkoder (`ffprobe`) → framing (porównaj z
   `test_payload_cam_emu_media.c`) → kanał (network). Nie zgaduj.
4. **`ffprobe`/`ffplay` to Twoi przyjaciele** — dają obiektywną ocenę, że kodujesz.
5. **Bez drona NIE testujesz `SendVideoStream`.** Dlatego masz stub. Używaj go.
