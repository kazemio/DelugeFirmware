# Refactor Roadmap — Working Context

This file is the saved context for the Deluge refactor-roadmap effort, written so the work can be
picked up cold (by a human or an AI assistant) without re-deriving anything. Last updated: 2026-06-11.

## Artifacts

- `docs/refactor-roadmap.html` — the presentable report. Minimal single-page HTML, no external deps.
  Section order: hero → vision → payoff → the rule (always-shippable) → phases 0–6 (45 PRs with
  why-lines) → priorities → verification → outcome → related work (emulator).
- `docs/refactor-roadmap-detailed.html` — same report plus a "Details —" line per PR citing
  verified file/function specifics. Also adds "The codebase is further along than it looks"
  (modern-C++ toolbox note) after the vision.
- A dev-chat draft was saved to `~/Desktop/deluge-dev-chat-draft.txt` (not in repo); the final
  user-edited text is preserved at the bottom of this file.

## Status / next step

The roadmap is **a proposal, not started work**. The immediate next step is social, not technical:
post the draft message + report to the community-firmware dev chat, get feedback from maintainers
(especially re: overlap with stellar_aria's "firestorm" work and the in-progress QEMU emulator),
and only then begin Phase 0. Nothing has been implemented. No code has changed; the only repo
additions are these three docs files.

## The diagnosis (why this effort exists)

The firmware (~290k lines, single-core Renesas RZ/A1L, 3MB SRAM + 32MB SDRAM, hard real-time
audio) is hard to change safely because its core invariants live in timing and memory pressure,
not in types:

1. **Re-entrant concurrency by convention.** During SD waits, the audio routine is hand-cranked
   from inside the waiting call stack (`routineForSD()`, `AudioEngine::routineWithClusterLoading()`).
   ~400 call sites reference this; 169 sites defer work via
   `ActionResult::REMIND_ME_OUTSIDE_CARD_ROUTINE`. Any function that touches storage has invisible
   yield points where the world can change underfoot — the source of the community's heisenbugs.
2. **Stealable memory.** The `GeneralMemoryAllocator` can "steal" (destruct) live `Stealable`
   objects (sample `Cluster`s) under pressure. Lifetime safety depends on ~70 manual
   `addReason`/`removeReason` call sites being exactly balanced.
3. **Model coupled to hardware.** 43 of 183 `model/` files include `gui/` or `hid/` headers
   (`model_stack.h` itself includes `display.h`), so the heart of the firmware can't compile or
   be tested off-device. ~13 unit-test files exist for 290k lines.
4. Historical cause, not incompetence: ~7 years of one-author commercial development (squashed
   into the May 2023 open-source initial commit), verified by ear on hardware. The community
   inherited the code without the verification method its practices assume.

## The vision (end state)

Same machine code on the same chip; the source gains four properties:
1. One explicit concurrency boundary (audio at interrupt priority, never blocks; lock-free queues).
2. Compiler-enforced cache ownership (RAII `ClusterPin`; use-after-steal unwritable).
3. A hardware-independent `model/` that compiles and runs on a laptop.
4. Asynchronous storage (no synchronous SD waits anywhere).

## The plan: 45 behavior-preserving PRs in 7 phases

Ground rules: functionality NEVER changes; main is shippable after every PR (strangler-fig
migrations, big changes dark behind build flags); one mechanical transformation per PR; each PR
mechanically equivalent or golden-master-verified; ratchet PRs (15, 30, 36, 42) make retired
patterns unexpressible. Phases 0–2 parallel; 5 depends on 2+3; 6 depends on 4+5.

- **Phase 0 (PRs 1–5), verification net, no prod code:** host build of `Serializer`/`Deserializer`
  (`storage/storage_manager.h`: `XMLSerializer`, `JsonSerializer`, `FileWriter`); golden-master
  song round-trips via `Song::readFromFile(Deserializer&)`/`writeToFile()`; sequencer replay
  harness (`PlaybackHandler`); QEMU input scripts (extend `tests/qemu/`, coordinate with community
  emulator); `DELUGE_ASSERT` macro (formalizes existing `ALPHA_OR_BETA_VERSION` +
  `display->freezeWithError("E…")` idiom) + card-routine re-entrancy depth counter.
- **Phase 1 (6–8), invariant visibility, tooling only:** yield-point map script (clang walk from
  `routineForSD` roots over `compile_commands.json`; conservative — virtual dispatch limits it);
  CI diff of the map; generated `/// @yields-to-audio` annotations.
- **Phase 2 (9–16), ClusterPin RAII:** wrap the asymmetric pair `Cluster::addReason()` /
  `AudioFileManager::removeReasonFromCluster(Cluster&, char const* errorCode, …)` in a move-only
  pin (idiom precedent: `memory/allocate_unique.h`). Convert by file: cluster/audio_file →
  audio_file_manager (`loadAnyEnqueuedClusters`, `ClusterPriorityQueue loadingQueue`) →
  holders/wave_table → sample_browser/waveform_renderer → playback path
  (`sample_low_level_reader`, voice unassign). PR 15 privatizes raw refcount (ratchet; note
  `numReasonsHeldBySampleRecorder` special case). PR 16 debug pin-leak detector against
  `numReasonsToBeLoaded` in `Cluster::steal()`/`destroy()`.
- **Phase 3 (17–30), host-runnable model:** `reportFatalError()` free fn replaces `display->`
  calls (16 model files); narrow `UiNotifier` interface; cut `display.h` from `model_stack.h`;
  split `RuntimeFeatureSettings`; move clip minders to `gui/` (pure move); then decouple by
  cluster: note_row → clip part 1 → instrument_clip (4,700 lines, hardest) → song (6,100 lines) →
  instruments/drums → samples → undo (consequence/action_logger) → remainder. PR 30: CI gate,
  host-compilable model file count may never decrease (ratchet).
- **Phase 4 (31–36), one deferral mechanism:** `DeferredActions` = fixed-capacity ring of POD
  command structs (deliberately NOT `std::function` — avoids GMA allocation at the boundary),
  drained from main loop in `deluge.cpp`. Convert sentinel sites by view group; PR 36 deletes
  `REMIND_ME_OUTSIDE_CARD_ROUTINE` from `ActionResult` (`src/definitions_cxx.hpp`) + the
  `allowSomeUserActionsEvenWhenInCardRoutine` escape hatch (ratchet).
- **Phase 5 (37–42), async SD:** request API above `drivers/sd/sd.c`/FatFS (precedent: cluster
  loading is already queued via `loadingQueue`); convert leaf-first: sample preview → waveform
  rendering → metadata reads → streaming prefetcher (PR 41, build-flagged, gated on phases 2+3).
  PR 42 removes `routineForSD()` per converted subsystem; empty yield map = phase 6 precondition.
- **Phase 6 (43–45), concurrency inversion:** render from the SSI/DMA completion interrupt
  (`drivers/ssi`, `drivers/dmac`) behind a build flag — render path must be noexcept end-to-end
  (exceptions are ENABLED in this codebase; `fast_allocator` throws `deluge::exception`); SPSC
  `std::atomic` command queue at the boundary; flip default + delete legacy cranking after
  community soak.

## Honest priority triage (in the report's Priorities section)

- **Do first — phases 0+2 (13 PRs):** high confidence, ~20% of effort for most of the
  risk-adjusted value, retires bugs the community actually hits.
- **Do incrementally — phase 3:** biggest enabler, heavier than advertised (~⅓ of the 43 files
  hide real design decisions). Estimates likely optimistic ~2x.
- **Decide later — phases 4–6:** architecturally right, practically optional; "inversion becomes
  small" is a thesis, not demonstrated; current scheduling demonstrably ships.
- **Known sequencing flaw:** PRs 2–3 partially depend on phase 3 (full `Song`/`PlaybackHandler`
  on host); both start with leaf objects / under QEMU and migrate to host later.
- **Real cost center:** maintainer review bandwidth, not authorship — especially with AI-assisted
  authoring (trust is lower, review is the bottleneck).

## Verified code facts (so they don't need re-deriving)

- STL allowed: `util/containers.h` aliases std containers over custom allocators
  (`deluge::vector` → `memory::external_allocator` → SDRAM via GMA; `fast_allocator` → fast RAM).
  C++23 (`std::expected`). Exceptions ON (`fast_allocator` throws `deluge::exception`,
  caught in `allocate_unique.h` → `std::expected`). RTTI OFF (`-fno-rtti`, CMakeLists.txt:183).
- `routineForSD()` declared `src/deluge/deluge.h:39`; `routineWithClusterLoading(bool)` in
  `processing/engines/audio_engine.h`; `sdRoutineLock` / `allowSomeUserActionsEvenWhenInCardRoutine`
  in `src/deluge/extern.h`.
- `REMIND_ME_OUTSIDE_CARD_ROUTINE` at `src/definitions_cxx.hpp:847` (169 .cpp call sites).
- Cluster API: `addReason()` on `Cluster` (storage/cluster/cluster.h, `numReasonsToBeLoaded`,
  `numReasonsHeldBySampleRecorder`); release via
  `AudioFileManager::removeReasonFromCluster()` (storage/audio/audio_file_manager.h:93). ~70 sites
  across ~10 files.
- 43/183 `model/` files include gui/hid; 16 call `display->`; `freezeWithError` virtual at
  hid/display/display.h:77; `extern Song* currentSong` at model/song/song.h:484.
- Tests: `tests/unit` (CppUTest, 13 files), `tests/32bit_unit_tests` (memory tests + mock GMA),
  `tests/qemu/spec` (one fixedpoint spec), `tests/spec`, `tests/fuzz`, `tests/tracking_allocator.h`.
- Big files: instrument_clip_view.cpp 7,870; song.cpp 6,120; sound.cpp 4,977; session_view.cpp
  4,869; instrument_clip.cpp 4,720; note_row.cpp 4,527.

## Community context

- **stellar_aria's "firestorm" work** — user is excited about it and explicitly does not want to
  build a dollar-store version of it. Check overlap before starting anything.
- **Community QEMU emulator in progress** — complementary (run-without-hardware vs
  prove-changes-safe). Coordinate before Phase 0: if the device model exposes input-injection /
  display-readback hooks, PR 4 shrinks to glue code.
- AI sensitivity: the community had past drama with someone spamming AI-generated PRs. The user
  (writes software professionally) is deliberately transparent about using Anthropic's Fable
  model for the planning, and committed to never dumping unreviewable generated code.

## Dev-chat draft (user's edited final version)

> Hi devs! I see a recurring theme here: working on the firmware is rough because a lot of it is
> coupled to the hardware, and refactors have a way of surfacing heisenbugs weeks later.
>
> I've been looking for ways to contribute and testing how far Anthropic's new model (Fable) can
> go on real-world planning/design so I pointed it at this codebase. What came out is a phased
> roadmap of 45 strictly behavior-preserving PRs aimed at making the code host-testable and making
> certain classes of bugs structurally impossible.
>
> I write software for a living and I'm well aware of AI slop. I did watch that drama unfold a
> while back with someone spamming the repo with AI-generated PRs and the last thing I'd want to
> do is dump un-reviewable generated code on maintainers. That said, while these tools can produce
> some truly outrageous code, they might also be our best bet at completing a refactor of this
> scale (when steered correctly and with oversight).
>
> I've grown to love this thing and want it to keep being the best little not-Ableton it can be.
> So, from the people who really know this codebase well, what do you think of Claude's assessment
> and plan? Is this a value-add worth pursuing or would I be wasting my time? Totally fine either
> way, I'd just rather find out here before I get started.
>
> Plan attached.

## How to resume

1. Read this file, then skim `docs/refactor-roadmap-detailed.html` for the per-PR specifics.
2. Check what dev-chat feedback came in; adjust scope (especially vs firestorm + emulator).
3. If proceeding: start with Phase 0 PR 1 (host build target for `Serializer`/`Deserializer` in
   `tests/`), keeping the always-shippable and one-transformation-per-PR rules absolute.
