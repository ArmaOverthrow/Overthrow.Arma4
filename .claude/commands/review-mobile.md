---
description: "Run a comprehensive, stack-aware mobile app quality review (auto-detects React Native/Expo, Flutter, native iOS, native Android, Capacitor). Usage: /review-mobile [scope]"
---

You have been asked to run a comprehensive, **stack-aware** mobile app quality review.

**Scope:** `$ARGUMENTS`

This command first DETECTS the mobile stack, then runs (A) universal dimensions that apply to any mobile app on both platforms, plus (B/C) the concrete checks for the detected stack. The **React Native / Expo** path is the deepest (first-class) path; Flutter / native iOS / native Android / Capacitor get solid detect-and-branch coverage that can be deepened with a focused `/deep-research` pass when a project actually uses that stack.

## Epic awareness

This command is epic-aware. Before resolving `$ARGUMENTS` to a feature path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, fuzzy-fall-back into
epic folders for bare names, and operate across all of the epic's features when the target is an epic (aggregate findings, keep per-feature attribution).
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution. This command's body is the review-mobile workflow; it does **not** re-specify those rules. `$ARGUMENTS` here is normally a scope keyword (`all` / `universal` / a layer name / `deep`). Only when it resolves to an **epic** (bare name with `epic-overview.md`, not a scope keyword) follow the **Epic-scope mode** below; stack detection (Step 0) and every layer are otherwise unchanged.

### Epic-scope mode (when `$ARGUMENTS` is an epic)

Review the **mobile surface across all of the epic's features**, then aggregate — keeping which feature each finding belongs to.

1. **Detect the stack ONCE** (Step 0) for the whole app — the stack + versions are project-wide, so detect and announce them a single time, not per feature.
2. **Read the epic.** Read `docs/features/<epic>/epic-overview.md` (Features table = the feature set) and map each child feature to the screens/components it owns under `$MOBILE_DIR` (from its `context.md` Key Files).
3. **Run the layers over the union of those screens** — Universal (Part A) + the detected stack's layers (Part B/C). Cross-feature consistency (shared design tokens, navigation integrity between features, duplicated list/image patterns) is the high-value signal a single-feature pass misses.
4. **Keep per-feature attribution.** Findings are `file:line + severity`; prefix each with the feature it lands in — `[base] app/(tabs)/x.tsx:42` , `[ux] …`. Group the Findings Summary per feature (or carry the `[feature]` prefix on every line). Never a flat list that drops the feature. Project-wide findings (e.g. Metro export fails, New-Arch not enabled) are attributed to the epic/app, not a single feature.
5. **One combined Findings Summary** for the epic (the layer table aggregates across features) with the detected stack header, then **STOP and ask** exactly as the single-scope flow.

A non-epic `$ARGUMENTS` (a scope keyword or empty) is unchanged — follow Step 0 and the layers below.

## CRITICAL RULE: Report First, Fix Later

**NEVER auto-fix without explicit user approval.**

1. Detect stack -> run the relevant checks
2. Collect + analyse findings
3. Present findings summary
4. **STOP and ask the user** which findings to fix
5. Only implement fixes the user explicitly approves

---

## Step 0 - Detect the Mobile Stack (ALWAYS FIRST)

Identify the stack + the mobile project dir + which platforms ship, then branch. Set `MOBILE_DIR` to the detected app root (a top-level app, or e.g. `apps/mobile` / `packages/mobile` in a monorepo - sniff the repo).

| Signal (search the repo) | Stack |
|---|---|
| `package.json` deps include `react-native` AND (`expo` or `expo-router`) | **React Native / Expo** (Part B - deep path) |
| `package.json` has `react-native` but NOT `expo` | **React Native (bare)** (Part B, skip Expo-only layers) |
| `pubspec.yaml` with `flutter:` | **Flutter** (Part C) |
| `capacitor.config.{ts,js,json}` or `ionic.config.json` | **Capacitor / Ionic hybrid** (Part C) |
| `*.xcodeproj` / `*.xcworkspace` / `Package.swift` + `.swift` files, no RN/Flutter | **Native iOS** (Part C) |
| `build.gradle(.kts)` + Kotlin/Java app module, no RN/Flutter | **Native Android** (Part C) |

