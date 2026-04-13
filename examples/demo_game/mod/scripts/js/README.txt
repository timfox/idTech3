JavaScript demo scripts for idtech3_demo (Duktape)

Path policy: files under scripts/js/ are loadable with:
  js_reload scripts/js/<file>.js

From a .cfg in the mod:
  js_reload scripts/js/demo_hooks.js

Requires engine built with USE_DUKTAPE. Defaults: js_allowEvents=1, js_allowExec=1.
