# Clipper (xBase programming language)

**Clipper** (Nantucket Clipper, later **CA-Clipper**) is an **xBase** compiler and language: a dBase III–style dialect compiled to DOS executables, aimed mainly at database-centric business applications (inventory, CRM, banking/insurance front-ends, etc.). It is **not** the same thing as polygon-clipping libraries often named “Clipper” in geometry (e.g. Angus Johnson’s library).

**Not part of this engine:** idTech3 does not embed, compile, or run Clipper code. This page is a **standalone reference** only.

## Origins and license

Developed by **Nantucket Corporation** (from 1984); sold to **Computer Associates** in 1992; product renamed **CA-Clipper**. Final CA release commonly cited: **CA Clipper 5.3b** (May 1997). Original platform: **DOS**; translator/toolchain was **proprietary**.

Compared to Ashton-Tate **dBASE III**, a major selling point was **compilation** to standalone programs instead of shipping an interpreted runtime for every line. Over time the language gained C/Pascal–like constructs, **object-oriented** features, and **code blocks** (mixing dBase-style macros / evaluation with function-pointer–like behavior). Nantucket’s **Aspen** line later fed **CA-Visual Objects** for Windows native code.

One classic **dBase** feature **not** in Clipper was the interactive **dot-prompt** command environment.

## Successors and clones

Active or historical implementations include **Harbour** and **xHarbour** (GPL and related), **XBase++** (Alaska Software), **FlagShip**, and others—often portable across DOS, Windows, Linux, Unix, and macOS, with extended runtimes and **RDD** (replaceable database drivers) for DBF/CDX/FoxPro-style formats, SQL bridges, etc.

**CA Clipper Tools** (CA library) became a **de facto** extension standard among many clones.

## Hello world

```clipper
Procedure Main       // Or Proc Main
    ? "Hello World!"
Return               // Or Retu
```

## Simple data-entry pattern (illustrative)

Classic `@ ... SAY ... GET ... READ` style (fragment; not a full program):

```clipper
USE Customer SHARED NEW
clear
@  1, 0 SAY "CustNum" GET Customer->CustNum PICT "999999" VALID Customer->CustNum > 0
@  3, 0 SAY "Contact" GET Customer->Contact VALID !empty(Customer->Contact)
@  4, 0 SAY "Address" GET Customer->Address
READ
```

## Version history (selected)

| Era | Examples |
|-----|----------|
| Nantucket “seasonal” | Winter ’84 (1985) … Summer ’87 (1987) |
| Clipper 5.x (Nantucket) | 5.00 (1990), 5.01 (1991), 5.01 Rev.129 (1992) |
| CA-Clipper 5.x | 5.20 (1993) … 5.3b (1997) |

Full release tables appear in historical vendor documentation and archives such as [The Oasis](https://harbour.github.io/the-oasis/docs/) (Clipper/xBase mirror; legacy site [the-oasis.net](http://www.the-oasis.net/)).

## Community / archives

- Usenet (historical): `comp.lang.clipper`, `comp.lang.clipper.visual-objects`
- **Harbour Project:** [https://harbour.github.io/](https://harbour.github.io/)
- CA/Grafx-era site is largely gone; last useful snapshots are often in the [Wayback Machine](https://web.archive.org/web/*/http://www.grafxsoft.com/clipper.htm)

## See also

- [dBase](https://en.wikipedia.org/wiki/DBase) — original interpreted xBase family
- [Harbour](https://en.wikipedia.org/wiki/Harbour_(software)) — open-source Clipper-like compiler
- [Visual FoxPro](https://en.wikipedia.org/wiki/Visual_FoxPro) / **FoxPro** — related xBase lineage on Windows

---

*Summary incorporates material from the Wikipedia article [“Clipper (programming language)”](https://en.wikipedia.org/wiki/Clipper_(programming_language)) (retrieved 2026); [Creative Commons Attribution-ShareAlike 4.0 International License](https://creativecommons.org/licenses/by-sa/4.0/).*
