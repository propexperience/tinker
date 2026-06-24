# Contributing to PropX Tinker

Thanks for your interest! PropX Tinker is an open-source hardware + firmware
project and contributions of all kinds are welcome — code, documentation,
photos, schematics, and bug reports.

## Ways to help

- **Documentation** — fix errors, clarify steps, add wiring examples.
- **Photos & diagrams** — see the wishlist in
  [`docs/images/README.md`](docs/images/README.md). Real photos of the board
  and connectors are especially valuable.
- **Firmware** — improvements to `src/main.cpp` and the serial console.
- **Hardware verification** — help close out the open items in
  [Known discrepancies](docs/HARDWARE.md#known-discrepancies--to-verify).

## Filing an issue

Please include:

- What you expected vs. what happened
- Board revision (silkscreen marking) if known
- Firmware version / commit
- Serial console output (copy the text from the monitor)
- A photo if it's a wiring or hardware question

## Proposing changes

1. Fork the repo and create a branch (`docs/...`, `fw/...`, or `hw/...`).
2. Keep changes focused — one topic per pull request.
3. For firmware, build clean before submitting:
   ```bash
   pio run
   ```
4. For documentation, keep the tone beginner-friendly and prefer tables and
   short examples over long prose.
5. Open a pull request describing **what** changed and **why**.

## Documentation style

- Write for someone new to electronics but comfortable with a terminal.
- Always state the **safe** way to do something, especially around 12 V.
- Use the existing image-placeholder convention so missing assets are obvious:
  ```markdown
  > 📷 **Image placeholder:** short description → `images/filename.png`
  ```
- Cross-link between `README.md`, `docs/HARDWARE.md`, and
  `docs/GETTING-STARTED.md` rather than duplicating content.

## Code of conduct

Be kind and constructive. This is a learning-oriented project — assume good
faith and help newcomers.

## License

> ⚠️ A project license has not been finalised yet (see the README). Until then,
> by contributing you agree your contributions may be released under the
> license eventually chosen for the project.
