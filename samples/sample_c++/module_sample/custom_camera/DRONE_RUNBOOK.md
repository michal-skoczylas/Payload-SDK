# Runbook — dzień z dronem (M350 RTK + E-Port + RPi 4B + RealSense D435i)

Ten dokument opisuje, jak przygotować kod i jak uruchomić pipeline wideo na dronie DJI
(M350 RTK przez PSDK, kanał E-Port: telemetria UART + wideo po Ethernet).

> Podstawa: cała robota z `PLAN.md` + sesje testowe na Macu i RPi 4B.
> Wszystkie komendy RPi wykonuj przez SSH (`ssh user@192.168.1.21`), hasło jak zwykle.

---

## 1. Przygotowanie kodu (ZROB PRZED WYJAZDEM — commit + push + pull na RPi)

| Plik | Zmiana | Po co |
|---|---|---|
| `samples/sample_c++/platform/linux/manifold2/application/dji_sdk_config.h:43` | `CONFIG_HARDWARE_CONNECTION` → `DJI_USE_UART_AND_NETWORK_DEVICE` | wideo po Ethernet (bez tego `SendVideoStream` nie ma kanału) |
| `samples/sample_c++/platform/linux/manifold2/hal/hal_network.h:43` | `LINUX_NETWORK_DEV` → nazwa karty E-Port na RPi (patrz niżej) | `HalNetWork_Init` robi `ifconfig` na tej karcie |
| `samples/sample_c++/platform/linux/manifold2/application/main.cpp` | odkomentuj `Application application(argc, argv);` | uruchamia init PSDK (teraz zakomentowane dla trybu offline) |

**Nazwa karty sieciowej na RPi:** po podpięciu E-Port sprawdź `ip link` — karta to zwykle
`eth0` (Ethernet RPi) albo nazwa USB-NIC (ASIX, VID/PID `0x0B95:0x1790`). Domylna wartość
w `hal_network.h` (`enxf8e43b7bbc2c`) pochodzi z innej maszyny (Manifold) i na RPi jest BŁĘDNA.

**UART:** `LINUX_UART_DEV1 /dev/ttyUSB0` (hal_uart.h) — poprawna dla adaptera USB-UART z zestawu E-Port.
Baudrate: `460800` (w `dji_sdk_app_info.h`).

---

## 2. Build w trybie DRONE (na RPi)

Ważne: obecny `~/Payload-SDK/build/` jest skonfigurowany z `-DOFFLINE_STREAM_ON=ON`
(makro `DJI_STREAM_SIMULATE` = strumień do pliku). **Na drona musi iść build BEZ tego makra.**

```bash
cd ~/Payload-SDK
git pull origin master
mkdir -p build_drone && cd build_drone
cmake ..                                   # BEZ -DOFFLINE_STREAM_ON
make -j4
```

- Sprawdź w output CMake: `Found RealSense SDK - RealsenseFrameSource enabled`, `Found OpenCV 4.15`.
- Sprawdź, że **nie ma** `-DDJI_STREAM_SIMULATE` w `build_drone/.../flags.make`.
- Binarka: `build_drone/bin/dji_sdk_demo_linux_cxx`.

---

## 3. Sekwencja uruchamiania NA MIEJSCU (KOLEJNOŚĆ KRYTYCZNA)

> Lekcja z dry-runu bez drona: SDK bez linku po ~70 s **crashuje** (SIGSEGV w `libpayloadsdk.a`)
> albo rzuca `Core init error`. Dlatego **dron i link MUSZĄ być gotowe, zanim wystartuje program.**

1. **Sprzęt:**
   - RPi zasilone (dobry PSU — kamera + adapter), D435i na **USB 3.0** (niebieski port),
   - E-Port podpięty: **UART → /dev/ttyUSB0** + **Ethernet** (karta z pkt. 1),
   - śmigła ZDJĘTE albo dron zabezpieczony.

