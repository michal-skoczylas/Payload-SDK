# Plan implementacji — strumień z dolnej kamery wizyjnej (percepcja)

Cel: stream obrazu z dolnej kamery wizyjnej (obserwacja przeszkód, moduł Perception
PSDK) do DJI Pilot 2 (widok kamery payloadu) przez istniejący pipeline wideo.

Decyzje (zatwierdzone przez użytkownika):

- kierunek: DOWN (`DJI_PERCEPTION_RECTIFY_DOWN`),
- kamera z pary: tylko LEWA (`RECTIFY_DOWN_LEFT`, filtr po `imageInfo.dataType`),
- cel strumienia: Pilot 2 (payload view) przez `DjiPayloadSender`,
- Opcja A: **reuse** wspólnych plików z `custom_camera/`
  (`IFrameSource`, `H264Encoder`, `DjiPayloadSender`, `VideoStreamPipeline`)
  — nowy folder zawiera tylko kod percepcyjny (kopiowanie `.cpp` dałoby
  duplikaty symboli w linkerze).

## Struktura

```
samples/sample_c++/module_sample/perception_cameras/
├── PLAN.md                      <- ten plik
├── PerceptionFrameSource.h
├── PerceptionFrameSource.cpp
├── perception_stream_sample.h
└── perception_stream_sample.cpp
```

Wspólne klasy pozostają w `custom_camera/` i są includowane przez
`#include "custom_camera/..."` (include path `module_sample/` już ustawiony
w `manifold2/CMakeLists.txt:31`).

## Architektura (już istnieje, bez zmian)

```
PerceptionFrameSource (grayscale CV_8U → cvtColor BGR)
    → H264Encoder (BGR → H.264 NAL)
    → DjiPayloadSender (AUD + cięcie ≤60000B)
    → DjiPayloadCamera_SendVideoStream → Pilot 2 | plik (offline)
```

## Zmiany w plikach

### 1. `PerceptionFrameSource.{h,cpp}` (NOWE)

Klasa `PerceptionFrameSource : public IFrameSource`:

- `open()`:
  - `DjiPerception_Init()`; nie-SUCCESS (w tym `NONSUPPORT`) → log + `false`,
  - statyczny wskaźnik `s_instance = this` (callback DJI nie ma user-data),
  - `DjiPerception_SubscribePerceptionImage(direction_, &ImageCallback)`,
  - czeka na pierwszą ramkę (max 1 s, `condition_variable`) → zapisuje
    `width_/height_` (zaokrąglone w dół do parzystych — wymóg YUV420P);
    timeout → unsubscribe + deinit + `false`.
- callback (wątek SDK, bez blokowania):
  - odrzuca prawą kamerę: `info.dataType != leftDataType_`,
  - kopiuje bufor do `cv::Mat(h, w, CV_8U)` pod mutexem, ustawia `newData_`,
    `notify_all()`.
- `readFrame(cv::Mat&)`: brak nowej ramki → `false`; inaczej kopiuj najnowszą
  i `cvtColor(gray, out, COLOR_GRAY2BGR)` → `true`.
- `close()`: `UnsubscribePerceptionImage` + `DjiPerception_Deinit`.
- gettery: `getFps()=20`, wymiary z pierwszej ramki.

Cały `.cpp` pod `#ifdef PERCEPTION_STREAM_ENABLED`.

### 2. `perception_stream_sample.{h,cpp}` (NOWE)

Runner `DjiUser_RunPerceptionStreamSample()` — kalka
`DjiUser_RunCustomRealsenseStreamSample` (`main.cpp:203`):

`PerceptionFrameSource(DJI_PERCEPTION_RECTIFY_DOWN)` → `open()` →
`H264Encoder(w, h, 20, 4000000)` → `DjiPayloadSender` →
`VideoStreamPipeline` → `start()`, pętla ~30 s, `stop()`. Całość w `try/catch`.

Cały `.cpp` pod `#ifdef PERCEPTION_STREAM_ENABLED`.

### 3. `manifold2/CMakeLists.txt` (EDYCJA)

- glob `MODULE_SAMPLE_SRC` (`:36-44`): dodać `../../../module_sample/perception_cameras/*.c*`,
- obok `option(OFFLINE_STREAM_ON ...)` (`:13-16`):
  ```cmake
  option(PERCEPTION_STREAM_ON "Stream down vision camera to Pilot 2" OFF)
  if (PERCEPTION_STREAM_ON)
      add_definitions(-DPERCEPTION_STREAM_ENABLED)
  endif ()
  ```

### 4. `main.cpp` (EDYCJA)

- `#ifdef PERCEPTION_STREAM_ENABLED` → `#include "perception_cameras/perception_stream_sample.h"`,
- menu: `[p] Stream down perception camera to Pilot 2`,
- `case 'p':` → `#ifdef PERCEPTION_STREAM_ENABLED` → `DjiUser_RunPerceptionStreamSample()`,
  `#else` → komunikat "not in this build".

## Weryfikacja

1. Build offline (Mac/RPi): `cmake .. && make` — zielony bez `-DPERCEPTION_STREAM_ENABLED`.
2. Build drone (RPi): `cmake .. -DPERCEPTION_STREAM_ON=ON && make`.
3. Na dronie (wg `custom_camera/DRONE_RUNBOOK.md`): `sudo ./bin/dji_sdk_demo_linux_cxx`
   → menu `p` → DJI Pilot 2 → widok kamery payloadu = obraz dolnej kamery.
   Logi: `DjiPerception_Init` OK (brak `NONSUPPORT`), bitrate < 8 Mb/s.

## Ryzyka (do potwierdzenia na sprzęcie)

- Przenoszenie danych percepcji po Ethernet (`DJI_USE_UART_AND_NETWORK_DEVICE`);
  w trybie USB-bulk działa na pewno.
- `DjiPerception_Init` może zwrócić `NONSUPPORT` na firmware M350.
- Rzeczywista rozdzielczość/fps dolnej kamery znana dopiero z pierwszej ramki.
