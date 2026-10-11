# 🔌 PCB-Unterlagen (TinkerThinker-Platine)

[⬅️ Zurück zur Hauptseite (`README.md`)](../README.md)

Dieser Ordner enthält die Hardware-Dokumentation zur TinkerThinker-Platine (Schaltplan, Platinen-Ansichten, 3D-Modell und Sicherheitshinweise).

## 📂 Enthaltene Dateien

| Datei | Beschreibung |
| :--- | :--- |
| **`Schematic_TinkerThinker-V4.4_2025-08-08.pdf`** | Vollständiger Schaltplan der Platine als PDF. |
| **`PCB_TOP_PCB_TinkerThinker-V4_2025-08-08.pdf`** | Oberseite der Leiterplatte (Bestückungs-/Layout-Ansicht im Maßstab 1:1). |
| **`PCB_BOT_PCB_TinkerThinker-V4_2025-08-08.pdf`** | Unterseite der Leiterplatte (Maßstab 1:1). |
| **`OBJ_PCB_TinkerThinker-V4_2025-08-30.zip`** | 3D-Modell der Platine (`.obj` im ZIP-Archiv) für CAD-Gehäuse und Halterungen. |
| **`TinkerThinker_Sicherheitshinweise (2).pdf`** | Wichtige Sicherheitshinweise für Betrieb, Akku und Workshops. |

## 💡 Tipps für den Aufbau & 3D-Druck

- **Maßstab 1:1 prüfen**: Drucke die `PCB_TOP`-PDF mit **100 % Skalierung (Tatsächliche Größe)** auf Papier aus, um Bohrlöcher und Abmessungen vor dem 3D-Druck oder Lasercutten direkt gegenzuprüfen.
- **3D-Integration in CAD**: Entpacke `OBJ_PCB_TinkerThinker-V4_2025-08-30.zip` und importiere die `.obj`-Datei in Fusion 360, FreeCAD, Blender oder deinen Slicer. Passende druckfertige Halterungen findest du direkt im Ordner **[`../CAD/`](../CAD/README.md)**.
- **Wichtiger Hinweis zu den Stromsensoren (Roboter-Arm)**:
  Auf der Platine verfügen **`Motor A` (`M0`)** und **`Motor B` (`M1`)** über integrierte Stromsensoren (`GPIO 36` & `GPIO 34`). Beim Bau des Roboter-Arms müssen dort zwingend **`② Arm 1 (Unterarm)`** und **`③ Arm 2 (Mittelarm)`** angeschlossen werden, während **`① Basis`** und **`④ Arm 3 (Handgelenk)`** an **`Motor C`** und **`Motor D`** gehören.
