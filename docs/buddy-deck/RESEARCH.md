# MAZ Pocket / Buddy Deck: Kickstarter research

Date: 2026-10-06. Method: web search + page reads. Every number has a link. `GUESS` = my estimate, not sourced. Verify before relying on it.

## 0. Bottom line

- Hardware alone is a thin moat. A $30 M5Stack board already runs Anthropic's Buddy firmware.
- The defensible part is the Wi-Fi/Tailscale hub, the action-key ecosystem, and the dev-focused brand. Not the metal case.
- M5Stack itself just raised HK$14.96M on Kickstarter for a Cardputer successor (section 1). Same audience, same form factor. Expect them to compete.
- A metal CNC case is the biggest BOM line at 1k units. Use it as a limited "Founders" tier, not the base product.
- Claude for Open Source is NOT capital and MAZ Pocket probably does not qualify. Do not plan on it (section 5).

## 1. Current AI hardware and agent-control gadgets

| Product | Price | Outcome and lesson | Source |
|---|---|---|---|
| Claude Desktop Buddy (Anthropic, open source) | Runs on ~$30 M5StickC Plus | Official reference. BLE only. Approve/deny via buttons, Tamagotchi-style pet, level-up every 50K tokens, "heart" if approved under 5 s. Tied to the Claude desktop app on macOS/Windows. | [CNX Software](https://www.cnx-software.com/2026/05/16/anthropics-open-source-claude-desktop-buddy-turns-esp32-s3-devices-into-interactive-ai-desk-companions/), [GitHub](https://github.com/anthropics/claude-desktop-buddy) |
| M5Stack Cardputer (Anthropic recommended it) | Cardputer ADV $29.90 | At the "Build with Claude" event Anthropic named the Cardputer a top option for agent-linked hardware. This is your current dev board. | [CNX](https://www.cnx-software.com/2026/05/16/anthropics-open-source-claude-desktop-buddy-turns-esp32-s3-devices-into-interactive-ai-desk-companions/), [M5Stack shop](https://shop.m5stack.com/products/m5stack-cardputer-adv-version-esp32-s3) |
| Claude Buddy ports / clones | ~$50 keyboard device; Waveshare AMOLED boards | Reddit port to a $50 keyboard device (hardware Enter key approves tool calls). ESP32 AMOLED port exists. Clawdmeter (token monitor) and Claudinho (Wi-Fi hooks, Claude Code, shows permission requests) are community projects. Wi-Fi approval already exists as a hobby project. | [Reddit](https://www.reddit.com/r/ClaudeAI/comments/1spxa1t/i_ported_anthropics_claudedesktopbuddy_to_a_50/), [CNX comments](https://www.cnx-software.com/2026/05/16/anthropics-open-source-claude-desktop-buddy-turns-esp32-s3-devices-into-interactive-ai-desk-companions/), [Claudinho](https://github.com/argeuthiesen/claudinho) |
| M5Stack CardputerZero (Kickstarter) | Pocket Pi-class Linux computer | HK$14,960,226 pledged on a HK$78,500 goal, 13,644 backers. $1M stretch goal hit. Proof the pocket-keyboard-computer niche is hot. Also your biggest direct competitor for attention. | [Kickstarter](https://www.kickstarter.com/projects/m5stack/cardputerzero) |
| Flipper Zero | Retail price not re-checked | Kickstarter 2020: $4,882,784 from 37,987 backers. The template for a pocket gadget with a gamified/hacker identity. Successor Flipper One reportedly priced $300-350 (YouTube claim, unverified). | [Kickstarter](https://www.kickstarter.com/projects/flipper-devices/flipper-zero-tamagochi-for-hackers/community), [video](https://www.youtube.com/watch?v=OI_ANlGw4eE) |
| Elgato Stream Deck MK.2 | $149.99 list, ~$119.99 deals | The benchmark for macro keys. 15 LCD keys. Proves people pay $120-150 for programmable keys. | [Elgato](https://www.elgato.com/us/en/p/stream-deck), [Best Buy](https://www.bestbuy.com/product/elgato-stream-deck-mk-2-full-size-wired-usb-keypad-with-15-customizable-lcd-keys-and-interchangeable-faceplate-black/J39QHHVV5X/sku/6470492) |
| Plaud NotePin / Note | NotePin $159; NotePin S $142.99 | Plaud claims 1M+ NotePins sold, profitable, targeting ~$250M (Techmeme post, company claim). Best "AI gadget that works": one job, clear value. | [Techmeme on X](https://x.com/Techmeme/status/1962955145936773410), [Plaud](https://www.plaud.ai/) |
| Rabbit R1 | $199, no subscription | Still shipping. Sold ~100k on pre-orders, then returns; slow replies (up to 10 s) and unreliable "Large Action Model". Lesson: demo vs delivery gap kills trust. | [Layer3 Labs](https://www.layer3labs.io/comparisons/rabbit-r1-vs-humane-ai-pin), [Digital Applied](https://www.digitalapplied.com/blog/ai-product-failures-2026-sora-humane-rabbit-lessons) |
| Humane AI Pin | $699 + $24/mo | Raised ~$230M, shipped under 10k units, discontinued 28 Feb 2025, HP bought IP for ~$116M, devices bricked. Lesson: do not depend on a cloud you can lose, do not replace the phone. | [Digital Applied](https://www.digitalapplied.com/blog/ai-product-failures-2026-sora-humane-rabbit-lessons) |
| Limitless pendant | $99 | Acquired by Meta, sales stopped 5 Dec 2025. Raised $33M+. | [Limitless](https://www.limitless.ai/), [AI Business](https://aibusiness.com/speech-recognition/meta-acquires-limitless) |
| Bee | $49 | Acquired by Amazon, July 2025. | [AI Business](https://aibusiness.com/speech-recognition/meta-acquires-limitless), [Umevo](https://www.umevo.ai/blogs/ume-all-posts/wearable-ai-wars-2026-limitless-pendant-vs-bee-pioneer-vs-plaud-notepin) |
| OpenAI / Jony Ive device | Not announced | OpenAI paid ~$6.4-6.5B for io. Court filing says no shipping before Feb 2027, name "io" dropped. Reported screenless/audio-first. Not a dev-agent tool. | [Business Insider](https://www.businessinsider.com/openai-timeline-hardware-ai-device-launch-jony-ive-iyo-2026-2), [9to5Mac](https://9to5mac.com/2026/02/10/jony-ives-ai-hardware-is-delayed-to-2027-and-wont-be-called-io/) |

Not researched (no time): Teenage Engineering pricing, Rabbit current sales.

What winners got right: one clear job, low price, no mandatory subscription, works on day one (Plaud, Stream Deck, Flipper).
What losers got wrong: replace-the-phone pitch, cloud dependency, vapor demos, subscription on top of hardware.

## 2. Gap and moat for a developer agent-control handheld

Facts:
- Official Buddy is BLE, tied to the desktop app, and the code depends on the M5StickC Plus library ([CNX](https://www.cnx-software.com/2026/05/16/anthropics-open-source-claude-desktop-buddy-turns-esp32-s3-devices-into-interactive-ai-desk-companions/)). Reply says "no API keys or internet connection are needed", i.e. local only.
- One commenter already asks for audio support ([CNX comments](https://www.cnx-software.com/2026/05/16/anthropics-open-source-claude-desktop-buddy-turns-esp32-s3-devices-into-interactive-ai-desk-companions/)).
- Wi-Fi approval exists only as hobby projects (Claudinho above).

Gap (my reading, GUESS on how big):
1. Away-from-desk approval. BLE range is a room. Wi-Fi/Tailscale reaches the sofa, garden, another site.
2. Many agents, many tools, one device. Buddy is Claude-desktop-specific. A hub can front Claude Code, Codex, Gemini CLI, scripts.
3. Stream-Deck-style action keys plus agent launches on a keyboard device. Stream Deck proves $120-150 willingness to pay; nobody sells this in agent form.
4. Push-to-talk to the agent from your pocket.

Moat, honestly:
- Hardware: none. $30 boards do it. Anyone can fork.
- Real moats (GUESS): the hub software and protocol, the action-key library, community firmware, a founder brand, first-mover pack in "agent approval hardware", and a metal premium case as a status object.
- Platform risk: Anthropic can change or extend the Buddy API; M5Stack can ship a Claude-ready Cardputer. Keep the protocol agent-agnostic.
- Differentiate on security: a physical approve key is a safety feature, mention a signed/pairing model (not researched).

## 3. BOM and enclosure

### 3a. Anchors (sourced)

- Cardputer ADV retail $29.90 (retail, includes margin), [M5Stack](https://shop.m5stack.com/products/m5stack-cardputer-adv-version-esp32-s3).
- Waveshare ESP32-S3 1.54" LCD dev board $14.99-18.99 retail, [Waveshare](https://www.waveshare.com/esp32-s3-lcd-1.54.htm).
- ES8311 + NS4150B audio module with mic, listed on AliExpress; one Reddit thread reports a dead mic on an ES8311 board, so budget audio bring-up time ([AliExpress](https://www.aliexpress.com/item/1005011805835653.html), [Reddit](https://www.reddit.com/r/Esphome/comments/1vdpmn0/esp32s3_with_es8311ns4150b_audio_module_anyone/)).
- Your own memory notes: ADV has TCA8418 keyboard, ES8311, no PSRAM, SD CS=G12.

### 3b. BOM estimate (all GUESS, no volume quotes found)

| Item | 1k units | 5k units |
|---|---|---|
| ESP32-S3 module (pre-certified, 8-16 MB flash) | $3.0 | $2.6 |
| 1.14" 240x135 IPS LCD | $1.5 | $1.2 |
| Keyboard (membrane/dome or tactile ~45 keys) + TCA8418 | $3.0 | $2.2 |
| Mic (MEMS) + ES8311 + amp + speaker | $2.5 | $2.0 |
| LiPo ~1000-1750 mAh + charger/PMIC | $3.0 | $2.5 |
| PCB (4-layer) + SMT + passives | $4.0 | $3.0 |
| USB-C, SD slot, buttons, misc | $1.5 | $1.2 |
| Assembly, test, flash | $2.5 | $1.8 |
| Packaging + cable | $2.0 | $1.5 |
| **Electronics + packaging subtotal** | **~$23** | **~$18** |
| Enclosure, see 3c | $5-45 | $3-35 |
| **Total landed ex-freight, ex-cert** | **~$28-68** | **~$21-53** |

Check these with a real quote (JLCPCB assembly, LCSC) before any public number.

### 3c. Enclosure options

Sourced from Davantech (a Dongguan factory, so a supplier's marketing numbers, not a PCBWay/JLC quote):
- CNC aluminium per part $25-80, die cast $3-10 at volume. Die cast tooling $10k-$50k. CNC best for 1-3,000 pcs, die cast for 3,000-100,000+. [Davantech CNC vs molding](https://www.davantech.com/cnc-machining-vs-injection-molding/)
- Enclosure-specific: CNC from billet needs no tooling, best at 1-500 pcs; die cast + CNC tooling $5k-$30k, mold lead time 6-10 weeks; extruded + CNC tooling $1k-$5k, best 100-10,000 pcs; die cast housings anodise poorly (usually powder-coated). [Davantech enclosures](https://www.davantech.com/aluminum-enclosures/)
- Aluminium cuts faster so total cost is lower; stainless is slower with higher tool wear. [JLCCNC](https://jlccnc.com/blog/cnc-enclosure-guide) Another supplier says stainless costs 2-4x 6061 per kilo (excerpt only). [YG Enclosure](https://www.yg-enclosure.com/article/cnc-machined-full-custom-enclosure.html)
- Wall thickness 2-3 mm typical for small aluminium housings; split top/bottom design cuts machining time. [JLCCNC](https://jlccnc.com/blog/cnc-enclosure-guide)
- Anodising Type II adds ~5-25 um, Type III 25-75 um; anodising is insulating, so mask grounding areas. [Davantech](https://www.davantech.com/aluminum-enclosures/)

My estimates (GUESS):
- Small two-piece 6061 case, bead-blast + anodise: ~$15-30 each at 1k, ~$10-20 at 5k. The $25-80 range above likely covers bigger, more complex parts.
- Stainless/steel: roughly 1.5-3x aluminium (supplier excerpt says 2-4x on raw material alone). Use only for a ~200-500 piece premium tier.
- Die cast (zinc or aluminium) tooling $10k-$30k, per-part $3-8 plus machining and finishing; needs ~3,000+ units to beat CNC. MIM: no sourced number, so unknown. Do not promise MIM without a quote.
- Hybrid (GUESS): injection-moulded plastic base + CNC aluminium top plate. Cuts metal cost, keeps the look.

### 3d. Margin sketch (GUESS)

- Retail $149 (below Stream Deck $149.99) with 1k units on aluminium: landed ~$50-60 incl. freight/duties/cert/returns (GUESS) -> gross margin ~60% before Kickstarter fees (about 8-10%, section 4) and marketing.
- Retail $99 plastic version: landed ~$30 -> ~65% gross, same caveats.
- Real cost often missed: returns, support, tooling, certification, VAT, tariffs. Plan 20-30% contingency (GUESS).

## 4. Kickstarter playbook (UK seller)

### 4a. Fees
- Platform fee 5% of collected funds. US processing 3% + $0.20 per pledge (5% + $0.05 under $10). All-in about 8-10%. Fees charged only if funded. Payout ~14 days after end. Dropped pledges 3-5%. [Scaleup Lab, 1 Oct 2026](https://www.thescaleuplab.com/blog/kickstarter-fees-2026)
- Non-US creators see different processing rates (3-5% plus fixed). Check the UK fee page before setting goal. Same source.
- UK VAT is due at point of sale for consignments up to GBP 135; US de minimis exemption suspended since Aug 2025, so ship in bulk to regional warehouses. Same source.

### 4b. Goal and list size
- CardputerZero goal was HK$78,500 (about US$10k, GUESS at 7.8 HKD/USD) and it raised HK$14.96M ([Kickstarter](https://www.kickstarter.com/projects/m5stack/cardputerzero)). Low goal + big audience = social proof. You do not have M5Stack's audience.
- Email-to-backer conversion reports: 0%-5%; one creator got ~0.5% from 2,000 leads; "realistic 2%, optimistic 5%, pessimistic 0.2%". [KickstarterForum](https://www.kickstarterforum.org/kickstarter-and-crowdfunding-questions-only-strict-f4/how-many-prelaunch-emails-t12209.html)
- Blazon agency: standard prelaunch lists convert ~2-3%. [Blazon](https://blazonagency.com/post/kickstarter-marketing-agency-2026)
- $1 reservation deposit lists are claimed to convert far better ("30 times" per LaunchBoom, vendor claim). [LaunchBoom](https://www.launchboom.com/crowdfunding-tips/how-to-build-a-prelaunch-email-list/)
- Sizing example (my arithmetic, GUESS inputs): goal GBP 40k, average pledge GBP 100 -> 400 backers. At 2% conversion, ~20,000 emails; at 5%, 8,000. Plus paid ads/PR. Realistic target before launching: 3,000-10,000 engaged dev emails or 1,000+ paid reservations.
- Day-one funding: aim for 30%+ of goal in the first 48 hours (rule of thumb from the forum thread above).

### 4c. Reward tiers (suggested, GUESS pricing)
1. Super Early Bird, plastic Pocket, GBP 79 (limited 300)
2. Pocket (plastic/hybrid), GBP 99
3. Pocket Metal Founders, CNC aluminium, GBP 169 (serial-numbered, limited 500)
4. Steel Founder, numbered, GBP 249 (limited 100-200)
5. 2-pack / team pack, GBP 189 plastic
6. Add-ons: action-key caps, LoRa/other expansion (CardputerZero sells caps), carry case
Ship only what the hub already does. Rabbit/Humane died on vapor features.

### 4d. Certification (radio devices: ESP32 Wi-Fi and BLE)
- Pre-certified module path: FCC host unintentional-radiator testing $3,000-$8,000, 2-6 weeks. Custom radio $18,000-$58,000, 8-16 weeks. EU CE lab testing $1,500-$3,000+, 4-6 weeks for simple products. Combined FCC+CE on the module path $4,500-$11,000. Pre-compliance $500-$2,000 recommended; failed re-tests $10k-$30k. [Promise Legal, July 2026](https://blog.promise.legal/fcc-certification-ce-marking-connected-devices-hardware-startups/)
- Metal case caution: the module's approval can be voided if you change antenna conditions. A metal enclosure around an ESP32 module affects the antenna, so you need an external/aperture antenna design. Same source on keep-out zones. This is a major engineering risk for metal.
- CE for radio = Radio Equipment Directive (RED) 2014/53/EU, standards EN 300 328, EN 301 489, EN 62368-1; RED Article 3.3 cyber requirements (EN 18031) and the EU Cyber Resilience Act (most obligations from Dec 2027). Same source.
- FCC: Reddit report says about $1,500 / 1-2 weeks for a simple case and about $4k total via a pre-certified module. [r/hwstartups](https://www.reddit.com/r/hwstartups/comments/1sl90et/how_much_does_fcc_certification_actually_cost_ive/) (anecdote)
- UK: CE marking is accepted in Great Britain indefinitely beyond Dec 2024, and the UKCA mark is also accepted; radio equipment is in the list. [trade.gov](https://www.trade.gov/market-intelligence/united-kingdom-extends-ce-mark-recognition), [GOV.UK](https://www.gov.uk/guidance/placing-ukca-or-ce-marked-products-on-the-market-in-great-britain). Practical: do CE (RED) once; it covers GB and EU. UKCA only if you want to. I did not verify the exact UK radio regulations text (Radio Equipment Regulations 2017); confirm with a lab.
- Battery shipping (UN38.3 test, lithium air-freight) was NOT researched. Budget for it (GUESS $1-3k for the test).
- Total GUESS for a first batch: $6k-$15k for certification, plus $2k-$5k for pre-compliance, plus UN38.3.

### 4e. Timeline (GUESS from the sources above)
- Prelaunch 8-16 weeks (list building).
- Campaign 30 days.
- Pledge manager + tooling 2-4 months (die cast tooling 6-10 weeks, per Davantech).
- Fulfilment 6-10 months from campaign end; hardware slips are the norm. Say dates generously in the page.

## 5. Claude for Open Source: status as of 2026-10-06

Official page, fetched today: [claude.com/contact-sales/claude-for-oss](https://claude.com/contact-sales/claude-for-oss). It offers 6 months of free Claude Max 20x.
Current "Who should apply" list (verbatim criteria, summarised):
- Maintainers: 500+ dependent repos, or 100+ dependent packages, or 200,000+ combined monthly downloads.
- Core contributors on a recognised foundation/language project (CPython, Rust, Node.js TSC, Apache PMC, CNCF, Kubernetes, Linux kernel, Django, Rails, or similar).
- Active contributors: 100+ merged PRs into repos they do not own in the last 12 months.
- Community builders: a repo with 20+ unique external contributors with merged PRs in the last 12 months.
- Critical infrastructure: repo with OpenSSF criticality score 0.4+.
- "Don't quite fit? apply anyway and tell us about it."
- At end, the subscription ends; paid plan resumes at prior rate unless cancelled. Terms: [anthropic.com/claude-for-oss-terms](https://www.anthropic.com/claude-for-oss-terms).

Change since launch: the earlier guide (Verdent, March 2026) listed a 5,000+ star / 1M+ monthly npm downloads track, activity in the last 3 months, up to 10,000 recipients, individual only ([Verdent](https://www.verdent.ai/guides/claude-max-20x-open-source)). A LinkedIn post says Claude for Open Source was expanded to more of the community ([LinkedIn, snippet only](https://www.linkedin.com/posts/adocomplete_calling-all-open-source-maintainers-were-activity-7480338514618785792-R6j5)). Launch date 26 Feb 2026 per Verdent. The five-track list above is the current official one; the star-count rule is the old one.

Fit for MAZ Pocket (GUESS, I did not check the repo's stats): a young hardware/firmware repo almost certainly misses all five tracks. Only the "apply anyway" clause is open. It also gives a subscription, not money, not API credits (Verdent), so it does nothing for Kickstarter capital. Cheap to try if the repo is public and has commits, but do not put it in the plan.

## 6. Recommendations (for the plan, not decisions)

1. Ship v1 as a plain Wi-Fi/Tailscale approve/deny + action-key device on the ADV-class design; do not wait for metal.
2. Treat metal as a limited Founders tier; prove the antenna/aperture design before promising it (cert risk).
3. Quote real BOM and enclosure with JLCPCB/PCBWay + JLCCNC at 1k/5k before choosing a price; my numbers above are guesses.
4. Get CE (RED) done on a pre-certified module; budget $6-15k plus UN38.3 (GUESS).
5. Build a list with $1 reservations; target several thousand dev emails before launching; set a low goal tied to MOQ.
6. Keep the hub agent-agnostic so an Anthropic API change cannot kill the product.
7. Open questions: Anthropic API terms for commercial devices, trademark use of "Claude" or Buddy art, M5Stack licensing for any Cardputer-derived design, exact UK fee page.
