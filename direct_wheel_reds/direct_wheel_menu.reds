// Menu-active detection via direct menu-controller lifecycle wrapping.
//
// We wrap the CP2077 classes that fire OnInitialize when a gameplay-
// blocking menu opens, and OnUninitialize when it closes. Each wrap
// sends a unique string tag to the plugin so the log tells us
// empirically which controllers fire for which on-screen events.
// The plugin maintains a set of currently-open tags; menuActive =
// (set non-empty), so duplicates from a derived + base wrap both
// firing are absorbed.
//
// Prior attempts:
//   - gameuiMenuGameController was always initialized at game load
//     and never uninitialized, inflating depth permanently. Removed.
//   - gameuiBaseMenuGameController does not have OnInitialize /
//     OnUninitialize at its level (redscript compile error
//     "no method with this name exists on the target type"). Removed.
//
// Current candidates:
//   - gameuiInGameMenuGameController — pause menu. Exists; needs
//     empirical verification whether the 2.31 pause-menu subclass
//     actually dispatches through this class's OnInitialize.
//   - SingleplayerMenuGameController — singleplayer hub (map,
//     inventory, journal, perks, crafting, phone). Community-
//     documented class name; validated at compile time.

@wrapMethod(gameuiInGameMenuGameController)
protected cb func OnInitialize() -> Bool {
  DirectWheel_MenuOpen("InGameMenu");
  return wrappedMethod();
}

@wrapMethod(gameuiInGameMenuGameController)
protected cb func OnUninitialize() -> Bool {
  DirectWheel_MenuClose("InGameMenu");
  return wrappedMethod();
}

@wrapMethod(SingleplayerMenuGameController)
protected cb func OnInitialize() -> Bool {
  DirectWheel_MenuOpen("SingleplayerMenu");
  return wrappedMethod();
}

@wrapMethod(SingleplayerMenuGameController)
protected cb func OnUninitialize() -> Bool {
  DirectWheel_MenuClose("SingleplayerMenu");
  return wrappedMethod();
}
