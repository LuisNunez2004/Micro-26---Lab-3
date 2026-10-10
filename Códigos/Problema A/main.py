#!/usr/bin/env python3
"""Grafica los datos del Problema A (control de temperatura, ATmega328P + DHT22).

Uso:
  python graficar.py --archivo log.txt                 # desde un log copiado de la terminal
  python graficar.py --puerto COM3                      # en vivo desde un puerto serie
  python graficar.py --puerto rfc2217://localhost:4000  # en vivo desde un simulador que lo ofrezca
"""
import argparse
import csv
import re
import sys

import matplotlib.pyplot as plt
from pathlib import Path

CARPETA = Path(__file__).resolve().parent

# D,<muestra>,<temperatura>,<punto medio>,<accion>,<pwm %>
PATRON = re.compile(r"D,(\d+),(-?\d+\.\d),(\d+),(\d),(\d+)")

ACCIONES = {0: "Calefactor", 1: "Neutro", 2: "Vent. bajo", 3: "Vent. medio", 4: "Vent. alto"}


def parsear(linea, periodo):
    m = PATRON.search(linea)
    if not m:
        return None
    n, temp, pm, acc, pwm = m.groups()
    return {
        "t": (int(n) - 1) * periodo,
        "temp": float(temp),
        "pm": int(pm),
        "acc": int(acc),
        "pwm": int(pwm),
    }


def dibujar(datos, ejes):
    ax_t, ax_c, ax_v = ejes
    for ax in ejes:
        ax.clear()

    t = [d["t"] for d in datos]
    temp = [d["temp"] for d in datos]
    bajo = [d["pm"] - 5 for d in datos]
    alto = [d["pm"] + 5 for d in datos]
    calefactor = [1 if d["acc"] == 0 else 0 for d in datos]
    pwm = [d["pwm"] for d in datos]

    # 1) Temperatura y rango ideal
    ax_t.plot(t, temp, "o-", color="tab:red", label="Temperatura medida")
    ax_t.fill_between(t, bajo, alto, step="post", color="tab:green", alpha=0.2,
                      label="Rango ideal (punto medio ±5 °C)")
    ax_t.set_ylabel("Temperatura (°C)")
    ax_t.legend(loc="upper right")
    ax_t.grid(True)

    # 2) Calefactor ON/OFF
    ax_c.step(t, calefactor, where="post", color="tab:orange")
    ax_c.set_yticks([0, 1])
    ax_c.set_yticklabels(["OFF", "ON"])
    ax_c.set_ylabel("Calefactor")
    ax_c.grid(True)

    # 3) Velocidad del ventilador
    ax_v.step(t, pwm, where="post", color="tab:blue")
    ax_v.set_yticks([0, 40, 70, 100])
    ax_v.set_ylabel("Ventilador (% PWM)")
    ax_v.set_xlabel("Tiempo (s)")
    ax_v.grid(True)

    ejes[0].figure.tight_layout(rect=[0, 0, 1, 0.96])


def guardar_csv(datos, ruta):
    with open(ruta, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["tiempo_s", "temperatura_C", "punto_medio_C", "accion", "pwm_pct"])
        for d in datos:
            w.writerow([d["t"], d["temp"], d["pm"], ACCIONES.get(d["acc"], d["acc"]), d["pwm"]])


def main():
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--archivo", help="log de texto con las lineas D,... (por defecto: log.txt)")
    g.add_argument("--puerto", help="puerto serie (COM3, /dev/ttyUSB0, rfc2217://...)")
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--periodo", type=float, default=5, help="segundos entre muestras (5 por defecto)")
    ap.add_argument("--csv", default=str(CARPETA / "datos.csv"))
    ap.add_argument("--png", default=str(CARPETA / "grafica.png"))
    args = ap.parse_args()
    if not args.archivo and not args.puerto:
        args.archivo = str(CARPETA / "log.txt")

    datos = []
    fig, ejes = plt.subplots(3, 1, sharex=True, figsize=(10, 8),
                             gridspec_kw={"height_ratios": [3, 1, 1]})
    fig.suptitle("Control de temperatura - ATmega328P + DHT22")

    if args.archivo:
        print("Buscando:", args.archivo)
        with open(args.archivo, encoding="utf-8", errors="ignore") as f:
            for linea in f:
                d = parsear(linea, args.periodo)
                if d:
                    datos.append(d)
        if not datos:
            sys.exit("No se encontraron lineas de datos (D,...) en el archivo.")
        dibujar(datos, ejes)
        guardar_csv(datos, args.csv)
        fig.savefig(args.png, dpi=150)
        plt.show()
    else:
        import serial
        with serial.serial_for_url(args.puerto, args.baud, timeout=1) as ser:
            plt.ion()
            plt.show()
            try:
                while plt.fignum_exists(fig.number):
                    linea = ser.readline().decode("utf-8", errors="ignore")
                    d = parsear(linea, args.periodo)
                    if d:
                        datos.append(d)
                        dibujar(datos, ejes)
                        fig.savefig(args.png, dpi=150)
                        guardar_csv(datos, args.csv)
                    plt.pause(0.05)
            except KeyboardInterrupt:
                pass


if __name__ == "__main__":
    main()