Detection commands:
```bash
# RN/Expo + key versions (drives version-correct rules below)
cat "$MOBILE_DIR/package.json" 2>/dev/null | grep -E '"(expo|react-native|react|@shopify/flash-list|react-native-reanimated|expo-router|expo-image|expo-secure-store)"'
# Flutter / native / hybrid
ls pubspec.yaml capacitor.config.* ionic.config.json Package.swift 2>/dev/null
find . -maxdepth 3 \( -name "*.xcodeproj" -o -name "build.gradle*" \) -not -path "*/node_modules/*" 2>/dev/null | head
```

**Announce the result before reviewing**, e.g.: `Detected: React Native / Expo (RN 0.83, Expo SDK 55, FlashList 2.x, Reanimated 4.x, New Architecture). Running Universal + RN/Expo deep layers.` Version detection matters - several rules below flip based on FlashList major version, Reanimated major version, and Expo SDK.

### Scope argument
- empty / `all` -> Universal + the full detected-stack layer set
- `universal` -> Part A only
- a single layer name (e.g. `touch`, `a11y`, `perf`, `security`, `lists`, `new-arch`, `metro`, `types`) -> that layer only
- `deep` -> everything, including the heavier commands (Metro export, expo-doctor, madge)

---

## Part A - Universal Dimensions (every stack, iOS + Android)

These are the cross-cutting majority of mobile quality. Apply to ALL stacks; only the platform thresholds / concrete tools differ (noted per stack in B/C). Items marked `[verified]` were checked against primary docs (see provenance note).

