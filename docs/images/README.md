# Image & Diagram Assets

This folder holds the photos, diagrams, and schematics referenced throughout
the documentation. The docs already link to these filenames — drop the real
files in here (matching the names) and they'll appear automatically.

Prefer **PNG** for photos, **SVG** for diagrams, **PDF** for schematics.

## Wishlist (referenced by the docs)

| Filename | Used in | Description |
|---|---|---|
| `banner.png` | README | Project banner / board hero shot |
| `board-top-annotated.png` | README, HARDWARE | Top-down board photo with every connector labelled |
| `connector-map.svg` | HARDWARE | Silkscreen outline showing where each connector is |
| `schematic.pdf` | HARDWARE | Full schematic |
| `connector-pinouts.svg` | HARDWARE | Per-connector pinout drawings (pin 1 marked) |
| `display-header.png` | HARDWARE | OLED plugged into the display header |
| `mosfet-output-wiring.png` | HARDWARE | An LED strip wired to a MOSFET output |
| `opto-input-wiring.svg` | HARDWARE | Optocoupler input wiring example |
| `jumper-detail.png` | HARDWARE | Close-up of a jumper (VCC vs GND position) |
| `power-tree.svg` | HARDWARE | Power tree (USB-C 5 V → 3.3 V; 12 V external) |
| `usb-connected.png` | GETTING-STARTED | Board connected to a laptop over USB-C |

## Naming conventions

- Lowercase, hyphen-separated: `board-top-annotated.png`
- Keep originals (high-res) and add a `-web` variant if you need a smaller copy
  for the eventual website.
- When you add an image, remove the matching `📷/📐/📄 placeholder` callout in
  the doc that references it.
