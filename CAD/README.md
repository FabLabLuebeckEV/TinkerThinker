# CAD-Dateien

Dieser Ordner enthält CAD-Dateien (Autodesk Fusion 360, STEP, DXF, 3MF) rund um das TinkerThinker-Projekt.

## Inhalt

### Roboter-Chassis & Halterungen
- `Roboter V3 v27.f3z`: Komplettes Fusion-Archiv (Baugruppe) des Roboters. Zum Import in Fusion 360: Datei > Öffnen > Upload.
- `RescueBot2025OLD PCB.f3z`: Älteres Fusion-Archiv mit PCB-Bezug (Historie/Referenz).
- `PlatinenhalterNEU v6.f3d`: Einzelteil (Platinenträger/Halter) als Fusion-Bauteil.

### Sumo-Roboter-Bausatz
- `Sumo Bausatz.step`: STEP-Baugruppe des vollständigen Sumo-Roboters.
- `Sumo Bausatz 3D Druck.3mf`: Druckfertige 3D-Druck-Komponenten für den Bausatz.
- `Sumo Bausatz Grundplatte.dxf`: DXF-Kontur der Grundplatte für Laser- oder Fräsbearbeitung.
- `Sumo Bausatz Wippe.dxf`: DXF-Kontur der beweglichen Wippe.

## Öffnen und Export
- Empfohlen: Autodesk Fusion 360 (native `.f3d` / `.f3z`).
- Alternativen: FreeCAD, Blender, PrusaSlicer, Bambu Studio o. ä. über STEP-, 3MF- und DXF-Formate.

## 3D-Druck-Hinweise (Platinenhalter & Chassis)
- Material: PETG oder PLA+ (für höhere Wärmebeständigkeit und Flexibilität wird PETG empfohlen).
- Layerhöhe: 0.2 mm (feiner bei Passflächen: 0.12 bis 0.16 mm).
- Infill: 20 bis 40% (je nach gewünschter Steifigkeit).
- Wände/Perimeter: 3 bis 4.
- Ausrichtung: So auf dem Druckbett platzieren, dass Rastnasen und Bohrungen sauber gedruckt werden; Stützen nur falls nötig.

## Mechanik-Abgleich
- Abmessungen prüfen: Platinen-Bohrungen und Außenkontur mit den Daten in `PCB/` abgleichen.
- Falls Passungstoleranzen nötig sind (z. B. +0.2 mm an Bohrungen), direkt im CAD oder im Slicer anpassen.

## Lizenz und Verwendung
- Sofern nicht anders angegeben, gelten die Lizenzbedingungen des Repository-Root (siehe `LICENSE`).
