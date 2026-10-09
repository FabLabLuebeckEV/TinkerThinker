# CAD-Dateien

Dieser Ordner enthaelt CAD-Dateien (Autodesk Fusion 360, STEP, DXF, 3MF) rund um das TinkerThinker-Projekt.

## Inhalt

### Roboter-Chassis & Halterungen
- `Roboter V3 v27.f3z`: Komplettes Fusion-Archiv (Baugruppe) des Roboters. Zum Import in Fusion 360: Datei > Oeffnen > Upload.
- `RescueBot2025OLD PCB.f3z`: Aelteres Fusion-Archiv mit PCB-Bezug (Historie/Referenz).
- `PlatinenhalterNEU v6.f3d`: Einzelteil (Platinentraeger/Halter) als Fusion-Bauteil.

### Sumo-Roboter-Bausatz
- `Sumo Bausatz.step`: STEP-Baugruppe des vollstaendigen Sumo-Roboters.
- `Sumo Bausatz 3D Druck.3mf`: Druckfertige 3D-Druck-Komponenten fuer den Bausatz.
- `Sumo Bausatz Grundplatte.dxf`: DXF-Kontur der Grundplatte fuer Laser- oder Fraesbearbeitung.
- `Sumo Bausatz Wippe.dxf`: DXF-Kontur der beweglichen Wippe.

## Oeffnen und Export
- Empfohlen: Autodesk Fusion 360 (native `.f3d` / `.f3z`).
- Alternativen: FreeCAD, Blender, PrusaSlicer, Bambu Studio o. ae. ueber STEP-, 3MF- und DXF-Formate.

## 3D-Druck-Hinweise (Platinenhalter & Chassis)
- Material: PETG oder PLA+ (fuer hoehere Waermebestaendigkeit und Flexibilitaet wird PETG empfohlen).
- Layerhoehe: 0.2 mm (feiner bei Passflaechen: 0.12 bis 0.16 mm).
- Infill: 20 bis 40% (je nach gewuenschter Steifigkeit).
- Waende/Perimeter: 3 bis 4.
- Ausrichtung: So auf dem Druckbett platzieren, dass Rastnasen und Bohrungen sauber gedruckt werden; Stuetzen nur falls noetig.

## Mechanik-Abgleich
- Abmessungen pruefen: Platinen-Bohrungen und Aussenkontur mit den Daten in `PCB/` abgleichen.
- Falls Passungstoleranzen noetig sind (z. B. +0.2 mm an Bohrungen), direkt im CAD oder im Slicer anpassen.

## Lizenz und Verwendung
- Sofern nicht anders angegeben, gelten die Lizenzbedingungen des Repository-Root (siehe `LICENSE`).
