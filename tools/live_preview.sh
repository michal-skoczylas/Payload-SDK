#!/usr/bin/env bash
# Live preview z kamery RealSense (RPi) -> H264Encoder -> TCP -> ffplay (Mac).
#
# Uzycie:
#   ./live_preview.sh            # uruchom podglad (auto-startuje serwer na RPi)
#   ./live_preview.sh stop       # zatrzymaj serwer na RPi
#   ./live_preview.sh status     # czy serwer dziala
#
# Opcje przez zmienne srodowiskowe:
#   RPI_IP=192.168.1.23 RPI_USER=user RPI_PORT=5555
#   RPI_SSH_KEY=/sciezka/klucza   # opcjonalny klucz do SSH (bez hasla)
#   RPI_WIDTH=1920 RPI_HEIGHT=1080 RPI_FPS=30 RPI_BITRATE=6000000
set -u

RPI_USER=${RPI_USER:-user}
RPI_IP=${RPI_IP:-192.168.1.23}
RPI_PORT=${RPI_PORT:-5555}
RPI_KEY=${RPI_SSH_KEY:-}
RPI_WIDTH=${RPI_WIDTH:-1280}
RPI_HEIGHT=${RPI_HEIGHT:-720}
RPI_FPS=${RPI_FPS:-30}
RPI_BITRATE=${RPI_BITRATE:-4000000}
RPI="$RPI_USER@$RPI_IP"

ssh_opts=(-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=5)
[ -n "$RPI_KEY" ] && ssh_opts+=(-i "$RPI_KEY")

rpi() { ssh "${ssh_opts[@]}" "$RPI" "cd ~/Payload-SDK && $*"; }

server_is_running() { rpi "pgrep -x stream_tcp_test >/dev/null 2>&1"; }

start_server() {
    if server_is_running; then
        echo "[preview] serwer juz dziala na $RPI"  >&2
        return 0
    fi
    echo "[preview] start serwera na $RPI (${RPI_WIDTH}x${RPI_HEIGHT}@${RPI_FPS}, bitrate ${RPI_BITRATE})..." >&2
    rpi "pkill -x stream_tcp_test 2>/dev/null; sleep 1; setsid nohup ./stream_tcp_test $RPI_PORT $RPI_WIDTH $RPI_HEIGHT $RPI_FPS $RPI_BITRATE >/tmp/tcp_stream.log 2>&1 </dev/null & sleep 3" >/dev/null
    sleep 1
    if server_is_running; then
        echo "[preview] OK, serwer nasluchuje na :$RPI_PORT (log: /tmp/tcp_stream.log)" >&2
    else
        echo "[preview] BLAD: serwer nie wystartowal" >&2
        exit 1
    fi
}

stop_server() {
    echo "[preview] zatrzymuje serwer na $RPI..." >&2
    rpi "pkill -x stream_tcp_test 2>/dev/null" >/dev/null
}

status_server() {
    if server_is_running; then
        echo "[preview] serwer: DZIALA (:$RPI_PORT)"
    else
        echo "[preview] serwer: nie dziala"
    fi
}

preview() {
    start_server
    echo "[preview] podglad z $RPI_IP:$RPI_PORT - zamknij okno ffplay lub Ctrl+C aby wyjsc" >&2
    ffplay -loglevel warning -fflags nobuffer -flags low_delay -framerate 30 -f h264 - < <(nc "$RPI_IP" "$RPI_PORT")
    echo "[preview] koniec podgladu - zatrzymuje serwer na RPi" >&2
    stop_server
}

case "${1:-preview}" in
    start)  start_server ;;
    stop)   stop_server ;;
    status) status_server ;;
    preview) preview ;;
    *) echo "uzycie: $0 [start|stop|status|preview]"; exit 1 ;;
esac