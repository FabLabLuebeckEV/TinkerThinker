# 🖨️ CAD- & 3D-Druck-Dateien

[⬅️ Zurück zur Hauptseite (`README.md`)](../README.md)

Dieser Ordner enthält alle mechanischen Konstruktions- und 3D-Druck-Dateien (Autodesk Fusion 360, STEP, DXF, 3MF) für das TinkerThinker-Projekt.

## 📂 Übersicht der Modelle

### 1. Roboter-Chassis & Platinenhalter
| Datei | Format | Beschreibung |
| :--- | :---: | :--- |
| **`PlatinenhalterNEU v6.f3d`** | Fusion 360 | Aktueller Träger/Halter für die TinkerThinker-Platine. |
| **`Roboter V3 v27.f3z`** | Fusion 360 Archiv | Komplette Baugruppe des Roboters (Import in Fusion 360 über *Datei -> Öffnen -> Upload*). |
| **`RescueBot2025OLD PCB.f3z`** | Fusion 360 Archiv | Älteres Referenz-Archiv des Rescue-Bots. |

### 2. Sumo-Roboter-Bausatz (3D-Druck & Lasercutter)
| Datei | Format | Beschreibung |
| :--- | :---: | :--- |
| **`Sumo Bausatz 3D Druck.3mf`** | 3MF (Slicer) | Druckfertige 3D-Druck-Bauteile direkt für Bambu Studio, PrusaSlicer oder OrcaSlicer. |
| **`Sumo Bausatz.step`** | STEP | Komplette 3D-Baugruppe des Sumo-Roboters (kompatibel mit allen CAD-Programmen wie FreeCAD & Fusion 360). |
| **`Sumo Bausatz Grundplatte.dxf`** | DXF (2D) | Laser- / Fräskontur der Grundplatte. |
| **`Sumo Bausatz Wippe.dxf`** | DXF (2D) | Laser- / Fräskontur der vorderen Sumo-Wippe. |

## ⚙️ Empfohlene 3D-Druck-Einstellungen

- **Material**: **PETG** oder **PLA+** (PETG wird wegen der höheren Bruchfestigkeit und Wärmebeständigkeit an den Rastnasen empfohlen).
- **Schichthöhe (Layer Height)**: `0.20 mm` (bei feinen Passflächen `0.16 mm`).
- **Wandlinien (Perimeters)**: `3 bis 4 Wände` für stabile Schraubdome.
- **Füllung (Infill)**: `20 % bis 35 %` (Gyroid oder Grid).
- **Platinen-Abgleich**: Das 3D-Modell und die Bohrungsmaße der Leiterplatte findest du im Ordner **[`../PCB/`](../PCB/README.md)**.
