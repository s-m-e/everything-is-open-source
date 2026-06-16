#import "@preview/polylux:0.4.0": *
#import "@preview/rustycure:0.2.0": qr-code

#let title = "Everything is Open Source"
#let release = "2026-06-16"
#let url = "github.com/s-m-e/everything-is-open-source"
#let color_bg = rgb("#333132")
#let color_fg = rgb("#bfbfbf")

#set page(
  paper: "presentation-16-9",
  footer: align(
    bottom,
    toolbox.full-width-block(
      fill: color_bg,
      inset: 8mm,
    )[
      #text(size: 12pt)[#release | #title | #url]
      #h(1fr)
      #text(size: 16pt)[#toolbox.slide-number / #toolbox.last-slide-number]
    ]
  ),
  margin: (bottom: 2em, rest: 1em),
  fill: rgb(color_bg),
)
#set text(
  font: "Open Sans",
  size: 20pt,
  fill: color_fg,
)
#show heading: set block(below: 2em)

#slide[
  #set page(footer: none)
  #set align(horizon)

  #qr-code(
    "https://" + url,
    width: 80mm,
    quiet-zone: false,
    dark-color: color_fg,
    light-color: color_bg,
  )

  #text(1.5em)[#title] \
  #text(0.8em)[Rust User Group Leipzig, #release]

  Sebastian M. Ernst \<ernst\@pleiszenburg.de\>

]

#slide[
  = What a time to be alive ...
  #show: later

  - "Anthropic Halts Access to Fable 5 and Mythos 5" (Serve The Home, 2026-06-14)
  - "The U.S. government has banned foreigners from accessing Antropic's latest artificial intelligence" (Maeil Business Korea, 2026-06-13)
  - "Amazon CEO reportedly raised Anthropic model concerns before government crackdown" (Tech Crunch, 2026-06-13)
  - "Amazon and Google have billions riding on Anthropic. The IPO will finally reveal how much." (Fortune, 2026-06-04)
  - "Mythos of Antropic, a real threat to humanity" (la Quotidienne, 2026-04-27)

]

#slide[
  = What is this all about?
  #show: later

  LLMs are breaking the internet, the economy, humanity ...

  ... and *compiled binaries*, apparently.

  - Disassembling and decompiling code is not complicated anymore?
  - Obfuscating functionality within propriety software - still a viable strategy?
  - "Script kiddies" turned "AI pros": Vulnerabilities are easier to find & exploit?
  - What languages, techniques & compilers "still hold" - for now?

  *What new constraints do we need to expect in production projects?*

  *What do we recommend to our employers & clients?*

]

#slide[
  = What is this NOT about?
  #show: later

  German law, known as the "Hackerparagraf":

  *§ 202c StGB Vorbereiten des Ausspähens und Abfangens von Daten*

  (1) Wer eine Straftat nach § 202a oder § 202b vorbereitet, indem er

      1. Passwörter oder sonstige Sicherungscodes, die den Zugang zu Daten (§ 202a Abs. 2) ermöglichen, oder
      2. Computerprogramme, deren Zweck die Begehung einer solchen Tat ist,

  herstellt, sich oder einem anderen verschafft, verkauft, einem anderen überlässt, verbreitet oder sonst zugänglich macht, wird mit Freiheitsstrafe bis zu zwei Jahren oder mit Geldstrafe bestraft.

  (2) § 149 Abs. 2 und 3 gilt entsprechend.

]

#slide[
  = What is this NOT about?
  #show: later

  German law, known as the "Hackerparagraf", machine-translated:

  *§ 202c StGB Preparation for the unauthorized interception or accessing of data*

  (1) Whoever prepares to commit an offense under § 202a or § 202b by producing, procuring for oneself or another, selling, making available to another, distributing, or otherwise making accessible

    1. passwords or other security codes that enable access to data (§ 202a para. 2), or
    2. computer programs designed for the commission of such an offense,

  shall be liable to imprisonment for a term not exceeding two years or to a fine.

  (2) § 149 paras. 2 and 3 shall apply accordingly.

]

#slide[
  = What is this NOT about?
  #show: later

  Looking into binaries not compiled from open source or your own code,
  *unless you have explicit written permission* to do so.

  By whom, you might ask? Good question.

]

#slide[
  = Rumors & hypotheses
  #show: later

  1. Plain C via `gcc` is a fairly trivial target? Compare to O#lower[n] / LLVM?
  2. Rust "appears" to be good, for now?
  3. Anything LLVM-based is ok-ish? C via LLVM?

  #show: later

  = Ideas for further experiments
  #show: later

  - Python compiled via Cython
  - Anything Go
  - Custom state machines / VMs / run-times?

  What comes to *YOUR* mind?

]

#slide[
  = Further reading

  #show: later
  - https://en.wikipedia.org/wiki/Ghidras
]