2. **Dron + RC:** włącz drona, odpal **DJI Pilot 2** na RC, poczekaj na status "ready".

3. **Weryfikacja na RPi:**
   ```bash
   ls /dev/ttyUSB0                  # UART adapter widoczny
   ip link                           # karta E-Port widoczna
   rs-enumerate-devices              # kamera OK (bez sudo)
   ```

4. **Start programu jako ROOT** (wymóg `HalNetWork_Init` → `ifconfig`):
   ```bash
   cd ~/Payload-SDK/build_drone
   sudo ./bin/dji_sdk_demo_linux_cxx
   ```

5. **Patrz na logi (pierwsze 10–20 s):**
   - `Try identify UART0 connection failed` musi **zniknąć** (link nawiązany).
     Jeśli retry trwają >60 s → problem z UART/Ethernet (patrz debug).
   - Nasz blok: `DjiPayloadCamera_Init` → `RegCommonHandler` → `SetVideoStreamType` — returnCode OK.
   - `Application start.` pojawia się tylko przy dobrym linku.

6. **Menu:** `r` → preset `1` (1280×720@30, HW enkoder) → w **DJI Pilot 2 otwórz widok kamery payloadu**
   → powinien być obraz z D435i.

7. (Opcjonalnie) `GetVideoStreamState`: kanał nie-busy, bitrate ~4 Mb/s < 8 Mb/s limitu DJI.

---

## 4. Debug — gdy coś nie działa

| Objaw | Przyczyna / fix |
|---|---|
| `Try identify UART0` w kółko (>60 s) | ttyUSB0 nie istnieje; zły baud (ma być 460800); UART nie podpięty |
| Crash/`Core init error` po ~70 s | dron/link nie były gotowe na starcie → popraw i restartuj (znane z dry-runu) |
| Brak obrazu w Pilot 2 | stream type (DJI-H264); bitrate >8 Mb/s; `busyState`; enkoder nie trzyma fps |
| Obraz zielony/zamarznięty | AUD w `DjiPayloadSender`; profile High OK; wymiary parzyste (YUV420P) |
| `chmod: Operation not permitted` | HAL UART chmod bez roota — kosmetyka; uruchamiaj jako root |
| Brak `Application start.` | SDK nie doszedł do ApplicationStart — link nie aktywny |

---

## 5. Checklista na dzień testu

- [ ] `CONFIG_HARDWARE_CONNECTION = DJI_USE_UART_AND_NETWORK_DEVICE`
- [ ] `LINUX_NETWORK_DEV` = karta E-Port na RPi (`ip link`)
- [ ] `Application application(argc, argv);` odkomentowane
- [ ] Build `build_drone` BEZ `-DOFFLINE_STREAM_ON`
- [ ] `sudo` przy uruchamianiu
- [ ] Dron + DJI Pilot 2 gotowe PRZED startem programu
- [ ] `/dev/ttyUSB0` i karta sieciowa widoczne na RPi
- [ ] Menu `r` → preset 1 → obraz w DJI Pilot 2

---

## 6. Uwagi

- **Klucze** (`dji_sdk_app_info.h`): wypełnione; pamiętaj, że plik jest w repo (rotacja = Twoja decyzja,
  nowe klucze tylko lokalnie + scp, nigdy przez gita).
- **Wersja SDK:** podpięty `libpayloadsdk.a` to **V3.16.0-beta.0** (log na starcie), nie 3.9.2 z PLAN.md —
  potwierdź z opiekunem właściwą wersję pod M350.
- **Tryb offline** (testy bez drona): wracasz do `build/` z `-DOFFLINE_STREAM_ON=ON` i menu `v`/`r`
  (stream do `stream_out.h264`). Oba katalogi build mogą istnieć obok siebie.
- **Live-test bez drona** (podgląd na Macu): procedura z FIFO — patrz notatki sesji / PLAN.md.