1. **Touch targets** - interactive elements >= **44pt (iOS HIG)** / **48dp (Android Material)**; small elements need an expanded hit area; primary actions reachable one-handed. NOTE the WCAG floor is only **24x24 CSS px** (SC 2.5.8, Level AA, with Spacing/Inline/Essential exceptions); 44/48 are the stricter *platform* guidance and 44px is WCAG **AAA** (SC 2.5.5). Cite 24px as the bare AA minimum but use the platform numbers as the operational floor. Domain apps may need larger (e.g. gloved/industrial users). Sources: w3.org/WAI/WCAG22/Understanding/target-size-minimum, target-size-enhanced. [verified]
2. **Accessibility - labels & roles** - every interactive element exposes a role + a meaningful label + state; images have text alternatives or are marked decorative; modals trap focus.
3. **Accessibility - dynamic type / font scaling** - UI must not break when OS font size is enlarged; don't blanket-disable scaling. `allowFontScaling` defaults to true; setting it `false` (esp. globally via `Text.defaultProps`) is an anti-pattern - **cap** with `maxFontSizeMultiplier` (>=1) instead. WCAG SC 1.4.4 (AA): text resizes to **200%** without loss. Check: `grep -rIn 'allowFontScaling={false}'` -> flag; flag long/critical text with no `maxFontSizeMultiplier`. (Native equivalents: iOS Dynamic Type text styles; Android `sp` units + don't lock `fontScale`.) Sources: reactnative.dev/docs/text, w3.org/TR/WCAG22 (1.4.4). [verified]
4. **Accessibility - reduce motion** - honour the OS "reduce motion" setting for non-essential animation. Check: `grep -rIn 'isReduceMotionEnabled|useReducedMotion|reduceMotionChanged'` - if the app uses an animation lib but has ZERO reduce-motion references, flag ungated motion. Source: reactnative.dev/docs/accessibilityinfo. [verified]
5. **Accessibility - reading/focus order & language** - control screen-reader order intentionally: a container marked accessible **groups its children into ONE** focusable element (fragments order if unintended); hide decorative/overlay layers (iOS `accessibilityElementsHidden` / Android `importantForAccessibility="no-hide-descendants"`), which also fixes focus on overlapping/absolute layers; set the content language for correct pronunciation. Source: reactnative.dev/docs/accessibility. [verified]
6. **Colour contrast** - WCAG **4.5:1** normal text, **3:1** large text (>=18pt or 14pt bold), AND **3:1** for UI components / graphical objects vs adjacent colours (SC 1.4.11 Non-text Contrast). Verify text on coloured backgrounds, low-opacity overlays, dark-on-dark, and control borders/icons. Source: w3.org/TR/WCAG22, w3.org/TR/wcag2mobile-22. [verified]
7. **Loading / error / empty states** - every async surface shows loading, handles error with a retry, and shows an empty state; mutations disable + show progress.
8. **Performance** - no avoidable per-frame/render work; long lists virtualised; images right-sized; animations on the compositor not the JS/main thread. (Concrete per stack below.)
9. **Security - secret storage** - tokens/secrets in the OS secure store (iOS **Keychain** / Android **Keystore**), NOT plaintext key-value (AsyncStorage / SharedPreferences / NSUserDefaults). RN/Expo: `expo-secure-store`. Check: `grep -rIn 'AsyncStorage' | grep -iE 'token|jwt|secret|password|refresh|access'` -> flag. (Caveat: SecureStore ~2KB/value; Android values don't survive uninstall.) Sources: docs.expo.dev/.../sdk/securestore, reactnative.dev/docs/security. [verified]
10. **Security - no secrets in the bundle** - EVERYTHING in the client bundle is PUBLIC. In Expo, `EXPO_PUBLIC_*` vars are inlined into the JS, `app.json`/`app.config` `extra` ships in the bundle, and an EAS "Secret" only protects the value on EAS servers - once embedded it's readable. No API keys/tokens/private keys in shipped code; true secrets live server-side (proxy) or in native secure storage. Checks: `grep -rIn -E "(api[_-]?key|secret|token|password|private[_-]?key)\s*[:=]\s*['\"][A-Za-z0-9_-]{12,}"`; `grep -rIn 'EXPO_PUBLIC_' | grep -iE 'secret|private|password|token'`; inspect `app.json`/`app.config` `extra`. Sources: docs.expo.dev/guides/environment-variables, eas/environment-variables. [verified]
11. **Security - deep-link / universal-link validation** - inbound link/intent params are UNTRUSTED input and the framework does NOT validate them for you (React Navigation `parse` is type-conversion only; Expo Router `redirectSystemPath` path "is not guaranteed valid"). Validate via allowlist/schema/host-check before using a param as a URL, redirect, id lookup, auth token, or WebView source. Source: reactnavigation.org/docs/deep-linking. [verified]
   - (Cert pinning / jailbreak-root detection are NOT default requirements - no primary source mandates them for typical apps; reserve for high-value finance/health apps.)
12. **Permissions hygiene** - request only permissions actually used; each has a purpose string/rationale; request at point-of-use, not all at launch. (Expo: docs.expo.dev/guides/permissions.)
13. **Crash resilience** - a top-level error boundary; crash/error reporting wired (e.g. Sentry); unhandled promise rejections captured; no crash on null/undefined from the network. [standard practice; not adversarially verified - re-verify if challenged]
14. **Offline / network resilience** - graceful offline behaviour; retries with backoff on transient failures; no infinite spinners when the network is down.
15. **i18n / RTL** - user-facing strings externalised (not hardcoded) if the app is/will be localised; layouts survive RTL. (Expo: docs.expo.dev/guides/localization. Skip if explicitly single-locale.)
16. **App config & assets** - icon/splash present at correct density; orientation/permissions declared in the native manifest/Info.plist match what's used; app size not bloated by stray assets.

---

## Part B - React Native / Expo (DEEPEST path)

Scan `$MOBILE_DIR/{app,src,components,lib}` (use a thorough search agent for large apps). Report each finding as `file:line` + severity + fix. These are the concrete RN/Expo implementations of Part A plus RN-specific layers. Adapt token/style paths to the project's design-token module (e.g. `lib/styles.ts`, `theme/`, NativeWind config).

### Core RN layers

**Touch targets** - `grep -rn 'height:\s*[0-3][0-9],\|minHeight:\s*[0-3][0-9],' "$MOBILE_DIR"` ; ensure Pressable/Touchable >= platform floor (44/48), `hitSlop` on small elements, tab items >= 56dp.

**Font compliance** - no tiny `fontSize` (< 12); prefer shared font-size tokens. `grep -rn 'fontSize:\s*[0-9]\b' "$MOBILE_DIR" --include="*.tsx"`. (See Universal #3 for scaling.)

**Design token enforcement** - no hardcoded hex / raw spacing / raw radii; use the design-token module. `grep -rn "'#[0-9A-Fa-f]\{3,8\}'" "$MOBILE_DIR" --include="*.tsx"`.

**Type safety** - zero `as any`, no `@ts-ignore` (use `@ts-expect-error` + reason); typed router (`Href`). `grep -rn 'as any\|@ts-ignore' "$MOBILE_DIR" --include="*.tsx"`.

**Accessibility** - every shared interactive component has `accessibilityRole` + `accessibilityLabel`; modals `accessibilityViewIsModal`; images role+label. PLUS Universal #3/#4/#5.

**Performance** - no inline arrows in JSX props for list rows; list items `React.memo`; `useCallback` for handlers passed as props; `useMemo` for derived; **granular store selectors** (e.g. Zustand - never `state => state`). `grep -rn 'onPress={() =>' "$MOBILE_DIR" --include="*.tsx" | grep -v useCallback`.

**Safe area & layout** - screen files use `SafeAreaView` (react-native-safe-area-context); tab screens `edges={['top']}`; `TextInput` screens use `KeyboardAvoidingView`; floating elements respect insets.

**Loading / error / empty states** - spinner/skeleton while loading; error + retry; empty state; buttons disable+spin on mutation.

**Haptics** - interactive `onPress` triggers haptics where appropriate (standard/destructive/selection intensities).

**Metro bundle** (CRITICAL, run first):
```bash
export NVM_DIR="$HOME/.nvm" && [ -s "$NVM_DIR/nvm.sh" ] && . "$NVM_DIR/nvm.sh"
cd "$MOBILE_DIR" && npx expo export --platform android --dev 2>&1 | grep -E 'Error|failed|Exported'
```
Must show `Exported`; any `Error`/`failed` is the #1 priority.

**Circular deps** - `npx madge --circular "$MOBILE_DIR"/{app,src,components,lib}` - any cycle is HIGH.

**Navigation integrity** - every `router.push|navigate` path matches a real file in the `app/` routes.

**Null safety** - guard `.toFixed()` (APIs return strings -> `Number()`), `.toLowerCase()`, `.map()` on possibly-undefined.

**Image null-uri** - `<Image source={{ uri }}>` handles null/undefined (fallback). (See expo-image layer for recycling/cache.)

**Storage key conflicts** - unique, prefixed AsyncStorage keys; no duplicates across stores. (Cross-check Universal #9: secrets must NOT be here.)

**Expo Doctor** - `cd "$MOBILE_DIR" && npx expo-doctor` - any failed check is HIGH (also the New-Arch readiness signal, below).

### 2025-2026 RN layers (version-matched)

**FlashList - VERSION-AWARE (the old "must have estimatedItemSize" rule is now WRONG on v2).**
Detect the version: `grep '@shopify/flash-list' "$MOBILE_DIR/package.json"`.
- **FlashList v2.x**: a New-Architecture-only rewrite that **auto-sizes** and **removed** `estimatedItemSize` / `estimatedListSize` / `estimatedFirstItemOffset`. **Flag the PRESENCE** of those props as stale v1 leftovers: `grep -rn 'estimatedItemSize\|estimatedListSize\|estimatedFirstItemOffset' "$MOBILE_DIR" --include="*.tsx"` -> should be ZERO. Still check: no plain `FlatList` for large lists, `keyExtractor` present, New Arch enabled (v2 requires it). Sources: shopify.github.io/flash-list/docs/v2-migration, /v2-changes. [verified]
- **FlashList v1.x**: original rule holds - `FlashList` MUST have `estimatedItemSize`. [verified]

**New Architecture readiness** (RN 0.76+; default in Expo SDK 52+, mandatory SDK 55+).
- Confirm enabled: SDK 55+ is always-on; for SDK 52-54 check `newArchEnabled` isn't `false`.
- Third-party **native** deps are the risk (pure-JS libs work unchanged): `npx expo-doctor` validates against React Native Directory; cross-check flagged libs at reactnative.directory/?newArchitecture=true. Treat `expo-*` first-party packages as compatible. Sources: docs.expo.dev/guides/new-architecture. [verified]
- (Do NOT use these refuted heuristics: a `codegenConfig` block is NOT required, and grepping `UIManager.dispatchViewManagerCommand`/`requireNativeComponent` as a "migration smell" did not verify.)

**expo-image in recycled lists** (vs RN `Image`).
- In FlashList/recycled `renderItem`, an `expo-image` `<Image>` should set **`recyclingKey`** (resets to placeholder before loading the new source - prevents showing the previous row's image). Flag list-row `<Image` without it. Source: docs.expo.dev/.../sdk/image. [verified]
- Flag `cachePolicy={'memory'}` on large image lists (purged aggressively -> OOM); prefer `disk`/`memory-disk` + matched `placeholderContentFit`. Source: expo/expo#26903. [verified]

**Reanimated worklets / perf - VERSION-AWARE.**
- **Reanimated 4.x is New-Architecture-only.** If `react-native-reanimated` >= 4, New Arch must be enabled.
- **Babel plugin moved:** v4 uses `react-native-worklets/plugin`, NOT `react-native-reanimated/plugin`. `grep -n 'react-native-reanimated/plugin' "$MOBILE_DIR/babel.config.js"` -> flag if reanimated >= 4.
- **API rename:** `runOnJS` -> `scheduleOnRN`, `runOnUI` -> `scheduleOnUI` (now in `react-native-worklets`). Grep BOTH for the overuse heuristic.
- **Shared-value misuse:** reading `sv.value` on the JS thread (render/handlers, outside a worklet/`useAnimatedStyle`) blocks the JS thread - flag `.value` reads outside worklet contexts; fix with `useAnimatedReaction` + `scheduleOnRN`. Source: docs.swmansion.com/.../performance. [verified]
- **Animate compositor props, not layout:** flag `useAnimatedStyle` animating `top/left/width/height/margin/padding` (per-frame layout recalc); prefer `transform` + `opacity`. [verified]

**EAS Update / OTA correctness** - read `runtimeVersion` in app.json/app.config:
- **Prefer `policy: "fingerprint"` over `"appVersion"`.** `appVersion` only bumps the runtime version when the app version is bumped, so forgetting to bump it while changing the native runtime causes a silent mismatch -> crash. `fingerprint` bumps whenever anything affecting the native runtime changes. Flag `runtimeVersion.policy: "appVersion"` as a footgun. (Refuted myth: `appVersion` is NOT the recommended default.)
- **Native changes can't ship via JS-only OTA** - native dep/config changes need a NEW build + bumped runtimeVersion.
- **Channels map to build profiles** in `eas.json`; confirm the mapping.
- **Rollback path exists**: `eas update:republish` a prior update, or roll back to the embedded build. Sources: docs.expo.dev/eas-update/runtime-versions, /rollbacks. [verified]

**Crash resilience (RN)** [standard practice; not adversarially verified] - top-level error boundary (e.g. Sentry `ErrorBoundary`); crash reporting initialised; unhandled promise rejections captured.

---

## Part C - Detect-and-branch (solid coverage; deepen on demand)

For a non-RN stack, run Part A (universal) + the matching section. These are real headline checks, not stubs. **To bring any of these to RN-level depth, run a focused `/deep-research` pass for that stack's current best practices, then expand this section.**

### Flutter (pubspec.yaml)
- `flutter analyze` (gate clean) + `dart format --set-exit-if-changed .`.
- A11y: tappables have a `Semantics` label / `tooltip`; images `semanticLabel`; respect `MediaQuery.textScaler` (no hardcoded sizes); honour `MediaQuery.disableAnimations` (reduce motion).
- Touch: `InkWell`/`GestureDetector` targets >= 48dp (`kMinInteractiveDimension`).
- Perf: `const` constructors; avoid whole-tree rebuilds (selectors/`ValueListenableBuilder`); long lists use `ListView.builder`/`SliverList`; images via `cached_network_image`.
- Security: secrets via `flutter_secure_storage` not `shared_preferences`; no keys in source; validate deep links (`go_router`/`uni_links`).
- States: `FutureBuilder`/`StreamBuilder` handle loading + error + empty.
- Crash: `runZonedGuarded` + `FlutterError.onError` -> crash reporting.

### Native iOS (Swift / SwiftUI / UIKit)
- Build/lint: `swiftlint` (if present); 0 warnings; no blanket `// swiftlint:disable`.
- A11y: controls have `accessibilityLabel`/`accessibilityValue`/traits; support **Dynamic Type** (text styles `.font(.body)`, no fixed point sizes); honour `UIAccessibility.isReduceMotionEnabled`; sensible VoiceOver order.
- Touch: tappable >= **44x44pt** (HIG).
- Security: secrets in **Keychain** (not `UserDefaults`); ATS not globally disabled (`NSAllowsArbitraryLoads`); validate universal-link params.
- Perf: heavy work off the main thread (no sync I/O in `body`/`viewDidLoad`); `List`/`LazyVStack` for long content; image caching.
- States + crash: loading/error/empty on async; crash reporter initialised.
- Permissions: every `NS*UsageDescription` maps to a used capability.

### Native Android (Kotlin / Jetpack Compose / Views)
- Build/lint: `./gradlew lint` (gate on errors); `ktlint`/`detekt` if present; no unexplained `@Suppress`.
- A11y: `contentDescription` on images/icon-buttons (or null if decorative); Compose `Modifier.semantics`; text in `sp` (support font scale, don't lock `fontScale`); honour reduce-motion (`ANIMATOR_DURATION_SCALE`).
- Touch: tappable >= **48x48dp** (`minimumInteractiveComponentSize`).
- Security: secrets via **EncryptedSharedPreferences / Keystore** not plain `SharedPreferences`; no keys in source/`BuildConfig`; exported components validate input; no `usesCleartextTraffic` in prod.
- Perf (Compose): avoid unstable params (recomposition); `LazyColumn`/`LazyRow` (with `key`) for long lists; `remember`/`derivedStateOf`; image caching (Coil).
- States + crash: loading/error/empty on async; crash reporter initialised.
- Permissions: every `<uses-permission>` is used; runtime permissions at point-of-use.

### Capacitor / Ionic (web -> native shell)
- The web app gets the standard web review; ALSO: `capacitor.config` `server.cleartext`/`allowNavigation` not overly permissive; secrets via a secure plugin (not `localStorage`); native permissions in the shells match plugin usage; deep-link (`appUrlOpen`) params validated; touch targets meet 44/48dp; safe-area via CSS `env()` insets.

---

## Severity Classification

- **CRITICAL** - breaks usability / crashes / data-loss / secret leak (token in plaintext storage, untappable control, Metro build fails)
- **HIGH** - a11y failure (screen reader broken, contrast/dynamic-type fail), New-Arch incompatibility, circular dep, stale FlashList-v1 props on v2, `appVersion` runtimeVersion footgun
- **MEDIUM** - best-practice violation (inline arrow in list row, layout-prop animation, missing recyclingKey, hardcoded token)
- **LOW** - polish / minor consistency
- **INFO** - already correct (document good patterns)

---

## Present Findings Summary

```
## /review-mobile Findings - Detected stack: <stack> (<versions>)

| Layer | Status | Critical | High | Medium | Low |
|-------|--------|----------|------|--------|-----|
| (Universal + detected-stack layers) | | | | | |

### Critical / High / Medium / Low (grouped, file:line)
### What's Already Good
### Outdated-rule / version notes (e.g. "FlashList 2.x -> estimatedItemSize correctly absent")

---
What would you like to fix?  "fix all" / "fix critical" / "fix [items]" / "none"
```

## Wait for User Response

Do NOT fix anything until the user explicitly says what to fix.

---

## Important Notes

- **Detect first, always.** Don't assume React Native - run Step 0 and announce the stack + versions.
- **Version-gated rules:** FlashList (v1 requires `estimatedItemSize`; v2 forbids it), Reanimated (v4 = New-Arch-only + worklets plugin + `scheduleOnRN`), Expo SDK (55+ = New Arch mandatory). Re-read versions from package.json before applying.
- The non-RN branches (Part C) are deliberately lighter; deepen with `/deep-research` when a project adopts that stack.
- **Domain extensions:** add project-specific layers as needed (e.g. oversized touch targets / high-contrast for industrial or gloved users, a design-token enforcement pass against your theme module).
- Research provenance: items tagged `[verified]` were adversarially verified against primary docs (W3C WCAG 2.2, React Native, Expo, React Navigation, Shopify FlashList, Software Mansion Reanimated) via deep-research passes (2026-06). Items tagged `[standard practice]` (crash resilience, cert pinning) have primary sources but were not adversarially verified - treat as strong guidance, re-verify if challenged. Re-verify version-gated rules against current docs over time.
