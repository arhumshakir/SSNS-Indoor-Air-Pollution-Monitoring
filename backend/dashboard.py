#!/usr/bin/env python3
"""
dashboard.py -- live IAQ dashboard.

Runs a background thread that reads JSON from the gateway serial port, keeps a
rolling history in memory, and serves a single-page live dashboard (Chart.js)
on http://localhost:8000.

Usage:
    python dashboard.py --port COM7
    python dashboard.py --port /dev/ttyACM0

If you have no hardware yet, run with --demo to feed synthetic data so you can
build and rehearse the dashboard:
    python dashboard.py --demo
"""
import argparse
import json
import random
import threading
import time
from collections import deque
from datetime import datetime

from flask import Flask, jsonify

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None

app = Flask(__name__)
HISTORY = deque(maxlen=240)   # ~20 min at 5 s cadence
LATEST = {}


def autodetect_port():
    if serial is None:
        return None
    for p in list_ports.comports():
        desc = (p.description or "").lower()
        if any(k in desc for k in ("acm", "cdc", "iaq", "usb serial")):
            return p.device
    ports = list_ports.comports()
    return ports[0].device if ports else None


def push(rec):
    rec["t"] = datetime.now().strftime("%H:%M:%S")
    HISTORY.append(rec)
    LATEST.clear()
    LATEST.update(rec)


def serial_reader(port, baud):
    ser = serial.Serial(port, baud, timeout=2)
    print(f"Reading {port} @ {baud}")
    while True:
        try:
            raw = ser.readline().decode("utf-8", "ignore").strip()
        except Exception as e:
            print("serial error:", e)
            time.sleep(2)
            continue
        if not raw:
            continue
        try:
            rec = json.loads(raw)
        except json.JSONDecodeError:
            continue
        if "co2" in rec:
            push(rec)


def demo_reader():
    print("DEMO mode: generating synthetic data")
    co2, pkt = 600, 0
    while True:
        co2 += random.randint(-30, 45)
        co2 = max(420, min(2200, co2))
        push({
            "co2": co2,
            "temp": round(21 + random.uniform(-1, 1), 2),
            "rh": round(45 + random.uniform(-3, 3), 2),
            "pm25": round(8 + random.uniform(0, 20), 1),
            "tvoc": random.randint(50, 600),
            "eco2": co2 + random.randint(-50, 50),
            "lux": random.randint(50, 800),
            "rssi": random.randint(-80, -45),
            "pkt": pkt % 256,
        })
        pkt += 1
        time.sleep(2)


@app.route("/api/latest")
def api_latest():
    return jsonify(LATEST)


@app.route("/api/history")
def api_history():
    return jsonify(list(HISTORY))


INDEX_HTML = """<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Indoor Air Quality - Live</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js"></script>
<style>
  :root { color-scheme: dark; }
  body { margin:0; font-family: system-ui, sans-serif; background:#0b0d10; color:#e7ecef; }
  header { padding:20px 28px; border-bottom:1px solid #1d2329; }
  h1 { margin:0; font-size:20px; letter-spacing:.3px; }
  h1 span { color:#bdf25b; }
  .sub { color:#8a949c; font-size:13px; margin-top:4px; }
  .cards { display:grid; grid-template-columns:repeat(auto-fit,minmax(150px,1fr)); gap:14px; padding:24px 28px; }
  .card { background:#12161b; border:1px solid #1d2329; border-radius:14px; padding:16px 18px; }
  .card .label { color:#8a949c; font-size:12px; text-transform:uppercase; letter-spacing:.6px; }
  .card .value { font-size:30px; font-weight:600; margin-top:6px; }
  .card .unit { font-size:13px; color:#8a949c; margin-left:4px; font-weight:400; }
  .chartwrap { padding:0 28px 32px; }
  .chartbox { background:#12161b; border:1px solid #1d2329; border-radius:14px; padding:18px; }
  .ok { color:#bdf25b; } .warn { color:#ffb454; } .bad { color:#ff6b6b; }
</style></head>
<body>
<header>
  <h1>Indoor <span>Air Quality</span> &middot; Live</h1>
  <div class="sub">Frankfurt UAS &middot; SSNS &middot; gateway feed updates every few seconds</div>
</header>
<div class="cards" id="cards"></div>
<div class="chartwrap"><div class="chartbox"><canvas id="chart" height="90"></canvas></div></div>
<script>
const CARDS = [
  ["co2","CO\u2082","ppm"], ["temp","Temp","\u00b0C"], ["rh","Humidity","%"],
  ["pm25","PM2.5","\u00b5g/m\u00b3"], ["tvoc","TVOC","ppb"],
  ["eco2","eCO\u2082","ppm"], ["lux","Light","lux"], ["rssi","Signal","dBm"]
];
function co2Class(v){ return v<800?"ok":v<1200?"warn":"bad"; }
function renderCards(d){
  document.getElementById("cards").innerHTML = CARDS.map(([k,l,u])=>{
    let v = d[k]; if(v===undefined) v="--";
    let cls = (k==="co2" && typeof d.co2==="number") ? co2Class(d.co2) : "";
    return `<div class="card"><div class="label">${l}</div>
      <div class="value ${cls}">${v}<span class="unit">${u}</span></div></div>`;
  }).join("");
}
const ctx = document.getElementById("chart");
const chart = new Chart(ctx, {
  type:"line",
  data:{ labels:[], datasets:[
    {label:"CO\u2082 (ppm)", data:[], borderColor:"#bdf25b", backgroundColor:"transparent", tension:.25, yAxisID:"y"},
    {label:"PM2.5 (\u00b5g/m\u00b3)", data:[], borderColor:"#5bd0f2", backgroundColor:"transparent", tension:.25, yAxisID:"y1"}
  ]},
  options:{ responsive:true, animation:false,
    scales:{ y:{position:"left", title:{display:true,text:"CO2"}, grid:{color:"#1d2329"}},
             y1:{position:"right", title:{display:true,text:"PM2.5"}, grid:{drawOnChartArea:false}},
             x:{grid:{color:"#1d2329"}} },
    plugins:{ legend:{labels:{color:"#e7ecef"}} } }
});
async function tick(){
  try{
    const hist = await (await fetch("/api/history")).json();
    chart.data.labels = hist.map(r=>r.t);
    chart.data.datasets[0].data = hist.map(r=>r.co2);
    chart.data.datasets[1].data = hist.map(r=>r.pm25);
    chart.update();
    const latest = await (await fetch("/api/latest")).json();
    renderCards(latest);
  }catch(e){}
}
renderCards({});
setInterval(tick, 2000); tick();
</script>
</body></html>"""


@app.route("/")
def index():
    return INDEX_HTML


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--demo", action="store_true")
    args = ap.parse_args()

    if args.demo:
        threading.Thread(target=demo_reader, daemon=True).start()
    else:
        if serial is None:
            raise SystemExit("pyserial not installed. pip install pyserial")
        port = args.port or autodetect_port()
        if not port:
            raise SystemExit("No serial port found. Use --port or --demo.")
        threading.Thread(target=serial_reader, args=(port, args.baud),
                         daemon=True).start()

    print("Dashboard at http://localhost:8000")
    app.run(host="0.0.0.0", port=8000, threaded=True)


if __name__ == "__main__":
    main()
